/// @file TypeContext.h
/// Owns interned types and built-in constructors (Unique, Shared, Ptr, list).

#pragma once

#include "sere/types/Type.h"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sere {

struct FunctionInstantiation {
  std::string sourceName;
  std::string llvmName;
  std::vector<std::string> typeParams;
  std::vector<const Type*> args;
  const Type* specializedType = nullptr;
  bool isMethod = false;
};

class TypeContext {
public:
  TypeContext();

  [[nodiscard]] const Type* primitive(std::string_view name) const;
  [[nodiscard]] const Type* generic(std::string_view ctor, const std::vector<const Type*>& args);
  [[nodiscard]] const Type* functionType(const std::vector<const Type*>& params,
                                         const Type* returnType);
  [[nodiscard]] const Type* defineRecord(const std::string& name,
                                         std::vector<RecordField> fields,
                                         const std::string& qualifier = "");
  void setRecordFields(const Type* record, std::vector<RecordField> fields);
  void addRecordMethod(const Type* record, RecordMethod method);
  void setRecordBases(const Type* record, std::vector<const Type*> bases);
  void setRecordTypeConstraints(const Type* record, std::vector<const Type*> constraints);
  void setRecordTypeParams(const Type* record, std::vector<std::string> typeParams);
  void setRecordAbstract(const Type* record, bool isAbstract);
  void setRecordEnum(const Type* record, bool isEnum);
  void setRecordStruct(const Type* record, bool isStruct);
  void setRecordFrozen(const Type* record, bool isFrozen);
  void setRecordFlags(const Type* record, bool isFlags);
  void replaceRecordMethod(const Type* record, RecordMethod method);
  [[nodiscard]] const Type* defineAlias(const std::string& name, const Type* underlying);
  [[nodiscard]] const Type* defineTypeParam(const std::string& name);
  [[nodiscard]] const Type* defineModule(const std::string& name, std::vector<RecordField> fields);
  [[nodiscard]] const Type* typeParam(std::string_view name) const;
  [[nodiscard]] const Type* record(std::string_view name) const;
  [[nodiscard]] const Type* alias(std::string_view name) const;
  [[nodiscard]] const Type* moduleType(std::string_view name) const;
  [[nodiscard]] const Type* lookupNamed(std::string_view name) const;
  [[nodiscard]] const Type* substitute(const Type* type,
                                       const std::unordered_map<std::string, const Type*>& subst);
  [[nodiscard]] const Type* instantiate(const Type* generic, const std::vector<const Type*>& args);
  [[nodiscard]] const FunctionInstantiation*
  instantiateFunction(const std::string& name,
                      const std::vector<std::string>& typeParams,
                      const Type* genericType,
                      const std::vector<const Type*>& args,
                      bool isMethod = false);
  [[nodiscard]] const std::vector<std::pair<const Type*, const Type*>>& instantiations() const;
  [[nodiscard]] const std::vector<FunctionInstantiation>& functionInstantiations() const;

  [[nodiscard]] const Type* neverType() const;
  [[nodiscard]] const Type* voidType() const;
  [[nodiscard]] const Type* noneType() const;
  [[nodiscard]] const Type* anyType() const;
  [[nodiscard]] const Type* boolType() const;
  [[nodiscard]] const Type* i8Type() const;
  [[nodiscard]] const Type* i32Type() const;
  [[nodiscard]] const Type* i64Type() const;
  [[nodiscard]] const Type* f32Type() const;
  [[nodiscard]] const Type* f64Type() const;
  [[nodiscard]] const Type* strType() const;
  [[nodiscard]] const Type* regexType() const;
  [[nodiscard]] const Type* uniqueType(const Type* pointee);
  [[nodiscard]] const Type* sharedType(const Type* pointee);
  [[nodiscard]] const Type* ptrType(const Type* pointee);
  [[nodiscard]] const Type* typeObject(const Type* instance);
  [[nodiscard]] const Type* ellipsisType();
  [[nodiscard]] const Type* paramList(const std::vector<const Type*>& params);
  [[nodiscard]] const Type* sizeType(std::int64_t value);
  [[nodiscard]] const Type* listType(const Type* element, std::int64_t size = -1);
  [[nodiscard]] const Type* arrayType(const Type* element);
  [[nodiscard]] const Type* dictType(const Type* key, const Type* value);
  [[nodiscard]] const Type* unionType(std::vector<const Type*> members);

private:
  [[nodiscard]] const Type* intern(const std::string& key, TypeKind kind, std::string name);
  const Type* internPrimitive(std::string name);
  Type* writable(const Type* type);

  std::unordered_map<std::string, std::unique_ptr<Type>> interned_{};
  std::unordered_map<std::string, const Type*> primitives_{};
  std::unordered_map<std::string, const Type*> records_{};
  std::unordered_map<std::string, const Type*> aliases_{};
  std::unordered_map<std::string, const Type*> typeParams_{};
  std::vector<std::pair<const Type*, const Type*>> instantiations_{};
  std::vector<FunctionInstantiation> functionInstantiations_{};
};

} // namespace sere
