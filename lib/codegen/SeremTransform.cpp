/// @file SeremTransform.cpp
/// Rewrite passes over the target-independent Serem IR.

#include "sere/codegen/SeremTransform.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace sere::serem {

namespace {

/// Function the module starts executing from. Everything else has to be reached
/// through a call or an attribute for the dead code pass to keep it.
constexpr std::string_view kEntryPoint = "main";

/// Upper bound on the fixed-point rounds. Every pass is monotone, so the
/// pipeline settles long before this; the bound only stops a buggy pass from
/// spinning forever.
constexpr int kMaxRounds = 8;

[[nodiscard]] const ConstantInt* intValue(const ValuePtr& value) {
  if (value == nullptr || value->valueKind() != ValueKind::ConstantInt) {
    return nullptr;
  }
  return static_cast<const ConstantInt*>(value.get());
}

[[nodiscard]] const ConstantFloat* floatValue(const ValuePtr& value) {
  if (value == nullptr || value->valueKind() != ValueKind::ConstantFloat) {
    return nullptr;
  }
  return static_cast<const ConstantFloat*>(value.get());
}

[[nodiscard]] ValuePtr operandAt(const Operation& operation, std::size_t index) {
  const std::vector<ValuePtr>& operands = operation.operands();
  return index < operands.size() ? operands[index] : nullptr;
}

[[nodiscard]] std::string attributeAt(const Operation& operation, std::string_view name) {
  const auto found = operation.attributes().find(std::string(name));
  return found == operation.attributes().end() ? std::string{} : found->second;
}

/// Bit width of an integer type, or 0 when the type is not an integer.
[[nodiscard]] int bitWidth(const IRType& type) {
  switch (type.kind()) {
  case IRType::Kind::Bool:
    return 1;
  case IRType::Kind::I8:
  case IRType::Kind::U8:
    return 8;
  case IRType::Kind::I16:
  case IRType::Kind::U16:
    return 16;
  case IRType::Kind::I32:
  case IRType::Kind::U32:
    return 32;
  case IRType::Kind::I64:
  case IRType::Kind::U64:
    return 64;
  default:
    return 0;
  }
}

[[nodiscard]] bool isFloatKind(const IRType& type) {
  return type.kind() == IRType::Kind::F32 || type.kind() == IRType::Kind::F64;
}

/// Wraps a folded value into its result type's width.
///
/// Folding `!true` on an `i1` produces -2 in the host integers, and materialising
/// that as an `i1` constant truncates to the wrong bit, so every folded integer
/// is narrowed the same way the backend's constant materialisation does.
[[nodiscard]] std::int64_t wrapTo(std::int64_t value, const IRType& type) {
  const int bits = bitWidth(type);
  if (bits <= 0 || bits >= 64) {
    return value;
  }
  const std::uint64_t mask = (std::uint64_t{1} << bits) - 1;
  return static_cast<std::int64_t>(static_cast<std::uint64_t>(value) & mask);
}

/// Reads a constant back at its own type's width, so an `i8 -1` compares as -1
/// rather than as the promoted host value.
[[nodiscard]] std::int64_t signedAt(std::int64_t value, const IRType& type) {
  const int bits = bitWidth(type);
  if (bits <= 0 || bits >= 64) {
    return value;
  }
  const std::uint64_t mask = (std::uint64_t{1} << bits) - 1;
  const std::uint64_t narrowed = static_cast<std::uint64_t>(value) & mask;
  const std::uint64_t signBit = std::uint64_t{1} << (bits - 1);
  return static_cast<std::int64_t>((narrowed ^ signBit) - signBit);
}

[[nodiscard]] ValuePtr boolConstant(bool value) {
  return std::make_shared<ConstantInt>(value ? 1 : 0, IRType::boolType());
}

/// Constant-folds one operation's result, or returns nullptr when the operands
/// are not all known yet.
[[nodiscard]] ValuePtr foldOperation(const Operation& operation) {
  const std::string& opcode = operation.opcode();
  const IRType& type = operation.type();
  const ValuePtr first = operandAt(operation, 0);

  if (opcode == "select") {
    const ConstantInt* condition = intValue(first);
    if (condition != nullptr) {
      return condition->value() != 0 ? operandAt(operation, 1) : operandAt(operation, 2);
    }
    return nullptr;
  }

  const ValuePtr second = operandAt(operation, 1);
  const ConstantInt* leftInt = intValue(first);
  const ConstantInt* rightInt = intValue(second);
  const ConstantFloat* leftFloat = floatValue(first);
  const ConstantFloat* rightFloat = floatValue(second);

  // Unary integer operations: `neg` is arithmetic, while `not` and `invert` are
  // the complement the backend lowers both spellings to.
  if (leftInt != nullptr && second == nullptr) {
    if (opcode == "neg") {
      return std::make_shared<ConstantInt>(wrapTo(-leftInt->value(), type), type);
    }
    if (opcode == "not" || opcode == "invert") {
      return std::make_shared<ConstantInt>(wrapTo(~leftInt->value(), type), type);
    }
    return nullptr;
  }

  if (opcode == "cast.value" && !isFloatKind(type) && bitWidth(type) <= 0) {
    // Pointer and aggregate casts need address formatting, so they stay as they
    // are.
    return nullptr;
  }
  if (opcode == "cast.value" && leftInt != nullptr) {
    if (bitWidth(type) > 0) {
      return std::make_shared<ConstantInt>(wrapTo(leftInt->value(), type), type);
    }
    if (isFloatKind(type)) {
      return std::make_shared<ConstantFloat>(static_cast<double>(leftInt->value()), type);
    }
    return nullptr;
  }
  if (opcode == "cast.value" && leftFloat != nullptr) {
    if (isFloatKind(type)) {
      return std::make_shared<ConstantFloat>(leftFloat->value(), type);
    }
    if (bitWidth(type) > 0) {
      return std::make_shared<ConstantInt>(
          wrapTo(static_cast<std::int64_t>(leftFloat->value()), type), type);
    }
    return nullptr;
  }

  if (opcode.starts_with("cmp.") && leftInt != nullptr && rightInt != nullptr) {
    const std::string predicate = opcode.substr(4);
    // The backend compares with the signed predicate regardless of the operands'
    // declared signedness, so folding has to read the constants the same way.
    const std::int64_t lhs = signedAt(leftInt->value(), first->type());
    const std::int64_t rhs = signedAt(rightInt->value(), second->type());
    if (predicate == "eq")
      return boolConstant(lhs == rhs);
    if (predicate == "ne")
      return boolConstant(lhs != rhs);
    if (predicate == "lt")
      return boolConstant(lhs < rhs);
    if (predicate == "le")
      return boolConstant(lhs <= rhs);
    if (predicate == "gt")
      return boolConstant(lhs > rhs);
    if (predicate == "ge")
      return boolConstant(lhs >= rhs);
    return nullptr;
  }

  if (leftFloat != nullptr && rightFloat != nullptr) {
    const double lhs = leftFloat->value();
    const double rhs = rightFloat->value();
    if (opcode == "fadd")
      return std::make_shared<ConstantFloat>(lhs + rhs, type);
    if (opcode == "fsub")
      return std::make_shared<ConstantFloat>(lhs - rhs, type);
    if (opcode == "fmul")
      return std::make_shared<ConstantFloat>(lhs * rhs, type);
    // A zero divisor would fold to an infinity the printer cannot round-trip.
    if (opcode == "fdiv" && rhs != 0.0)
      return std::make_shared<ConstantFloat>(lhs / rhs, type);
    return nullptr;
  }

  if (leftInt == nullptr || rightInt == nullptr) {
    return nullptr;
  }
  const std::int64_t lhs = leftInt->value();
  const std::int64_t rhs = rightInt->value();
  if (opcode == "add")
    return std::make_shared<ConstantInt>(wrapTo(lhs + rhs, type), type);
  if (opcode == "sub")
    return std::make_shared<ConstantInt>(wrapTo(lhs - rhs, type), type);
  if (opcode == "mul")
    return std::make_shared<ConstantInt>(wrapTo(lhs * rhs, type), type);
  if (opcode == "and")
    return std::make_shared<ConstantInt>(wrapTo(lhs & rhs, type), type);
  if (opcode == "or")
    return std::make_shared<ConstantInt>(wrapTo(lhs | rhs, type), type);
  if (opcode == "xor")
    return std::make_shared<ConstantInt>(wrapTo(lhs ^ rhs, type), type);
  if (opcode == "shl" || opcode == "shr") {
    const int bits = bitWidth(type);
    if (bits <= 0 || rhs < 0 || rhs >= bits) {
      return nullptr;
    }
    if (opcode == "shl") {
      return std::make_shared<ConstantInt>(
          wrapTo(static_cast<std::int64_t>(static_cast<std::uint64_t>(lhs) << rhs), type), type);
    }
    return std::make_shared<ConstantInt>(wrapTo(signedAt(lhs, first->type()) >> rhs, type), type);
  }
  if (opcode == "div" || opcode == "rem") {
    const std::int64_t divisor = signedAt(rhs, second->type());
    if (divisor == 0) {
      return nullptr;
    }
    const std::int64_t dividend = signedAt(lhs, first->type());
    // INT_MIN / -1 overflows the host type even though the target wraps it.
    const int bits = bitWidth(first->type());
    if (bits > 0 && bits < 64 && divisor == -1 && dividend == -(std::int64_t{1} << (bits - 1))) {
      return nullptr;
    }
    if (dividend == INT64_MIN && divisor == -1) {
      return nullptr;
    }
    const std::int64_t result = opcode == "div" ? dividend / divisor : dividend % divisor;
    return std::make_shared<ConstantInt>(wrapTo(result, type), type);
  }
  return nullptr;
}

/// One sweep of constant folding, reporting whether the module changed.
///
/// Replacements are collected from the whole module before any of them is
/// applied, so a use that appears earlier in the block order than its definition
/// still sees the folded value on the same round.
[[nodiscard]] bool foldRound(IRModule& module) {
  std::unordered_map<const Value*, ValuePtr> replacements;
  for (const std::unique_ptr<IRFunction>& function : module.functions()) {
    if (function->isExternal()) {
      continue;
    }
    for (const std::unique_ptr<BasicBlock>& block : function->blocks()) {
      for (const std::shared_ptr<Operation>& operation : block->operations()) {
        if (operation == nullptr) {
          continue;
        }
        if (ValuePtr folded = foldOperation(*operation)) {
          replacements.insert_or_assign(operation.get(), std::move(folded));
        }
      }
    }
  }
  const auto substitute = [&replacements](const ValuePtr& value) -> ValuePtr {
    if (value == nullptr) {
      return value;
    }
    const auto found = replacements.find(value.get());
    return found == replacements.end() ? value : found->second;
  };

  bool changed = !replacements.empty();
  for (const std::unique_ptr<IRFunction>& function : module.functions()) {
    if (function->isExternal()) {
      continue;
    }
    for (const std::unique_ptr<BasicBlock>& block : function->blocks()) {
      std::vector<std::shared_ptr<Operation>> kept;
      kept.reserve(block->operations().size());
      for (const std::shared_ptr<Operation>& operation : block->operations()) {
        if (operation == nullptr) {
          continue;
        }
        if (replacements.contains(operation.get())) {
          continue;
        }
        std::vector<ValuePtr> operands = operation->operands();
        bool rewritten = false;
        for (ValuePtr& operand : operands) {
          ValuePtr folded = substitute(operand);
          if (folded != operand) {
            operand = std::move(folded);
            rewritten = true;
          }
        }
        if (rewritten) {
          operation->setOperands(std::move(operands));
        }
        if (operation->opcode() == "cond_branch" && !operation->operands().empty()) {
          const ConstantInt* condition = intValue(operation->operands()[0]);
          if (condition != nullptr) {
            // A branch on a constant is decided at compile time, so the test and
            // the edge it can never take both disappear.
            const std::string target = condition->value() != 0 ? attributeAt(*operation, "true")
                                                               : attributeAt(*operation, "false");
            kept.push_back(std::make_shared<Operation>(
                "branch",
                IRType::voidType(),
                std::string{},
                std::vector<ValuePtr>{},
                std::unordered_map<std::string, std::string>{{"target", target}}));
            changed = true;
            continue;
          }
        }
        kept.push_back(operation);
      }
      block->setOperations(std::move(kept));
    }
  }
  return changed;
}

class ConstantFoldPass final : public TransformPass {
public:
  [[nodiscard]] std::string_view name() const override { return "constant-fold"; }
  bool run(IRModule& module) override {
    bool changed = false;
    for (int round = 0; round < kMaxRounds; ++round) {
      if (!foldRound(module)) {
        break;
      }
      changed = true;
    }
    return changed;
  }
};

/// Keeps only the definitions reachable from the module entry point.
///
/// The generator materialises every function the program declares, including
/// each prelude helper, so a program that calls none of `clamp`, `min`, or
/// `sign` still carries them. Reachability follows call operands and attribute
/// references, because a renderer or a constructor names its callee in an
/// attribute rather than in an operand.
class DeadCodePass final : public TransformPass {
public:
  [[nodiscard]] std::string_view name() const override { return "dead-code"; }
  bool run(IRModule& module) override {
    if (module.findFunction(kEntryPoint) == nullptr) {
      // No entry point means a library module, where every definition is part of
      // the published surface.
      return false;
    }
    std::unordered_set<std::string> known;
    for (const std::unique_ptr<IRFunction>& function : module.functions()) {
      known.insert(function->name());
    }
    std::unordered_set<std::string> live;
    std::vector<std::string> pending;
    const auto mark = [&](const std::string& name) {
      if (known.contains(name) && live.insert(name).second) {
        pending.push_back(name);
      }
    };
    mark(std::string(kEntryPoint));
    while (!pending.empty()) {
      const std::string current = pending.back();
      pending.pop_back();
      const IRFunction* function = module.findFunction(current);
      if (function == nullptr) {
        continue;
      }
      for (const std::unique_ptr<BasicBlock>& block : function->blocks()) {
        for (const std::shared_ptr<Operation>& operation : block->operations()) {
          if (operation == nullptr) {
            continue;
          }
          for (const ValuePtr& operand : operation->operands()) {
            if (operand != nullptr && operand->valueKind() == ValueKind::FunctionRef) {
              mark(static_cast<const FunctionRef&>(*operand).name());
            }
          }
          for (const auto& [attribute, text] : operation->attributes()) {
            mark(text);
          }
        }
      }
    }
    std::vector<std::string> dead;
    for (const std::unique_ptr<IRFunction>& function : module.functions()) {
      if (!live.contains(function->name())) {
        dead.push_back(function->name());
      }
    }
    bool changed = false;
    for (const std::string& name : dead) {
      changed = module.removeFunction(name) || changed;
    }
    return changed;
  }
};

/// Removes blocks nothing can branch to.
///
/// Folding a branch on a constant leaves the arm it can never take behind, and
/// printing that arm suggests the program still tests something at runtime.
class UnreachableBlockPass final : public TransformPass {
public:
  [[nodiscard]] std::string_view name() const override { return "unreachable-blocks"; }
  bool run(IRModule& module) override {
    bool changed = false;
    for (const std::unique_ptr<IRFunction>& function : module.functions()) {
      if (function->isExternal()) {
        continue;
      }
      changed = pruneFunction(*function) || changed;
    }
    return changed;
  }

private:
  [[nodiscard]] static bool pruneFunction(IRFunction& function) {
    const std::vector<std::unique_ptr<BasicBlock>>& blocks = function.blocks();
    if (blocks.empty()) {
      return false;
    }
    // The first block is the entry: the backend lowers the blocks in this order
    // and LLVM treats the first one it is given as the entry too.
    std::unordered_set<std::string> labels;
    for (const std::unique_ptr<BasicBlock>& block : blocks) {
      labels.insert(block->label());
    }
    std::unordered_set<std::string> reachable;
    std::vector<std::string> pending;
    reachable.insert(blocks.front()->label());
    pending.push_back(blocks.front()->label());
    while (!pending.empty()) {
      const std::string current = pending.back();
      pending.pop_back();
      const BasicBlock* block = nullptr;
      for (const std::unique_ptr<BasicBlock>& candidate : blocks) {
        if (candidate->label() == current) {
          block = candidate.get();
          break;
        }
      }
      if (block == nullptr) {
        continue;
      }
      for (const std::shared_ptr<Operation>& operation : block->operations()) {
        if (operation == nullptr) {
          continue;
        }
        const std::string& opcode = operation->opcode();
        if (opcode != "branch" && opcode != "cond_branch") {
          // A `throw` and a failed `assert` reach their handler through an
          // attribute rather than a branch edge, so the dispatch block has to
          // stay reachable.
          if (opcode == "throw" || opcode == "assert" || opcode == "error.check") {
            const std::string label = attributeAt(*operation, "handler");
            if (!label.empty() && labels.contains(label) && reachable.insert(label).second) {
              pending.push_back(label);
            }
          }
          continue;
        }
        for (const char* key : {"target", "true", "false"}) {
          const std::string label = attributeAt(*operation, key);
          if (!label.empty() && labels.contains(label) && reachable.insert(label).second) {
            pending.push_back(label);
          }
        }
      }
    }
    std::vector<std::string> dead;
    for (const std::unique_ptr<BasicBlock>& block : blocks) {
      if (!reachable.contains(block->label())) {
        dead.push_back(block->label());
      }
    }
    bool changed = false;
    for (const std::string& label : dead) {
      changed = function.removeBlock(label) || changed;
    }
    return changed;
  }
};

/// Drops string literals no surviving function references. Literals are named
/// globals, so a folded branch or a removed function leaves them behind.
class UnusedGlobalPass final : public TransformPass {
public:
  [[nodiscard]] std::string_view name() const override { return "unused-globals"; }
  bool run(IRModule& module) override {
    std::unordered_set<std::string> used;
    for (const std::unique_ptr<IRFunction>& function : module.functions()) {
      if (function->isExternal()) {
        continue;
      }
      for (const std::unique_ptr<BasicBlock>& block : function->blocks()) {
        for (const std::shared_ptr<Operation>& operation : block->operations()) {
          if (operation == nullptr) {
            continue;
          }
          for (const ValuePtr& operand : operation->operands()) {
            if (operand == nullptr || operand->valueKind() != ValueKind::ConstantString) {
              continue;
            }
            const std::string& name = static_cast<const ConstantString&>(*operand).globalName();
            if (!name.empty()) {
              used.insert(name);
            }
          }
        }
      }
    }
    std::vector<std::string> dead;
    for (const std::unique_ptr<GlobalConstant>& global : module.globals()) {
      if (!used.contains(global->name())) {
        dead.push_back(global->name());
      }
    }
    bool changed = false;
    for (const std::string& name : dead) {
      changed = module.removeGlobal(name) || changed;
    }
    return changed;
  }
};

/// Rewrites every operand through `replacements` and drops the replaced
/// operations from their block, so a rewrite is visible to the printer and to
/// both backends.
[[nodiscard]] bool
applyReplacements(IRModule& module,
                  const std::unordered_map<const Value*, ValuePtr>& replacements) {
  const auto substitute = [&replacements](const ValuePtr& value) -> ValuePtr {
    if (value == nullptr) {
      return value;
    }
    const auto found = replacements.find(value.get());
    return found == replacements.end() ? value : found->second;
  };
  bool changed = false;
  for (const std::unique_ptr<IRFunction>& function : module.functions()) {
    if (function->isExternal()) {
      continue;
    }
    for (const std::unique_ptr<BasicBlock>& block : function->blocks()) {
      std::vector<std::shared_ptr<Operation>> kept;
      kept.reserve(block->operations().size());
      for (const std::shared_ptr<Operation>& operation : block->operations()) {
        if (operation == nullptr) {
          continue;
        }
        if (replacements.contains(operation.get())) {
          changed = true;
          continue;
        }
        std::vector<ValuePtr> operands = operation->operands();
        bool rewritten = false;
        for (ValuePtr& operand : operands) {
          ValuePtr replacement = substitute(operand);
          if (replacement != operand) {
            operand = std::move(replacement);
            rewritten = true;
          }
        }
        if (rewritten) {
          operation->setOperands(std::move(operands));
        }
        kept.push_back(operation);
      }
      if (kept.size() != block->operations().size()) {
        block->setOperations(std::move(kept));
      }
    }
  }
  return changed;
}

/// True for integer kinds whose division and remainder round toward zero the
/// way a shift or a mask does.
[[nodiscard]] bool isUnsignedKind(const IRType& type) {
  switch (type.kind()) {
  case IRType::Kind::Bool:
  case IRType::Kind::U8:
  case IRType::Kind::U16:
  case IRType::Kind::U32:
  case IRType::Kind::U64:
    return true;
  default:
    return false;
  }
}

/// Exact `log2` of a positive power of two, or -1.
[[nodiscard]] int powerOfTwoShift(std::int64_t value) {
  if (value <= 1) {
    return -1;
  }
  int shift = 0;
  std::uint64_t remaining = static_cast<std::uint64_t>(value);
  while ((remaining & 1u) == 0 && remaining != 0) {
    remaining >>= 1;
    ++shift;
  }
  return remaining == 1 ? shift : -1;
}

/// The rewrite `reduceOperation` produces for one operation, or nullptr.
[[nodiscard]] ValuePtr reduceOperation(const Operation& operation) {
  const std::string& opcode = operation.opcode();
  const IRType& type = operation.type();
  if (bitWidth(type) == 0 || operation.operands().size() != 2) {
    return nullptr;
  }
  const ValuePtr left = operation.operands()[0];
  const ValuePtr right = operation.operands()[1];
  const ConstantInt* leftInt = intValue(left);
  const ConstantInt* rightInt = intValue(right);
  const auto zero = [&type]() { return std::make_shared<ConstantInt>(0, type); };
  const auto shiftBy = [&operation,
                        &type](const ValuePtr& value, int shift, const char* opcodeName) {
    return std::make_shared<Operation>(
        opcodeName,
        type,
        operation.resultName(),
        std::vector<ValuePtr>{value, std::make_shared<ConstantInt>(shift, type)},
        std::unordered_map<std::string, std::string>{});
  };

  if (opcode == "add") {
    if (rightInt != nullptr && rightInt->value() == 0) {
      return left;
    }
    if (leftInt != nullptr && leftInt->value() == 0) {
      return right;
    }
  } else if (opcode == "sub") {
    if (rightInt != nullptr && rightInt->value() == 0) {
      return left;
    }
    if (leftInt != nullptr && rightInt != nullptr && leftInt->value() == rightInt->value()) {
      return zero();
    }
  } else if (opcode == "mul") {
    if (rightInt != nullptr && rightInt->value() == 1) {
      return left;
    }
    if (leftInt != nullptr && leftInt->value() == 1) {
      return right;
    }
    if ((rightInt != nullptr && rightInt->value() == 0) ||
        (leftInt != nullptr && leftInt->value() == 0)) {
      return zero();
    }
    // Multiplying by a power of two is a shift.
    if (rightInt != nullptr && leftInt == nullptr) {
      const int shift = powerOfTwoShift(rightInt->value());
      if (shift > 0) {
        return shiftBy(left, shift, "shl");
      }
    }
  } else if (opcode == "and") {
    if (rightInt != nullptr && rightInt->value() == -1) {
      return left;
    }
    if (leftInt != nullptr && leftInt->value() == -1) {
      return right;
    }
    if ((rightInt != nullptr && rightInt->value() == 0) ||
        (leftInt != nullptr && leftInt->value() == 0)) {
      return zero();
    }
  } else if (opcode == "or" || opcode == "xor") {
    if (rightInt != nullptr && rightInt->value() == 0) {
      return left;
    }
    if (leftInt != nullptr && leftInt->value() == 0) {
      return right;
    }
    if (opcode == "xor" && leftInt != nullptr && rightInt != nullptr &&
        leftInt->value() == rightInt->value()) {
      return zero();
    }
  } else if (opcode == "shl" || opcode == "shr") {
    if (rightInt != nullptr && rightInt->value() == 0) {
      return left;
    }
  } else if ((opcode == "div" || opcode == "rem") && isUnsignedKind(type)) {
    // Unsigned division by a power of two is a shift; the remainder is a mask.
    if (rightInt != nullptr && leftInt == nullptr) {
      const int shift = powerOfTwoShift(rightInt->value());
      if (shift > 0) {
        if (opcode == "div") {
          return shiftBy(left, shift, "shr");
        }
        return std::make_shared<Operation>(
            "and",
            type,
            operation.resultName(),
            std::vector<ValuePtr>{left, std::make_shared<ConstantInt>(rightInt->value() - 1, type)},
            std::unordered_map<std::string, std::string>{});
      }
    }
  }
  return nullptr;
}

/// True for the arithmetic opcodes whose repeated computation is redundant.
[[nodiscard]] bool isPureOpcode(std::string_view opcode) {
  return opcode == "add" || opcode == "sub" || opcode == "mul" || opcode == "div" ||
         opcode == "rem" || opcode == "and" || opcode == "or" || opcode == "xor" ||
         opcode == "shl" || opcode == "shr" || opcode == "select" || opcode == "invert" ||
         opcode == "neg" || opcode == "not" || opcode == "pointer.is_null" ||
         opcode.starts_with("cmp.") || opcode.starts_with("cast.") ||
         opcode.starts_with("aggregate.");
}

class StrengthReducePass final : public TransformPass {
public:
  [[nodiscard]] std::string_view name() const override { return "strength-reduce"; }
  bool run(IRModule& module) override {
    std::unordered_map<const Value*, ValuePtr> replacements;
    for (const std::unique_ptr<IRFunction>& function : module.functions()) {
      if (function->isExternal()) {
        continue;
      }
      for (const std::unique_ptr<BasicBlock>& block : function->blocks()) {
        for (const std::shared_ptr<Operation>& operation : block->operations()) {
          if (operation == nullptr) {
            continue;
          }
          if (ValuePtr reduced = reduceOperation(*operation)) {
            replacements.insert_or_assign(operation.get(), std::move(reduced));
          }
        }
      }
    }
    if (replacements.empty()) {
      return false;
    }
    return applyReplacements(module, replacements);
  }
};

class CommonSubexpressionPass final : public TransformPass {
public:
  [[nodiscard]] std::string_view name() const override { return "cse"; }
  bool run(IRModule& module) override {
    std::unordered_map<const Value*, ValuePtr> replacements;
    for (const std::unique_ptr<IRFunction>& function : module.functions()) {
      if (function->isExternal()) {
        continue;
      }
      for (const std::unique_ptr<BasicBlock>& block : function->blocks()) {
        // Visibility never crosses a block boundary, so the table is per block.
        std::unordered_map<std::string, ValuePtr> seen;
        for (const std::shared_ptr<Operation>& operation : block->operations()) {
          if (operation == nullptr || !isPureOpcode(operation->opcode())) {
            continue;
          }
          std::string key = operation->opcode();
          key += '|';
          key += operation->type().display();
          for (const ValuePtr& operand : operation->operands()) {
            key += '|';
            key += operand == nullptr ? std::string{"(null)"} : operand->reference();
          }
          for (const auto& [name, value] : operation->attributes()) {
            key += '|';
            key += name;
            key += '=';
            key += value;
          }
          const auto [found, inserted] = seen.try_emplace(std::move(key), operation);
          if (!inserted) {
            replacements.insert_or_assign(operation.get(), found->second);
          }
        }
      }
    }
    if (replacements.empty()) {
      return false;
    }
    return applyReplacements(module, replacements);
  }
};

/// True for the pending-error operations a release build does not need.
[[nodiscard]] bool isErrorBookkeeping(std::string_view opcode) {
  return opcode == "error.enter" || opcode == "error.leave" || opcode == "error.bind";
}

class RuntimeCheckStripPass final : public TransformPass {
public:
  [[nodiscard]] std::string_view name() const override { return "runtime-checks"; }
  bool run(IRModule& module) override {
    std::unordered_map<const Value*, ValuePtr> replacements;
    std::unordered_set<const Value*> dropped;
    for (const std::unique_ptr<IRFunction>& function : module.functions()) {
      if (function->isExternal()) {
        continue;
      }
      for (const std::unique_ptr<BasicBlock>& block : function->blocks()) {
        for (const std::shared_ptr<Operation>& operation : block->operations()) {
          if (operation == nullptr) {
            continue;
          }
          const std::string& opcode = operation->opcode();
          if (isErrorBookkeeping(opcode)) {
            dropped.insert(operation.get());
            continue;
          }
          if (opcode == "error.isa") {
            // Nothing raised, so no handler matches.
            replacements.insert_or_assign(operation.get(), boolConstant(false));
          }
        }
      }
    }
    if (replacements.empty() && dropped.empty()) {
      return false;
    }
    bool changed = applyReplacements(module, replacements);
    for (const std::unique_ptr<IRFunction>& function : module.functions()) {
      if (function->isExternal()) {
        continue;
      }
      for (const std::unique_ptr<BasicBlock>& block : function->blocks()) {
        std::vector<std::shared_ptr<Operation>> kept;
        kept.reserve(block->operations().size());
        for (const std::shared_ptr<Operation>& operation : block->operations()) {
          if (operation == nullptr || dropped.contains(operation.get())) {
            changed = true;
            continue;
          }
          kept.push_back(operation);
        }
        if (kept.size() != block->operations().size()) {
          block->setOperations(std::move(kept));
        }
      }
    }
    return changed;
  }
};

} // namespace

std::unique_ptr<TransformPass> makeConstantFoldPass() {
  return std::make_unique<ConstantFoldPass>();
}

std::unique_ptr<TransformPass> makeDeadCodePass() {
  return std::make_unique<DeadCodePass>();
}

std::unique_ptr<TransformPass> makeUnreachableBlockPass() {
  return std::make_unique<UnreachableBlockPass>();
}

std::unique_ptr<TransformPass> makeUnusedGlobalPass() {
  return std::make_unique<UnusedGlobalPass>();
}

std::unique_ptr<TransformPass> makeStrengthReducePass() {
  return std::make_unique<StrengthReducePass>();
}

std::unique_ptr<TransformPass> makeCommonSubexpressionPass() {
  return std::make_unique<CommonSubexpressionPass>();
}

std::unique_ptr<TransformPass> makeRuntimeCheckStripPass() {
  return std::make_unique<RuntimeCheckStripPass>();
}

std::vector<std::unique_ptr<TransformPass>> defaultTransformPasses() {
  std::vector<std::unique_ptr<TransformPass>> passes;
  passes.push_back(makeDeadCodePass());
  passes.push_back(makeConstantFoldPass());
  passes.push_back(makeUnreachableBlockPass());
  passes.push_back(makeUnusedGlobalPass());
  return passes;
}

std::vector<std::unique_ptr<TransformPass>> transformPasses(const OptimizationOptions& options) {
  std::vector<std::unique_ptr<TransformPass>> passes;
  // The error machinery goes first: dropping it makes the handler blocks
  // unreachable, and every later pass then skips them.
  if (!options.runtimeChecks) {
    passes.push_back(makeRuntimeCheckStripPass());
  }
  passes.push_back(makeDeadCodePass());
  passes.push_back(makeConstantFoldPass());
  if (options.peephole || options.strengthReduce) {
    passes.push_back(makeStrengthReducePass());
  }
  if (options.cse) {
    passes.push_back(makeCommonSubexpressionPass());
    passes.push_back(makeConstantFoldPass());
  }
  passes.push_back(makeUnreachableBlockPass());
  passes.push_back(makeDeadCodePass());
  passes.push_back(makeUnusedGlobalPass());
  return passes;
}

int runOptimizingTransformers(IRModule& module, const OptimizationOptions& options) {
  std::vector<std::unique_ptr<TransformPass>> passes = transformPasses(options);
  int applied = 0;
  for (int round = 0; round < kMaxRounds; ++round) {
    bool changed = false;
    for (const std::unique_ptr<TransformPass>& pass : passes) {
      if (pass->run(module)) {
        changed = true;
        ++applied;
      }
    }
    if (!changed) {
      break;
    }
  }
  return applied;
}

int runTransformers(IRModule& module) {
  std::vector<std::unique_ptr<TransformPass>> passes = defaultTransformPasses();
  int applied = 0;
  for (int round = 0; round < kMaxRounds; ++round) {
    bool changed = false;
    for (const std::unique_ptr<TransformPass>& pass : passes) {
      if (pass->run(module)) {
        changed = true;
        ++applied;
      }
    }
    if (!changed) {
      break;
    }
  }
  return applied;
}

} // namespace sere::serem
