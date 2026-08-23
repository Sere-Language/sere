/// @file Type.cpp
/// Type queries and display formatting.

#include "sere/types/Type.h"

namespace sere {

Type::Type(TypeKind kind, std::string name) : kind_(kind), name_(std::move(name)) {}

TypeKind Type::kind() const { return kind_; }

const std::string& Type::name() const { return name_; }

const std::vector<const Type*>& Type::args() const { return canonical()->args_; }

const std::vector<RecordField>& Type::fields() const { return canonical()->fields_; }

const std::vector<RecordMethod>& Type::methods() const { return canonical()->methods_; }

const std::vector<const Type*>& Type::paramTypes() const { return paramTypes_; }

const Type* Type::returnType() const { return returnType_; }

const Type* Type::canonical() const {
  const Type* current = this;
  int depth = 0;
  while (current->kind_ == TypeKind::Alias && current->underlying_ != nullptr && depth < 32) {
    current = current->underlying_;
    ++depth;
  }
  return current;
}

bool Type::isNamed(std::string_view name) const {
  const Type* type = canonical();
  return type->kind_ == TypeKind::Primitive && type->name_ == name;
}

bool Type::isStrLayout() const { return isNamed("str") || isNamed("regex"); }

bool Type::isRecord() const {
  const TypeKind kind = canonical()->kind_;
  return kind == TypeKind::Record;
}

bool Type::isTypeParam() const { return canonical()->kind_ == TypeKind::TypeParam; }

bool Type::isModule() const { return canonical()->kind_ == TypeKind::Module; }

bool Type::isAbstract() const { return canonical()->isAbstract_; }

bool Type::isEnum() const { return canonical()->isEnum_; }

bool Type::isStruct() const { return canonical()->isStruct_; }

bool Type::isFrozen() const { return canonical()->isFrozen_; }

bool Type::isNever() const { return isNamed("never"); }

bool Type::isUnion() const { return canonical()->kind_ == TypeKind::Union; }

int Type::unionMemberIndex(const Type* member) const {
  if (!isUnion() || member == nullptr) {
    return -1;
  }
  member = member->canonical();
  const std::vector<const Type*>& members = canonical()->args_;
  for (std::size_t index = 0; index < members.size(); ++index) {
    if (members[index] != nullptr && members[index]->canonical() == member) {
      return static_cast<int>(index);
    }
  }
  return -1;
}

const std::vector<const Type*>& Type::bases() const { return canonical()->bases_; }

const std::vector<std::string>& Type::typeParams() const { return canonical()->typeParams_; }

bool Type::isSubtypeOf(const Type* other) const {
  if (other == nullptr) {
    return false;
  }
  const Type* from = canonical();
  const Type* to = other->canonical();
  if (from == to) {
    return true;
  }
  for (const Type* base : from->bases_) {
    if (base != nullptr && base->isSubtypeOf(to)) {
      return true;
    }
  }
  return false;
}

bool Type::isInteger() const {
  if (isUnion()) {
    for (const Type* member : canonical()->args_) {
      if (member == nullptr || !member->isInteger()) {
        return false;
      }
    }
    return !canonical()->args_.empty();
  }
  return isScalarInteger();
}

bool Type::isScalarInteger() const {
  return isNamed("i8") || isNamed("i16") || isNamed("i32") || isNamed("i64") ||
         isNamed("u8") || isNamed("u16") || isNamed("u32") || isNamed("u64");
}

bool Type::hasEnumPayload() const {
  if (!isEnum()) {
    return false;
  }
  for (const RecordField& field : fields()) {
    if (!field.payloadTypes.empty()) {
      return true;
    }
  }
  return false;
}

bool Type::isUnsignedInteger() const {
  return isNamed("u8") || isNamed("u16") || isNamed("u32") || isNamed("u64");
}

bool Type::isFloat() const { return isNamed("f32") || isNamed("f64"); }

int Type::integerBitWidth() const {
  if (isNamed("bool")) {
    return 1;
  }
  if (isNamed("i8") || isNamed("u8")) {
    return 8;
  }
  if (isNamed("i16") || isNamed("u16")) {
    return 16;
  }
  if (isNamed("i32") || isNamed("u32") || isEnum()) {
    return 32;
  }
  if (isNamed("i64") || isNamed("u64")) {
    return 64;
  }
  return 0;
}

bool Type::isGenericCtor(std::string_view name) const {
  const Type* type = canonical();
  return type->kind_ == TypeKind::Generic && type->name_ == name;
}

const Type* Type::genericArg(std::size_t index) const {
  const Type* type = canonical();
  if (index >= type->args_.size()) {
    return nullptr;
  }
  return type->args_[index];
}

bool Type::isPointerLike() const {
  return isGenericCtor("Unique") || isGenericCtor("Shared") || isGenericCtor("Ptr");
}

bool Type::isList() const { return isGenericCtor("list"); }

bool Type::isArray() const { return isGenericCtor("array"); }

bool Type::isDict() const { return isGenericCtor("dict"); }

bool Type::isSequence() const { return isList() || isArray(); }

bool Type::isIndexable() const {
  if (isSequence() || isDict() || isNamed("str")) {
    return true;
  }
  return methodIndex("__getitem__") >= 0;
}

bool Type::isIterable() const {
  if (isSequence() || isNamed("str") || isDict()) {
    return true;
  }
  return methodIndex("__iter__") >= 0;
}

const Type* Type::dunderReturn(std::string_view methodName) const {
  const int index = methodIndex(methodName);
  if (index < 0) {
    return nullptr;
  }
  const Type* fn = methods()[static_cast<std::size_t>(index)].type;
  return fn == nullptr ? nullptr : fn->returnType();
}

const Type* Type::pointeeType() const {
  if (!isPointerLike()) {
    return nullptr;
  }
  return genericArg(0);
}

const Type* Type::elementType() const {
  if (isNamed("str")) {
    return nullptr;
  }
  if (!isSequence()) {
    return nullptr;
  }
  return genericArg(0);
}

const Type* Type::dictKeyType() const { return isDict() ? genericArg(0) : nullptr; }

const Type* Type::dictValueType() const { return isDict() ? genericArg(1) : nullptr; }

const RecordField* Type::findField(std::string_view fieldName) const {
  const Type* type = canonical();
  for (const RecordField& field : type->fields_) {
    if (field.name == fieldName) {
      return &field;
    }
  }
  return nullptr;
}

int Type::fieldIndex(std::string_view fieldName) const {
  const Type* type = canonical();
  int gep = 0;
  for (const RecordField& field : type->fields_) {
    if (field.isStatic) {
      continue;
    }
    if (field.name == fieldName) {
      return gep;
    }
    ++gep;
  }
  return -1;
}

int Type::methodIndex(std::string_view methodName) const {
  const Type* type = canonical();
  for (std::size_t index = 0; index < type->methods_.size(); ++index) {
    if (type->methods_[index].name == methodName) {
      return static_cast<int>(index);
    }
  }
  return -1;
}

std::string Type::display() const {
  if (kind_ == TypeKind::Union) {
    std::string text;
    for (std::size_t index = 0; index < args_.size(); ++index) {
      if (index != 0) {
        text += " | ";
      }
      text += args_[index] == nullptr ? "?" : args_[index]->display();
    }
    return text;
  }
  if (kind_ == TypeKind::Primitive || kind_ == TypeKind::Record || kind_ == TypeKind::Alias ||
      kind_ == TypeKind::TypeParam || kind_ == TypeKind::Module) {
    return name_;
  }
  if (kind_ == TypeKind::Generic) {
    std::string text = name_ + "[";
    for (std::size_t index = 0; index < args_.size(); ++index) {
      if (index != 0) {
        text += ", ";
      }
      text += args_[index]->display();
    }
    text += "]";
    return text;
  }
  std::string text = "(";
  for (std::size_t index = 0; index < paramTypes_.size(); ++index) {
    if (index != 0) {
      text += ", ";
    }
    text += paramTypes_[index]->display();
  }
  text += ") -> ";
  text += returnType_ == nullptr ? "void" : returnType_->display();
  return text;
}

}  // namespace sere
