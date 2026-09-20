/// @file Serem.h
/// Target-independent SSA IR used by the optional Serem code generation path.

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sere::serem {

class IRType {
public:
  enum class Kind {
    Void,
    Bool,
    I8,
    I16,
    I32,
    I64,
    U8,
    U16,
    U32,
    U64,
    F32,
    F64,
    Ptr,
    Array,
    Function,
    Struct,
    Label,
    String,
  };

  static IRType voidType();
  static IRType boolType();
  static IRType i8();
  static IRType i16();
  static IRType i32();
  static IRType i64();
  static IRType u8();
  static IRType u16();
  static IRType u32();
  static IRType u64();
  static IRType f32();
  static IRType f64();
  static IRType ptr(IRType pointee);
  static IRType array(IRType element, std::int64_t length);
  static IRType function(IRType result, std::vector<IRType> parameters);
  static IRType structType(std::string name, std::vector<IRType> fields = {});
  static IRType label();
  static IRType stringType();

  [[nodiscard]] Kind kind() const;
  [[nodiscard]] const IRType* pointee() const;
  [[nodiscard]] const std::vector<IRType>& elements() const;
  [[nodiscard]] const std::vector<IRType>& parameters() const;
  [[nodiscard]] const std::string& name() const;
  [[nodiscard]] std::int64_t length() const;
  [[nodiscard]] std::string display() const;
  [[nodiscard]] bool isVoid() const;

private:
  explicit IRType(Kind kind);

  Kind kind_;
  std::shared_ptr<IRType> pointee_{};
  std::vector<IRType> elements_{};
  std::vector<IRType> parameters_{};
  std::string name_{};
  std::int64_t length_ = 0;
};

enum class ValueKind {
  ConstantInt,
  ConstantFloat,
  ConstantString,
  Argument,
  FunctionRef,
  Operation,
};

class Value {
public:
  explicit Value(IRType type);
  virtual ~Value() = default;

  [[nodiscard]] const IRType& type() const;
  [[nodiscard]] virtual ValueKind valueKind() const = 0;
  [[nodiscard]] virtual std::string display() const = 0;
  [[nodiscard]] virtual std::string reference() const;

private:
  IRType type_;
};

using ValuePtr = std::shared_ptr<Value>;

class ConstantInt final : public Value {
public:
  ConstantInt(std::int64_t value, IRType type = IRType::i64());
  [[nodiscard]] std::int64_t value() const;
  [[nodiscard]] ValueKind valueKind() const override;
  [[nodiscard]] std::string display() const override;

private:
  std::int64_t value_;
};

class ConstantFloat final : public Value {
public:
  ConstantFloat(double value, IRType type = IRType::f64());
  [[nodiscard]] double value() const;
  [[nodiscard]] ValueKind valueKind() const override;
  [[nodiscard]] std::string display() const override;

private:
  double value_;
};

class ConstantString final : public Value {
public:
  explicit ConstantString(std::string value);
  ConstantString(std::string value, std::string globalName);
  [[nodiscard]] const std::string& value() const;
  [[nodiscard]] ValueKind valueKind() const override;
  [[nodiscard]] std::string display() const override;
  [[nodiscard]] std::string reference() const override;

private:
  std::string value_;
  std::string globalName_;
};

class Argument final : public Value {
public:
  Argument(std::string name, IRType type);
  [[nodiscard]] const std::string& name() const;
  [[nodiscard]] ValueKind valueKind() const override;
  [[nodiscard]] std::string display() const override;

private:
  std::string name_;
};

class FunctionRef final : public Value {
public:
  FunctionRef(std::string name, IRType type);
  [[nodiscard]] const std::string& name() const;
  [[nodiscard]] ValueKind valueKind() const override;
  [[nodiscard]] std::string display() const override;

private:
  std::string name_;
};

class Operation final : public Value {
public:
  Operation(std::string opcode,
            IRType resultType,
            std::string resultName,
            std::vector<ValuePtr> operands,
            std::unordered_map<std::string, std::string> attributes = {});

  [[nodiscard]] const std::string& opcode() const;
  [[nodiscard]] const std::string& resultName() const;
  [[nodiscard]] const std::vector<ValuePtr>& operands() const;
  [[nodiscard]] const std::unordered_map<std::string, std::string>& attributes() const;
  [[nodiscard]] ValueKind valueKind() const override;
  [[nodiscard]] std::string display() const override;
  [[nodiscard]] std::string reference() const override;

private:
  std::string opcode_;
  std::string resultName_;
  std::vector<ValuePtr> operands_;
  std::unordered_map<std::string, std::string> attributes_;
};

class BasicBlock {
public:
  explicit BasicBlock(std::string label);

  [[nodiscard]] const std::string& label() const;
  [[nodiscard]] const std::vector<std::shared_ptr<Operation>>& operations() const;
  [[nodiscard]] bool isTerminated() const;
  void append(std::shared_ptr<Operation> operation);
  void setTerminated();
  [[nodiscard]] std::string display() const;

private:
  std::string label_;
  std::vector<std::shared_ptr<Operation>> operations_;
  bool terminated_ = false;
};

class IRFunction {
public:
  IRFunction(std::string name, std::vector<IRType> parameters, IRType result);

  [[nodiscard]] const std::string& name() const;
  [[nodiscard]] const std::vector<IRType>& parameters() const;
  [[nodiscard]] const IRType& resultType() const;
  [[nodiscard]] const std::vector<std::shared_ptr<Argument>>& arguments() const;
  [[nodiscard]] const std::vector<std::unique_ptr<BasicBlock>>& blocks() const;
  [[nodiscard]] std::shared_ptr<Argument> argument(std::size_t index) const;
  [[nodiscard]] BasicBlock& addBlock(std::string label);
  [[nodiscard]] std::string nextValueName();
  void setAsync(bool value);
  void setGenerator(bool value);
  void setExternal(bool value);
  [[nodiscard]] bool isExternal() const;
  void setAttribute(std::string name, std::string value);
  [[nodiscard]] std::string display() const;

private:
  std::string name_;
  std::vector<IRType> parameters_;
  IRType result_;
  std::vector<std::shared_ptr<Argument>> arguments_;
  std::vector<std::unique_ptr<BasicBlock>> blocks_;
  std::unordered_map<std::string, std::string> attributes_;
  std::size_t nextValue_ = 0;
  bool async_ = false;
  bool generator_ = false;
  bool external_ = false;
};

class TypeDef {
public:
  TypeDef(std::string name, IRType type, std::vector<std::string> attributes = {});

  [[nodiscard]] const std::string& name() const;
  [[nodiscard]] const IRType& type() const;
  [[nodiscard]] const std::vector<std::string>& attributes() const;
  [[nodiscard]] std::string display() const;

private:
  std::string name_;
  IRType type_;
  std::vector<std::string> attributes_;
};

class GlobalConstant {
public:
  GlobalConstant(std::string name, IRType type, std::string value);
  [[nodiscard]] const std::string& name() const;
  [[nodiscard]] const IRType& type() const;
  [[nodiscard]] const std::string& value() const;
  [[nodiscard]] std::string display() const;

private:
  std::string name_;
  IRType type_;
  std::string value_;
};

class IRModule {
public:
  explicit IRModule(std::string name);

  [[nodiscard]] const std::string& name() const;
  TypeDef& addType(std::unique_ptr<TypeDef> type);
  GlobalConstant& addGlobal(std::unique_ptr<GlobalConstant> global);
  IRFunction& addFunction(std::unique_ptr<IRFunction> function);
  [[nodiscard]] IRFunction* findFunction(std::string_view name) const;
  [[nodiscard]] const std::vector<std::unique_ptr<TypeDef>>& types() const;
  [[nodiscard]] const std::vector<std::unique_ptr<GlobalConstant>>& globals() const;
  [[nodiscard]] const std::vector<std::unique_ptr<IRFunction>>& functions() const;
  [[nodiscard]] std::string display() const;

private:
  std::string name_;
  std::vector<std::unique_ptr<TypeDef>> types_;
  std::vector<std::unique_ptr<GlobalConstant>> globals_;
  std::vector<std::unique_ptr<IRFunction>> functions_;
};

class IRBuilder {
public:
  explicit IRBuilder(IRFunction& function);

  void setInsertBlock(BasicBlock& block);
  [[nodiscard]] BasicBlock& currentBlock() const;
  [[nodiscard]] std::shared_ptr<Operation>
  operation(std::string opcode,
            IRType resultType,
            std::vector<ValuePtr> operands = {},
            std::unordered_map<std::string, std::string> attributes = {});
  [[nodiscard]] std::shared_ptr<Operation> add(ValuePtr lhs, ValuePtr rhs, IRType type);
  [[nodiscard]] std::shared_ptr<Operation> sub(ValuePtr lhs, ValuePtr rhs, IRType type);
  [[nodiscard]] std::shared_ptr<Operation> mul(ValuePtr lhs, ValuePtr rhs, IRType type);
  [[nodiscard]] std::shared_ptr<Operation> div(ValuePtr lhs, ValuePtr rhs, IRType type);
  [[nodiscard]] std::shared_ptr<Operation> rem(ValuePtr lhs, ValuePtr rhs, IRType type);
  [[nodiscard]] std::shared_ptr<Operation> fadd(ValuePtr lhs, ValuePtr rhs, IRType type);
  [[nodiscard]] std::shared_ptr<Operation> fsub(ValuePtr lhs, ValuePtr rhs, IRType type);
  [[nodiscard]] std::shared_ptr<Operation> fmul(ValuePtr lhs, ValuePtr rhs, IRType type);
  [[nodiscard]] std::shared_ptr<Operation> fdiv(ValuePtr lhs, ValuePtr rhs, IRType type);
  [[nodiscard]] std::shared_ptr<Operation> bitAnd(ValuePtr lhs, ValuePtr rhs, IRType type);
  [[nodiscard]] std::shared_ptr<Operation> bitOr(ValuePtr lhs, ValuePtr rhs, IRType type);
  [[nodiscard]] std::shared_ptr<Operation> bitXor(ValuePtr lhs, ValuePtr rhs, IRType type);
  [[nodiscard]] std::shared_ptr<Operation> shiftLeft(ValuePtr value, ValuePtr amount, IRType type);
  [[nodiscard]] std::shared_ptr<Operation> shiftRight(ValuePtr value, ValuePtr amount, IRType type);
  [[nodiscard]] std::shared_ptr<Operation> compare(std::string predicate,
                                                   ValuePtr lhs,
                                                   ValuePtr rhs);
  [[nodiscard]] std::shared_ptr<Operation> cast(std::string kind, ValuePtr value, IRType type);
  [[nodiscard]] std::shared_ptr<Operation> load(ValuePtr pointer, IRType type);
  [[nodiscard]] std::shared_ptr<Operation> alloca(IRType type);
  [[nodiscard]] std::shared_ptr<Operation> getElement(ValuePtr aggregate,
                                                       ValuePtr index,
                                                       IRType type);
  [[nodiscard]] std::shared_ptr<Operation> extract(ValuePtr aggregate,
                                                    std::size_t index,
                                                    IRType type);
  [[nodiscard]] std::shared_ptr<Operation> insert(ValuePtr aggregate,
                                                   ValuePtr value,
                                                   std::size_t index,
                                                   IRType type);
  [[nodiscard]] std::shared_ptr<Operation>
  call(ValuePtr callee, std::vector<ValuePtr> arguments, IRType resultType);
  [[nodiscard]] std::shared_ptr<Operation> phi(IRType type, std::vector<ValuePtr> incoming);
  [[nodiscard]] std::shared_ptr<Operation>
  select(ValuePtr condition, ValuePtr ifTrue, ValuePtr ifFalse, IRType type);
  [[nodiscard]] std::shared_ptr<Operation> branch(BasicBlock& target);
  [[nodiscard]] std::shared_ptr<Operation>
  conditionalBranch(ValuePtr condition, BasicBlock& ifTrue, BasicBlock& ifFalse);
  [[nodiscard]] std::shared_ptr<Operation> unreachable();
  void store(ValuePtr value, ValuePtr pointer);
  void ret(ValuePtr value);
  void retVoid();
  [[nodiscard]] std::shared_ptr<Operation> await(ValuePtr value, IRType type);
  [[nodiscard]] std::shared_ptr<Operation> coroBegin();
  void coroSuspend(ValuePtr token);
  void coroEnd();
  [[nodiscard]] std::shared_ptr<Operation> asyncCreate(ValuePtr function,
                                                         std::vector<ValuePtr> arguments,
                                                         IRType type);
  void asyncResume(ValuePtr task);
  void asyncDestroy(ValuePtr task);
  void yield(ValuePtr value);
  [[nodiscard]] std::shared_ptr<Operation> invoke(ValuePtr callee,
                                                   std::vector<ValuePtr> arguments,
                                                   IRType resultType);
  void throwValue(ValuePtr value);

private:
  IRFunction* function_;
  BasicBlock* block_;
};

} // namespace sere::serem
