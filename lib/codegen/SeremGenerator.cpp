/// @file SeremGenerator.cpp
/// Typed Sere AST to Serem lowering for the initial supported language slice.

#include "sere/codegen/SeremGenerator.h"

#include "sere/diag/DiagnosticEngine.h"
#include "sere/types/Type.h"
#include "sere/types/TypeContext.h"

#include <charconv>
#include <limits>
#include <unordered_set>
#include <utility>

namespace sere {
namespace {

[[nodiscard]] serem::IRType integerType(const Type* type) {
  if (type->isNamed("i8"))
    return serem::IRType::i8();
  if (type->isNamed("i16"))
    return serem::IRType::i16();
  if (type->isNamed("i32"))
    return serem::IRType::i32();
  if (type->isNamed("u8"))
    return serem::IRType::u8();
  if (type->isNamed("u16"))
    return serem::IRType::u16();
  if (type->isNamed("u32"))
    return serem::IRType::u32();
  if (type->isNamed("u64"))
    return serem::IRType::u64();
  return serem::IRType::i64();
}

[[nodiscard]] const Type* functionType(const FunctionDef& function) {
  return function.resolvedType();
}

/// Element interpretation for list repr and list construction. The code is part
/// of the Serem dialect contract so the backend layout and the runtime formatter
/// agree without either side keeping its own type-name switch.
[[nodiscard]] serem::ListElementKind listElementKindOf(const Type* type) {
  if (type == nullptr)
    return serem::ListElementKind::Ptr;
  const Type* canonical = type->canonical();
  if (canonical->isNamed("str"))
    return serem::ListElementKind::Str;
  if (canonical->isNamed("bool"))
    return serem::ListElementKind::Bool;
  if (canonical->isNamed("f32"))
    return serem::ListElementKind::Float32;
  if (canonical->isNamed("f64"))
    return serem::ListElementKind::Float64;
  if (canonical->isNamed("i32") || canonical->isNamed("u32"))
    return serem::ListElementKind::Int32;
  if (canonical->isNamed("i8"))
    return serem::ListElementKind::Int8;
  if (canonical->isNamed("i16"))
    return serem::ListElementKind::Int16;
  if (canonical->isNamed("u8"))
    return serem::ListElementKind::UInt8;
  if (canonical->isNamed("u16"))
    return serem::ListElementKind::UInt16;
  if (canonical->isInteger())
    return serem::ListElementKind::Int64;
  return serem::ListElementKind::Ptr;
}

[[nodiscard]] std::string listElementKindText(const Type* type) {
  return std::to_string(static_cast<std::int32_t>(listElementKindOf(type)));
}

/// Key interpretation the runtime dict hashes with: 1 int32, 2 int64, 3 string.
/// Any other key is compared bytewise, which the runtime names 0.
[[nodiscard]] std::string dictKeyKindText(const Type* type) {
  if (type == nullptr)
    return "0";
  const Type* canonical = type->canonical();
  if (canonical->isNamed("str"))
    return "3";
  if (canonical->isNamed("i32") || canonical->isNamed("u32"))
    return "1";
  if (canonical->isInteger())
    return "2";
  return "0";
}

/// Whether a record carries its concrete type in the first word of its storage.
/// Enums keep a variant tag of their own and structs are plain values, so only
/// classes get the header.
[[nodiscard]] bool recordHasTypeId(const Type* type) {
  return type != nullptr && type->isRecord() && !type->isEnum() && !type->isStruct();
}

/// Physical slot of a field inside a record. The type-id header occupies slot
/// zero, so every semantic field index shifts by one.
[[nodiscard]] int fieldSlot(const Type* record, int index) {
  if (index < 0 || !recordHasTypeId(record)) {
    return index;
  }
  return index + 1;
}

/// Use the semantic symbol so inherited statics share storage while unrelated
/// classes (including nested classes) keep their fields separate.
[[nodiscard]] std::string staticFieldSymbol(const MemberExpr& member) {
  const Type* record = member.object().resolvedType();
  if (record == nullptr)
    return {};
  record = record->canonical();
  if (record->isTypeObject())
    record = record->typeObjectInstance();
  if (record == nullptr)
    return {};
  const RecordField* field = record->findField(member.field());
  if (field == nullptr || !field->isStatic || !field->stored)
    return {};
  const std::string owner =
      record->qualifier().empty() ? record->name() : record->qualifier() + "." + record->name();
  return field->llvmName.empty() ? owner + "." + member.field() : field->llvmName;
}

/// Discriminant of the enum variant a constructor call names, or an empty
/// string for a non-enum constructor. Enums store the variant index in the
/// first word, so `Message.Goodbye(...)` must tag its payload with that index
/// rather than the arm position it happens to match against.
[[nodiscard]] std::string enumVariantTag(const CallExpr& expression) {
  const Type* type = expression.resolvedType();
  if (type == nullptr || !type->isEnum() || expression.callee().kind() != NodeKind::MemberExpr) {
    return {};
  }
  const auto& member = static_cast<const MemberExpr&>(expression.callee());
  const RecordField* field = type->canonical()->findField(member.field());
  return field == nullptr ? std::string{} : field->llvmName;
}

/// Discriminant an enum variant is stored under. Sema records it as the
/// decimal value the variant declares, so reading it back keeps `Color.Green`
/// equal to 2 both as a value and inside `match`.
[[nodiscard]] std::int32_t enumTagOf(const RecordField* field) {
  if (field == nullptr || field->llvmName.empty()) {
    return 0;
  }
  char* end = nullptr;
  const long long parsed = std::strtoll(field->llvmName.c_str(), &end, 10);
  if (end == field->llvmName.c_str() || *end != '\0') {
    return 0;
  }
  if (parsed < std::numeric_limits<std::int32_t>::min() ||
      parsed > std::numeric_limits<std::int32_t>::max()) {
    return 0;
  }
  return static_cast<std::int32_t>(parsed);
}

/// The enum an expression names, looking through a `Class[T]`-style type object
/// so `Color` and `Color.Green` resolve to the same record.
[[nodiscard]] const Type* enumRecordOf(const Type* type) {
  if (type == nullptr)
    return nullptr;
  const Type* canonical = type->canonical();
  if (canonical->isTypeObject())
    canonical = canonical->typeObjectInstance();
  if (canonical == nullptr)
    return nullptr;
  canonical = canonical->canonical();
  return canonical->isEnum() ? canonical : nullptr;
}

/// Tag a value of `type` is stored under inside `unionType`, mirroring the
/// boxing side: a class the union does not list verbatim takes the tag of the
/// member it derives from.
[[nodiscard]] int unionMemberFor(const Type* unionType, const Type* type) {
  if (unionType == nullptr || type == nullptr || !unionType->isUnion()) {
    return -1;
  }
  const int exact = unionType->unionMemberIndex(type);
  if (exact >= 0) {
    return exact;
  }
  const Type* canonical = type->canonical();
  const std::vector<const Type*>& members = unionType->args();
  for (std::size_t index = 0; index < members.size(); ++index) {
    if (members[index] != nullptr && canonical->isSubtypeOf(members[index]->canonical())) {
      return static_cast<int>(index);
    }
  }
  return -1;
}

/// Tags of the union members that are themselves instances of `type`, so
/// `is Base` on a `Sub | Other` union matches through `Sub`'s tag.
[[nodiscard]] std::string unionMemberTagsDerivedFrom(const Type* unionType, const Type* type) {
  if (unionType == nullptr || type == nullptr || !unionType->isUnion()) {
    return {};
  }
  const Type* canonical = type->canonical();
  std::string tags;
  const std::vector<const Type*>& members = unionType->args();
  for (std::size_t index = 0; index < members.size(); ++index) {
    if (members[index] == nullptr || !members[index]->canonical()->isSubtypeOf(canonical)) {
      continue;
    }
    if (!tags.empty()) {
      tags += ',';
    }
    tags += std::to_string(index);
  }
  return tags;
}

/// Appends one class name to an exception chain without duplicating a name that
/// already appears through a shared base.
void appendExceptionName(std::string& chain, const std::string& name) {
  if (name.empty()) {
    return;
  }
  std::size_t pos = 0;
  while (pos < chain.size()) {
    const std::size_t stop = chain.find(';', pos);
    const std::size_t end = stop == std::string::npos ? chain.size() : stop;
    if (end - pos == name.size() && chain.compare(pos, end - pos, name) == 0) {
      return;
    }
    if (stop == std::string::npos) {
      break;
    }
    pos = stop + 1;
  }
  if (!chain.empty()) {
    chain += ';';
  }
  chain.append(name);
}

/// Builds the `Sub;Base;Exception` chain the runtime matches `sere_error_isa`
/// against, so `except Base` catches an instance of `Sub`.
void appendExceptionType(std::string& chain, const Type* type) {
  if (type == nullptr) {
    return;
  }
  appendExceptionName(chain, type->name());
  const Type* canonical = type->canonical();
  if (canonical != nullptr && canonical != type) {
    appendExceptionName(chain, canonical->name());
    type = canonical;
  }
  for (const Type* base : type->bases()) {
    appendExceptionType(chain, base);
  }
}

} // namespace

SeremGenerator::SeremGenerator(DiagnosticEngine& diagnostics, TypeContext& types)
    : diagnostics_(&diagnostics), types_(&types) {
}

serem::IRType SeremGenerator::lowerType(const Type* type) const {
  if (type == nullptr || type->isVoidLike())
    return serem::IRType::voidType();
  if (!subst_.empty()) {
    type = types_->substitute(type, subst_);
    if (type == nullptr)
      return serem::IRType::ptr(serem::IRType::i8());
  }
  // An instantiated generic record is named after its type arguments, which the
  // record branches below keep, so the instance's layout is not confused with
  // the generic one.
  const bool instanceRecord = type->isRecord() && (type->name().find('[') != std::string::npos);
  if (!instanceRecord) {
    type = type->canonical();
  }
  if (type->isNamed("bool"))
    return serem::IRType::boolType();
  if (type->isInteger())
    return integerType(type);
  if (type->isNamed("f32"))
    return serem::IRType::f32();
  if (type->isNamed("f64"))
    return serem::IRType::f64();
  if (type->isNamed("str"))
    return serem::IRType::stringType();
  if (type->isPointerLike())
    return serem::IRType::ptr(lowerType(type->pointeeType()));
  if (type->isGenericCtor("tuple")) {
    std::vector<serem::IRType> fields;
    for (const Type* element : type->args())
      fields.push_back(lowerType(element));
    return serem::IRType::structType(type->display(), std::move(fields));
  }
  if (type->kind() == TypeKind::Function) {
    std::vector<serem::IRType> params;
    for (const Type* param : type->paramTypes())
      params.push_back(lowerType(param));
    return serem::IRType::function(lowerType(type->returnType()), std::move(params));
  }
  if (type->isRecord() && !type->isEnum() && !type->isStruct()) {
    return serem::IRType::ptr(serem::IRType::structType(type->name()));
  }
  if (type->isRecord() && !type->isEnum()) {
    std::vector<serem::IRType> fields;
    for (const RecordField& field : type->fields()) {
      if (!field.isStatic && field.stored)
        fields.push_back(lowerType(field.type));
    }
    return serem::IRType::structType(type->name(), std::move(fields));
  }
  if (type->isEnum()) {
    // An enum without payloads is just its discriminant, the way the direct
    // backend lowers it; only a variant carrying data needs the aggregate.
    if (!type->hasEnumPayload())
      return serem::IRType::i32();
    return serem::IRType::structType(
        type->name(), {serem::IRType::i32(), serem::IRType::ptr(serem::IRType::i8())});
  }
  if (type->isUnion())
    return serem::IRType::structType("union", {serem::IRType::i32(), serem::IRType::i64()});
  return serem::IRType::ptr(serem::IRType::i8());
}

std::string SeremGenerator::functionName(const FunctionDef& function) const {
  const auto known = functionNames_.find(&function);
  if (known != functionNames_.end())
    return known->second;
  if (function.isExtern())
    return function.externName();
  // An imported declaration names its own module, which is also the prefix the
  // call site resolves through sema; preferring it keeps both spellings equal.
  const std::string& module = function.modulePrefix();
  if (function.isMethod() && !function.ownerClass().empty()) {
    if (!module.empty())
      return module + "_" + function.ownerClass() + "." + function.name();
    return function.ownerClass() + "." + function.name();
  }
  if (!module.empty())
    return module + "_" + function.name();
  return function.name();
}

std::unique_ptr<serem::IRModule>
SeremGenerator::emit(const Module& module,
                     std::string moduleName,
                     const std::vector<const Module*>* imported,
                     const std::vector<std::string>* importedNames) {
  module_ = std::make_unique<serem::IRModule>(std::move(moduleName));
  functions_.clear();
  definitions_.clear();
  functionSymbols_.clear();
  methodSymbols_.clear();
  decorators_.clear();
  classBases_.clear();
  classFields_.clear();
  classes_.clear();
  functionNames_.clear();
  std::vector<const Module*> modules{&module};
  if (imported != nullptr)
    modules.insert(modules.end(), imported->begin(), imported->end());
  for (const Module* current : modules)
    declareTypes(*current);
  declareInstances(modules);
  for (const Module* current : modules)
    for (const std::unique_ptr<Stmt>& statement : current->statements()) {
      if (statement != nullptr && statement->kind() == NodeKind::ClassDef) {
        const auto& classDef = static_cast<const ClassDef&>(*statement);
        if (!classDef.bases().empty())
          classBases_[classDef.name()] = classDef.bases().front();
      }
    }
  for (std::size_t moduleIndex = 0; moduleIndex < modules.size(); ++moduleIndex) {
    const Module* current = modules[moduleIndex];
    const std::string prefix =
        moduleIndex == 0 || importedNames == nullptr || moduleIndex - 1 >= importedNames->size()
            ? std::string{}
            : (*importedNames)[moduleIndex - 1];
    for (const std::unique_ptr<Stmt>& statement : current->statements()) {
      if (statement != nullptr && statement->kind() == NodeKind::FunctionDef) {
        const auto& function = static_cast<const FunctionDef&>(*statement);
        // The function's own module prefix wins: it is what the call site names.
        if (!prefix.empty() && !function.isExtern() && function.modulePrefix().empty()) {
          functionNames_[&function] = prefix + "_" + function.name();
        }
        if (!function.decorators().empty()) {
          decorators_[functionName(function)] = function.decorators().front();
          decorators_[function.name()] = function.decorators().front();
        }
        const Type* type = functionType(function);
        if (type != nullptr) {
          const std::string symbol = functionName(function);
          functions_.insert_or_assign(symbol, lowerType(type));
          definitions_[symbol] = &function;
          functionSymbols_.insert_or_assign(function.name(), symbol);
        }
      } else if (statement != nullptr && statement->kind() == NodeKind::ClassDef) {
        const auto& classDef = static_cast<const ClassDef&>(*statement);
        // A generic class only has concrete methods once a type argument is
        // known, so its unspecialised bodies are skipped and the per-instantiation
        // symbols registered below stand in for them.
        const bool genericClass =
            classDef.resolvedType() != nullptr && !classDef.resolvedType()->typeParams().empty();
        if (genericClass) {
          continue;
        }
        for (const std::unique_ptr<FunctionDef>& method : classDef.methods()) {
          if (!prefix.empty())
            functionNames_[method.get()] = prefix + "_" + functionName(*method);
          const Type* type = functionType(*method);
          if (type != nullptr) {
            functions_.insert_or_assign(functionName(*method), lowerType(type));
            definitions_[functionName(*method)] = method.get();
            methodSymbols_[classDef.name() + "::" + method->name()] = functionName(*method);
          }
        }
      } else if (statement != nullptr && statement->kind() == NodeKind::EnumDef) {
        // An enum's methods take the enum itself as the receiver, so they are
        // named after it the way a class names its own.
        const auto& enumDef = static_cast<const EnumDef&>(*statement);
        if (!enumDef.typeParams().empty())
          continue;
        for (const std::unique_ptr<FunctionDef>& method : enumDef.methods()) {
          const Type* type = functionType(*method);
          if (type == nullptr) {
            continue;
          }
          const std::string symbol = enumDef.name() + "." + method->name();
          functionNames_[method.get()] = symbol;
          functions_.insert_or_assign(symbol, lowerType(type));
          definitions_[symbol] = method.get();
          methodSymbols_[enumDef.name() + "::" + method->name()] = symbol;
        }
      }
    }
  }
  // Generic instantiations are emitted per concrete type argument, so a call
  // site can name the specialized symbol directly.
  for (const FunctionInstantiation& inst : types_->functionInstantiations()) {
    if (inst.specializedType != nullptr && !functions_.contains(inst.llvmName)) {
      functions_.insert_or_assign(inst.llvmName, lowerType(inst.specializedType));
    }
  }
  // A method of a generic class exists once per instantiated type argument, and
  // the instance owns the specialized symbol the call site has to name.
  for (const auto& entry : std::vector(types_->instantiations())) {
    const Type* generic = entry.first;
    const Type* instance = entry.second;
    if (instance->hasTypeParameters())
      continue;
    if (generic == nullptr || instance == nullptr || generic->typeParams().empty() ||
        generic->typeParams().size() != instance->args().size()) {
      continue;
    }
    for (const RecordMethod& method : instance->methods()) {
      if (method.llvmName.empty() || method.type == nullptr || !method.typeParams.empty()) {
        continue;
      }
      functions_.insert_or_assign(method.llvmName, lowerType(method.type));
      methodSymbols_[instance->name() + "::" + method.name] = method.llvmName;
    }
  }
  for (const Module* current : modules) {
    for (const auto& statement : current->statements()) {
      if (statement->kind() == NodeKind::FunctionDef &&
          !emitFunction(static_cast<const FunctionDef&>(*statement)))
        return nullptr;
      if (statement->kind() == NodeKind::ClassDef) {
        const auto& classDef = static_cast<const ClassDef&>(*statement);
        if (classDef.resolvedType() != nullptr && !classDef.resolvedType()->typeParams().empty()) {
          continue;
        }
        for (const auto& method : classDef.methods())
          if (!emitFunction(*method))
            return nullptr;
      }
      if (statement->kind() == NodeKind::EnumDef) {
        if (!static_cast<const EnumDef&>(*statement).typeParams().empty())
          continue;
        for (const auto& method : static_cast<const EnumDef&>(*statement).methods())
          if (!emitFunction(*method))
            return nullptr;
      }
    }
  }
  for (const FunctionInstantiation& inst : types_->functionInstantiations()) {
    subst_.clear();
    for (std::size_t index = 0; index < inst.typeParams.size() && index < inst.args.size();
         ++index) {
      subst_[inst.typeParams[index]] = inst.args[index];
    }
    const FunctionDef* source = nullptr;
    for (const Module* current : modules) {
      for (const auto& statement : current->statements()) {
        if (statement->kind() == NodeKind::FunctionDef &&
            static_cast<const FunctionDef&>(*statement).name() == inst.sourceName) {
          source = static_cast<const FunctionDef*>(statement.get());
          break;
        }
        const std::vector<std::unique_ptr<FunctionDef>>* methods = nullptr;
        const Type* owner = nullptr;
        if (statement->kind() == NodeKind::ClassDef) {
          const auto& def = static_cast<const ClassDef&>(*statement);
          methods = &def.methods();
          owner = def.resolvedType();
        } else if (statement->kind() == NodeKind::EnumDef) {
          const auto& def = static_cast<const EnumDef&>(*statement);
          methods = &def.methods();
          owner = def.resolvedType();
        }
        if (methods != nullptr && inst.isMethod) {
          const Type* receiver = inst.specializedType->paramTypes()[0];
          for (const auto& method : *methods) {
            const int index = receiver->methodIndex(method->name());
            if (index >= 0 && receiver->methods()[index].llvmName == inst.sourceName &&
                receiver->name().substr(0, receiver->name().find('[')) == owner->name()) {
              source = method.get();
              for (std::size_t i = 0; i < owner->typeParams().size(); ++i)
                subst_[owner->typeParams()[i]] = receiver->args()[i];
              receiverOverride_ = receiver;
              break;
            }
          }
        }
        if (source != nullptr)
          break;
      }
      if (source != nullptr)
        break;
    }
    if (source != nullptr && !emitFunction(*source, inst.llvmName)) {
      subst_.clear();
      return nullptr;
    }
    receiverOverride_ = nullptr;
    subst_.clear();
  }
  // A generic class or enum has no body of its own: each instance is lowered
  // with its type arguments substituted, under the symbol the instance declares.
  for (const auto& entry : std::vector(types_->instantiations())) {
    const Type* generic = entry.first;
    const Type* instance = entry.second;
    if (instance->hasTypeParameters())
      continue;
    if (generic == nullptr || instance == nullptr || generic->typeParams().empty() ||
        generic->typeParams().size() != instance->args().size()) {
      continue;
    }
    const std::vector<std::unique_ptr<FunctionDef>>* sourceMethods = nullptr;
    for (const Module* current : modules) {
      for (const auto& statement : current->statements()) {
        if (statement->kind() == NodeKind::ClassDef &&
            static_cast<const ClassDef&>(*statement).resolvedType() == generic) {
          sourceMethods = &static_cast<const ClassDef&>(*statement).methods();
          break;
        }
        // An enum instantiation is matched by name: a generic enum's
        // `resolvedType` stays the unspecialised declaration, while the
        // instance's record carries the specialised methods.
        if (statement->kind() == NodeKind::EnumDef &&
            static_cast<const EnumDef&>(*statement).name() == generic->name()) {
          sourceMethods = &static_cast<const EnumDef&>(*statement).methods();
          break;
        }
      }
      if (sourceMethods != nullptr) {
        break;
      }
    }
    if (sourceMethods == nullptr) {
      continue;
    }
    subst_.clear();
    for (std::size_t index = 0; index < generic->typeParams().size(); ++index) {
      subst_[generic->typeParams()[index]] = instance->args()[index];
    }
    const std::vector<RecordMethod>& methods = instance->methods();
    const std::size_t count = std::min(sourceMethods->size(), methods.size());
    for (std::size_t index = 0; index < count; ++index) {
      if (methods[index].llvmName.empty() || !methods[index].typeParams.empty()) {
        continue;
      }
      const Type* previousReceiver = receiverOverride_;
      receiverOverride_ = instance;
      const bool ok = emitFunction(*(*sourceMethods)[index], methods[index].llvmName);
      receiverOverride_ = previousReceiver;
      if (!ok) {
        subst_.clear();
        return nullptr;
      }
    }
    subst_.clear();
  }
  return std::move(module_);
}

void SeremGenerator::declareTypes(const Module& module) {
  for (const std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement == nullptr)
      continue;
    if (statement->kind() == NodeKind::ClassDef) {
      const auto& classDef = static_cast<const ClassDef&>(*statement);
      if (classDef.resolvedType() != nullptr)
        classes_.push_back(classDef.resolvedType());
      declareClass(classDef);
    } else if (statement->kind() == NodeKind::EnumDef) {
      declareEnum(static_cast<const EnumDef&>(*statement));
    }
  }
}

void SeremGenerator::declareClass(const ClassDef& classDef) {
  std::vector<serem::IRType> fields;
  std::vector<std::string> attributes;
  attributes.push_back(classDef.isStruct() ? "struct" : "class");
  // The first word of a class is its type id, so a value boxed into a union can
  // still be recognised as the subclass it really is. A derived class reuses the
  // base header instead of adding a second one.
  if (!classDef.isStruct()) {
    fields.push_back(serem::IRType::i32());
  }
  if (!classDef.bases().empty()) {
    const auto base = classFields_.find(classDef.bases().front());
    if (base != classFields_.end())
      fields = base->second;
  }
  std::size_t fieldIndex = 0;
  fieldIndex = fields.size();
  for (const FieldDecl& field : classDef.fields()) {
    if (!field.isStatic) {
      fields.push_back(lowerType(field.type == nullptr ? nullptr : field.type->resolvedType()));
      attributes.push_back("field=" + field.name + ":" + std::to_string(fieldIndex));
      ++fieldIndex;
    }
  }
  classFields_[classDef.name()] = fields;
  for (const std::string& decorator : classDef.decorators())
    attributes.push_back("decorator=" + decorator);
  (void)module_->addType(std::make_unique<serem::TypeDef>(
      classDef.name(),
      serem::IRType::structType(classDef.name(), std::move(fields)),
      std::move(attributes)));
}

void SeremGenerator::declareInstances(const std::vector<const Module*>& modules) {
  for (const auto& entry : std::vector(types_->instantiations())) {
    const Type* generic = entry.first;
    const Type* instance = entry.second;
    if (instance->hasTypeParameters())
      continue;
    if (generic == nullptr || instance == nullptr || generic->typeParams().empty() ||
        generic->typeParams().size() != instance->args().size()) {
      continue;
    }
    const ClassDef* classDef = nullptr;
    for (const Module* current : modules) {
      for (const std::unique_ptr<Stmt>& statement : current->statements()) {
        if (statement != nullptr && statement->kind() == NodeKind::ClassDef &&
            static_cast<const ClassDef&>(*statement).resolvedType() == generic) {
          classDef = static_cast<const ClassDef*>(statement.get());
          break;
        }
      }
      if (classDef != nullptr) {
        break;
      }
    }
    if (classDef == nullptr) {
      continue;
    }
    std::vector<serem::IRType> fields;
    std::vector<std::string> attributes;
    attributes.push_back(classDef->isStruct() ? "struct" : "class");
    // The layout mirrors the generic declaration, with the type arguments
    // substituted, so both see the same slot numbers.
    if (!classDef->isStruct()) {
      fields.push_back(serem::IRType::i32());
    }
    if (!classDef->bases().empty()) {
      const auto base = classFields_.find(classDef->bases().front());
      if (base != classFields_.end())
        fields = base->second;
    }
    std::size_t fieldIndex = fields.size();
    for (const RecordField& field : instance->fields()) {
      if (field.isStatic) {
        continue;
      }
      fields.push_back(lowerType(field.type));
      attributes.push_back("field=" + field.name + ":" + std::to_string(fieldIndex));
      ++fieldIndex;
    }
    (void)module_->addType(std::make_unique<serem::TypeDef>(
        instance->name(),
        serem::IRType::structType(instance->name(), std::move(fields)),
        std::move(attributes)));
  }
}

void SeremGenerator::declareEnum(const EnumDef& enumDef) {
  std::vector<std::string> attributes;
  attributes.push_back("enum");
  if (enumDef.isFlags())
    attributes.push_back("flags");
  for (const EnumVariant& variant : enumDef.variants())
    attributes.push_back("variant=" + variant.name);
  (void)module_->addType(std::make_unique<serem::TypeDef>(
      enumDef.name(),
      serem::IRType::structType(enumDef.name(),
                                {serem::IRType::i32(), serem::IRType::ptr(serem::IRType::i8())}),
      std::move(attributes)));
}

bool SeremGenerator::emitFunction(const FunctionDef& function, std::string symbol) {
  const Type* genericType = functionType(function);
  if (genericType == nullptr)
    return unsupported(function, "function without a resolved type");
  // A generic instantiation substitutes the caller's type arguments, so the
  // emitted parameters, locals, and return type are concrete types.
  const Type* type = subst_.empty() ? genericType : types_->substitute(genericType, subst_);
  if (type == nullptr)
    type = genericType;
  // A specialised method is lowered against the instantiated class, so its
  // receiver reads the instance's layout instead of the generic one. The
  // receiver is overridden while lowering rather than by rebuilding the function
  // type, which would canonicalise the instance back to the generic class.
  std::vector<serem::IRType> params;
  for (std::size_t index = 0; index < type->paramTypes().size(); ++index) {
    const Type* param =
        index == 0 && receiverOverride_ != nullptr ? receiverOverride_ : type->paramTypes()[index];
    params.push_back(lowerType(param));
  }
  auto irFunction =
      std::make_unique<serem::IRFunction>(symbol.empty() ? functionName(function) : symbol,
                                          std::move(params),
                                          lowerType(type->returnType()));
  if (function.isExtern() && function.body().empty()) {
    // Extern declarations are defined by the runtime or a C library. Emitting a
    // body here would clash with that definition at link time, so keep the
    // function bodyless and let the backend emit a declaration.
    irFunction->setExternal(true);
    (void)module_->addFunction(std::move(irFunction));
    return true;
  }
  function_ = &module_->addFunction(std::move(irFunction));
  currentOwnerClass_ = function.ownerClass();
  function_->setAsync(function.isAsync());
  function_->setGenerator(function.isGenerator());
  function_->setExternal(function.isExtern());
  if (function.isExtern()) {
    // External functions are declarations only: the implementation is supplied by
    // the runtime library or a linked native library under the extern symbol.
    return true;
  }
  for (const std::string& decorator : function.decorators()) {
    function_->setAttribute("decorator." + decorator, "true");
  }
  builder_ = std::make_unique<serem::IRBuilder>(*function_);
  locals_.clear();
  localTypes_.clear();
  functionStatics_.clear();
  staticTypes_.clear();
  defers_.clear();
  tryHandlers_.clear();
  returnType_ = type->returnType();
  coroutineToken_.reset();
  if (function.isAsync() || function.isGenerator()) {
    // A generator coroutine's promise holds the yielded element type, so the
    // backend can size the frame and the iterator can read it back.
    const Type* element = type->returnType();
    if (function.isGenerator() && element != nullptr && element->isGenericCtor("Iterator")) {
      element = element->genericArg(0);
    }
    coroutineToken_ = builder_->coroBegin(lowerType(element), function.isGenerator());
  }
  for (std::size_t index = 0; index < function.params().size(); ++index) {
    if (index < function_->arguments().size()) {
      locals_[function.params()[index].name] = function_->argument(index);
      localTypes_[function.params()[index].name] = index == 0 && receiverOverride_ != nullptr
                                                       ? receiverOverride_
                                                       : type->paramTypes()[index];
    }
  }
  if (!emitBlock(function.body()))
    return false;
  if (coroutineToken_ != nullptr && function.isGenerator()) {
    // A generator body falls through to its final suspend; there is no return
    // statement to emit.
    if (!builder_->currentBlock().isTerminated()) {
      emitDeferred();
      builder_->coroEnd();
    }
  } else if (!builder_->currentBlock().isTerminated()) {
    emitDeferred();
    if (type->returnType() != nullptr && type->returnType()->isVoidLike()) {
      builder_->retVoid();
    } else {
      (void)builder_->unreachable();
    }
    if (coroutineToken_ != nullptr)
      builder_->coroEnd();
  }
  builder_.reset();
  function_ = nullptr;
  currentOwnerClass_.clear();
  locals_.clear();
  defers_.clear();
  tryHandlers_.clear();
  return true;
}

void SeremGenerator::pushFunctionState() {
  FunctionState state;
  state.builder = std::move(builder_);
  state.function = function_;
  state.locals = std::move(locals_);
  state.localTypes = std::move(localTypes_);
  state.statics = std::move(functionStatics_);
  state.staticTypes = std::move(staticTypes_);
  state.returnType = returnType_;
  state.ownerClass = std::move(currentOwnerClass_);
  state.tryHandlers = std::move(tryHandlers_);
  state.defers = std::move(defers_);
  state.breakTargets = std::move(breakTargets_);
  state.continueTargets = std::move(continueTargets_);
  state.coroutine = std::move(coroutineToken_);
  state.captures = std::move(captureSymbols_);
  savedStates_.push_back(std::move(state));
  builder_.reset();
  function_ = nullptr;
  locals_.clear();
  localTypes_.clear();
  functionStatics_.clear();
  staticTypes_.clear();
  returnType_ = nullptr;
  currentOwnerClass_.clear();
  tryHandlers_.clear();
  defers_.clear();
  breakTargets_.clear();
  continueTargets_.clear();
  coroutineToken_.reset();
  captureSymbols_.clear();
}

void SeremGenerator::popFunctionState() {
  if (savedStates_.empty())
    return;
  FunctionState state = std::move(savedStates_.back());
  savedStates_.pop_back();
  builder_ = std::move(state.builder);
  function_ = state.function;
  locals_ = std::move(state.locals);
  localTypes_ = std::move(state.localTypes);
  functionStatics_ = std::move(state.statics);
  staticTypes_ = std::move(state.staticTypes);
  returnType_ = state.returnType;
  currentOwnerClass_ = std::move(state.ownerClass);
  tryHandlers_ = std::move(state.tryHandlers);
  defers_ = std::move(state.defers);
  breakTargets_ = std::move(state.breakTargets);
  continueTargets_ = std::move(state.continueTargets);
  coroutineToken_ = std::move(state.coroutine);
  captureSymbols_ = std::move(state.captures);
}

bool SeremGenerator::emitLambdaFunction(const LambdaExpr& expression) {
  const Type* type = expression.resolvedType();
  if (type == nullptr || expression.llvmName().empty()) {
    return unsupported(expression, "lambda without a resolved signature");
  }
  pushFunctionState();
  std::vector<serem::IRType> params;
  for (const Type* param : type->paramTypes())
    params.push_back(lowerType(param));
  auto irFunction = std::make_unique<serem::IRFunction>(
      expression.llvmName(), std::move(params), lowerType(type->returnType()));
  function_ = &module_->addFunction(std::move(irFunction));
  builder_ = std::make_unique<serem::IRBuilder>(*function_);
  returnType_ = type->returnType();
  for (std::size_t index = 0; index < expression.params().size(); ++index) {
    if (index >= function_->arguments().size())
      break;
    locals_[expression.params()[index].name] = function_->argument(index);
    localTypes_[expression.params()[index].name] =
        index < type->paramTypes().size() ? type->paramTypes()[index] : nullptr;
  }
  serem::ValuePtr result = emitExpression(expression.body());
  if (!builder_->currentBlock().isTerminated()) {
    if (result == nullptr) {
      builder_->retVoid();
    } else {
      builder_->ret(
          coerce(std::move(result), expression.body().resolvedType(), type->returnType()));
    }
  }
  popFunctionState();
  functions_.insert_or_assign(expression.llvmName(), lowerType(type));
  return true;
}

bool SeremGenerator::emitNestedFunction(const FunctionDef& function) {
  const Type* type = functionType(function);
  if (type == nullptr)
    return unsupported(function, "nested function without a resolved type");
  const std::string symbol = function.name();
  // A nested body reaches an outer local through a module global, which is what
  // a function value passed between them can carry in this ABI.
  std::unordered_map<std::string, std::string> captures;
  for (const FunctionDef::Capture& capture : function.captures()) {
    const std::string slot = "sere.capture." + symbol + "." + capture.name;
    const auto known = localTypes_.find(capture.name);
    const Type* stored = known == localTypes_.end() ? capture.type : known->second;
    const serem::IRType irType = lowerType(stored);
    serem::ValuePtr value = local(capture.name);
    if (value != nullptr && value->valueKind() != serem::ValueKind::Argument) {
      value = builder_->load(value, irType);
    }
    if (value != nullptr && irType.kind() == serem::IRType::Kind::Function) {
      // A function value is parked as an address so the nested frame can call it
      // indirectly.
      value = builder_->operation("cast.value",
                                  serem::IRType::ptr(serem::IRType::i8()),
                                  {std::move(value)},
                                  {{"unsigned", "false"}});
    }
    (void)builder_->operation(
        "static.set", serem::IRType::voidType(), {std::move(value)}, {{"symbol", slot}});
    captures.insert_or_assign(capture.name, slot);
  }
  pushFunctionState();
  captureSymbols_ = std::move(captures);
  const bool ok = emitFunction(function, symbol);
  popFunctionState();
  if (ok) {
    functionNames_[&function] = symbol;
    functions_.insert_or_assign(symbol, lowerType(type));
  }
  return ok;
}

serem::ValuePtr
SeremGenerator::emitBoundMethod(const Type* owner, const std::string& method, const Expr& object) {
  const Type* record = owner == nullptr ? nullptr : owner->valueType();
  const std::string symbol = methodSymbol(record, method);
  if (symbol.empty())
    return nullptr;
  const auto found = functions_.find(symbol);
  if (found == functions_.end())
    return nullptr;
  const serem::IRType methodType = found->second;
  if (methodType.kind() != serem::IRType::Kind::Function || methodType.parameters().empty()) {
    return nullptr;
  }
  // The receiver outlives the expression that produced the bound method, so it
  // is parked in a global the thunk reads back.
  const std::size_t ordinal = boundThunks_++;
  const std::string slot = "sere.bound." + symbol + "." + std::to_string(ordinal);
  const serem::IRType receiverType = methodType.parameters().front();
  const std::string thunkName = symbol + "$bound$" + std::to_string(ordinal);
  const std::vector<serem::IRType> boundParams(methodType.parameters().begin() + 1,
                                               methodType.parameters().end());
  // The assignment is part of the enclosing expression's order, so bind first.
  (void)builder_->operation(
      "static.set", serem::IRType::voidType(), {emitExpression(object)}, {{"symbol", slot}});
  const serem::IRType boundType = serem::IRType::function(*methodType.pointee(), boundParams);
  pushFunctionState();
  auto thunk = std::make_unique<serem::IRFunction>(thunkName, boundParams, *methodType.pointee());
  function_ = &module_->addFunction(std::move(thunk));
  builder_ = std::make_unique<serem::IRBuilder>(*function_);
  returnType_ = nullptr;
  std::vector<serem::ValuePtr> arguments;
  arguments.push_back(builder_->operation("static.get", receiverType, {}, {{"symbol", slot}}));
  for (std::size_t index = 0; index < boundParams.size(); ++index) {
    arguments.push_back(function_->argument(index));
  }
  serem::ValuePtr inner = builder_->call(std::make_shared<serem::FunctionRef>(symbol, methodType),
                                         std::move(arguments),
                                         *methodType.pointee());
  if (!builder_->currentBlock().isTerminated()) {
    if (methodType.pointee()->isVoid()) {
      builder_->retVoid();
    } else {
      builder_->ret(std::move(inner));
    }
  }
  popFunctionState();
  functions_.insert_or_assign(thunkName, boundType);
  return std::make_shared<serem::FunctionRef>(thunkName, boundType);
}

bool SeremGenerator::emitBlock(const std::vector<std::unique_ptr<Stmt>>& statements) {
  for (const std::unique_ptr<Stmt>& statement : statements) {
    if (statement != nullptr && !emitStatement(*statement))
      return false;
  }
  return true;
}

void SeremGenerator::emitDeferred() {
  // Snapshot the pending defers so a defer body that itself defers cannot
  // invalidate the walk; the nested body runs on the next scope exit.
  const std::vector<const DeferStmt*> pending = defers_;
  for (auto it = pending.rbegin(); it != pending.rend(); ++it) {
    if (*it != nullptr && !emitBlock((*it)->body()))
      return;
  }
}

bool SeremGenerator::emitStatement(const Stmt& statement) {
  switch (statement.kind()) {
  case NodeKind::VarDecl: {
    const auto& declaration = static_cast<const VarDecl&>(statement);
    const Type* type = declaration.resolvedType();
    const serem::IRType irType = lowerType(type);
    // A `static` local lives in a module global rather than on the frame, so its
    // value survives the call. A companion guard makes the initializer run once.
    if (declaration.isStatic() && function_ != nullptr) {
      const std::string symbol = function_->name() + "." + declaration.name();
      functionStatics_[declaration.name()] = symbol;
      staticTypes_[declaration.name()] = type;
      if (declaration.init() != nullptr) {
        serem::ValuePtr done = builder_->operation(
            "static.get", serem::IRType::i64(), {}, {{"symbol", symbol + ".once"}});
        serem::ValuePtr unset = builder_->compare(
            "eq", std::move(done), std::make_shared<serem::ConstantInt>(0, serem::IRType::i64()));
        serem::BasicBlock& initBlock = function_->addBlock("static.init");
        serem::BasicBlock& contBlock = function_->addBlock("static.cont");
        (void)builder_->conditionalBranch(unset, initBlock, contBlock);
        builder_->setInsertBlock(initBlock);
        serem::ValuePtr value =
            coerce(emitExpression(*declaration.init()), declaration.init()->resolvedType(), type);
        (void)builder_->operation(
            "static.set", serem::IRType::voidType(), {value}, {{"symbol", symbol}});
        (void)builder_->operation("static.set",
                                  serem::IRType::voidType(),
                                  {std::make_shared<serem::ConstantInt>(1, serem::IRType::i64())},
                                  {{"symbol", symbol + ".once"}});
        (void)builder_->branch(contBlock);
        builder_->setInsertBlock(contBlock);
      }
      return true;
    }
    auto slot = builder_->alloca(irType);
    locals_[declaration.name()] = slot;
    localTypes_[declaration.name()] = type;
    if (declaration.init() != nullptr) {
      serem::ValuePtr value =
          coerce(emitExpression(*declaration.init()), declaration.init()->resolvedType(), type);
      if (value != nullptr && value->valueKind() == serem::ValueKind::ConstantFloat &&
          (irType.kind() == serem::IRType::Kind::F32 ||
           irType.kind() == serem::IRType::Kind::F64)) {
        value = std::make_shared<serem::ConstantFloat>(
            static_cast<const serem::ConstantFloat&>(*value).value(), irType);
      }
      builder_->store(std::move(value), slot);
    }
    return true;
  }
  case NodeKind::AssignStmt: {
    const auto& assign = static_cast<const AssignStmt&>(statement);
    // `a, b = pair` reads each element of the aggregate out by position.
    if (assign.target().kind() == NodeKind::TupleExpr) {
      const auto& tuple = static_cast<const TupleExpr&>(assign.target());
      serem::ValuePtr packed = coerce(emitExpression(assign.value()),
                                      assign.value().resolvedType(),
                                      assign.target().resolvedType());
      if (packed == nullptr) {
        return false;
      }
      const Type* target = assign.target().resolvedType();
      const Type* canonical = target == nullptr ? nullptr : target->canonical();
      for (std::size_t index = 0; index < tuple.elements().size(); ++index) {
        const Expr& element = *tuple.elements()[index];
        if (element.kind() != NodeKind::NameExpr) {
          continue;
        }
        const auto& name = static_cast<const NameExpr&>(element);
        const Type* elementType = canonical != nullptr && index < canonical->args().size()
                                      ? canonical->args()[index]
                                      : name.resolvedType();
        const serem::IRType lowered = lowerType(elementType);
        serem::ValuePtr extracted = builder_->extract(packed, index, lowered);
        serem::ValuePtr slot = builder_->alloca(lowered);
        builder_->store(std::move(extracted), slot);
        locals_[name.name()] = slot;
        localTypes_[name.name()] = elementType;
      }
      return true;
    }
    serem::ValuePtr value = coerce(emitExpression(assign.value()),
                                   assign.value().resolvedType(),
                                   assign.target().resolvedType());
    if (assign.target().kind() == NodeKind::MemberExpr) {
      const auto& member = static_cast<const MemberExpr&>(assign.target());
      if (const std::string symbol = staticFieldSymbol(member); !symbol.empty()) {
        (void)builder_->operation(
            "static.set", serem::IRType::voidType(), {value}, {{"symbol", symbol}});
        return true;
      }
      const Type* objectType = member.object().resolvedType();
      const serem::IRType lowered = lowerType(objectType);
      // A value `struct` lives in a stack slot rather than behind a pointer, so
      // the field is updated by storing the rebuilt aggregate back into it.
      if (lowered.kind() == serem::IRType::Kind::Struct &&
          member.object().kind() == NodeKind::NameExpr) {
        const auto& name = static_cast<const NameExpr&>(member.object());
        if (serem::ValuePtr slot = local(name.name())) {
          const int index = objectType == nullptr ? -1 : objectType->fieldIndex(member.field());
          if (index >= 0) {
            const serem::ValuePtr current = builder_->load(slot, lowered);
            const serem::ValuePtr updated =
                builder_->insert(current, value, static_cast<std::size_t>(index), lowered);
            builder_->store(updated, slot);
            return true;
          }
        }
      }
      (void)builder_->operation(
          "member.set",
          serem::IRType::voidType(),
          {emitExpression(member.object()), value},
          {{"field", member.field()},
           {"index",
            std::to_string(
                fieldSlot(member.object().resolvedType(),
                          member.object().resolvedType()->fieldIndex(member.field())))}});
      return true;
    }
    if (assign.target().kind() == NodeKind::IndexExpr) {
      const auto& index = static_cast<const IndexExpr&>(assign.target());
      const Type* objectType = index.object().resolvedType();
      const Type* record = objectType == nullptr ? nullptr : objectType->valueType();
      if (record != nullptr && record->isRecord() && record->methodIndex("__setitem__") >= 0) {
        serem::ValuePtr receiver = emitExpression(index.object());
        std::vector<serem::ValuePtr> arguments;
        if (index.hasStart())
          arguments.push_back(emitExpression(*index.start()));
        arguments.push_back(value);
        (void)callMethod(record, "__setitem__", std::move(receiver), arguments);
        return true;
      }
      std::vector<serem::ValuePtr> operands{emitExpression(index.object())};
      if (index.hasStart())
        operands.push_back(emitExpression(*index.start()));
      operands.push_back(value);
      std::unordered_map<std::string, std::string> attributes;
      if (objectType != nullptr && objectType->isDict()) {
        attributes["container"] = "dict";
        attributes["key.kind"] = listElementKindText(objectType->dictKeyType());
        attributes["key.dict.kind"] = dictKeyKindText(objectType->dictKeyType());
        attributes["value.kind"] = listElementKindText(objectType->dictValueType());
      }
      (void)builder_->operation(
          "index.set", serem::IRType::voidType(), std::move(operands), std::move(attributes));
      return true;
    }
    if (assign.target().kind() == NodeKind::UnaryExpr &&
        static_cast<const UnaryExpr&>(assign.target()).op() == UnaryOp::Deref) {
      const auto& deref = static_cast<const UnaryExpr&>(assign.target());
      (void)builder_->operation(
          "store.indirect", serem::IRType::voidType(), {value, emitExpression(deref.operand())});
      return true;
    }
    if (assign.target().kind() != NodeKind::NameExpr) {
      (void)builder_->operation("assign.dynamic", serem::IRType::voidType(), {value});
      return true;
    }
    const auto& name = static_cast<const NameExpr&>(assign.target());
    // A `static` local is a module global, so the write goes through it and a
    // compound assignment reads the global before storing the updated value.
    const auto statik = functionStatics_.find(name.name());
    if (statik != functionStatics_.end()) {
      const Type* type = assign.target().resolvedType();
      const serem::IRType irType = lowerType(type);
      if (assign.op() != AssignOp::Assign) {
        const serem::ValuePtr current =
            builder_->operation("static.get", irType, {}, {{"symbol", statik->second}});
        BinaryOp binaryOp = BinaryOp::Add;
        if (binaryOpForAssign(assign.op(), binaryOp)) {
          switch (binaryOp) {
          case BinaryOp::Add:
            value = builder_->add(current, value, irType);
            break;
          case BinaryOp::Sub:
            value = builder_->sub(current, value, irType);
            break;
          case BinaryOp::Mul:
            value = builder_->mul(current, value, irType);
            break;
          case BinaryOp::Div:
            value = builder_->div(current, value, irType);
            break;
          case BinaryOp::Mod:
            value = builder_->rem(current, value, irType);
            break;
          case BinaryOp::BitAnd:
            value = builder_->bitAnd(current, value, irType);
            break;
          case BinaryOp::BitOr:
            value = builder_->bitOr(current, value, irType);
            break;
          case BinaryOp::BitXor:
            value = builder_->bitXor(current, value, irType);
            break;
          case BinaryOp::Shl:
            value = builder_->shiftLeft(current, value, irType);
            break;
          case BinaryOp::Shr:
            value = builder_->shiftRight(current, value, irType);
            break;
          default:
            break;
          }
        }
      }
      (void)builder_->operation(
          "static.set", serem::IRType::voidType(), {value}, {{"symbol", statik->second}});
      return true;
    }
    serem::ValuePtr slot = local(name.name());
    if (slot == nullptr) {
      slot = builder_->alloca(value->type());
      // The name is introduced by this assignment, so its slot type comes from
      // the target: reading it back must not use a narrowed view of the name.
      const Type* declared = assign.target().resolvedType() == nullptr
                                 ? assign.value().resolvedType()
                                 : assign.target().resolvedType();
      (void)bindLocal(name.name(), slot, declared);
      builder_->store(std::move(value), slot);
      return true;
    }
    if (assign.op() != AssignOp::Assign) {
      const serem::ValuePtr current =
          builder_->load(slot, lowerType(assign.target().resolvedType()));
      BinaryOp binaryOp = BinaryOp::Add;
      if (binaryOpForAssign(assign.op(), binaryOp)) {
        switch (binaryOp) {
        case BinaryOp::Add:
          value = builder_->add(current, value, lowerType(assign.target().resolvedType()));
          break;
        case BinaryOp::Sub:
          value = builder_->sub(current, value, lowerType(assign.target().resolvedType()));
          break;
        case BinaryOp::Mul:
          value = builder_->mul(current, value, lowerType(assign.target().resolvedType()));
          break;
        case BinaryOp::Div:
          value = builder_->div(current, value, lowerType(assign.target().resolvedType()));
          break;
        case BinaryOp::Mod:
          value = builder_->rem(current, value, lowerType(assign.target().resolvedType()));
          break;
        case BinaryOp::BitAnd:
          value = builder_->bitAnd(current, value, lowerType(assign.target().resolvedType()));
          break;
        case BinaryOp::BitOr:
          value = builder_->bitOr(current, value, lowerType(assign.target().resolvedType()));
          break;
        case BinaryOp::BitXor:
          value = builder_->bitXor(current, value, lowerType(assign.target().resolvedType()));
          break;
        case BinaryOp::Shl:
          value = builder_->shiftLeft(current, value, lowerType(assign.target().resolvedType()));
          break;
        case BinaryOp::Shr:
          value = builder_->shiftRight(current, value, lowerType(assign.target().resolvedType()));
          break;
        default:
          break;
        }
      }
    }
    builder_->store(std::move(value), slot);
    return true;
  }
  case NodeKind::IfStmt:
    return emitIf(static_cast<const IfStmt&>(statement));
  case NodeKind::MatchStmt: {
    const auto& match = static_cast<const MatchStmt&>(statement);
    const serem::ValuePtr subject = emitExpression(match.subject());
    const Type* subjectType = match.subject().resolvedType();
    // Only an enum subject carries a variant tag; the tag of an integer or a
    // string subject is not the arm position.
    const bool enumSubject = subjectType != nullptr && subjectType->canonical()->isEnum();
    const serem::ValuePtr tag =
        enumSubject ? builder_->operation("enum.tag", serem::IRType::i32(), {subject}) : nullptr;
    serem::BasicBlock* merge = &function_->addBlock("match.end");
    std::vector<serem::BasicBlock*> arms;
    for (std::size_t index = 0; index < match.arms().size(); ++index) {
      arms.push_back(&function_->addBlock("match.arm" + std::to_string(index)));
    }
    for (std::size_t index = 0; index < match.arms().size(); ++index) {
      const MatchArm& arm = match.arms()[index];
      serem::BasicBlock* next = index + 1 < arms.size() ? arms[index + 1] : merge;
      serem::BasicBlock* body = &function_->addBlock("match.body" + std::to_string(index));
      const Expr& pattern = *arm.pattern;
      // A bare name is the wildcard arm, which matches whatever is left.
      const bool wildcard = pattern.kind() == NodeKind::NameExpr;
      serem::ValuePtr condition = nullptr;
      if (wildcard) {
        (void)builder_->branch(*body);
      } else if (pattern.kind() == NodeKind::IntegerLiteral) {
        const auto& literal = static_cast<const IntegerLiteral&>(pattern);
        const serem::IRType subjectIr = lowerType(subjectType);
        condition = builder_->compare(
            "eq", subject, std::make_shared<serem::ConstantInt>(literal.value(), subjectIr));
      } else if (enumSubject) {
        // `case Color.Green` and `case Message.Move(x, y)` name a variant, whose
        // tag the enum records in the field table.
        const MemberExpr* member = nullptr;
        if (pattern.kind() == NodeKind::MemberExpr) {
          member = static_cast<const MemberExpr*>(&pattern);
        } else if (pattern.kind() == NodeKind::CallExpr &&
                   static_cast<const CallExpr&>(pattern).callee().kind() == NodeKind::MemberExpr) {
          member = static_cast<const MemberExpr*>(&static_cast<const CallExpr&>(pattern).callee());
        }
        const RecordField* field =
            member == nullptr ? nullptr : subjectType->canonical()->findField(member->field());
        if (field != nullptr) {
          std::int64_t variant = static_cast<std::int64_t>(index);
          if (!field->llvmName.empty()) {
            variant = std::strtoll(field->llvmName.c_str(), nullptr, 10);
          }
          condition = builder_->compare(
              "eq", tag, std::make_shared<serem::ConstantInt>(variant, serem::IRType::i32()));
        }
      }
      if (condition == nullptr && !wildcard) {
        condition = builder_->compare("eq",
                                      tag == nullptr ? subject : tag,
                                      std::make_shared<serem::ConstantInt>(
                                          static_cast<std::int64_t>(index), serem::IRType::i32()));
      }
      if (condition != nullptr) {
        (void)builder_->conditionalBranch(condition, *body, *next);
      }
      builder_->setInsertBlock(*body);
      if (arm.pattern->kind() == NodeKind::CallExpr) {
        const auto& call = static_cast<const CallExpr&>(*arm.pattern);
        for (std::size_t argument = 0; argument < call.arguments().size(); ++argument) {
          if (call.arguments()[argument]->kind() != NodeKind::NameExpr) {
            continue;
          }
          const auto& name = static_cast<const NameExpr&>(*call.arguments()[argument]);
          // `_` is a wildcard: it binds nothing, so its resolved type is void
          // and there is no payload slot to create for it.
          const Type* payloadType = name.resolvedType();
          if (name.name() == "_" || payloadType == nullptr || payloadType->isVoidLike()) {
            continue;
          }
          auto payload = builder_->operation("enum.payload",
                                             lowerType(payloadType),
                                             {subject},
                                             {{"index", std::to_string(argument)}});
          auto slot = builder_->alloca(payload->type());
          builder_->store(payload, slot);
          (void)bindLocal(name.name(), slot, payloadType);
        }
      }
      if (!emitBlock(arm.body))
        return false;
      if (!builder_->currentBlock().isTerminated())
        (void)builder_->branch(*merge);
      if (next == merge)
        break;
      builder_->setInsertBlock(*next);
    }
    builder_->setInsertBlock(*merge);
    return true;
  }
  case NodeKind::WhileStmt:
    return emitWhile(static_cast<const WhileStmt&>(statement));
  case NodeKind::ForStmt:
    return emitFor(static_cast<const ForStmt&>(statement));
  case NodeKind::BreakStmt:
    if (breakTargets_.empty())
      return unsupported(statement, "break outside a loop");
    (void)builder_->branch(*breakTargets_.back());
    return true;
  case NodeKind::ContinueStmt:
    if (continueTargets_.empty())
      return unsupported(statement, "continue outside a loop");
    (void)builder_->branch(*continueTargets_.back());
    return true;
  case NodeKind::YieldStmt: {
    const auto& yield = static_cast<const YieldStmt&>(statement);
    if (coroutineToken_ == nullptr)
      return unsupported(statement, "yield in a non-generator");
    if (yield.value() == nullptr) {
      (void)builder_->operation("yield", serem::IRType::voidType());
    } else {
      builder_->yield(emitExpression(*yield.value()));
    }
    builder_->coroSuspend(coroutineToken_);
    return true;
  }
  case NodeKind::RaiseStmt: {
    const auto& raise = static_cast<const RaiseStmt&>(statement);
    const std::string handler = tryHandlers_.empty() ? std::string{} : tryHandlers_.back();
    if (raise.value() == nullptr) {
      (void)builder_->operation(
          "throw", serem::IRType::voidType(), {}, {{"handler", handler}, {"rethrow", "true"}});
      return true;
    }
    const Expr& value = *raise.value();
    const Type* type = value.resolvedType();
    if (type != nullptr && type->isTypeObject() && type->typeObjectInstance() != nullptr) {
      type = type->typeObjectInstance();
    }
    std::string chain;
    appendExceptionType(chain, type);
    const int messageIndex = type == nullptr ? -1 : fieldSlot(type, type->fieldIndex("message"));
    (void)builder_->operation(
        "throw",
        serem::IRType::voidType(),
        {emitExpression(value)},
        {{"handler", handler}, {"type", chain}, {"field", std::to_string(messageIndex)}});
    return true;
  }
  case NodeKind::AssertStmt: {
    const auto& assertion = static_cast<const AssertStmt&>(statement);
    std::vector<serem::ValuePtr> operands{emitExpression(assertion.condition())};
    if (assertion.message() != nullptr && assertion.message()->resolvedType() != nullptr &&
        assertion.message()->resolvedType()->isNamed("str")) {
      operands.push_back(emitExpression(*assertion.message()));
    }
    (void)builder_->operation(
        "assert",
        serem::IRType::voidType(),
        std::move(operands),
        {{"handler", tryHandlers_.empty() ? std::string{} : tryHandlers_.back()}});
    return true;
  }
  case NodeKind::WithStmt: {
    const auto& with = static_cast<const WithStmt&>(statement);
    const Type* contextType = with.context().resolvedType();
    const Type* record = contextType == nullptr ? nullptr : contextType->valueType();
    const serem::ValuePtr context = emitExpression(with.context());
    const bool managed =
        record != nullptr && record->isRecord() && record->methodIndex("__enter__") >= 0;
    if (managed) {
      const Type* entered = record->dunderReturn("__enter__");
      const Type* bound = entered == nullptr ? record : entered;
      const serem::ValuePtr value = callMethod(record, "__enter__", context, {});
      if (!with.name().empty()) {
        auto slot = builder_->alloca(lowerType(bound));
        builder_->store(coerce(value, entered, bound), slot);
        locals_[with.name()] = slot;
        localTypes_[with.name()] = bound;
      }
    } else if (!with.name().empty()) {
      (void)bindLocal(with.name(), context);
    }
    if (!emitBlock(with.body()))
      return false;
    if (managed)
      (void)callMethod(record, "__exit__", context, {});
    return true;
  }
  case NodeKind::TryStmt: {
    const auto& tryStatement = static_cast<const TryStmt&>(statement);
    // The try body runs inline: a raise inside it branches to the dispatch block
    // while normal completion falls through to the else body and then finally.
    serem::BasicBlock* dispatch = &function_->addBlock("try.dispatch");
    serem::BasicBlock* finallyBlock = &function_->addBlock("try.finally");
    serem::BasicBlock* after = &function_->addBlock("try.after");
    tryHandlers_.push_back(dispatch->label());
    const bool okBody = emitBlock(tryStatement.body());
    tryHandlers_.pop_back();
    if (!okBody)
      return false;
    if (!builder_->currentBlock().isTerminated()) {
      if (!emitBlock(tryStatement.elseBody()))
        return false;
      if (!builder_->currentBlock().isTerminated())
        (void)builder_->branch(*finallyBlock);
    }
    // Dispatch the pending error against each handler in source order.
    builder_->setInsertBlock(*dispatch);
    for (const ExceptHandler& handler : tryStatement.handlers()) {
      serem::BasicBlock* taken = &function_->addBlock("try.except");
      serem::BasicBlock* next = &function_->addBlock("try.next");
      if (handler.type == nullptr) {
        (void)builder_->branch(*taken);
      } else {
        const Type* caught = handler.type->resolvedType();
        const Type* canonical = caught == nullptr ? nullptr : caught->canonical();
        const serem::ValuePtr matched = builder_->operation(
            "error.isa",
            serem::IRType::boolType(),
            {},
            {{"type", canonical == nullptr ? std::string{} : canonical->name()}});
        (void)builder_->conditionalBranch(matched, *taken, *next);
      }
      builder_->setInsertBlock(*taken);
      // Save and clear the matched error before the handler runs. error.leave
      // consumes this frame; without enter, an unnamed handler leaves the
      // original error pending at program exit.
      if (handler.name.empty()) {
        (void)builder_->operation("error.enter", serem::IRType::voidType());
      }
      if (!handler.name.empty()) {
        const Type* caught = handler.type == nullptr ? nullptr : handler.type->resolvedType();
        const serem::ValuePtr object = builder_->operation(
            "error.bind",
            lowerType(caught),
            {},
            {{"field",
              std::to_string(
                  caught == nullptr ? -1 : fieldSlot(caught, caught->fieldIndex("message")))}});
        auto slot = builder_->alloca(object->type());
        builder_->store(object, slot);
        locals_[handler.name] = slot;
        localTypes_[handler.name] = caught;
        (void)builder_->operation("error.enter", serem::IRType::voidType());
      }
      if (!emitBlock(handler.body))
        return false;
      if (!builder_->currentBlock().isTerminated()) {
        // The exception was handled: release the error before running finally.
        (void)builder_->operation(
            "error.leave", serem::IRType::voidType(), {}, {{"restore", "false"}});
        (void)builder_->branch(*finallyBlock);
      }
      builder_->setInsertBlock(*next);
    }
    if (!builder_->currentBlock().isTerminated())
      (void)builder_->branch(*finallyBlock);
    // Finally suspends any pending error so its own body starts clean, then
    // restores it on the way out.
    builder_->setInsertBlock(*finallyBlock);
    (void)builder_->operation("error.enter", serem::IRType::voidType());
    if (!emitBlock(tryStatement.finallyBody()))
      return false;
    if (!builder_->currentBlock().isTerminated()) {
      (void)builder_->operation(
          "error.leave", serem::IRType::voidType(), {}, {{"restore", "true"}});
      (void)builder_->branch(*after);
    }
    builder_->setInsertBlock(*after);
    return true;
  }
  case NodeKind::DeferStmt:
    defers_.push_back(&static_cast<const DeferStmt&>(statement));
    return true;
  case NodeKind::DelStmt: {
    const Expr& target = static_cast<const DelStmt&>(statement).target();
    // `del container[key]` removes the entry itself; the container is the
    // operand, not the value stored at that key.
    if (target.kind() == NodeKind::IndexExpr) {
      const auto& index = static_cast<const IndexExpr&>(target);
      const Type* objectType = index.object().resolvedType();
      if (objectType != nullptr && index.hasStart()) {
        std::vector<serem::ValuePtr> operands{emitExpression(index.object()),
                                              emitExpression(*index.start())};
        std::unordered_map<std::string, std::string> attributes;
        if (objectType->isDict()) {
          attributes["container"] = "dict";
          attributes["key.kind"] = listElementKindText(objectType->dictKeyType());
          attributes["key.dict.kind"] = dictKeyKindText(objectType->dictKeyType());
        }
        (void)builder_->operation(
            "index.delete", serem::IRType::voidType(), std::move(operands), std::move(attributes));
        return true;
      }
    }
    (void)builder_->operation("destroy", serem::IRType::voidType(), {emitExpression(target)});
    return true;
  }
  case NodeKind::ReturnStmt: {
    const auto& result = static_cast<const ReturnStmt&>(statement);
    emitDeferred();
    if (result.value() == nullptr) {
      // A generator's frame is owned by its coroutine: a bare `return` suspends
      // for the last time instead of returning a value the frame does not hold.
      if (coroutineToken_ != nullptr) {
        builder_->coroEnd();
        return true;
      }
      builder_->retVoid();
    } else {
      builder_->ret(
          coerce(emitExpression(*result.value()), result.value()->resolvedType(), returnType_));
    }
    return true;
  }
  case NodeKind::ExprStmt:
    (void)emitExpression(static_cast<const ExprStmt&>(statement).expression());
    return true;
  case NodeKind::FunctionDef:
    // An inner `def` becomes a function of its own, which is what makes it a
    // callable value for the enclosing body.
    return emitNestedFunction(static_cast<const FunctionDef&>(statement));
  case NodeKind::PassStmt:
    return true;
  default:
    (void)builder_->operation("sere.statement",
                              serem::IRType::voidType(),
                              {},
                              {{"kind", std::to_string(static_cast<int>(statement.kind()))}});
    return true;
  }
}

serem::ValuePtr SeremGenerator::emitDo(const DoExpr& expression) {
  return emitBlockValue(expression.body(), expression.resolvedType());
}

serem::ValuePtr SeremGenerator::emitBlockValue(const std::vector<std::unique_ptr<Stmt>>& body,
                                               const Type* resultType) {
  const auto savedLocals = locals_;
  const auto savedLocalTypes = localTypes_;
  const bool wantsValue = resultType != nullptr && !resultType->isVoidLike();
  serem::ValuePtr result;
  for (std::size_t index = 0; index < body.size(); ++index) {
    const Stmt& statement = *body[index];
    const bool isLast = index + 1 == body.size();
    if (isLast && statement.kind() == NodeKind::ExprStmt) {
      result = emitExpression(static_cast<const ExprStmt&>(statement).expression());
      break;
    }
    if (isLast && wantsValue && statement.kind() == NodeKind::IfStmt) {
      result = emitIfValue(static_cast<const IfStmt&>(statement), resultType);
      break;
    }
    if (!emitStatement(statement)) {
      result = nullptr;
      break;
    }
    if (builder_->currentBlock().isTerminated()) {
      break;
    }
  }
  locals_ = std::move(savedLocals);
  localTypes_ = std::move(savedLocalTypes);
  if (wantsValue && result == nullptr) {
    result = std::make_shared<serem::ConstantInt>(0, lowerType(resultType));
  }
  return result;
}

serem::ValuePtr SeremGenerator::emitIfValue(const IfStmt& statement, const Type* resultType) {
  const serem::IRType irType = lowerType(resultType);
  // Serem's `phi` cannot join values defined in sibling blocks, so the branches
  // write into a slot that the merge block reloads.
  auto slot = builder_->alloca(irType);
  const std::vector<IfBranch>& branches = statement.branches();
  serem::BasicBlock* merge = &function_->addBlock("do.if.end");
  std::vector<serem::BasicBlock*> tests(branches.size(), nullptr);
  std::vector<serem::BasicBlock*> bodies;
  bodies.reserve(branches.size());
  for (std::size_t index = 0; index < branches.size(); ++index) {
    if (index > 0 && branches[index].condition != nullptr) {
      tests[index] = &function_->addBlock("do.if.test" + std::to_string(index));
    }
    bodies.push_back(&function_->addBlock("do.if.body" + std::to_string(index)));
  }
  for (std::size_t index = 0; index < branches.size(); ++index) {
    const IfBranch& branch = branches[index];
    serem::BasicBlock* fallthrough = merge;
    if (index + 1 < branches.size()) {
      fallthrough = branches[index + 1].condition != nullptr ? tests[index + 1] : bodies[index + 1];
    }
    if (branch.condition == nullptr) {
      if (&builder_->currentBlock() != bodies[index] && !builder_->currentBlock().isTerminated()) {
        (void)builder_->branch(*bodies[index]);
      }
    } else {
      const serem::ValuePtr condition = emitExpression(*branch.condition);
      (void)builder_->conditionalBranch(condition, *bodies[index], *fallthrough);
    }
    builder_->setInsertBlock(*bodies[index]);
    serem::ValuePtr value = emitBlockValue(branch.body, resultType);
    if (!builder_->currentBlock().isTerminated()) {
      serem::ValuePtr stored =
          value != nullptr ? std::move(value) : std::make_shared<serem::ConstantInt>(0, irType);
      builder_->store(std::move(stored), slot);
      (void)builder_->branch(*merge);
    }
    if (branch.condition == nullptr)
      break;
    builder_->setInsertBlock(*fallthrough);
  }
  builder_->setInsertBlock(*merge);
  return builder_->load(slot, irType);
}

bool SeremGenerator::emitIf(const IfStmt& statement) {
  const std::vector<IfBranch>& branches = statement.branches();
  serem::BasicBlock* merge = &function_->addBlock("if.end");
  // Each `elif` needs a test block of its own: evaluating the condition inside
  // the body of its own branch makes the true edge point back at the block it
  // was emitted in, which lowers to an endless loop.
  std::vector<serem::BasicBlock*> tests(branches.size(), nullptr);
  std::vector<serem::BasicBlock*> bodies;
  bodies.reserve(branches.size());
  for (std::size_t index = 0; index < branches.size(); ++index) {
    if (index > 0 && branches[index].condition != nullptr) {
      tests[index] = &function_->addBlock("if.test" + std::to_string(index));
    }
    bodies.push_back(&function_->addBlock("if.body" + std::to_string(index)));
  }
  for (std::size_t index = 0; index < branches.size(); ++index) {
    const IfBranch& branch = branches[index];
    // Where control goes when this branch does not match: the next test, the
    // `else` body, or past the whole statement.
    serem::BasicBlock* fallthrough = merge;
    if (index + 1 < branches.size()) {
      fallthrough = branches[index + 1].condition != nullptr ? tests[index + 1] : bodies[index + 1];
    }
    if (branch.condition == nullptr) {
      // The `else` body is already the fallthrough of the last test, so jumping
      // to it would branch the block onto itself.
      if (&builder_->currentBlock() != bodies[index] && !builder_->currentBlock().isTerminated()) {
        (void)builder_->branch(*bodies[index]);
      }
    } else {
      const serem::ValuePtr condition = emitExpression(*branch.condition);
      (void)builder_->conditionalBranch(condition, *bodies[index], *fallthrough);
    }
    builder_->setInsertBlock(*bodies[index]);
    if (!emitBlock(branch.body))
      return false;
    if (!builder_->currentBlock().isTerminated())
      (void)builder_->branch(*merge);
    if (branch.condition == nullptr)
      break;
    builder_->setInsertBlock(*fallthrough);
  }
  builder_->setInsertBlock(*merge);
  return true;
}

bool SeremGenerator::emitWhile(const WhileStmt& statement) {
  serem::BasicBlock* condition = &function_->addBlock("while.cond");
  serem::BasicBlock* body = &function_->addBlock("while.body");
  serem::BasicBlock* exit = &function_->addBlock("while.end");
  if (!builder_->currentBlock().isTerminated())
    (void)builder_->branch(*condition);
  builder_->setInsertBlock(*condition);
  (void)builder_->conditionalBranch(emitExpression(statement.condition()), *body, *exit);
  builder_->setInsertBlock(*body);
  breakTargets_.push_back(exit);
  continueTargets_.push_back(condition);
  const bool ok = emitBlock(statement.body());
  continueTargets_.pop_back();
  breakTargets_.pop_back();
  if (!ok)
    return false;
  if (!builder_->currentBlock().isTerminated())
    (void)builder_->branch(*condition);
  builder_->setInsertBlock(*exit);
  return true;
}

bool SeremGenerator::emitFor(const ForStmt& statement) {
  const Type* iterable = statement.iterable().resolvedType();
  const Type* custom = iterable == nullptr ? nullptr : iterable->valueType();
  serem::ValuePtr customIterator;
  if (custom != nullptr && custom->isRecord() && custom->methodIndex("__iter__") >= 0) {
    customIterator = callMethod(custom, "__iter__", emitExpression(statement.iterable()), {});
    iterable = custom->dunderReturn("__iter__");
  }
  // A generator function call produces values through a coroutine: resume it,
  // read the promise while it is suspended, and stop once it is done.
  if (iterable != nullptr && iterable->isGenericCtor("Iterator")) {
    const Type* elementType = iterable->genericArg(0);
    const serem::ValuePtr iterator =
        customIterator != nullptr ? customIterator : emitExpression(statement.iterable());
    const std::unordered_map<std::string, std::string> attributes{
        {"element", elementType == nullptr ? std::string{} : elementType->display()}};
    const serem::IRType loweredElement = lowerType(elementType);
    serem::BasicBlock* condition = &function_->addBlock("for.cond");
    serem::BasicBlock* body = &function_->addBlock("for.body");
    serem::BasicBlock* finish = &function_->addBlock("for.finish");
    serem::BasicBlock* exit = &function_->addBlock("for.end");
    if (!builder_->currentBlock().isTerminated())
      (void)builder_->branch(*condition);
    builder_->setInsertBlock(*condition);
    (void)builder_->operation("coro.resume", serem::IRType::voidType(), {iterator}, attributes);
    auto done = builder_->operation("coro.done", serem::IRType::boolType(), {iterator}, attributes);
    (void)builder_->conditionalBranch(done, *finish, *body);
    builder_->setInsertBlock(*body);
    auto promise = builder_->operation(
        "coro.promise", serem::IRType::ptr(loweredElement), {iterator}, attributes);
    auto value = builder_->load(promise, loweredElement);
    auto slot = builder_->alloca(value->type());
    builder_->store(value, slot);
    locals_[statement.name()] = slot;
    localTypes_[statement.name()] = elementType;
    breakTargets_.push_back(exit);
    continueTargets_.push_back(condition);
    const bool ok = emitBlock(statement.body());
    continueTargets_.pop_back();
    breakTargets_.pop_back();
    if (!ok)
      return false;
    if (!builder_->currentBlock().isTerminated())
      (void)builder_->branch(*condition);
    builder_->setInsertBlock(*finish);
    (void)builder_->operation("coro.destroy", serem::IRType::voidType(), {iterator}, attributes);
    (void)builder_->branch(*exit);
    builder_->setInsertBlock(*exit);
    return true;
  }
  const serem::ValuePtr source = emitExpression(statement.iterable());
  const Type* iterableType = statement.iterable().resolvedType();
  const Type* elementType = iterableType == nullptr ? nullptr : iterableType->elementType();
  if (elementType == nullptr) {
    return unsupported(statement, "for-in over an iterable without an element type");
  }
  const std::unordered_map<std::string, std::string> iteratorAttributes{
      {"element", elementType == nullptr ? std::string{} : elementType->display()}};
  const serem::ValuePtr iterator = builder_->operation(
      "iter.begin", serem::IRType::ptr(serem::IRType::i64()), {source}, iteratorAttributes);
  serem::BasicBlock* condition = &function_->addBlock("for.cond");
  serem::BasicBlock* body = &function_->addBlock("for.body");
  serem::BasicBlock* exit = &function_->addBlock("for.end");
  if (!builder_->currentBlock().isTerminated())
    (void)builder_->branch(*condition);
  builder_->setInsertBlock(*condition);
  auto hasNext = builder_->operation(
      "iter.has_next", serem::IRType::boolType(), {source, iterator}, iteratorAttributes);
  (void)builder_->conditionalBranch(hasNext, *body, *exit);
  builder_->setInsertBlock(*body);
  auto next = builder_->operation(
      "iter.next", lowerType(elementType), {source, iterator}, iteratorAttributes);
  auto slot = builder_->alloca(next->type());
  builder_->store(next, slot);
  locals_[statement.name()] = slot;
  localTypes_[statement.name()] = elementType;
  breakTargets_.push_back(exit);
  continueTargets_.push_back(condition);
  const bool ok = emitBlock(statement.body());
  continueTargets_.pop_back();
  breakTargets_.pop_back();
  if (!ok)
    return false;
  if (!builder_->currentBlock().isTerminated())
    (void)builder_->branch(*condition);
  builder_->setInsertBlock(*exit);
  return true;
}

serem::ValuePtr SeremGenerator::stringValue(std::string_view value) {
  const std::string text(value);
  const std::string name = "str." + std::to_string(module_->globals().size());
  (void)module_->addGlobal(std::make_unique<serem::GlobalConstant>(
      name, serem::IRType::stringType(), serem::ConstantString(text).display()));
  return std::make_shared<serem::ConstantString>(text, name);
}

serem::ValuePtr SeremGenerator::emitExpression(const Expr& expression) {
  switch (expression.kind()) {
  case NodeKind::IntegerLiteral: {
    const auto& literal = static_cast<const IntegerLiteral&>(expression);
    return std::make_shared<serem::ConstantInt>(literal.value(),
                                                lowerType(expression.resolvedType()));
  }
  case NodeKind::FloatLiteral: {
    const auto& literal = static_cast<const FloatLiteral&>(expression);
    return std::make_shared<serem::ConstantFloat>(literal.value(),
                                                  lowerType(expression.resolvedType()));
  }
  case NodeKind::StringLiteral: {
    const std::string& value = static_cast<const StringLiteral&>(expression).value();
    return stringValue(value);
  }
  case NodeKind::InterpolatedStringExpr: {
    const auto& interpolated = static_cast<const InterpolatedStringExpr&>(expression);
    std::vector<serem::ValuePtr> parts;
    for (const StringPart& part : interpolated.parts()) {
      if (!part.literal.empty())
        parts.push_back(stringValue(part.literal));
      if (part.value != nullptr) {
        // Every interpolated value goes through the runtime formatter, with an
        // empty spec when the f-string names none, so a number renders as text
        // instead of reaching `string.concat` as a raw word.
        serem::ValuePtr formatted = formatValue(*part.value, part.spec);
        if (formatted == nullptr)
          return nullptr;
        parts.push_back(std::move(formatted));
      }
    }
    if (parts.empty())
      return stringValue("");
    return builder_->operation("string.concat", serem::IRType::stringType(), std::move(parts));
  }
  case NodeKind::BooleanLiteral:
    return std::make_shared<serem::ConstantInt>(
        static_cast<const BooleanLiteral&>(expression).value() ? 1 : 0, serem::IRType::boolType());
  case NodeKind::NoneLiteral:
    return std::make_shared<serem::ConstantInt>(0, serem::IRType::i8());
  case NodeKind::NameExpr:
    return emitName(static_cast<const NameExpr&>(expression));
  case NodeKind::BinaryExpr:
    return emitBinary(static_cast<const BinaryExpr&>(expression));
  case NodeKind::UnaryExpr:
    return emitUnary(static_cast<const UnaryExpr&>(expression));
  case NodeKind::CallExpr: {
    auto result = emitCall(static_cast<const CallExpr&>(expression));
    (void)builder_->operation("error.check", serem::IRType::voidType(), {},
        {{"handler", tryHandlers_.empty() ? std::string{} : tryHandlers_.back()}});
    return result;
  }
  case NodeKind::AwaitExpr: {
    const auto& await = static_cast<const AwaitExpr&>(expression);
    return builder_->await(emitExpression(await.operand()), lowerType(expression.resolvedType()));
  }
  case NodeKind::MemberExpr:
    return emitMember(static_cast<const MemberExpr&>(expression));
  case NodeKind::IndexExpr:
    return emitIndex(static_cast<const IndexExpr&>(expression));
  case NodeKind::ListLiteral:
  case NodeKind::DictLiteral:
  case NodeKind::TupleExpr:
    return emitAggregate(expression);
  case NodeKind::ComprehensionExpr:
    return emitComprehension(static_cast<const ComprehensionExpr&>(expression));
  case NodeKind::TernaryExpr: {
    const auto& ternary = static_cast<const TernaryExpr&>(expression);
    return builder_->select(emitExpression(ternary.condition()),
                            emitExpression(ternary.thenValue()),
                            emitExpression(ternary.elseValue()),
                            lowerType(expression.resolvedType()));
  }
  case NodeKind::WalrusExpr: {
    const auto& walrus = static_cast<const WalrusExpr&>(expression);
    const serem::ValuePtr value = emitExpression(walrus.value());
    auto slot = builder_->alloca(value->type());
    builder_->store(value, slot);
    (void)bindLocal(walrus.name(), slot, walrus.resolvedType());
    return value;
  }
  case NodeKind::DoExpr:
    return emitDo(static_cast<const DoExpr&>(expression));
  case NodeKind::CastExpr: {
    const auto& cast = static_cast<const CastExpr&>(expression);
    return coerce(
        emitExpression(cast.value()), cast.value().resolvedType(), expression.resolvedType());
  }
  case NodeKind::LambdaExpr: {
    const auto& lambda = static_cast<const LambdaExpr&>(expression);
    const auto known = functions_.find(lambda.llvmName());
    if (known == functions_.end() && !emitLambdaFunction(lambda)) {
      return std::make_shared<serem::ConstantInt>(0, serem::IRType::ptr(serem::IRType::i8()));
    }
    const auto emitted = functions_.find(lambda.llvmName());
    const serem::IRType type =
        emitted == functions_.end() ? lowerType(expression.resolvedType()) : emitted->second;
    return std::make_shared<serem::FunctionRef>(lambda.llvmName(), type);
  }
  default:
    return builder_->operation("sere.expression",
                               lowerType(expression.resolvedType()),
                               {},
                               {{"kind", std::to_string(static_cast<int>(expression.kind()))}});
  }
}

serem::ValuePtr SeremGenerator::coerce(serem::ValuePtr value, const Type* from, const Type* to) {
  from = types_->substitute(from, subst_);
  to = types_->substitute(to, subst_);
  if (value == nullptr || from == nullptr || to == nullptr || from == to)
    return value;
  if (to->isAny() && !from->isAny()) {
    return builder_->operation(
        "any.box",
        lowerType(to),
        {value},
        {{"name", from->display()}, {"tag", std::to_string(serem::recordTypeId(from->display()))}});
  }
  if (from->isAny() && !to->isAny()) {
    return builder_->operation(
        "any.unbox",
        lowerType(to),
        {value},
        {{"name", to->display()}, {"tag", std::to_string(serem::recordTypeId(to->display()))}});
  }
  if (from->isVoidLike() && lowerType(to).kind() == serem::IRType::Kind::Ptr)
    return builder_->operation("pointer.null", lowerType(to));
  if (to->isUnion() && !from->isUnion()) {
    int tag = to->unionMemberIndex(from);
    if (tag < 0) {
      for (std::size_t i = 0; i < to->args().size(); ++i) {
        if (from->isSubtypeOf(to->args()[i])) {
          tag = static_cast<int>(i);
          value = coerce(value, from, to->args()[i]);
          break;
        }
      }
    }
    return builder_->operation(
        "union.pack", lowerType(to), {value}, {{"tag", std::to_string(tag)}});
  }
  if (from->isUnion() && !to->isUnion()) {
    return builder_->operation("union.extract", lowerType(to), {value});
  }
  // A class reference is a pointer either way, and a base and its derived class
  // share their layout prefix. Re-typing the value would make a field access use
  // the wrong record, which is exactly what happens when an instantiated generic
  // class is viewed through its generic declaration.
  if (from->isRecord() && to->isRecord() && !from->isEnum() && !to->isEnum() && !from->isStruct() &&
      !to->isStruct()) {
    return value;
  }
  if (lowerType(from).display() != lowerType(to).display()) {
    return builder_->operation("cast.value",
                               lowerType(to),
                               {value},
                               {{"unsigned", from->isUnsignedInteger() ? "true" : "false"}});
  }
  return value;
}

std::string SeremGenerator::methodSymbol(const Type* record, std::string_view name) const {
  if (record == nullptr)
    return {};
  // An instantiated generic class owns the symbol of its specialised method, so
  // the exact type name is tried before the generic one.
  const auto exact = methodSymbols_.find(record->name() + "::" + std::string(name));
  if (exact != methodSymbols_.end())
    return exact->second;
  // Candidate class names: the type's own name, its bare name without generic
  // arguments, and the same without a module qualifier, then each base class.
  std::vector<std::string> candidates;
  const auto addCandidate = [&](std::string text) {
    if (text.empty())
      return;
    const std::size_t bracket = text.find('[');
    if (bracket != std::string::npos)
      text.erase(bracket);
    if (text.empty())
      return;
    candidates.push_back(text);
    const std::size_t dot = text.rfind('.');
    if (dot != std::string::npos && dot + 1 < text.size()) {
      candidates.push_back(text.substr(dot + 1));
    }
  };
  addCandidate(record->name());
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    const std::string& owner = candidates[index];
    const auto found = methodSymbols_.find(owner + "::" + std::string(name));
    if (found != methodSymbols_.end())
      return found->second;
    const auto base = classBases_.find(owner);
    if (base != classBases_.end())
      addCandidate(base->second);
  }
  return {};
}

serem::ValuePtr SeremGenerator::callMethod(const Type* record,
                                           const std::string& name,
                                           serem::ValuePtr self,
                                           const std::vector<serem::ValuePtr>& arguments) {
  if (record == nullptr || self == nullptr)
    return nullptr;
  const std::string symbol = methodSymbol(record, name);
  if (symbol.empty())
    return nullptr;
  const auto found = functions_.find(symbol);
  if (found == functions_.end())
    return nullptr;
  std::vector<serem::ValuePtr> args;
  args.push_back(std::move(self));
  args.insert(args.end(), arguments.begin(), arguments.end());
  // The backend converts every argument to the callee's declared parameter type.
  return builder_->call(std::make_shared<serem::FunctionRef>(symbol, found->second),
                        std::move(args),
                        lowerType(record->dunderReturn(name)));
}

std::string SeremGenerator::renderSymbol(const Type* record) const {
  std::string symbol = methodSymbol(record, "__repr__");
  return symbol.empty() ? methodSymbol(record, "__str__") : symbol;
}

serem::ValuePtr SeremGenerator::formatValue(const Expr& expression, const std::string& spec) {
  const Type* type = expression.resolvedType();
  std::unordered_map<std::string, std::string> attributes{{"spec", spec}};
  std::vector<serem::ValuePtr> operands;
  // Kinds match runtime/sere_rt.c: 0 int, 1 float, 2 str, 3 bool. The runtime
  // reads the operand out of the matching slot, so only one is ever live.
  if (type != nullptr && type->isNamed("bool")) {
    attributes["kind"] = "3";
    // A bool widens with zero-extension, so `True` formats as `1` and not as
    // the all-ones word a sign extension of a single bit would produce.
    operands.push_back(builder_->operation(
        "cast.value", serem::IRType::i64(), {emitExpression(expression)}, {{"unsigned", "true"}}));
  } else if (type != nullptr && (type->isInteger() || type->isIntEnum())) {
    attributes["kind"] = "0";
    operands.push_back(coerce(emitExpression(expression), type, types_->i64Type()));
  } else if (type != nullptr && type->isFloat()) {
    attributes["kind"] = "1";
    operands.push_back(coerce(emitExpression(expression), type, types_->f64Type()));
  } else {
    // Everything else formats from its text form, `__str__` and container
    // rendering included.
    serem::ValuePtr text = printable(emitExpression(expression), type);
    if (text == nullptr)
      return nullptr;
    if (text->type().kind() != serem::IRType::Kind::String) {
      // A value with no textual form still has to reach `string.concat` as a
      // string, so its type name stands in, the way a record without `__str__`
      // renders as its class name.
      text = stringValue(type == nullptr ? std::string("None") : type->name());
    }
    attributes["kind"] = "2";
    operands.push_back(std::move(text));
  }
  if (operands.empty() || operands[0] == nullptr)
    return nullptr;
  return builder_->operation(
      "runtime.format", serem::IRType::stringType(), std::move(operands), std::move(attributes));
}

serem::ValuePtr
SeremGenerator::enumNameValue(const Type* record, serem::ValuePtr value, bool qualified) {
  if (record == nullptr || value == nullptr)
    return value;
  // The discriminant selects the name, so the operation carries `tag, name`
  // pairs the backend folds into one selection.
  std::vector<serem::ValuePtr> operands{std::move(value)};
  std::unordered_set<std::int32_t> seen;
  for (const RecordField& field : record->fields()) {
    if (field.name.empty() || !field.isStatic)
      continue;
    const std::int32_t tag = enumTagOf(&field);
    if (!seen.insert(tag).second)
      continue;
    operands.push_back(std::make_shared<serem::ConstantInt>(tag, serem::IRType::i32()));
    operands.push_back(stringValue(qualified ? record->name() + "." + field.name : field.name));
  }
  if (operands.size() == 1)
    return stringValue(record->name());
  return builder_->operation("enum.name", serem::IRType::stringType(), std::move(operands));
}

serem::ValuePtr SeremGenerator::printable(serem::ValuePtr value, const Type* type) {
  if (type == nullptr)
    return value;
  if (type->isEnum()) {
    // An enum renders as its variant name, so `Message.Write("x")` reads back
    // the way it was written.
    return enumNameValue(type->canonical(), std::move(value), true);
  }
  if (type->isList() || type->isArray()) {
    const Type* element = type->elementType();
    std::unordered_map<std::string, std::string> attributes{
        {"kind", "list"}, {"element.kind", listElementKindText(element)}};
    if (element != nullptr && element->isRecord() && !element->isStruct() && !element->isEnum()) {
      attributes["element.name"] = element->name();
      attributes["element.repr"] = renderSymbol(element);
    }
    return builder_->operation("value.repr", serem::IRType::stringType(), {value}, attributes);
  }
  if (type->isRecord() && !type->isEnum()) {
    const std::string symbol = methodSymbol(type, "__str__");
    if (!symbol.empty()) {
      return builder_->call(std::make_shared<serem::FunctionRef>(symbol, functions_.at(symbol)),
                            {value},
                            serem::IRType::stringType());
    }
    return stringValue(type->name());
  }
  return value;
}

serem::ValuePtr SeremGenerator::emitName(const NameExpr& expression) {
  // A `static` local reads its module global, which outlives the frame.
  const auto statik = functionStatics_.find(expression.name());
  if (statik != functionStatics_.end()) {
    const auto known = staticTypes_.find(expression.name());
    const Type* stored = known == staticTypes_.end() ? expression.resolvedType() : known->second;
    serem::ValuePtr value =
        builder_->operation("static.get", lowerType(stored), {}, {{"symbol", statik->second}});
    return coerce(std::move(value), stored, expression.resolvedType());
  }
  if (serem::ValuePtr value = local(expression.name())) {
    const auto known = localTypes_.find(expression.name());
    const Type* stored = known == localTypes_.end() ? expression.resolvedType() : known->second;
    if (value->valueKind() != serem::ValueKind::Argument) {
      value = builder_->load(value, lowerType(stored));
    }
    return coerce(value, stored, expression.resolvedType());
  }
  // A captured outer local was parked in a module global when the inner body was
  // defined, and reads back as the address it was stored under.
  const auto captured = captureSymbols_.find(expression.name());
  if (captured != captureSymbols_.end()) {
    return builder_->operation(
        "static.get", lowerType(expression.resolvedType()), {}, {{"symbol", captured->second}});
  }
  const auto symbol = functionSymbols_.find(expression.name());
  const auto found =
      functions_.find(symbol == functionSymbols_.end() ? expression.name() : symbol->second);
  if (found != functions_.end()) {
    return std::make_shared<serem::FunctionRef>(
        symbol == functionSymbols_.end() ? expression.name() : symbol->second, found->second);
  }
  if (expression.resolvedType() != nullptr && expression.resolvedType()->isTypeObject()) {
    // A class used as a value only ever stands for its type object, which a
    // `Class[T]` parameter passes along to a constructor. Nothing dereferences
    // it, so the null pointer is enough of a stand-in.
    return std::make_shared<serem::ConstantInt>(0, serem::IRType::ptr(serem::IRType::i8()));
  }
  return std::make_shared<serem::FunctionRef>(expression.name(),
                                              lowerType(expression.resolvedType()));
}

serem::ValuePtr SeremGenerator::emitUnionEquality(serem::ValuePtr left,
                                                  serem::ValuePtr right,
                                                  const Type* leftType,
                                                  const Type* rightType) {
  const auto boolean = [](bool value) {
    return std::make_shared<serem::ConstantInt>(value, serem::IRType::boolType());
  };
  if (!leftType->isUnion() && rightType->isUnion())
    return emitUnionEquality(right, left, rightType, leftType);
  if (leftType->isUnion()) {
    const auto slot = builder_->alloca(serem::IRType::boolType());
    builder_->store(boolean(false), slot);
    auto* done = &function_->addBlock("union.eq.end");
    for (std::size_t index = 0; index < leftType->args().size(); ++index) {
      const Type* member = leftType->args()[index];
      if (!rightType->isUnion() && member->canonical() != rightType->canonical())
        continue;
      auto* match = &function_->addBlock("union.eq.member");
      auto* next = &function_->addBlock("union.eq.next");
      auto tag = builder_->operation(
          "union.is", serem::IRType::boolType(), {left}, {{"tag", std::to_string(index)}});
      (void)builder_->conditionalBranch(tag, *match, *next);
      builder_->setInsertBlock(*match);
      auto payload = member->isVoidLike() ? boolean(false) : coerce(left, leftType, member);
      builder_->store(emitUnionEquality(payload, right, member, rightType), slot);
      (void)builder_->branch(*done);
      builder_->setInsertBlock(*next);
    }
    (void)builder_->branch(*done);
    builder_->setInsertBlock(*done);
    return builder_->load(slot, serem::IRType::boolType());
  }
  if (leftType->canonical() != rightType->canonical())
    return boolean(false);
  if (leftType->isVoidLike())
    return boolean(true);
  if (leftType->isList()) {
    const Type* element = leftType->elementType();
    int kind = element->isNamed("str")   ? 1
               : element->isNamed("f32") ? 2
               : element->isNamed("f64") ? 3
                                         : 0;
    return builder_->operation(
        "list.equal", serem::IRType::boolType(), {left, right}, {{"kind", std::to_string(kind)}});
  }
  return builder_->compare("eq", left, right);
}

serem::ValuePtr SeremGenerator::emitBinary(const BinaryExpr& expression) {
  // `and` / `or` short-circuit, so the right operand gets its own block and the
  // result is read back from a slot afterwards.
  if (expression.op() == BinaryOp::And || expression.op() == BinaryOp::Or) {
    const serem::IRType type = lowerType(expression.resolvedType());
    const serem::ValuePtr left = emitExpression(expression.left());
    const serem::ValuePtr slot = builder_->alloca(type);
    builder_->store(left, slot);
    serem::BasicBlock* rhs = &function_->addBlock("logic.rhs");
    serem::BasicBlock* done = &function_->addBlock("logic.end");
    // `and` evaluates the right operand only when the left is true; `or` only
    // when it is false.
    const bool conjunction = expression.op() == BinaryOp::And;
    (void)builder_->conditionalBranch(left, conjunction ? *rhs : *done, conjunction ? *done : *rhs);
    builder_->setInsertBlock(*rhs);
    builder_->store(emitExpression(expression.right()), slot);
    (void)builder_->branch(*done);
    builder_->setInsertBlock(*done);
    return builder_->load(slot, type);
  }
  // The type checker records which class implements an operator, so lowering
  // only has to call that method.
  const BinaryDunderNames names = binaryDunderNames(expression.op());
  if (expression.overload() != BinaryOverload::None) {
    const bool reflected = expression.overload() == BinaryOverload::Right;
    const Expr& receiver = reflected ? expression.right() : expression.left();
    const Expr& argument = reflected ? expression.left() : expression.right();
    const Type* record =
        receiver.resolvedType() == nullptr ? nullptr : receiver.resolvedType()->valueType();
    const char* method = expression.overload() == BinaryOverload::EqFallback
                             ? "__eq__"
                             : (reflected ? names.reflected : names.method);
    if (record != nullptr && record->isRecord() && method != nullptr) {
      serem::ValuePtr result =
          callMethod(record, method, emitExpression(receiver), {emitExpression(argument)});
      if (result != nullptr) {
        if (expression.overload() == BinaryOverload::EqFallback) {
          return builder_->operation("not", serem::IRType::boolType(), {result});
        }
        return result;
      }
    }
  }
  serem::ValuePtr left = emitExpression(expression.left());
  serem::ValuePtr right = expression.op() == BinaryOp::Is || expression.op() == BinaryOp::IsNot
                              ? nullptr
                              : emitExpression(expression.right());
  // `x is None` / `x == None` is a null check for a pointer-shaped value, but a
  // tagged optional (`str | None`) keeps its value inline, so the test is on the
  // union tag instead. `pointer.is_null` on the aggregate would be invalid IR.
  const auto noneTest =
      [&](const Expr& operand, serem::ValuePtr value, bool negated) -> serem::ValuePtr {
    const Type* source = operand.resolvedType();
    if (!subst_.empty() && source != nullptr) {
      source = types_->substitute(source, subst_);
    }
    if (source != nullptr && source->isUnion() &&
        lowerType(source).kind() == serem::IRType::Kind::Struct) {
      const int member = source->unionMemberIndex(types_->noneType());
      std::unordered_map<std::string, std::string> attributes{
          {"negated", negated ? "true" : "false"}};
      if (member >= 0) {
        attributes["tag"] = std::to_string(member);
      }
      return builder_->operation(
          "union.is", serem::IRType::boolType(), {std::move(value)}, std::move(attributes));
    }
    return builder_->operation("pointer.is_null",
                               serem::IRType::boolType(),
                               {std::move(value)},
                               {{"negated", negated ? "true" : "false"}});
  };
  // `None` is the absence of a value, so `x == None` and `x != None` are null
  // checks rather than a comparison against a literal.
  const bool leftNone = expression.left().kind() == NodeKind::NoneLiteral;
  const bool rightNone = expression.right().kind() == NodeKind::NoneLiteral;
  if (leftNone != rightNone &&
      (expression.op() == BinaryOp::Eq || expression.op() == BinaryOp::Ne)) {
    const bool positive = expression.op() == BinaryOp::Eq;
    return noneTest(
        leftNone ? expression.right() : expression.left(), leftNone ? right : left, !positive);
  }
  const serem::IRType type = lowerType(expression.resolvedType());
  const Type* lhsType = expression.left().resolvedType();
  const Type* rhsType = expression.right().resolvedType();
  lhsType = types_->substitute(lhsType, subst_);
  rhsType = types_->substitute(rhsType, subst_);
  if ((expression.op() == BinaryOp::Eq || expression.op() == BinaryOp::Ne) &&
      (lhsType->isUnion() || rhsType->isUnion())) {
    auto equal = emitUnionEquality(left, right, lhsType, rhsType);
    return expression.op() == BinaryOp::Ne
               ? builder_->operation("not", serem::IRType::boolType(), {equal})
               : equal;
  }
  if (lhsType != nullptr && rhsType != nullptr && lhsType->isList() && rhsType->isList() &&
      (expression.op() == BinaryOp::Eq || expression.op() == BinaryOp::Ne)) {
    const Type* element = lhsType->elementType();
    const int kind = element->isNamed("str")   ? 1
                     : element->isNamed("f32") ? 2
                     : element->isNamed("f64") ? 3
                                               : 0;
    return builder_->operation("list.equal",
                               type,
                               {left, right},
                               {{"kind", std::to_string(kind)},
                                {"negated", expression.op() == BinaryOp::Ne ? "true" : "false"}});
  }
  if (expression.op() == BinaryOp::Add && lhsType->isList() && rhsType->isList())
    return builder_->operation("list.concat", type, {left, right});
  if (expression.op() == BinaryOp::Mul && (lhsType->isList() || rhsType->isList()))
    return builder_->operation("list.repeat",
                               type,
                               lhsType->isList() ? std::vector<serem::ValuePtr>{left, right}
                                                 : std::vector<serem::ValuePtr>{right, left});
  if (expression.op() == BinaryOp::Add && lhsType->isNamed("str"))
    return builder_->operation("string.concat", type, {left, right});
  if (expression.op() == BinaryOp::Mul && (lhsType->isNamed("str") || rhsType->isNamed("str")))
    return builder_->operation("string.repeat",
                               type,
                               lhsType->isNamed("str") ? std::vector<serem::ValuePtr>{left, right}
                                                       : std::vector<serem::ValuePtr>{right, left});
  switch (expression.op()) {
  case BinaryOp::Add:
    return builder_->add(left, right, type);
  case BinaryOp::Sub:
    return builder_->sub(left, right, type);
  case BinaryOp::Mul:
    return builder_->mul(left, right, type);
  case BinaryOp::Div:
  case BinaryOp::FloorDiv:
    return builder_->div(left, right, type);
  case BinaryOp::Mod:
    return builder_->rem(left, right, type);
  case BinaryOp::BitAnd:
    return builder_->bitAnd(left, right, type);
  case BinaryOp::BitOr:
    return builder_->bitOr(left, right, type);
  case BinaryOp::BitXor:
    return builder_->bitXor(left, right, type);
  case BinaryOp::Shl:
    return builder_->shiftLeft(left, right, type);
  case BinaryOp::Shr:
    return builder_->shiftRight(left, right, type);
  case BinaryOp::Eq:
    return builder_->compare("eq", left, right);
  case BinaryOp::Ne:
    return builder_->compare("ne", left, right);
  case BinaryOp::In:
  case BinaryOp::NotIn: {
    std::unordered_map<std::string, std::string> attributes{
        {"negated", expression.op() == BinaryOp::NotIn ? "true" : "false"}};
    const Type* rightType = expression.right().resolvedType();
    const Type* container = rightType == nullptr ? nullptr : rightType->valueType();
    if (container != nullptr && container->isRecord() &&
        container->methodIndex("__contains__") >= 0) {
      serem::ValuePtr result =
          callMethod(container, "__contains__", emitExpression(expression.right()), {left});
      if (result != nullptr) {
        return expression.op() == BinaryOp::NotIn
                   ? builder_->operation("not", serem::IRType::boolType(), {result})
                   : result;
      }
    }
    if (lhsType != nullptr && lhsType->isIntEnum() && rightType != nullptr &&
        (rightType->isIntEnum() || rightType->isInteger())) {
      // A flags value answers `in` with a bit test rather than a scan, and the
      // operand is already its discriminant.
      const auto zero = std::make_shared<serem::ConstantInt>(0, serem::IRType::i32());
      serem::ValuePtr masked = builder_->bitAnd(left, right, serem::IRType::i32());
      serem::ValuePtr present = builder_->compare("ne", masked, zero);
      if (expression.op() == BinaryOp::NotIn) {
        return builder_->operation("not", serem::IRType::boolType(), {present});
      }
      return present;
    }
    if (rightType != nullptr && rightType->isList() && rightType->elementType() != nullptr) {
      attributes["element"] = rightType->elementType()->display();
    }
    if (rightType != nullptr && rightType->isDict()) {
      attributes["container"] = "dict";
      attributes["key.kind"] = listElementKindText(rightType->dictKeyType());
      attributes["key.dict.kind"] = dictKeyKindText(rightType->dictKeyType());
    }
    return builder_->operation(
        "contains", serem::IRType::boolType(), {left, right}, std::move(attributes));
  }
  case BinaryOp::Lt:
    return builder_->compare("lt", left, right);
  case BinaryOp::Le:
    return builder_->compare("le", left, right);
  case BinaryOp::Gt:
    return builder_->compare("gt", left, right);
  case BinaryOp::Ge:
    return builder_->compare("ge", left, right);
  case BinaryOp::Is:
  case BinaryOp::IsNot: {
    const Type* tested = expression.right().resolvedType();
    if (!subst_.empty() && tested != nullptr) {
      tested = types_->substitute(tested, subst_);
    }
    if (tested != nullptr && tested->isTypeObject() && tested->typeObjectInstance() != nullptr) {
      tested = tested->typeObjectInstance();
    }
    if (expression.right().kind() == NodeKind::NoneLiteral) {
      return noneTest(expression.left(), left, expression.op() == BinaryOp::IsNot);
    }
    const Type* source = expression.left().resolvedType();
    if (!subst_.empty() && source != nullptr) {
      source = types_->substitute(source, subst_);
    }
    if (source != nullptr && source->isUnion()) {
      std::unordered_map<std::string, std::string> attributes{
          {"negated", expression.op() == BinaryOp::IsNot ? "true" : "false"}};
      const int member = tested == nullptr ? -1 : source->unionMemberIndex(tested);
      if (member >= 0) {
        attributes["tag"] = std::to_string(member);
      } else {
        // A class the union does not list verbatim is boxed under the member it
        // derives from, so the tag only settles which member was stored and the
        // header word has to confirm the concrete class.
        const int base = unionMemberFor(source, tested);
        attributes["tag"] = std::to_string(base);
        if (base >= 0 && recordHasTypeId(tested)) {
          attributes["type.id"] =
              std::to_string(static_cast<std::int32_t>(serem::recordTypeId(tested->name())));
        } else if (base < 0 && tested != nullptr) {
          // The tested type can be a base of one or more stored members, and
          // every one of their tags matches.
          const std::string tags = unionMemberTagsDerivedFrom(source, tested);
          if (!tags.empty()) {
            attributes["tags"] = tags;
          }
        }
      }
      return builder_->operation(
          "union.is", serem::IRType::boolType(), {left}, std::move(attributes));
    }
    bool matches = source != nullptr && tested != nullptr && source->isSubtypeOf(tested);
    if (expression.op() == BinaryOp::IsNot)
      matches = !matches;
    return std::make_shared<serem::ConstantInt>(matches, serem::IRType::boolType());
  }
  default:
    return builder_->operation("binary.dynamic",
                               type,
                               {left, right},
                               {{"operator", std::to_string(static_cast<int>(expression.op()))}});
  }
}

void SeremGenerator::appendDefaults(const std::string& symbol,
                                    std::vector<serem::ValuePtr>& arguments) {
  const auto found = definitions_.find(symbol);
  if (found == definitions_.end())
    return;
  const FunctionDef& function = *found->second;
  for (std::size_t index = arguments.size(); index < function.params().size(); ++index) {
    const auto& parameter = function.params()[index];
    if (parameter.defaultValue == nullptr)
      break;
    arguments.push_back(coerce(emitExpression(*parameter.defaultValue),
                               parameter.defaultValue->resolvedType(),
                               function.resolvedType()->paramTypes()[index]));
  }
}

serem::ValuePtr SeremGenerator::emitCall(const CallExpr& expression) {
  if (expression.intrinsic() == IntrinsicKind::Str && !expression.arguments().empty()) {
    const Expr& argument = *expression.arguments()[0];
    const Type* source = types_->substitute(argument.resolvedType(), subst_);
    auto value = emitExpression(argument);
    if (source->isNamed("str"))
      return value;
    if (source->isVoidLike())
      return stringValue("None");
    if (source->methodIndex("__str__") >= 0)
      return callMethod(source, "__str__", value, {});
    return builder_->operation("string.convert",
                               serem::IRType::stringType(),
                               {value},
                               {{"unsigned", source->isUnsignedInteger() ? "true" : "false"}});
  }
  if (expression.isCast() && !expression.arguments().empty()) {
    const Expr& source = *expression.arguments()[0];
    if (expression.resolvedType()->isNamed("bool") &&
        source.resolvedType()->methodIndex("__bool__") >= 0)
      return callMethod(source.resolvedType(), "__bool__", emitExpression(source), {});
    return coerce(emitExpression(source), source.resolvedType(), expression.resolvedType());
  }
  if (expression.intrinsic() == IntrinsicKind::Load)
    return builder_->load(emitExpression(*expression.arguments()[0]),
                          lowerType(expression.resolvedType()));
  if (expression.intrinsic() == IntrinsicKind::Store) {
    builder_->store(coerce(emitExpression(*expression.arguments()[1]),
                           expression.arguments()[1]->resolvedType(),
                           expression.arguments()[0]->resolvedType()->pointeeType()),
                    emitExpression(*expression.arguments()[0]));
    return nullptr;
  }
  if (expression.intrinsic() == IntrinsicKind::Alloc)
    return builder_->operation("heap.alloc", lowerType(expression.resolvedType()));
  if (expression.intrinsic() == IntrinsicKind::Free)
    return builder_->operation(
        "heap.free", serem::IRType::voidType(), {emitExpression(*expression.arguments()[0])});
  // `Enum.variants()` reports the variant names in declaration order, which the
  // type table already records.
  if (expression.callee().kind() == NodeKind::MemberExpr) {
    const auto& callee = static_cast<const MemberExpr&>(expression.callee());
    if (callee.field() == "variants") {
      if (const Type* record = enumRecordOf(callee.object().resolvedType()); record != nullptr) {
        std::vector<serem::ValuePtr> names;
        std::unordered_set<std::string> seen;
        for (const RecordField& field : record->fields()) {
          if (field.name.empty() || !field.isStatic || !seen.insert(field.name).second)
            continue;
          names.push_back(stringValue(field.name));
        }
        const Type* element = expression.resolvedType() == nullptr
                                  ? nullptr
                                  : expression.resolvedType()->elementType();
        return builder_->operation(
            "aggregate.list",
            lowerType(expression.resolvedType()),
            std::move(names),
            {{"element", element == nullptr ? std::string{} : element->display()},
             {"element.kind", listElementKindText(element)}});
      }
    }
  }
  if (expression.callee().kind() == NodeKind::MemberExpr) {
    const auto& member = static_cast<const MemberExpr&>(expression.callee());
    const Type* receiver = member.object().resolvedType();
    if (receiver != nullptr && receiver->isModule()) {
      const RecordField* field = receiver->findField(member.field());
      const std::string symbol = !expression.loweredName().empty() ? expression.loweredName()
                                 : field == nullptr                ? member.field()
                                                                   : field->llvmName;
      std::vector<serem::ValuePtr> args;
      for (const auto& argument : expression.arguments())
        args.push_back(emitExpression(*argument));
      return builder_->call(std::make_shared<serem::FunctionRef>(
                                symbol, lowerType(expression.callee().resolvedType())),
                            std::move(args),
                            lowerType(expression.resolvedType()));
    }
  }
  if (expression.intrinsic() == IntrinsicKind::SharedNew ||
      expression.intrinsic() == IntrinsicKind::UniqueNew) {
    std::vector<serem::ValuePtr> args;
    for (const std::unique_ptr<Expr>& argument : expression.arguments()) {
      args.push_back(emitExpression(*argument));
    }
    return builder_->operation(
        expression.intrinsic() == IntrinsicKind::SharedNew ? "shared.new" : "unique.new",
        lowerType(expression.resolvedType()),
        std::move(args),
        {{"pointee",
          expression.resolvedType() == nullptr ? std::string{}
                                               : expression.resolvedType()->display()}});
  }
  if (expression.intrinsic() == IntrinsicKind::ArrayNew ||
      expression.intrinsic() == IntrinsicKind::Range) {
    std::vector<serem::ValuePtr> args;
    for (const auto& argument : expression.arguments())
      args.push_back(emitExpression(*argument));
    const Type* element = expression.resolvedType()->elementType();
    return builder_->operation(
        expression.intrinsic() == IntrinsicKind::ArrayNew ? "aggregate.array" : "aggregate.range",
        lowerType(expression.resolvedType()),
        args,
        {{"element.kind", listElementKindText(element)}});
  }
  if (expression.intrinsic() == IntrinsicKind::ListNew) {
    std::vector<serem::ValuePtr> args;
    for (const std::unique_ptr<Expr>& argument : expression.arguments()) {
      args.push_back(emitExpression(*argument));
    }
    const Type* element =
        expression.resolvedType() == nullptr ? nullptr : expression.resolvedType()->elementType();
    return builder_->operation(
        "aggregate.list",
        lowerType(expression.resolvedType()),
        std::move(args),
        {{"element", element == nullptr ? std::string{} : element->display()},
         {"element.kind", listElementKindText(element)}});
  }
  if (expression.intrinsic() == IntrinsicKind::DictNew) {
    std::vector<serem::ValuePtr> args;
    for (const std::unique_ptr<Expr>& argument : expression.arguments()) {
      args.push_back(emitExpression(*argument));
    }
    const Type* dict = expression.resolvedType();
    const Type* key = dict == nullptr ? nullptr : dict->dictKeyType();
    const Type* value = dict == nullptr ? nullptr : dict->dictValueType();
    return builder_->operation("aggregate.dict",
                               lowerType(dict),
                               std::move(args),
                               {{"key", key == nullptr ? std::string{} : key->display()},
                                {"key.kind", listElementKindText(key)},
                                {"key.dict.kind", dictKeyKindText(key)},
                                {"value", value == nullptr ? std::string{} : value->display()},
                                {"value.kind", listElementKindText(value)}});
  }
  if (expression.intrinsic() == IntrinsicKind::Len) {
    const Type* argumentType =
        expression.arguments().empty() ? nullptr : expression.arguments()[0]->resolvedType();
    serem::ValuePtr value =
        expression.arguments().empty() ? nullptr : emitExpression(*expression.arguments()[0]);
    if (argumentType != nullptr && argumentType->isRecord()) {
      const std::string symbol = methodSymbol(argumentType, "__len__");
      if (!symbol.empty()) {
        return builder_->call(std::make_shared<serem::FunctionRef>(symbol, functions_.at(symbol)),
                              {value},
                              lowerType(expression.resolvedType()));
      }
    }
    return builder_->operation(
        "runtime.len",
        lowerType(expression.resolvedType()),
        {value},
        {{"kind", argumentType == nullptr ? std::string{} : argumentType->valueType()->display()}});
  }
  if (expression.intrinsic() == IntrinsicKind::BuiltinMethod &&
      expression.callee().kind() == NodeKind::MemberExpr) {
    const auto& member = static_cast<const MemberExpr&>(expression.callee());
    std::vector<serem::ValuePtr> args{emitExpression(member.object())};
    for (const std::unique_ptr<Expr>& argument : expression.arguments()) {
      args.push_back(emitExpression(*argument));
    }
    std::unordered_map<std::string, std::string> attributes{{"name", expression.loweredName()}};
    const Type* objectType = member.object().resolvedType();
    if (objectType != nullptr && objectType->isList() && objectType->elementType() != nullptr) {
      attributes["element"] = objectType->elementType()->display();
    }
    return builder_->operation("builtin.method",
                               lowerType(expression.resolvedType()),
                               std::move(args),
                               std::move(attributes));
  }
  if (expression.callee().kind() == NodeKind::NameExpr &&
      static_cast<const NameExpr&>(expression.callee()).name() == "input") {
    std::vector<serem::ValuePtr> args;
    for (const std::unique_ptr<Expr>& argument : expression.arguments()) {
      args.push_back(emitExpression(*argument));
    }
    return builder_->operation(
        "runtime.input", lowerType(expression.resolvedType()), std::move(args));
  }
  if (expression.callee().kind() == NodeKind::NameExpr &&
      static_cast<const NameExpr&>(expression.callee()).name() == "super") {
    if (serem::ValuePtr self = local("self"))
      return self;
  }
  if (expression.intrinsic() == IntrinsicKind::Print) {
    std::vector<serem::ValuePtr> args;
    for (const std::unique_ptr<Expr>& argument : expression.arguments()) {
      serem::ValuePtr value = emitExpression(*argument);
      const Type* valueType = argument->resolvedType();
      value = printable(value, valueType);
      args.push_back(std::move(value));
    }
    return builder_->operation("runtime.print", serem::IRType::voidType(), std::move(args));
  }
  if (expression.intrinsic() == IntrinsicKind::TypeOf) {
    if (!expression.arguments().empty() && expression.arguments()[0]->resolvedType()->isAny())
      return builder_->operation(
          "any.name", serem::IRType::stringType(), {emitExpression(*expression.arguments()[0])});
    const std::string typeName =
        expression.arguments().empty() || expression.arguments()[0]->resolvedType() == nullptr
            ? "Any"
            : expression.arguments()[0]->resolvedType()->display();
    return stringValue(typeName);
  }
  if (expression.intrinsic() == IntrinsicKind::IsInstance) {
    const Type* source =
        expression.arguments().empty() || expression.arguments()[0]->resolvedType() == nullptr
            ? nullptr
            : expression.arguments()[0]->resolvedType()->canonical();
    const Type* target = nullptr;
    if (!expression.typeArgs().empty() && expression.typeArgs()[0]->resolvedType() != nullptr) {
      target = expression.typeArgs()[0]->resolvedType()->canonical();
    } else if (expression.arguments().size() >= 2 &&
               expression.arguments()[1]->resolvedType() != nullptr) {
      target = expression.arguments()[1]->resolvedType()->canonical();
    }
    const serem::ValuePtr value =
        expression.arguments().empty() ? nullptr : emitExpression(*expression.arguments()[0]);
    if (source != nullptr && source->isAny() && target != nullptr) {
      if (target->isAny())
        return std::make_shared<serem::ConstantInt>(1, serem::IRType::boolType());
      return builder_->operation("any.is",
                                 serem::IRType::boolType(),
                                 {value},
                                 {{"tag", std::to_string(serem::recordTypeId(target->display()))}});
    }
    // A class records its concrete type id in its first word, so the test reads
    // that back and accepts every class the value may hold that derives from the
    // target.
    if (source != nullptr && target != nullptr && target->isRecord() && recordHasTypeId(source)) {
      std::string ids;
      for (const Type* record : classes_) {
        if (record == nullptr || !recordHasTypeId(record) || !record->isSubtypeOf(target) ||
            !record->isSubtypeOf(source)) {
          continue;
        }
        if (!ids.empty())
          ids += ",";
        ids += std::to_string(static_cast<std::int32_t>(serem::recordTypeId(record->name())));
      }
      if (ids.empty()) {
        return std::make_shared<serem::ConstantInt>(0, serem::IRType::boolType());
      }
      return builder_->operation("object.isa", serem::IRType::boolType(), {value}, {{"ids", ids}});
    }
    if (source != nullptr && source->isUnion() && target != nullptr) {
      const int member = source->unionMemberIndex(target);
      if (member >= 0) {
        return builder_->operation(
            "union.is", serem::IRType::boolType(), {value}, {{"tag", std::to_string(member)}});
      }
      const std::string tags = unionMemberTagsDerivedFrom(source, target);
      if (!tags.empty()) {
        return builder_->operation(
            "union.is", serem::IRType::boolType(), {value}, {{"tags", tags}});
      }
      return std::make_shared<serem::ConstantInt>(0, serem::IRType::boolType());
    }
    const bool matches = source != nullptr && target != nullptr &&
                         (target->isAny() || source->matchesInstance(target));
    return std::make_shared<serem::ConstantInt>(matches ? 1 : 0, serem::IRType::boolType());
  }
  std::vector<serem::ValuePtr> args;
  if (expression.callee().kind() == NodeKind::NameExpr) {
    const std::string& name = static_cast<const NameExpr&>(expression.callee()).name();
    const auto decorated = decorators_.find(name);
    if (decorated != decorators_.end()) {
      std::vector<serem::ValuePtr> decoratedArgs;
      decoratedArgs.push_back(std::make_shared<serem::FunctionRef>(
          decorated->second,
          serem::IRType::function(serem::IRType::ptr(serem::IRType::i8()),
                                  {serem::IRType::ptr(serem::IRType::i8())})));
      decoratedArgs.push_back(std::make_shared<serem::FunctionRef>(
          name, serem::IRType::function(lowerType(expression.resolvedType()), {})));
      // Sema reorders a call with keyword or default arguments, so the bound
      // list is what the callee expects positionally.
      if (!expression.boundArguments().empty()) {
        for (const Expr* argument : expression.boundArguments()) {
          decoratedArgs.push_back(argument == nullptr ? nullptr : emitExpression(*argument));
        }
      } else {
        for (const std::unique_ptr<Expr>& argument : expression.arguments()) {
          decoratedArgs.push_back(emitExpression(*argument));
        }
      }
      return builder_->operation(
          "decorated.call", lowerType(expression.resolvedType()), std::move(decoratedArgs));
    }
  }
  if (expression.isConstructor()) {
    for (const Expr* argument : expression.boundArguments()) {
      if (argument != nullptr)
        args.push_back(emitExpression(*argument));
    }
    if (args.empty()) {
      for (const std::unique_ptr<Expr>& argument : expression.arguments()) {
        args.push_back(emitExpression(*argument));
      }
    }
    const Type* record = types_->substitute(expression.resolvedType(), subst_);
    if (record->isEnum() && expression.callee().kind() == NodeKind::MemberExpr) {
      const auto& member = static_cast<const MemberExpr&>(expression.callee());
      if (const auto* field = record->findField(member.field())) {
        for (std::size_t index = 0; index < args.size() && index < field->payloadTypes.size(); ++index) {
          args[index] = coerce(args[index], expression.arguments()[index]->resolvedType(),
                               field->payloadTypes[index]);
        }
      }
    }
    return builder_->operation(
        "construct",
        lowerType(expression.resolvedType()),
        std::move(args),
        {{"type",
          expression.resolvedType() == nullptr ? std::string{} : expression.resolvedType()->name()},
         {"type.id",
          recordHasTypeId(expression.resolvedType())
              ? std::to_string(static_cast<std::int32_t>(
                    serem::recordTypeId(expression.resolvedType()->name())))
              : std::string{}},
         {"tag", enumVariantTag(expression)},
         {"init", methodSymbol(expression.resolvedType(), "__init__")}});
  }
  if (expression.callee().kind() == NodeKind::MemberExpr && !expression.isMethod()) {
    const auto& member = static_cast<const MemberExpr&>(expression.callee());
    if (member.object().kind() == NodeKind::NameExpr) {
      const auto& object = static_cast<const NameExpr&>(member.object());
      const std::string qualified = object.name() + "_" + member.field();
      const auto found = functions_.find(qualified);
      if (found != functions_.end()) {
        const auto& bound = expression.boundArguments();
        if (!bound.empty()) {
          for (const Expr* argument : bound)
            if (argument != nullptr)
              args.push_back(emitExpression(*argument));
        } else {
          for (const auto& argument : expression.arguments())
            args.push_back(emitExpression(*argument));
        }
        appendDefaults(qualified, args);
        return builder_->call(std::make_shared<serem::FunctionRef>(qualified, found->second),
                              args,
                              lowerType(expression.resolvedType()));
      }
    }
  }
  if (expression.callee().kind() == NodeKind::MemberExpr &&
      expression.callee().resolvedType() != nullptr &&
      expression.callee().resolvedType()->kind() == TypeKind::Function) {
    const auto& member = static_cast<const MemberExpr&>(expression.callee());
    if (member.object().kind() == NodeKind::CallExpr &&
        static_cast<const CallExpr&>(member.object()).callee().kind() == NodeKind::NameExpr &&
        static_cast<const NameExpr&>(static_cast<const CallExpr&>(member.object()).callee())
                .name() == "super") {
      const auto base = classBases_.find(currentOwnerClass_);
      if (base != classBases_.end()) {
        serem::ValuePtr callee = std::make_shared<serem::FunctionRef>(
            base->second + "." + member.field(), lowerType(expression.callee().resolvedType()));
        args.push_back(local("self"));
        for (const std::unique_ptr<Expr>& argument : expression.arguments()) {
          args.push_back(emitExpression(*argument));
        }
        return builder_->call(
            std::move(callee), std::move(args), lowerType(expression.resolvedType()));
      }
    }
    if (member.object().resolvedType() != nullptr) {
      const std::string owner = member.object().resolvedType()->canonical()->name();
      const std::string qualified = owner + "." + member.field();
      const std::string method = methodSymbol(member.object().resolvedType(), member.field());
      const std::string symbol = !expression.loweredName().empty() &&
                                         functions_.contains(expression.loweredName())
                                     ? expression.loweredName()
                                     : !method.empty() ? method
                                     : functions_.contains(qualified) ? qualified : member.field();
      serem::ValuePtr callee = std::make_shared<serem::FunctionRef>(
          symbol, lowerType(expression.callee().resolvedType()));
      args.push_back(emitExpression(member.object()));
      for (const Expr* argument : expression.boundArguments()) {
        if (argument != nullptr)
          args.push_back(emitExpression(*argument));
      }
      if (expression.boundArguments().empty())
        for (const std::unique_ptr<Expr>& argument : expression.arguments()) {
          args.push_back(emitExpression(*argument));
        }
      return builder_->call(
          std::move(callee), std::move(args), lowerType(expression.resolvedType()));
    }
  }
  // A record value with `__call__` is invoked through that method.
  const Type* signature = expression.callee().resolvedType();
  const Type* callable = signature == nullptr ? nullptr : signature->valueType();
  if (callable != nullptr && callable->isRecord() && callable->methodIndex("__call__") >= 0 &&
      !methodSymbol(callable, "__call__").empty()) {
    std::vector<serem::ValuePtr> arguments;
    for (const std::unique_ptr<Expr>& argument : expression.arguments()) {
      arguments.push_back(emitExpression(*argument));
    }
    return callMethod(callable, "__call__", emitExpression(expression.callee()), arguments);
  }
  serem::ValuePtr callee;
  // A generic instantiation names its specialized symbol, which the type
  // checker records on the call as the lowered name.
  if (!expression.loweredName().empty() && expression.callee().kind() == NodeKind::NameExpr) {
    const auto found = functions_.find(expression.loweredName());
    if (found != functions_.end()) {
      callee = std::make_shared<serem::FunctionRef>(expression.loweredName(), found->second);
    }
  }
  if (callee == nullptr) {
    callee = emitExpression(expression.callee());
  }
  for (const std::unique_ptr<Expr>& argument : expression.arguments()) {
    const Type* expected = signature != nullptr && args.size() < signature->paramTypes().size()
                               ? signature->paramTypes()[args.size()]
                               : argument->resolvedType();
    args.push_back(coerce(emitExpression(*argument), argument->resolvedType(), expected));
  }
  if (callee != nullptr && callee->valueKind() == serem::ValueKind::FunctionRef)
    appendDefaults(static_cast<const serem::FunctionRef&>(*callee).name(), args);
  return builder_->call(std::move(callee), std::move(args), lowerType(expression.resolvedType()));
}

serem::ValuePtr SeremGenerator::emitMember(const MemberExpr& expression) {
  // Every enum member comes from the record sema built rather than from stored
  // state: `Color.Green` is its discriminant, `tone.value` that same number, and
  // `tone.name` the variant that number selects.
  if (const Type* record = enumRecordOf(expression.object().resolvedType()); record != nullptr) {
    const std::string& fieldName = expression.field();
    if (fieldName == "__name__") {
      return stringValue(record->name());
    }
    if (fieldName == "value" || fieldName == "name") {
      serem::ValuePtr value = emitExpression(expression.object());
      if (fieldName == "value") {
        if (!record->hasEnumPayload())
          return value;
        return builder_->operation("enum.tag", serem::IRType::i32(), {value});
      }
      return enumNameValue(record, std::move(value), false);
    }
    if (const RecordField* field = record->findField(fieldName);
        field != nullptr && field->isStatic) {
      const std::int32_t tag = enumTagOf(field);
      if (!record->hasEnumPayload()) {
        return std::make_shared<serem::ConstantInt>(tag, serem::IRType::i32());
      }
      return builder_->operation(
          "enum.unit", lowerType(record), {}, {{"tag", std::to_string(tag)}});
    }
  }
  if (const std::string symbol = staticFieldSymbol(expression); !symbol.empty()) {
    return builder_->operation(
        "static.get", lowerType(expression.resolvedType()), {}, {{"symbol", symbol}});
  }
  if (expression.object().kind() == NodeKind::NameExpr && functions_.contains(expression.field())) {
    return std::make_shared<serem::FunctionRef>(expression.field(),
                                                lowerType(expression.resolvedType()));
  }
  if (expression.object().kind() == NodeKind::NameExpr) {
    const auto& object = static_cast<const NameExpr&>(expression.object());
    const std::string qualified = object.name() + "_" + expression.field();
    if (functions_.contains(qualified)) {
      return std::make_shared<serem::FunctionRef>(qualified, lowerType(expression.resolvedType()));
    }
  }
  // A method reached through a value, or through its class name, is a callable
  // of its own: the symbol has to name the class that declares it, and a
  // receiver taken from a value is bound by a thunk.
  if (expression.resolvedType() != nullptr &&
      expression.resolvedType()->kind() == TypeKind::Function) {
    const Type* owner = expression.object().resolvedType();
    const bool throughTypeName = expression.object().kind() == NodeKind::NameExpr &&
                                 owner != nullptr && owner->isTypeObject();
    if (throughTypeName) {
      const Type* record = owner->typeObjectInstance();
      const std::string symbol = methodSymbol(record, expression.field());
      if (!symbol.empty()) {
        const auto found = functions_.find(symbol);
        if (found != functions_.end()) {
          return std::make_shared<serem::FunctionRef>(symbol, found->second);
        }
      }
      return std::make_shared<serem::FunctionRef>(expression.field(),
                                                  lowerType(expression.resolvedType()));
    }
    if (serem::ValuePtr bound = emitBoundMethod(owner, expression.field(), expression.object())) {
      return bound;
    }
  }
  std::unordered_map<std::string, std::string> attributes{{"field", expression.field()}};
  if (expression.object().resolvedType() != nullptr) {
    attributes["index"] = std::to_string(
        fieldSlot(expression.object().resolvedType(),
                  expression.object().resolvedType()->fieldIndex(expression.field())));
  }
  return builder_->operation("member.get",
                             lowerType(expression.resolvedType()),
                             {emitExpression(expression.object())},
                             std::move(attributes));
}

serem::ValuePtr SeremGenerator::emitIndex(const IndexExpr& expression) {
  const Type* objectType = expression.object().resolvedType();
  const Type* record = objectType == nullptr ? nullptr : objectType->valueType();
  if (record != nullptr && record->isRecord() && !expression.isSlice() && expression.hasStart() &&
      record->methodIndex("__getitem__") >= 0) {
    return callMethod(record,
                      "__getitem__",
                      emitExpression(expression.object()),
                      {emitExpression(*expression.start())});
  }
  std::vector<serem::ValuePtr> operands{emitExpression(expression.object())};
  if (expression.hasStart())
    operands.push_back(emitExpression(*expression.start()));
  if (expression.hasStop())
    operands.push_back(emitExpression(*expression.stop()));
  std::unordered_map<std::string, std::string> attributes{
      {"has.start", expression.hasStart() ? "true" : "false"},
      {"has.stop", expression.hasStop() ? "true" : "false"}};
  if (objectType != nullptr && objectType->isDict()) {
    const Type* key = objectType->dictKeyType();
    const Type* value = objectType->dictValueType();
    attributes["container"] = "dict";
    attributes["key.kind"] = listElementKindText(key);
    attributes["key.dict.kind"] = dictKeyKindText(key);
    attributes["value.kind"] = listElementKindText(value);
    attributes["value"] = value == nullptr ? std::string{} : value->display();
  }
  return builder_->operation(expression.isSlice() ? "slice" : "index",
                             lowerType(expression.resolvedType()),
                             std::move(operands),
                             std::move(attributes));
}

serem::ValuePtr SeremGenerator::emitAggregate(const Expr& expression) {
  std::vector<serem::ValuePtr> operands;
  std::string kind;
  if (expression.kind() == NodeKind::ListLiteral) {
    kind = "list";
    for (const std::unique_ptr<Expr>& element :
         static_cast<const ListLiteral&>(expression).elements()) {
      operands.push_back(coerce(emitExpression(*element),
                                element->resolvedType(),
                                expression.resolvedType()->elementType()));
    }
  } else if (expression.kind() == NodeKind::TupleExpr) {
    kind = "tuple";
    for (const std::unique_ptr<Expr>& element :
         static_cast<const TupleExpr&>(expression).elements()) {
      operands.push_back(emitExpression(*element));
    }
  } else {
    kind = "dict";
    const auto& dict = static_cast<const DictLiteral&>(expression);
    // The entries take the literal's declared layouts, so an `Any` value is
    // boxed here rather than stored as its unboxed self.
    const Type* keyType =
        expression.resolvedType() == nullptr ? nullptr : expression.resolvedType()->dictKeyType();
    const Type* valueType =
        expression.resolvedType() == nullptr ? nullptr : expression.resolvedType()->dictValueType();
    for (std::size_t index = 0; index < dict.keys().size(); ++index) {
      operands.push_back(
          coerce(emitExpression(*dict.keys()[index]), dict.keys()[index]->resolvedType(), keyType));
      operands.push_back(coerce(
          emitExpression(*dict.values()[index]), dict.values()[index]->resolvedType(), valueType));
    }
  }
  std::unordered_map<std::string, std::string> attributes;
  if (expression.kind() == NodeKind::ListLiteral && expression.resolvedType() != nullptr &&
      expression.resolvedType()->elementType() != nullptr) {
    const Type* element = expression.resolvedType()->elementType();
    attributes["element"] = element->display();
    attributes["element.kind"] = listElementKindText(element);
  }
  // A dict literal carries the key and value layouts, which is what sizes its
  // bucket entries at runtime.
  if (expression.kind() == NodeKind::DictLiteral && expression.resolvedType() != nullptr) {
    const Type* key = expression.resolvedType()->dictKeyType();
    const Type* value = expression.resolvedType()->dictValueType();
    attributes["key"] = key == nullptr ? std::string{} : key->display();
    attributes["key.kind"] = listElementKindText(key);
    attributes["key.dict.kind"] = dictKeyKindText(key);
    attributes["value"] = value == nullptr ? std::string{} : value->display();
    attributes["value.kind"] = listElementKindText(value);
  }
  return builder_->operation("aggregate." + kind,
                             lowerType(expression.resolvedType()),
                             std::move(operands),
                             std::move(attributes));
}

serem::ValuePtr SeremGenerator::emitComprehension(const ComprehensionExpr& expression) {
  const Type* resultType = expression.resolvedType();
  const Type* elementType =
      resultType != nullptr && resultType->isList() ? resultType->elementType() : resultType;
  if (elementType == nullptr) {
    (void)unsupported(expression, "comprehension without a result element type");
    return std::make_shared<serem::ConstantInt>(0, lowerType(resultType));
  }
  const Type* iterableType = expression.iterable().resolvedType();
  // A class with `__iter__` yields a generator, which the coroutine protocol
  // drives; a list or range is walked with the iterator helpers instead.
  const Type* custom = iterableType == nullptr ? nullptr : iterableType->valueType();
  serem::ValuePtr generator;
  const Type* generatedElement = nullptr;
  if (custom != nullptr && custom->isRecord() && custom->methodIndex("__iter__") >= 0) {
    generator = callMethod(custom, "__iter__", emitExpression(expression.iterable()), {});
    const Type* returned = custom->dunderReturn("__iter__");
    if (returned != nullptr && returned->isGenericCtor("Iterator")) {
      generatedElement = returned->genericArg(0);
    }
  }
  const Type* iterElement = generatedElement != nullptr
                                ? generatedElement
                                : (iterableType == nullptr ? nullptr : iterableType->elementType());
  if (iterElement == nullptr) {
    (void)unsupported(expression, "comprehension over an iterable without an element type");
    return std::make_shared<serem::ConstantInt>(0, lowerType(resultType));
  }
  std::unordered_map<std::string, std::string> listAttributes;
  listAttributes["element"] = elementType->display();
  listAttributes["element.kind"] = listElementKindText(elementType);
  const serem::ValuePtr list =
      builder_->operation("aggregate.list", lowerType(resultType), {}, listAttributes);
  serem::ValuePtr source;
  serem::ValuePtr iterator;
  if (generator == nullptr) {
    source = emitExpression(expression.iterable());
  }
  std::unordered_map<std::string, std::string> iteratorAttributes{
      {"element", iterElement->display()}};
  if (generator == nullptr) {
    iterator = builder_->operation(
        "iter.begin", serem::IRType::ptr(serem::IRType::i64()), {source}, iteratorAttributes);
  }
  serem::BasicBlock* condition = &function_->addBlock("comp.cond");
  serem::BasicBlock* body = &function_->addBlock("comp.body");
  serem::BasicBlock* exit = &function_->addBlock("comp.end");
  if (!builder_->currentBlock().isTerminated())
    (void)builder_->branch(*condition);
  builder_->setInsertBlock(*condition);
  serem::ValuePtr element;
  if (generator != nullptr) {
    (void)builder_->operation(
        "coro.resume", serem::IRType::voidType(), {generator}, iteratorAttributes);
    serem::ValuePtr done = builder_->operation(
        "coro.done", serem::IRType::boolType(), {generator}, iteratorAttributes);
    (void)builder_->conditionalBranch(std::move(done), *exit, *body);
    builder_->setInsertBlock(*body);
    element = builder_->load(builder_->operation("coro.promise",
                                                 serem::IRType::ptr(lowerType(iterElement)),
                                                 {generator},
                                                 iteratorAttributes),
                             lowerType(iterElement));
  } else {
    serem::ValuePtr hasNext = builder_->operation(
        "iter.has_next", serem::IRType::boolType(), {source, iterator}, iteratorAttributes);
    (void)builder_->conditionalBranch(std::move(hasNext), *body, *exit);
    builder_->setInsertBlock(*body);
    element = builder_->operation(
        "iter.next", lowerType(iterElement), {source, iterator}, iteratorAttributes);
  }
  serem::ValuePtr slot = builder_->alloca(lowerType(iterElement));
  builder_->store(element, slot);
  // The loop variable only exists inside the comprehension.
  const auto previousLocal = locals_.find(expression.name());
  const auto previousType = localTypes_.find(expression.name());
  const bool hadLocal = previousLocal != locals_.end();
  const bool hadType = previousType != localTypes_.end();
  const serem::ValuePtr savedLocal = hadLocal ? previousLocal->second : nullptr;
  const Type* savedType = hadType ? previousType->second : nullptr;
  locals_[expression.name()] = slot;
  localTypes_[expression.name()] = iterElement;
  serem::ValuePtr value = coerce(
      emitExpression(expression.element()), expression.element().resolvedType(), elementType);
  std::unordered_map<std::string, std::string> appendAttributes = listAttributes;
  appendAttributes["name"] = "list.append";
  (void)builder_->operation(
      "builtin.method", serem::IRType::voidType(), {list, value}, std::move(appendAttributes));
  if (!builder_->currentBlock().isTerminated())
    (void)builder_->branch(*condition);
  builder_->setInsertBlock(*exit);
  if (generator != nullptr) {
    (void)builder_->operation(
        "coro.destroy", serem::IRType::voidType(), {generator}, iteratorAttributes);
  }
  if (hadLocal) {
    locals_[expression.name()] = savedLocal;
  } else {
    locals_.erase(expression.name());
  }
  if (hadType) {
    localTypes_[expression.name()] = savedType;
  } else {
    localTypes_.erase(expression.name());
  }
  return list;
}

serem::ValuePtr SeremGenerator::emitUnary(const UnaryExpr& expression) {
  // A class operand is converted by its dunder method: `not v` calls `__bool__`
  // and `-v` calls `__neg__`. Records are pointers in the Serem ABI, so the
  // LLVM instruction forms would be invalid.
  const Type* operandType = expression.operand().resolvedType();
  const Type* record = operandType == nullptr ? nullptr : operandType->valueType();
  const char* dunder = nullptr;
  switch (expression.op()) {
  case UnaryOp::Not:
    dunder = "__bool__";
    break;
  case UnaryOp::Invert:
    dunder = "__invert__";
    break;
  case UnaryOp::Neg:
    dunder = "__neg__";
    break;
  case UnaryOp::Pos:
    dunder = "__pos__";
    break;
  default:
    break;
  }
  if (record != nullptr && record->isRecord() && dunder != nullptr &&
      record->methodIndex(dunder) >= 0) {
    serem::ValuePtr result = callMethod(record, dunder, emitExpression(expression.operand()), {});
    if (result != nullptr) {
      if (expression.op() == UnaryOp::Not) {
        return builder_->operation("not", serem::IRType::boolType(), {result});
      }
      return coerce(result, record->dunderReturn(dunder), expression.resolvedType());
    }
  }
  if (expression.op() == UnaryOp::AddrOf && expression.operand().kind() == NodeKind::IndexExpr) {
    const auto& index = static_cast<const IndexExpr&>(expression.operand());
    return builder_->operation("index.address",
                               lowerType(expression.resolvedType()),
                               {emitExpression(index.object()), emitExpression(*index.start())});
  }
  const serem::ValuePtr operand = emitExpression(expression.operand());
  const serem::IRType type = lowerType(expression.resolvedType());
  switch (expression.op()) {
  case UnaryOp::Neg:
    return builder_->operation("neg", type, {operand});
  case UnaryOp::Pos:
    return operand;
  case UnaryOp::Not:
    return builder_->operation("not", type, {operand});
  case UnaryOp::Invert:
    return builder_->operation("invert", type, {operand});
  case UnaryOp::PreInc:
  case UnaryOp::PreDec:
  case UnaryOp::PostInc:
  case UnaryOp::PostDec: {
    // Increment and decrement need the target's storage, so the operand is
    // lowered as an lvalue rather than as a value.
    const bool increment =
        expression.op() == UnaryOp::PreInc || expression.op() == UnaryOp::PostInc;
    const bool prefix = expression.op() == UnaryOp::PreInc || expression.op() == UnaryOp::PreDec;
    serem::ValuePtr slot = nullptr;
    if (expression.operand().kind() == NodeKind::NameExpr) {
      const auto& name = static_cast<const NameExpr&>(expression.operand());
      const auto statik = functionStatics_.find(name.name());
      if (statik != functionStatics_.end()) {
        const Type* storedType = expression.operand().resolvedType();
        const serem::IRType stored = lowerType(storedType);
        const serem::ValuePtr current =
            builder_->operation("static.get", stored, {}, {{"symbol", statik->second}});
        const serem::ValuePtr one = std::make_shared<serem::ConstantInt>(1, stored);
        const serem::ValuePtr updated =
            increment ? builder_->add(current, one, stored) : builder_->sub(current, one, stored);
        (void)builder_->operation(
            "static.set", serem::IRType::voidType(), {updated}, {{"symbol", statik->second}});
        return prefix ? updated : current;
      }
      slot = local(name.name());
    } else if (expression.operand().kind() == NodeKind::IndexExpr) {
      const auto& index = static_cast<const IndexExpr&>(expression.operand());
      const Type* objectType = index.object().resolvedType();
      slot = builder_->operation("index.address",
                                 serem::IRType::ptr(lowerType(index.object().resolvedType())),
                                 {emitExpression(index.object()), emitExpression(*index.start())});
      (void)objectType;
    }
    if (slot != nullptr) {
      const serem::IRType stored = lowerType(expression.operand().resolvedType());
      const serem::ValuePtr current = builder_->load(slot, stored);
      const serem::ValuePtr one = std::make_shared<serem::ConstantInt>(1, stored);
      const serem::ValuePtr updated =
          increment ? builder_->add(current, one, stored) : builder_->sub(current, one, stored);
      builder_->store(updated, slot);
      return prefix ? updated : current;
    }
    return builder_->operation("unary.dynamic",
                               type,
                               {operand},
                               {{"operator", std::to_string(static_cast<int>(expression.op()))}});
  }
  case UnaryOp::AddrOf:
    if (expression.operand().kind() == NodeKind::NameExpr) {
      const auto& name = static_cast<const NameExpr&>(expression.operand());
      if (serem::ValuePtr slot = local(name.name()))
        return slot;
    }
    return builder_->operation("address.of", type, {operand});
  case UnaryOp::Deref:
    return builder_->operation("deref", type, {operand});
  default:
    return builder_->operation("unary.dynamic",
                               type,
                               {operand},
                               {{"operator", std::to_string(static_cast<int>(expression.op()))}});
  }
}

bool SeremGenerator::bindLocal(const std::string& name, serem::ValuePtr value, const Type* type) {
  locals_[name] = std::move(value);
  if (type != nullptr) {
    localTypes_[name] = type;
  }
  return true;
}

serem::ValuePtr SeremGenerator::local(const std::string& name) const {
  const auto found = locals_.find(name);
  return found == locals_.end() ? nullptr : found->second;
}

bool SeremGenerator::unsupported(const Node& node, std::string_view feature) {
  diagnostics_->error(node.range(), "Serem lowering does not support " + std::string(feature));
  return false;
}

} // namespace sere
