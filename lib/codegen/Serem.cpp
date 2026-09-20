/// @file Serem.cpp
/// Core target-independent SSA IR for Serem.

#include "sere/codegen/Serem.h"

#include <iomanip>
#include <sstream>

namespace sere::serem {

IRType::IRType(Kind kind) : kind_(kind) {}

IRType IRType::voidType() { return IRType(Kind::Void); }
IRType IRType::boolType() { return IRType(Kind::Bool); }
IRType IRType::i8() { return IRType(Kind::I8); }
IRType IRType::i16() { return IRType(Kind::I16); }
IRType IRType::i32() { return IRType(Kind::I32); }
IRType IRType::i64() { return IRType(Kind::I64); }
IRType IRType::u8() { return IRType(Kind::U8); }
IRType IRType::u16() { return IRType(Kind::U16); }
IRType IRType::u32() { return IRType(Kind::U32); }
IRType IRType::u64() { return IRType(Kind::U64); }
IRType IRType::f32() { return IRType(Kind::F32); }
IRType IRType::f64() { return IRType(Kind::F64); }

IRType IRType::ptr(IRType pointee) {
  IRType type(Kind::Ptr);
  type.pointee_ = std::make_shared<IRType>(std::move(pointee));
  return type;
}

IRType IRType::array(IRType element, std::int64_t length) {
  IRType type(Kind::Array);
  type.elements_.push_back(std::move(element));
  type.length_ = length;
  return type;
}

IRType IRType::function(IRType result, std::vector<IRType> parameters) {
  IRType type(Kind::Function);
  type.pointee_ = std::make_shared<IRType>(std::move(result));
  type.parameters_ = std::move(parameters);
  return type;
}

IRType IRType::structType(std::string name, std::vector<IRType> fields) {
  IRType type(Kind::Struct);
  type.name_ = std::move(name);
  type.elements_ = std::move(fields);
  return type;
}

IRType IRType::label() { return IRType(Kind::Label); }
IRType IRType::stringType() { return IRType(Kind::String); }
IRType::Kind IRType::kind() const { return kind_; }
const IRType* IRType::pointee() const { return pointee_.get(); }
const std::vector<IRType>& IRType::elements() const { return elements_; }
const std::vector<IRType>& IRType::parameters() const { return parameters_; }
const std::string& IRType::name() const { return name_; }
std::int64_t IRType::length() const { return length_; }
bool IRType::isVoid() const { return kind_ == Kind::Void; }

std::string IRType::display() const {
  switch (kind_) {
  case Kind::Void: return "void";
  case Kind::Bool: return "i1";
  case Kind::I8: return "i8";
  case Kind::I16: return "i16";
  case Kind::I32: return "i32";
  case Kind::I64: return "i64";
  case Kind::U8: return "u8";
  case Kind::U16: return "u16";
  case Kind::U32: return "u32";
  case Kind::U64: return "u64";
  case Kind::F32: return "f32";
  case Kind::F64: return "f64";
  case Kind::Ptr: return "Ptr[" + pointee_->display() + "]";
  case Kind::Array: return "[" + std::to_string(length_) + " x " + elements_[0].display() + "]";
  case Kind::Function: {
    std::string result = "(";
    for (std::size_t index = 0; index < parameters_.size(); ++index) {
      if (index != 0) result += ", ";
      result += parameters_[index].display();
    }
    return result + ") -> " + pointee_->display();
  }
  case Kind::Struct: return "%" + name_;
  case Kind::Label: return "label";
  case Kind::String: return "str";
  }
  return "<invalid>";
}

Value::Value(IRType type) : type_(std::move(type)) {}
const IRType& Value::type() const { return type_; }
std::string Value::reference() const { return display(); }

ConstantInt::ConstantInt(std::int64_t value, IRType type) : Value(std::move(type)), value_(value) {}
std::int64_t ConstantInt::value() const { return value_; }
ValueKind ConstantInt::valueKind() const { return ValueKind::ConstantInt; }
std::string ConstantInt::display() const { return std::to_string(value_); }

ConstantFloat::ConstantFloat(double value, IRType type) : Value(std::move(type)), value_(value) {}
double ConstantFloat::value() const { return value_; }
ValueKind ConstantFloat::valueKind() const { return ValueKind::ConstantFloat; }
std::string ConstantFloat::display() const {
  std::ostringstream stream;
  stream << std::setprecision(17) << value_;
  return stream.str();
}

ConstantString::ConstantString(std::string value) : Value(IRType::stringType()), value_(std::move(value)) {}
ConstantString::ConstantString(std::string value, std::string globalName)
    : Value(IRType::stringType()), value_(std::move(value)), globalName_(std::move(globalName)) {}
const std::string& ConstantString::value() const { return value_; }
const std::string& ConstantString::globalName() const { return globalName_; }
ValueKind ConstantString::valueKind() const { return ValueKind::ConstantString; }
std::string ConstantString::display() const { return "\"" + value_ + "\""; }
std::string ConstantString::reference() const {
  return globalName_.empty() ? display() : "@" + globalName_;
}

Argument::Argument(std::string name, IRType type) : Value(std::move(type)), name_(std::move(name)) {}
const std::string& Argument::name() const { return name_; }
ValueKind Argument::valueKind() const { return ValueKind::Argument; }
std::string Argument::display() const { return "%" + name_; }

FunctionRef::FunctionRef(std::string name, IRType type) : Value(std::move(type)), name_(std::move(name)) {}
const std::string& FunctionRef::name() const { return name_; }
ValueKind FunctionRef::valueKind() const { return ValueKind::FunctionRef; }
std::string FunctionRef::display() const { return "@" + name_; }

Operation::Operation(std::string opcode,
                     IRType resultType,
                     std::string resultName,
                     std::vector<ValuePtr> operands,
                     std::unordered_map<std::string, std::string> attributes)
    : Value(std::move(resultType)), opcode_(std::move(opcode)), resultName_(std::move(resultName)),
      operands_(std::move(operands)), attributes_(std::move(attributes)) {}
const std::string& Operation::opcode() const { return opcode_; }
const std::string& Operation::resultName() const { return resultName_; }
const std::vector<ValuePtr>& Operation::operands() const { return operands_; }
const std::unordered_map<std::string, std::string>& Operation::attributes() const { return attributes_; }
void Operation::setOperands(std::vector<ValuePtr> operands) { operands_ = std::move(operands); }
ValueKind Operation::valueKind() const { return ValueKind::Operation; }

std::string Operation::display() const {
  std::string text;
  if (!resultName_.empty()) text = "%" + resultName_ + " = ";
  text += opcode_;
  if (!type().isVoid()) text += " " + type().display();
  if (!operands_.empty()) {
    text += " ";
    for (std::size_t index = 0; index < operands_.size(); ++index) {
      if (index != 0) text += ", ";
      text += operands_[index] == nullptr ? "<null>" : operands_[index]->reference();
    }
  }
  if (!attributes_.empty()) {
    text += " {";
    bool first = true;
    for (const auto& [key, value] : attributes_) {
      if (!first) text += ", ";
      text += key + " = " + value;
      first = false;
    }
    text += "}";
  }
  return text;
}

std::string Operation::reference() const {
  return resultName_.empty() ? display() : "%" + resultName_;
}

BasicBlock::BasicBlock(std::string label) : label_(std::move(label)) {}
const std::string& BasicBlock::label() const { return label_; }
const std::vector<std::shared_ptr<Operation>>& BasicBlock::operations() const { return operations_; }
bool BasicBlock::isTerminated() const { return terminated_; }
void BasicBlock::append(std::shared_ptr<Operation> operation) { operations_.push_back(std::move(operation)); }
void BasicBlock::setOperations(std::vector<std::shared_ptr<Operation>> operations) {
  operations_ = std::move(operations);
}
void BasicBlock::setTerminated() { terminated_ = true; }
std::string BasicBlock::display() const {
  std::string text = label_ + ":\n";
  for (const auto& operation : operations_) text += "  " + operation->display() + "\n";
  return text;
}

IRFunction::IRFunction(std::string name, std::vector<IRType> parameters, IRType result)
    : name_(std::move(name)), parameters_(std::move(parameters)), result_(std::move(result)) {
  for (std::size_t index = 0; index < parameters_.size(); ++index) {
    arguments_.push_back(std::make_shared<Argument>("arg" + std::to_string(index), parameters_[index]));
  }
}
const std::string& IRFunction::name() const { return name_; }
const std::vector<IRType>& IRFunction::parameters() const { return parameters_; }
const IRType& IRFunction::resultType() const { return result_; }
const std::vector<std::shared_ptr<Argument>>& IRFunction::arguments() const { return arguments_; }
const std::vector<std::unique_ptr<BasicBlock>>& IRFunction::blocks() const { return blocks_; }
std::shared_ptr<Argument> IRFunction::argument(std::size_t index) const {
  if (index >= arguments_.size()) return nullptr;
  return arguments_[index];
}
BasicBlock& IRFunction::addBlock(std::string label) {
  const std::string base = label;
  std::size_t suffix = 0;
  while (true) {
    bool exists = false;
    for (const auto& block : blocks_) {
      if (block->label() == label) {
        exists = true;
        break;
      }
    }
    if (!exists) break;
    label = base + "." + std::to_string(++suffix);
  }
  blocks_.push_back(std::make_unique<BasicBlock>(std::move(label)));
  return *blocks_.back();
}
bool IRFunction::removeBlock(std::string_view label) {
  for (auto iterator = blocks_.begin(); iterator != blocks_.end(); ++iterator) {
    if ((*iterator)->label() == label) {
      blocks_.erase(iterator);
      return true;
    }
  }
  return false;
}
std::string IRFunction::nextValueName() { return std::to_string(nextValue_++); }
void IRFunction::setAsync(bool value) { async_ = value; }
void IRFunction::setGenerator(bool value) { generator_ = value; }
void IRFunction::setExternal(bool value) { external_ = value; }
bool IRFunction::isExternal() const { return external_; }
void IRFunction::setAttribute(std::string name, std::string value) {
  attributes_.insert_or_assign(std::move(name), std::move(value));
}
std::string IRFunction::display() const {
  std::string text = std::string(external_ ? "extern " : "") +
                     std::string(async_ ? "async " : "") +
                     std::string(generator_ ? "generator " : "") + "func @" + name_ + "(";
  for (std::size_t index = 0; index < arguments_.size(); ++index) {
    if (index != 0) text += ", ";
    text += arguments_[index]->display() + ": " + arguments_[index]->type().display();
  }
  text += ") -> " + result_.display();  if (external_) return text + "\n";  if (!attributes_.empty()) {
    text += " [";
    bool first = true;
    for (const auto& [key, value] : attributes_) {
      if (!first) text += ", ";
      text += key + " = " + value;
      first = false;
    }
    text += "]";
  }
  text += " {\n";
  for (const auto& block : blocks_) text += block->display();
  return text + "}\n";
}

TypeDef::TypeDef(std::string name, IRType type, std::vector<std::string> attributes)
    : name_(std::move(name)), type_(std::move(type)), attributes_(std::move(attributes)) {}
const std::string& TypeDef::name() const { return name_; }
const IRType& TypeDef::type() const { return type_; }
const std::vector<std::string>& TypeDef::attributes() const { return attributes_; }
std::string TypeDef::display() const {
  std::string text = "type @" + name_ + " = " + type_.display();
  if (!attributes_.empty()) {
    text += " [";
    for (std::size_t index = 0; index < attributes_.size(); ++index) {
      if (index != 0) text += ", ";
      text += attributes_[index];
    }
    text += "]";
  }
  return text + "\n";
}

GlobalConstant::GlobalConstant(std::string name, IRType type, std::string value)
    : name_(std::move(name)), type_(std::move(type)), value_(std::move(value)) {}
const std::string& GlobalConstant::name() const { return name_; }
const IRType& GlobalConstant::type() const { return type_; }
const std::string& GlobalConstant::value() const { return value_; }
std::string GlobalConstant::display() const {
  return "global @" + name_ + " = " + type_.display() + " " + value_ + "\n";
}

IRModule::IRModule(std::string name) : name_(std::move(name)) {}
const std::string& IRModule::name() const { return name_; }
TypeDef& IRModule::addType(std::unique_ptr<TypeDef> type) {
  TypeDef& result = *type;
  types_.push_back(std::move(type));
  return result;
}
GlobalConstant& IRModule::addGlobal(std::unique_ptr<GlobalConstant> global) {
  GlobalConstant& result = *global;
  globals_.push_back(std::move(global));
  return result;
}
IRFunction& IRModule::addFunction(std::unique_ptr<IRFunction> function) {
  IRFunction& result = *function;
  functions_.push_back(std::move(function));
  return result;
}
IRFunction* IRModule::findFunction(std::string_view name) const {
  for (const auto& function : functions_) if (function->name() == name) return function.get();
  return nullptr;
}
bool IRModule::removeFunction(std::string_view name) {
  for (auto iterator = functions_.begin(); iterator != functions_.end(); ++iterator) {
    if ((*iterator)->name() == name) {
      functions_.erase(iterator);
      return true;
    }
  }
  return false;
}
bool IRModule::removeGlobal(std::string_view name) {
  for (auto iterator = globals_.begin(); iterator != globals_.end(); ++iterator) {
    if ((*iterator)->name() == name) {
      globals_.erase(iterator);
      return true;
    }
  }
  return false;
}
const std::vector<std::unique_ptr<TypeDef>>& IRModule::types() const { return types_; }
const std::vector<std::unique_ptr<GlobalConstant>>& IRModule::globals() const { return globals_; }
const std::vector<std::unique_ptr<IRFunction>>& IRModule::functions() const { return functions_; }
std::string IRModule::display() const {
  std::string text = "module @" + name_ + "\n";
  for (const auto& type : types_) text += type->display();
  for (const auto& global : globals_) text += global->display();
  for (const auto& function : functions_) text += function->display();
  return text;
}

IRBuilder::IRBuilder(IRFunction& function) : function_(&function), block_(&function.addBlock("entry")) {}
void IRBuilder::setInsertBlock(BasicBlock& block) { block_ = &block; }
BasicBlock& IRBuilder::currentBlock() const { return *block_; }
std::shared_ptr<Operation> IRBuilder::operation(std::string opcode,
                                                IRType resultType,
                                                std::vector<ValuePtr> operands,
                                                std::unordered_map<std::string, std::string> attributes) {
  const std::string resultName = resultType.isVoid() ? "" : function_->nextValueName();
  auto result = std::make_shared<Operation>(std::move(opcode), std::move(resultType), resultName,
                                            std::move(operands), std::move(attributes));
  block_->append(result);
  return result;
}
std::shared_ptr<Operation> IRBuilder::add(ValuePtr lhs, ValuePtr rhs, IRType type) {
  return operation("add", std::move(type), {std::move(lhs), std::move(rhs)});
}
std::shared_ptr<Operation> IRBuilder::sub(ValuePtr lhs, ValuePtr rhs, IRType type) {
  return operation("sub", std::move(type), {std::move(lhs), std::move(rhs)});
}
std::shared_ptr<Operation> IRBuilder::mul(ValuePtr lhs, ValuePtr rhs, IRType type) {
  return operation("mul", std::move(type), {std::move(lhs), std::move(rhs)});
}
std::shared_ptr<Operation> IRBuilder::div(ValuePtr lhs, ValuePtr rhs, IRType type) {
  return operation("div", std::move(type), {std::move(lhs), std::move(rhs)});
}
std::shared_ptr<Operation> IRBuilder::rem(ValuePtr lhs, ValuePtr rhs, IRType type) {
  return operation("rem", std::move(type), {std::move(lhs), std::move(rhs)});
}
std::shared_ptr<Operation> IRBuilder::fadd(ValuePtr lhs, ValuePtr rhs, IRType type) {
  return operation("fadd", std::move(type), {std::move(lhs), std::move(rhs)});
}
std::shared_ptr<Operation> IRBuilder::fsub(ValuePtr lhs, ValuePtr rhs, IRType type) {
  return operation("fsub", std::move(type), {std::move(lhs), std::move(rhs)});
}
std::shared_ptr<Operation> IRBuilder::fmul(ValuePtr lhs, ValuePtr rhs, IRType type) {
  return operation("fmul", std::move(type), {std::move(lhs), std::move(rhs)});
}
std::shared_ptr<Operation> IRBuilder::fdiv(ValuePtr lhs, ValuePtr rhs, IRType type) {
  return operation("fdiv", std::move(type), {std::move(lhs), std::move(rhs)});
}
std::shared_ptr<Operation> IRBuilder::bitAnd(ValuePtr lhs, ValuePtr rhs, IRType type) {
  return operation("and", std::move(type), {std::move(lhs), std::move(rhs)});
}
std::shared_ptr<Operation> IRBuilder::bitOr(ValuePtr lhs, ValuePtr rhs, IRType type) {
  return operation("or", std::move(type), {std::move(lhs), std::move(rhs)});
}
std::shared_ptr<Operation> IRBuilder::bitXor(ValuePtr lhs, ValuePtr rhs, IRType type) {
  return operation("xor", std::move(type), {std::move(lhs), std::move(rhs)});
}
std::shared_ptr<Operation> IRBuilder::shiftLeft(ValuePtr value, ValuePtr amount, IRType type) {
  return operation("shl", std::move(type), {std::move(value), std::move(amount)});
}
std::shared_ptr<Operation> IRBuilder::shiftRight(ValuePtr value, ValuePtr amount, IRType type) {
  return operation("shr", std::move(type), {std::move(value), std::move(amount)});
}
std::shared_ptr<Operation> IRBuilder::compare(std::string predicate, ValuePtr lhs, ValuePtr rhs) {
  return operation("cmp." + std::move(predicate), IRType::boolType(),
                   {std::move(lhs), std::move(rhs)});
}
std::shared_ptr<Operation> IRBuilder::cast(std::string kind, ValuePtr value, IRType type) {
  return operation("cast." + std::move(kind), std::move(type), {std::move(value)});
}
std::shared_ptr<Operation> IRBuilder::load(ValuePtr pointer, IRType type) {
  return operation("load", std::move(type), {std::move(pointer)});
}
std::shared_ptr<Operation> IRBuilder::alloca(IRType type) {
  return operation("alloca", IRType::ptr(type));
}
std::shared_ptr<Operation> IRBuilder::getElement(ValuePtr aggregate,
                                                  ValuePtr index,
                                                  IRType type) {
  return operation("get_element", IRType::ptr(type), {std::move(aggregate), std::move(index)});
}
std::shared_ptr<Operation> IRBuilder::extract(ValuePtr aggregate, std::size_t index, IRType type) {
  return operation("extract", std::move(type), {std::move(aggregate)},
                   {{"index", std::to_string(index)}});
}
std::shared_ptr<Operation> IRBuilder::insert(ValuePtr aggregate,
                                             ValuePtr value,
                                             std::size_t index,
                                             IRType type) {
  return operation("insert", std::move(type), {std::move(aggregate), std::move(value)},
                   {{"index", std::to_string(index)}});
}
std::shared_ptr<Operation> IRBuilder::call(ValuePtr callee,
                                           std::vector<ValuePtr> arguments,
                                           IRType resultType) {
  std::vector<ValuePtr> operands;
  operands.push_back(std::move(callee));
  for (auto& argument : arguments) operands.push_back(std::move(argument));
  return operation("call", std::move(resultType), std::move(operands));
}
std::shared_ptr<Operation> IRBuilder::phi(IRType type, std::vector<ValuePtr> incoming) {
  return operation("phi", std::move(type), std::move(incoming));
}
std::shared_ptr<Operation> IRBuilder::select(ValuePtr condition,
                                             ValuePtr ifTrue,
                                             ValuePtr ifFalse,
                                             IRType type) {
  return operation("select", std::move(type),
                   {std::move(condition), std::move(ifTrue), std::move(ifFalse)});
}
std::shared_ptr<Operation> IRBuilder::branch(BasicBlock& target) {
  auto result = operation("branch", IRType::voidType(), {}, {{"target", target.label()}});
  block_->setTerminated();
  return result;
}
std::shared_ptr<Operation> IRBuilder::conditionalBranch(ValuePtr condition,
                                                         BasicBlock& ifTrue,
                                                         BasicBlock& ifFalse) {
  auto result = operation("cond_branch", IRType::voidType(), {std::move(condition)},
                          {{"true", ifTrue.label()}, {"false", ifFalse.label()}});
  block_->setTerminated();
  return result;
}
std::shared_ptr<Operation> IRBuilder::unreachable() {
  auto result = operation("unreachable", IRType::voidType());
  block_->setTerminated();
  return result;
}
void IRBuilder::store(ValuePtr value, ValuePtr pointer) {
  (void)operation("store", IRType::voidType(), {std::move(value), std::move(pointer)});
}
void IRBuilder::ret(ValuePtr value) {
  (void)operation("return", IRType::voidType(), {std::move(value)});
  block_->setTerminated();
}
void IRBuilder::retVoid() {
  (void)operation("return", IRType::voidType());
  block_->setTerminated();
}
std::shared_ptr<Operation> IRBuilder::await(ValuePtr value, IRType type) {
  return operation("await", std::move(type), {std::move(value)});
}
std::shared_ptr<Operation> IRBuilder::coroBegin(IRType element, bool generator) {
  return operation("coro.begin", IRType::ptr(std::move(element)), {},
                   {{"generator", generator ? "true" : "false"}});
}
void IRBuilder::coroSuspend(ValuePtr token) {
  (void)operation("coro.suspend", IRType::voidType(), {std::move(token)});
}
void IRBuilder::coroEnd() {
  (void)operation("coro.end", IRType::voidType());
}
std::shared_ptr<Operation> IRBuilder::asyncCreate(ValuePtr function,
                                                   std::vector<ValuePtr> arguments,
                                                   IRType type) {
  std::vector<ValuePtr> operands;
  operands.push_back(std::move(function));
  for (auto& argument : arguments) operands.push_back(std::move(argument));
  return operation("async.create", std::move(type), std::move(operands));
}
void IRBuilder::asyncResume(ValuePtr task) {
  (void)operation("async.resume", IRType::voidType(), {std::move(task)});
}
void IRBuilder::asyncDestroy(ValuePtr task) {
  (void)operation("async.destroy", IRType::voidType(), {std::move(task)});
}
void IRBuilder::yield(ValuePtr value) {
  (void)operation("yield", IRType::voidType(), {std::move(value)});
}
std::shared_ptr<Operation> IRBuilder::invoke(ValuePtr callee,
                                             std::vector<ValuePtr> arguments,
                                             IRType resultType) {
  std::vector<ValuePtr> operands;
  operands.push_back(std::move(callee));
  for (auto& argument : arguments) operands.push_back(std::move(argument));
  return operation("invoke", std::move(resultType), std::move(operands));
}
void IRBuilder::throwValue(ValuePtr value) {
  (void)operation("throw", IRType::voidType(), {std::move(value)});
  block_->setTerminated();
}

} // namespace sere::serem
