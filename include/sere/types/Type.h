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
  bool stored = true;
  std::string llvmName{};
  std::string getterLlvm{};
  std::string setterLlvm{};
  bool getterPublic = true;
  bool setterPublic = true;
  std::vector<std::string> paramNames{};
  std::vector<const Type*> payloadTypes{};
  std::size_t requiredArgs = 0;
};

struct RecordMethod {
  std::string name;
  const class Type* type = nullptr;
  std::string llvmName;
  std::vector<std::string> paramNames{};
  std::vector<std::string> typeParams{};
  std::vector<const Type*> typeConstraints{};
  std::size_t requiredAfterSelf = 0;
  bool isAbstract = false;
  bool isPublic = true;
};

class Type {
public:
  [[nodiscard]] TypeKind kind() const;
  [[nodiscard]] const std::string& name() const;
  [[nodiscard]] const std::string& qualifier() const;
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
  [[nodiscard]] bool isClass() const;
  /// Built-in payload of a subclass, or this type when no payload is present.
  [[nodiscard]] const Type* valueType() const;
  [[nodiscard]] bool isInteger() const;
  /// Named integer primitive such as i32, not an integer union like Int.
  [[nodiscard]] bool isScalarInteger() const;
  [[nodiscard]] bool isUnsignedInteger() const;
  [[nodiscard]] bool isFloat() const;
  [[nodiscard]] int integerBitWidth() const;
  [[nodiscard]] bool isGenericCtor(std::string_view name) const;
  [[nodiscard]] const Type* genericArg(std::size_t index) const;
  /// `type[T]`: a class object that constructs T.
  [[nodiscard]] bool isTypeObject() const;
  [[nodiscard]] const Type* typeObjectInstance() const;
  /// A generic alias template such as `type Optional[T] = T | None`.
  /// The template itself is not a usable value type; instantiate it with
  /// `aliasTypeParams()` and `aliasUnderlying()`.
  [[nodiscard]] bool isGenericAlias() const;
  [[nodiscard]] const std::vector<std::string>& aliasTypeParams() const;
  [[nodiscard]] const std::vector<const Type*>& aliasTypeConstraints() const;
  [[nodiscard]] const Type* aliasUnderlying() const;
  [[nodiscard]] bool isEllipsis() const;
  [[nodiscard]] bool isParamList() const;
  [[nodiscard]] bool isCallableConstraint() const;
  [[nodiscard]] bool isClassConstraint() const;
  [[nodiscard]] bool isFunctionValue() const;
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
  /// Unit / flags enum (no payload variants). Usable as an integer.
  [[nodiscard]] bool isIntEnum() const;
  [[nodiscard]] bool isStruct() const;
  [[nodiscard]] bool isFrozen() const;
  [[nodiscard]] bool isFlags() const;
  [[nodiscard]] bool isUnion() const;
  [[nodiscard]] bool isIndexable() const;
  [[nodiscard]] bool isIterable() const;
  [[nodiscard]] bool isNever() const;
  [[nodiscard]] bool isAny() const;
  [[nodiscard]] bool isVoidLike() const;
  [[nodiscard]] bool hasEnumPayload() const;
  [[nodiscard]] int unionMemberIndex(const Type* member) const;
  [[nodiscard]] const std::vector<const Type*>& bases() const;
  [[nodiscard]] const std::vector<std::string>& typeParams() const;
  [[nodiscard]] const std::vector<const Type*>& typeConstraints() const;
  [[nodiscard]] bool isSubtypeOf(const Type* other) const;
  [[nodiscard]] bool matchesInstance(const Type* target) const;
  [[nodiscard]] bool isSizeLiteral() const;
  [[nodiscard]] std::int64_t sizeLiteral() const;
  [[nodiscard]] std::int64_t listSize() const;
  [[nodiscard]] const Type* dunderReturn(std::string_view methodName) const;

private:
  friend class TypeContext;
  Type(TypeKind kind, std::string name);

  TypeKind kind_;
  std::string name_;
  std::string qualifier_{};
  std::vector<const Type*> args_{};
  std::vector<RecordField> fields_{};
  std::vector<RecordMethod> methods_{};
  std::vector<const Type*> paramTypes_{};
  const Type* returnType_ = nullptr;
  const Type* underlying_ = nullptr;
  std::vector<const Type*> bases_{};
  std::vector<std::string> typeParams_{};
  std::vector<const Type*> typeConstraints_{};
  bool isAbstract_ = false;
  bool isEnum_ = false;
  bool isStruct_ = false;
  bool isFrozen_ = false;
  bool isFlags_ = false;
};

} // namespace sere
