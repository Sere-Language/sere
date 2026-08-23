/// @file TypeContext.cpp
/// Interns types so equivalent types share a single instance.

#include "sere/types/TypeContext.h"

#include <algorithm>

namespace sere {
namespace {

std::string genericKey(std::string_view ctor, const std::vector<const Type*>& args) {
  std::string key = "G:";
  key += ctor;
  key += '<';
  for (std::size_t index = 0; index < args.size(); ++index) {
    if (index != 0) {
      key += ',';
    }
    key += args[index]->display();
  }
  key += '>';
  return key;
}

[[nodiscard]] std::string mangleTypeArgs(const std::vector<const Type*>& args) {
  std::string mangled;
  for (const Type* arg : args) {
    mangled += "_";
    if (arg == nullptr) {
      mangled += "unknown";
      continue;
    }
    for (const char ch : arg->display()) {
      mangled += (ch == '[' || ch == ']' || ch == ',' || ch == ' ') ? '_' : ch;
    }
  }
  return mangled;
}

std::string functionKey(const std::vector<const Type*>& params, const Type* returnType) {
  std::string key = "F:(";
  for (std::size_t index = 0; index < params.size(); ++index) {
    if (index != 0) {
      key += ',';
    }
    key += params[index]->display();
  }
  key += ")->";
  key += returnType->display();
  return key;
}

}  // namespace

TypeContext::TypeContext() {
  internPrimitive("void");
  internPrimitive("bool");
  internPrimitive("i8");
  internPrimitive("i16");
  internPrimitive("i32");
  internPrimitive("i64");
  internPrimitive("u8");
  internPrimitive("u16");
  internPrimitive("u32");
  internPrimitive("u64");
  internPrimitive("f32");
  internPrimitive("f64");
  internPrimitive("str");
  internPrimitive("regex");
  internPrimitive("never");
  static_cast<void>(defineAlias("byte", primitive("u8")));
}

const Type* TypeContext::internPrimitive(std::string name) {
  const std::string key = "P:" + name;
  const Type* type = intern(key, TypeKind::Primitive, name);
  primitives_[std::move(name)] = type;
  return type;
}

const Type* TypeContext::intern(const std::string& key, TypeKind kind, std::string name) {
  const auto found = interned_.find(key);
  if (found != interned_.end()) {
    return found->second.get();
  }
  auto type = std::unique_ptr<Type>(new Type(kind, std::move(name)));
  const Type* pointer = type.get();
  interned_.emplace(key, std::move(type));
  return pointer;
}

const Type* TypeContext::primitive(std::string_view name) const {
  const auto found = primitives_.find(std::string(name));
  if (found == primitives_.end()) {
    return nullptr;
  }
  return found->second;
}

const Type* TypeContext::generic(std::string_view ctor, const std::vector<const Type*>& args) {
  std::vector<const Type*> canonical;
  canonical.reserve(args.size());
  for (const Type* arg : args) {
    canonical.push_back(arg == nullptr ? nullptr : arg->canonical());
  }
  const std::string key = genericKey(ctor, canonical);
  const auto found = interned_.find(key);
  if (found != interned_.end()) {
    return found->second.get();
  }
  auto type = std::unique_ptr<Type>(new Type(TypeKind::Generic, std::string(ctor)));
  type->args_ = std::move(canonical);
  const Type* pointer = type.get();
  interned_.emplace(key, std::move(type));
  return pointer;
}

const Type* TypeContext::functionType(const std::vector<const Type*>& params,
                                      const Type* returnType) {
  std::vector<const Type*> canonicalParams;
  canonicalParams.reserve(params.size());
  for (const Type* param : params) {
    canonicalParams.push_back(param == nullptr ? nullptr : param->canonical());
  }
  const Type* canonicalReturn = returnType == nullptr ? nullptr : returnType->canonical();
  const std::string key = functionKey(canonicalParams, canonicalReturn);
  const auto found = interned_.find(key);
  if (found != interned_.end()) {
    return found->second.get();
  }
  auto type = std::unique_ptr<Type>(new Type(TypeKind::Function, "fn"));
  type->paramTypes_ = std::move(canonicalParams);
  type->returnType_ = canonicalReturn;
  const Type* pointer = type.get();
  interned_.emplace(key, std::move(type));
  return pointer;
}

Type* TypeContext::writable(const Type* type) {
  if (type == nullptr) {
    return nullptr;
  }
  for (auto& entry : interned_) {
    if (entry.second.get() == type) {
      return entry.second.get();
    }
  }
  return nullptr;
}

const Type* TypeContext::defineRecord(const std::string& name, std::vector<RecordField> fields) {
  const std::string key = "R:" + name;
  const auto found = interned_.find(key);
  if (found != interned_.end()) {
    return found->second.get();
  }
  auto type = std::unique_ptr<Type>(new Type(TypeKind::Record, name));
  type->fields_ = std::move(fields);
  const Type* pointer = type.get();
  interned_.emplace(key, std::move(type));
  records_[name] = pointer;
  return pointer;
}

void TypeContext::setRecordFields(const Type* record, std::vector<RecordField> fields) {
  if (Type* writableRecord = writable(record)) {
    writableRecord->fields_ = std::move(fields);
  }
}

void TypeContext::addRecordMethod(const Type* record, RecordMethod method) {
  if (Type* writableRecord = writable(record)) {
    writableRecord->methods_.push_back(std::move(method));
  }
}

void TypeContext::setRecordBases(const Type* record, std::vector<const Type*> bases) {
  if (Type* writableRecord = writable(record)) {
    writableRecord->bases_ = std::move(bases);
  }
}

void TypeContext::setRecordTypeParams(const Type* record, std::vector<std::string> typeParams) {
  if (Type* writableRecord = writable(record)) {
    writableRecord->typeParams_ = std::move(typeParams);
  }
}

void TypeContext::setRecordAbstract(const Type* record, bool isAbstract) {
  if (Type* writableRecord = writable(record)) {
    writableRecord->isAbstract_ = isAbstract;
  }
}

void TypeContext::setRecordEnum(const Type* record, bool isEnum) {
  if (Type* writableRecord = writable(record)) {
    writableRecord->isEnum_ = isEnum;
  }
}

void TypeContext::setRecordStruct(const Type* record, bool isStruct) {
  if (Type* writableRecord = writable(record)) {
    writableRecord->isStruct_ = isStruct;
  }
}

void TypeContext::setRecordFrozen(const Type* record, bool isFrozen) {
  if (Type* writableRecord = writable(record)) {
    writableRecord->isFrozen_ = isFrozen;
  }
}

void TypeContext::replaceRecordMethod(const Type* record, RecordMethod method) {
  Type* writableRecord = writable(record);
  if (writableRecord == nullptr) {
    return;
  }
  for (RecordMethod& existing : writableRecord->methods_) {
    if (existing.name == method.name) {
      existing = std::move(method);
      return;
    }
  }
  writableRecord->methods_.push_back(std::move(method));
}

const Type* TypeContext::defineTypeParam(const std::string& name) {
  if (const Type* existing = typeParam(name)) {
    return existing;
  }
  const Type* type = intern("TP:" + name, TypeKind::TypeParam, name);
  typeParams_[name] = type;
  return type;
}

const Type* TypeContext::defineModule(const std::string& name, std::vector<RecordField> fields) {
  const std::string key = "M:" + name;
  const auto found = interned_.find(key);
  if (found != interned_.end()) {
    found->second->fields_ = std::move(fields);
    return found->second.get();
  }
  auto type = std::unique_ptr<Type>(new Type(TypeKind::Module, name));
  type->fields_ = std::move(fields);
  const Type* pointer = type.get();
  interned_.emplace(key, std::move(type));
  return pointer;
}

const Type* TypeContext::typeParam(std::string_view name) const {
  const auto found = typeParams_.find(std::string(name));
  return found == typeParams_.end() ? nullptr : found->second;
}

const Type* TypeContext::substitute(const Type* type,
                                    const std::unordered_map<std::string, const Type*>& subst) {
  if (type == nullptr) {
    return nullptr;
  }
  type = type->canonical();
  if (type->isTypeParam()) {
    const auto found = subst.find(type->name());
    return found == subst.end() ? type : found->second;
  }
  if (type->kind() == TypeKind::Generic) {
    std::vector<const Type*> args;
    for (const Type* arg : type->args()) {
      args.push_back(substitute(arg, subst));
    }
    return generic(type->name(), args);
  }
  if (type->kind() == TypeKind::Function) {
    std::vector<const Type*> params;
    for (const Type* param : type->paramTypes()) {
      params.push_back(substitute(param, subst));
    }
    return functionType(params, substitute(type->returnType(), subst));
  }
  if (type->isUnion()) {
    std::vector<const Type*> members;
    for (const Type* member : type->args()) {
      members.push_back(substitute(member, subst));
    }
    return unionType(std::move(members));
  }
  return type;
}

const Type* TypeContext::instantiate(const Type* generic, const std::vector<const Type*>& args) {
  if (generic == nullptr) {
    return nullptr;
  }
  generic = generic->canonical();
  if (generic->typeParams().size() != args.size() || args.empty()) {
    return generic;
  }
  std::string instName = generic->name() + "[";
  for (std::size_t index = 0; index < args.size(); ++index) {
    if (index != 0) {
      instName += ", ";
    }
    instName += args[index]->display();
  }
  instName += "]";
  if (const Type* existing = record(instName)) {
    return existing;
  }
  std::unordered_map<std::string, const Type*> subst;
  for (std::size_t index = 0; index < args.size(); ++index) {
    subst[generic->typeParams()[index]] = args[index];
  }
  std::vector<RecordField> fields;
  for (const RecordField& field : generic->fields()) {
    RecordField copy = field;
    copy.type = substitute(field.type, subst);
    fields.push_back(std::move(copy));
  }
  const Type* instance = defineRecord(instName, std::move(fields));
  std::vector<const Type*> bases;
  for (const Type* base : generic->bases()) {
    if (base != nullptr && !base->typeParams().empty() && base->typeParams().size() == args.size()) {
      bases.push_back(instantiate(base, args));
    } else {
      bases.push_back(base);
    }
  }
  setRecordBases(instance, std::move(bases));
  setRecordAbstract(instance, generic->isAbstract());
  if (Type* writableInstance = writable(instance)) {
    writableInstance->args_ = args;
  }
  const std::string mangled = mangleTypeArgs(args);
  for (const RecordMethod& method : generic->methods()) {
    RecordMethod copy = method;
    if (method.type != nullptr) {
      std::vector<const Type*> params;
      for (const Type* param : method.type->paramTypes()) {
        const Type* replaced = substitute(param, subst);
        params.push_back(param == method.type->paramTypes()[0] ? instance : replaced);
      }
      if (!params.empty()) {
        params[0] = instance;
      }
      copy.type = functionType(params, substitute(method.type->returnType(), subst));
    }
    copy.llvmName = generic->name() + mangled + "_" + method.name;
    addRecordMethod(instance, std::move(copy));
  }
  instantiations_.emplace_back(generic, instance);
  return instance;
}

const FunctionInstantiation* TypeContext::instantiateFunction(
    const std::string& name,
    const std::vector<std::string>& typeParams,
    const Type* genericType,
    const std::vector<const Type*>& args) {
  if (genericType == nullptr || typeParams.size() != args.size() || args.empty()) {
    return nullptr;
  }
  const std::string llvmName = name + mangleTypeArgs(args);
  for (const FunctionInstantiation& existing : functionInstantiations_) {
    if (existing.llvmName == llvmName) {
      return &existing;
    }
  }
  std::unordered_map<std::string, const Type*> subst;
  for (std::size_t index = 0; index < typeParams.size(); ++index) {
    subst[typeParams[index]] = args[index];
  }
  FunctionInstantiation inst;
  inst.sourceName = name;
  inst.llvmName = llvmName;
  inst.typeParams = typeParams;
  inst.args = args;
  inst.specializedType = substitute(genericType, subst);
  functionInstantiations_.push_back(std::move(inst));
  return &functionInstantiations_.back();
}

const std::vector<std::pair<const Type*, const Type*>>& TypeContext::instantiations() const {
  return instantiations_;
}

const std::vector<FunctionInstantiation>& TypeContext::functionInstantiations() const {
  return functionInstantiations_;
}

const Type* TypeContext::defineAlias(const std::string& name, const Type* underlying) {
  const auto existing = aliases_.find(name);
  if (existing != aliases_.end()) {
    return existing->second;
  }
  const std::string key = "A:" + name;
  auto type = std::unique_ptr<Type>(new Type(TypeKind::Alias, name));
  type->underlying_ = underlying;
  const Type* pointer = type.get();
  interned_.emplace(key, std::move(type));
  aliases_[name] = pointer;
  return pointer;
}

const Type* TypeContext::record(std::string_view name) const {
  const auto found = records_.find(std::string(name));
  if (found == records_.end()) {
    return nullptr;
  }
  return found->second;
}

const Type* TypeContext::alias(std::string_view name) const {
  const auto found = aliases_.find(std::string(name));
  if (found == aliases_.end()) {
    return nullptr;
  }
  return found->second;
}

const Type* TypeContext::neverType() const { return primitive("never"); }

const Type* TypeContext::voidType() const { return primitive("void"); }

const Type* TypeContext::boolType() const { return primitive("bool"); }

const Type* TypeContext::i32Type() const { return primitive("i32"); }

const Type* TypeContext::i64Type() const { return primitive("i64"); }

const Type* TypeContext::f32Type() const { return primitive("f32"); }

const Type* TypeContext::f64Type() const { return primitive("f64"); }

const Type* TypeContext::strType() const { return primitive("str"); }

const Type* TypeContext::regexType() const { return primitive("regex"); }

const Type* TypeContext::uniqueType(const Type* pointee) {
  return generic("Unique", {pointee});
}

const Type* TypeContext::sharedType(const Type* pointee) {
  return generic("Shared", {pointee});
}

const Type* TypeContext::ptrType(const Type* pointee) { return generic("Ptr", {pointee}); }

const Type* TypeContext::listType(const Type* element) { return generic("list", {element}); }

const Type* TypeContext::arrayType(const Type* element) { return generic("array", {element}); }

const Type* TypeContext::dictType(const Type* key, const Type* value) {
  return generic("dict", {key, value});
}

const Type* TypeContext::unionType(std::vector<const Type*> members) {
  std::vector<const Type*> flat;
  for (const Type* member : members) {
    if (member == nullptr) {
      continue;
    }
    member = member->canonical();
    if (member->isUnion()) {
      for (const Type* nested : member->args()) {
        if (nested != nullptr) {
          flat.push_back(nested->canonical());
        }
      }
      continue;
    }
    flat.push_back(member);
  }
  std::vector<const Type*> unique;
  for (const Type* member : flat) {
    bool seen = false;
    for (const Type* existing : unique) {
      if (existing == member) {
        seen = true;
        break;
      }
    }
    if (!seen) {
      unique.push_back(member);
    }
  }
  if (unique.empty()) {
    return voidType();
  }
  if (unique.size() == 1) {
    return unique[0];
  }
  std::sort(unique.begin(), unique.end(), [](const Type* left, const Type* right) {
    return left->display() < right->display();
  });
  std::string key = "U:";
  std::string name;
  for (std::size_t index = 0; index < unique.size(); ++index) {
    if (index != 0) {
      key += "|";
      name += " | ";
    }
    key += unique[index]->display();
    name += unique[index]->display();
  }
  const auto found = interned_.find(key);
  if (found != interned_.end()) {
    return found->second.get();
  }
  auto type = std::unique_ptr<Type>(new Type(TypeKind::Union, std::move(name)));
  type->args_ = std::move(unique);
  const Type* pointer = type.get();
  interned_.emplace(std::move(key), std::move(type));
  return pointer;
}

}  // namespace sere
