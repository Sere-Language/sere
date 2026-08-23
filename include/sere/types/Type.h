/// @file Type.h
/// Interned semantic types. Pointer identity is equality.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sere {

enum class TypeKind {
  Primitive,
  Generic,
  Record,
  Function,
  Alias,
  TypeParam,
  Module,
  Union,
};

struct RecordField {
  std::string name;
  const class Type* type = nullptr;
  bool isPublic = true;
  bool isStatic = false;
  std::string llvmName{};
  std::vector<std::string> paramNames{};
  std::vector<const Type*> payloadTypes{};
  std::size_t requiredArgs = 0;
};

struct RecordMethod {
  std::string name;
  const class Type* type = nullptr;
  std::string llvmName;
  std::vector<std::string> paramNames{};
  std::size_t requiredAfterSelf = 0;
  bool isAbstract = false;
};

class Type {
public:
  [[nodiscard]] TypeKind kind() const;
  [[nodiscard]] const std::string& name() const;
  [[nodiscard]] const std::vector<const Type*>& args() const;
  [[nodiscard]] const std::vector<RecordField>& fields() const;
  [[nodiscard]] const std::vector<RecordMethod>& methods() const;
  [[nodiscard]] const std::vector<const Type*>& paramTypes() const;
  [[nodiscard]] const Type* returnType() const;
  [[nodiscard]] const Type* canonical() const;
  [[nodiscard]] std::string display() const;
  [[nodiscard]] bool isNamed(std::string_view name) const;
  [[nodiscard]] bool isStrLayout() const;
  [[nodiscard]] bool isRecord() const;
  [[nodiscard]] bool isInteger() const;
  /// Named integer primitive such as i32, not an integer union like Int.
  [[nodiscard]] bool isScalarInteger() const;
  [[nodiscard]] bool isUnsignedInteger() const;
  [[nodiscard]] bool isFloat() const;
  [[nodiscard]] int integerBitWidth() const;
  [[nodiscard]] bool isGenericCtor(std::string_view name) const;
  [[nodiscard]] const Type* genericArg(std::size_t index) const;
  [[nodiscard]] bool isPointerLike() const;
  [[nodiscard]] bool isList() const;
  [[nodiscard]] bool isArray() const;
  [[nodiscard]] bool isDict() const;
  [[nodiscard]] bool isSequence() const;
  [[nodiscard]] const Type* pointeeType() const;
  [[nodiscard]] const Type* elementType() const;
  [[nodiscard]] const Type* dictKeyType() const;
  [[nodiscard]] const Type* dictValueType() const;
  [[nodiscard]] const RecordField* findField(std::string_view fieldName) const;
  [[nodiscard]] int fieldIndex(std::string_view fieldName) const;
  [[nodiscard]] int methodIndex(std::string_view methodName) const;
  [[nodiscard]] bool isTypeParam() const;
  [[nodiscard]] bool isModule() const;
  [[nodiscard]] bool isAbstract() const;
  [[nodiscard]] bool isEnum() const;
  [[nodiscard]] bool isStruct() const;
  [[nodiscard]] bool isFrozen() const;
  [[nodiscard]] bool isUnion() const;
  [[nodiscard]] bool isIndexable() const;
  [[nodiscard]] bool isIterable() const;
  [[nodiscard]] bool isNever() const;
  [[nodiscard]] bool hasEnumPayload() const;
  [[nodiscard]] int unionMemberIndex(const Type* member) const;
  [[nodiscard]] const std::vector<const Type*>& bases() const;
  [[nodiscard]] const std::vector<std::string>& typeParams() const;
  [[nodiscard]] bool isSubtypeOf(const Type* other) const;
  [[nodiscard]] const Type* dunderReturn(std::string_view methodName) const;

private:
  friend class TypeContext;
  Type(TypeKind kind, std::string name);

  TypeKind kind_;
  std::string name_;
  std::vector<const Type*> args_{};
  std::vector<RecordField> fields_{};
  std::vector<RecordMethod> methods_{};
  std::vector<const Type*> paramTypes_{};
  const Type* returnType_ = nullptr;
  const Type* underlying_ = nullptr;
  std::vector<const Type*> bases_{};
  std::vector<std::string> typeParams_{};
  bool isAbstract_ = false;
  bool isEnum_ = false;
  bool isStruct_ = false;
  bool isFrozen_ = false;
};

}  // namespace sere
