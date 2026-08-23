/// @file Query.cpp
/// Walks the typed AST to find the innermost node covering an offset.

#include "sere/ast/Query.h"

#include <string>
#include <string_view>
#include <vector>

namespace sere {
namespace {

[[nodiscard]] const Node* firstNotNull(const Node* left, const Node* right) {
  return left != nullptr ? left : right;
}

[[nodiscard]] const Node* searchExpr(const Expr& expr, std::uint32_t offset);
[[nodiscard]] const Node* searchStmt(const Stmt& stmt, std::uint32_t offset);

const Node* searchExpr(const Expr& expr, std::uint32_t offset) {
  const Node* inner = nullptr;
  switch (expr.kind()) {
  case NodeKind::CallExpr: {
    const auto& call = static_cast<const CallExpr&>(expr);
    inner = searchExpr(call.callee(), offset);
    for (const std::unique_ptr<Expr>& arg : call.arguments()) {
      inner = firstNotNull(inner, searchExpr(*arg, offset));
    }
    break;
  }
  case NodeKind::MemberExpr:
    inner = searchExpr(static_cast<const MemberExpr&>(expr).object(), offset);
    break;
  case NodeKind::BinaryExpr: {
    const auto& binary = static_cast<const BinaryExpr&>(expr);
    inner = firstNotNull(searchExpr(binary.left(), offset), searchExpr(binary.right(), offset));
    break;
  }
  case NodeKind::UnaryExpr:
    inner = searchExpr(static_cast<const UnaryExpr&>(expr).operand(), offset);
    break;
  case NodeKind::CastExpr: {
    const auto& cast = static_cast<const CastExpr&>(expr);
    if (rangeContains(cast.target().range(), offset)) {
      return &cast.target();
    }
    inner = searchExpr(cast.value(), offset);
    break;
  }
  case NodeKind::IndexExpr: {
    const auto& index = static_cast<const IndexExpr&>(expr);
    inner = searchExpr(index.object(), offset);
    if (index.start() != nullptr) {
      inner = firstNotNull(inner, searchExpr(*index.start(), offset));
    }
    if (index.stop() != nullptr) {
      inner = firstNotNull(inner, searchExpr(*index.stop(), offset));
    }
    break;
  }
  case NodeKind::ListLiteral:
    for (const std::unique_ptr<Expr>& item : static_cast<const ListLiteral&>(expr).elements()) {
      inner = firstNotNull(inner, searchExpr(*item, offset));
    }
    break;
  case NodeKind::DictLiteral: {
    const auto& dict = static_cast<const DictLiteral&>(expr);
    for (std::size_t index = 0; index < dict.keys().size(); ++index) {
      inner = firstNotNull(inner, searchExpr(*dict.keys()[index], offset));
      inner = firstNotNull(inner, searchExpr(*dict.values()[index], offset));
    }
    break;
  }
  case NodeKind::ComprehensionExpr: {
    const auto& comp = static_cast<const ComprehensionExpr&>(expr);
    inner = firstNotNull(searchExpr(comp.element(), offset), searchExpr(comp.iterable(), offset));
    break;
  }
  case NodeKind::InterpolatedStringExpr:
    for (const StringPart& part : static_cast<const InterpolatedStringExpr&>(expr).parts()) {
      if (part.value != nullptr) {
        inner = firstNotNull(inner, searchExpr(*part.value, offset));
      }
    }
    break;
  case NodeKind::TernaryExpr: {
    const auto& ternary = static_cast<const TernaryExpr&>(expr);
    inner = firstNotNull(searchExpr(ternary.thenValue(), offset),
                         searchExpr(ternary.condition(), offset));
    inner = firstNotNull(inner, searchExpr(ternary.elseValue(), offset));
    break;
  }
  case NodeKind::TupleExpr:
    for (const std::unique_ptr<Expr>& item : static_cast<const TupleExpr&>(expr).elements()) {
      inner = firstNotNull(inner, searchExpr(*item, offset));
    }
    break;
  default:
    break;
  }
  if (inner != nullptr) {
    return inner;
  }
  return rangeContains(expr.range(), offset) ? &expr : nullptr;
}

const Node* searchStmt(const Stmt& stmt, std::uint32_t offset) {
  const Node* inner = nullptr;
  switch (stmt.kind()) {
  case NodeKind::VarDecl: {
    const auto& decl = static_cast<const VarDecl&>(stmt);
    if (rangeContains(decl.type().range(), offset)) {
      return &decl.type();
    }
    if (decl.init() != nullptr) {
      inner = searchExpr(*decl.init(), offset);
    }
    break;
  }
  case NodeKind::AssignStmt: {
    const auto& assign = static_cast<const AssignStmt&>(stmt);
    inner = firstNotNull(searchExpr(assign.target(), offset), searchExpr(assign.value(), offset));
    break;
  }
  case NodeKind::ReturnStmt: {
    const auto& ret = static_cast<const ReturnStmt&>(stmt);
    if (ret.value() != nullptr) {
      inner = searchExpr(*ret.value(), offset);
    }
    break;
  }
  case NodeKind::ExprStmt:
    inner = searchExpr(static_cast<const ExprStmt&>(stmt).expression(), offset);
    break;
  case NodeKind::IfStmt:
    for (const IfBranch& branch : static_cast<const IfStmt&>(stmt).branches()) {
      if (branch.condition != nullptr) {
        inner = firstNotNull(inner, searchExpr(*branch.condition, offset));
      }
      for (const std::unique_ptr<Stmt>& bodyStmt : branch.body) {
        inner = firstNotNull(inner, searchStmt(*bodyStmt, offset));
      }
    }
    break;
  case NodeKind::WhileStmt: {
    const auto& loop = static_cast<const WhileStmt&>(stmt);
    inner = searchExpr(loop.condition(), offset);
    for (const std::unique_ptr<Stmt>& bodyStmt : loop.body()) {
      inner = firstNotNull(inner, searchStmt(*bodyStmt, offset));
    }
    break;
  }
  case NodeKind::ForStmt: {
    const auto& loop = static_cast<const ForStmt&>(stmt);
    inner = searchExpr(loop.iterable(), offset);
    for (const std::unique_ptr<Stmt>& bodyStmt : loop.body()) {
      inner = firstNotNull(inner, searchStmt(*bodyStmt, offset));
    }
    break;
  }
  case NodeKind::AssertStmt: {
    const auto& assertion = static_cast<const AssertStmt&>(stmt);
    inner = searchExpr(assertion.condition(), offset);
    if (assertion.message() != nullptr) {
      inner = firstNotNull(inner, searchExpr(*assertion.message(), offset));
    }
    break;
  }
  case NodeKind::RaiseStmt:
    if (static_cast<const RaiseStmt&>(stmt).value() != nullptr) {
      inner = searchExpr(*static_cast<const RaiseStmt&>(stmt).value(), offset);
    }
    break;
  case NodeKind::TryStmt: {
    const auto& tryStmt = static_cast<const TryStmt&>(stmt);
    for (const std::unique_ptr<Stmt>& bodyStmt : tryStmt.body()) {
      inner = firstNotNull(inner, searchStmt(*bodyStmt, offset));
    }
    for (const ExceptHandler& handler : tryStmt.handlers()) {
      if (handler.type != nullptr && rangeContains(handler.type->range(), offset)) {
        return handler.type.get();
      }
      for (const std::unique_ptr<Stmt>& bodyStmt : handler.body) {
        inner = firstNotNull(inner, searchStmt(*bodyStmt, offset));
      }
    }
    for (const std::unique_ptr<Stmt>& bodyStmt : tryStmt.elseBody()) {
      inner = firstNotNull(inner, searchStmt(*bodyStmt, offset));
    }
    for (const std::unique_ptr<Stmt>& bodyStmt : tryStmt.finallyBody()) {
      inner = firstNotNull(inner, searchStmt(*bodyStmt, offset));
    }
    break;
  }
  case NodeKind::MatchStmt: {
    const auto& match = static_cast<const MatchStmt&>(stmt);
    inner = searchExpr(match.subject(), offset);
    for (const MatchArm& arm : match.arms()) {
      if (arm.pattern != nullptr) {
        inner = firstNotNull(inner, searchExpr(*arm.pattern, offset));
      }
      if (arm.guard != nullptr) {
        inner = firstNotNull(inner, searchExpr(*arm.guard, offset));
      }
      for (const std::unique_ptr<Stmt>& bodyStmt : arm.body) {
        inner = firstNotNull(inner, searchStmt(*bodyStmt, offset));
      }
    }
    break;
  }
  case NodeKind::DelStmt:
    inner = searchExpr(static_cast<const DelStmt&>(stmt).target(), offset);
    break;
  case NodeKind::DeferStmt:
    for (const std::unique_ptr<Stmt>& bodyStmt : static_cast<const DeferStmt&>(stmt).body()) {
      inner = firstNotNull(inner, searchStmt(*bodyStmt, offset));
    }
    break;
  case NodeKind::EnumDef:
    for (const std::unique_ptr<FunctionDef>& method : static_cast<const EnumDef&>(stmt).methods()) {
      inner = firstNotNull(inner, searchStmt(*method, offset));
    }
    break;
  case NodeKind::FunctionDef: {
    const auto& function = static_cast<const FunctionDef&>(stmt);
    if (rangeContains(function.returnType().range(), offset)) {
      return &function.returnType();
    }
    for (const ParamDecl& param : function.params()) {
      if (param.type != nullptr && rangeContains(param.type->range(), offset)) {
        return param.type.get();
      }
    }
    for (const std::unique_ptr<Stmt>& bodyStmt : function.body()) {
      inner = firstNotNull(inner, searchStmt(*bodyStmt, offset));
    }
    break;
  }
  case NodeKind::ClassDef: {
    const auto& classDef = static_cast<const ClassDef&>(stmt);
    for (const FieldDecl& field : classDef.fields()) {
      if (field.type != nullptr && rangeContains(field.type->range(), offset)) {
        return field.type.get();
      }
      if (field.init != nullptr) {
        inner = firstNotNull(inner, searchExpr(*field.init, offset));
      }
    }
    for (const std::unique_ptr<FunctionDef>& method : classDef.methods()) {
      inner = firstNotNull(inner, searchStmt(*method, offset));
    }
    break;
  }
  case NodeKind::TypeAlias: {
    const auto& alias = static_cast<const TypeAlias&>(stmt);
    if (rangeContains(alias.type().range(), offset)) {
      return &alias.type();
    }
    break;
  }
  case NodeKind::MacroDef: {
    const auto& def = static_cast<const MacroDef&>(stmt);
    for (const std::unique_ptr<Stmt>& bodyStmt : def.quoteBody()) {
      inner = firstNotNull(inner, searchStmt(*bodyStmt, offset));
    }
    for (const MacroMatchArm& arm : def.matchArms()) {
      for (const std::unique_ptr<Stmt>& bodyStmt : arm.body) {
        inner = firstNotNull(inner, searchStmt(*bodyStmt, offset));
      }
    }
    break;
  }
  case NodeKind::MacroInvokeStmt:
    break;
  default:
    break;
  }
  if (inner != nullptr) {
    return inner;
  }
  return rangeContains(stmt.range(), offset) ? &stmt : nullptr;
}

}  // namespace

bool rangeContains(SourceRange range, std::uint32_t offset) {
  return offset >= range.start.offset && offset <= range.end.offset;
}

SourceRange identifierRange(SourceRange start, std::string_view name) {
  SourceRange range = start;
  range.end = start.start;
  range.end.offset = start.start.offset + static_cast<std::uint32_t>(name.size());
  range.end.column = start.start.column + static_cast<std::uint32_t>(name.size());
  return range;
}

const Node* findNodeAt(const Node& root, std::uint32_t offset) {
  if (root.kind() != NodeKind::Module) {
    return nullptr;
  }
  const Node* found = nullptr;
  for (const std::unique_ptr<Stmt>& statement : static_cast<const Module&>(root).statements()) {
    if (statement->fromPrelude()) {
      continue;
    }
    found = firstNotNull(found, searchStmt(*statement, offset));
  }
  return found;
}

const CallExpr* findCallAt(const Node& root, std::uint32_t offset) {
  std::vector<const CallExpr*> calls;
  collectCalls(root, calls);
  const CallExpr* best = nullptr;
  std::uint32_t bestSpan = ~0u;
  for (const CallExpr* call : calls) {
    if (!rangeContains(call->range(), offset)) {
      continue;
    }
    const std::uint32_t span = call->range().end.offset - call->range().start.offset;
    if (best == nullptr || span <= bestSpan) {
      best = call;
      bestSpan = span;
    }
  }
  return best;
}

namespace {

void collectExprCalls(const Expr& expr, std::vector<const CallExpr*>& out) {
  if (expr.kind() == NodeKind::CallExpr) {
    const auto& call = static_cast<const CallExpr&>(expr);
    out.push_back(&call);
    collectExprCalls(call.callee(), out);
    for (const std::unique_ptr<Expr>& arg : call.arguments()) {
      collectExprCalls(*arg, out);
    }
    return;
  }
  if (expr.kind() == NodeKind::MemberExpr) {
    collectExprCalls(static_cast<const MemberExpr&>(expr).object(), out);
    return;
  }
  if (expr.kind() == NodeKind::BinaryExpr) {
    const auto& binary = static_cast<const BinaryExpr&>(expr);
    collectExprCalls(binary.left(), out);
    collectExprCalls(binary.right(), out);
    return;
  }
  if (expr.kind() == NodeKind::UnaryExpr) {
    collectExprCalls(static_cast<const UnaryExpr&>(expr).operand(), out);
    return;
  }
  if (expr.kind() == NodeKind::IndexExpr) {
    const auto& index = static_cast<const IndexExpr&>(expr);
    collectExprCalls(index.object(), out);
    if (index.start() != nullptr) {
      collectExprCalls(*index.start(), out);
    }
    if (index.stop() != nullptr) {
      collectExprCalls(*index.stop(), out);
    }
    return;
  }
  if (expr.kind() == NodeKind::ListLiteral) {
    for (const std::unique_ptr<Expr>& item : static_cast<const ListLiteral&>(expr).elements()) {
      collectExprCalls(*item, out);
    }
    return;
  }
  if (expr.kind() == NodeKind::ComprehensionExpr) {
    const auto& comp = static_cast<const ComprehensionExpr&>(expr);
    collectExprCalls(comp.element(), out);
    collectExprCalls(comp.iterable(), out);
    return;
  }
  if (expr.kind() == NodeKind::DictLiteral) {
    const auto& dict = static_cast<const DictLiteral&>(expr);
    for (std::size_t index = 0; index < dict.keys().size(); ++index) {
      collectExprCalls(*dict.keys()[index], out);
      collectExprCalls(*dict.values()[index], out);
    }
    return;
  }
  if (expr.kind() == NodeKind::InterpolatedStringExpr) {
    for (const StringPart& part : static_cast<const InterpolatedStringExpr&>(expr).parts()) {
      if (part.value != nullptr) {
        collectExprCalls(*part.value, out);
      }
    }
  }
}

void collectStmtCalls(const Stmt& stmt, std::vector<const CallExpr*>& out) {
  switch (stmt.kind()) {
  case NodeKind::VarDecl:
    if (static_cast<const VarDecl&>(stmt).init() != nullptr) {
      collectExprCalls(*static_cast<const VarDecl&>(stmt).init(), out);
    }
    break;
  case NodeKind::AssignStmt:
    collectExprCalls(static_cast<const AssignStmt&>(stmt).target(), out);
    collectExprCalls(static_cast<const AssignStmt&>(stmt).value(), out);
    break;
  case NodeKind::ReturnStmt:
    if (static_cast<const ReturnStmt&>(stmt).value() != nullptr) {
      collectExprCalls(*static_cast<const ReturnStmt&>(stmt).value(), out);
    }
    break;
  case NodeKind::ExprStmt:
    collectExprCalls(static_cast<const ExprStmt&>(stmt).expression(), out);
    break;
  case NodeKind::IfStmt:
    for (const IfBranch& branch : static_cast<const IfStmt&>(stmt).branches()) {
      if (branch.condition != nullptr) {
        collectExprCalls(*branch.condition, out);
      }
      for (const std::unique_ptr<Stmt>& bodyStmt : branch.body) {
        collectStmtCalls(*bodyStmt, out);
      }
    }
    break;
  case NodeKind::WhileStmt:
    collectExprCalls(static_cast<const WhileStmt&>(stmt).condition(), out);
    for (const std::unique_ptr<Stmt>& bodyStmt : static_cast<const WhileStmt&>(stmt).body()) {
      collectStmtCalls(*bodyStmt, out);
    }
    break;
  case NodeKind::ForStmt:
    collectExprCalls(static_cast<const ForStmt&>(stmt).iterable(), out);
    for (const std::unique_ptr<Stmt>& bodyStmt : static_cast<const ForStmt&>(stmt).body()) {
      collectStmtCalls(*bodyStmt, out);
    }
    break;
  case NodeKind::AssertStmt:
    collectExprCalls(static_cast<const AssertStmt&>(stmt).condition(), out);
    if (static_cast<const AssertStmt&>(stmt).message() != nullptr) {
      collectExprCalls(*static_cast<const AssertStmt&>(stmt).message(), out);
    }
    break;
  case NodeKind::RaiseStmt:
    if (static_cast<const RaiseStmt&>(stmt).value() != nullptr) {
      collectExprCalls(*static_cast<const RaiseStmt&>(stmt).value(), out);
    }
    break;
  case NodeKind::TryStmt: {
    const auto& tryStmt = static_cast<const TryStmt&>(stmt);
    for (const std::unique_ptr<Stmt>& bodyStmt : tryStmt.body()) {
      collectStmtCalls(*bodyStmt, out);
    }
    for (const ExceptHandler& handler : tryStmt.handlers()) {
      for (const std::unique_ptr<Stmt>& bodyStmt : handler.body) {
        collectStmtCalls(*bodyStmt, out);
      }
    }
    break;
  }
  case NodeKind::MatchStmt:
    collectExprCalls(static_cast<const MatchStmt&>(stmt).subject(), out);
    for (const MatchArm& arm : static_cast<const MatchStmt&>(stmt).arms()) {
      if (arm.pattern != nullptr) {
        collectExprCalls(*arm.pattern, out);
      }
      if (arm.guard != nullptr) {
        collectExprCalls(*arm.guard, out);
      }
      for (const std::unique_ptr<Stmt>& bodyStmt : arm.body) {
        collectStmtCalls(*bodyStmt, out);
      }
    }
    break;
  case NodeKind::EnumDef:
    for (const std::unique_ptr<FunctionDef>& method : static_cast<const EnumDef&>(stmt).methods()) {
      collectStmtCalls(*method, out);
    }
    break;
  case NodeKind::FunctionDef:
    for (const std::unique_ptr<Stmt>& bodyStmt : static_cast<const FunctionDef&>(stmt).body()) {
      collectStmtCalls(*bodyStmt, out);
    }
    break;
  case NodeKind::ClassDef:
    for (const FieldDecl& field : static_cast<const ClassDef&>(stmt).fields()) {
      if (field.init != nullptr) {
        collectExprCalls(*field.init, out);
      }
    }
    for (const std::unique_ptr<FunctionDef>& method :
         static_cast<const ClassDef&>(stmt).methods()) {
      collectStmtCalls(*method, out);
    }
    break;
  default:
    break;
  }
}

}  // namespace

void collectNameRefsFromExpr(const Expr& expr, std::string_view name, std::vector<SourceRange>& out) {
  if (expr.kind() == NodeKind::NameExpr && static_cast<const NameExpr&>(expr).name() == name) {
    out.push_back(expr.range());
  }
  if (expr.kind() == NodeKind::MacroInvokeExpr) {
    const auto& invoke = static_cast<const MacroInvokeExpr&>(expr);
    if (invoke.name() == name) {
      out.push_back(identifierRange(invoke.range(), invoke.name()));
    }
    return;
  }
  if (expr.kind() == NodeKind::SpliceExpr && static_cast<const SpliceExpr&>(expr).name() == name) {
    out.push_back(expr.range());
  }
  if (expr.kind() == NodeKind::MemberExpr) {
    const auto& member = static_cast<const MemberExpr&>(expr);
    if (member.field() == name) {
      out.push_back(member.range());
    }
    collectNameRefsFromExpr(member.object(), name, out);
    return;
  }
  if (expr.kind() == NodeKind::CallExpr) {
    const auto& call = static_cast<const CallExpr&>(expr);
    collectNameRefsFromExpr(call.callee(), name, out);
    for (const std::unique_ptr<Expr>& arg : call.arguments()) {
      collectNameRefsFromExpr(*arg, name, out);
    }
    return;
  }
  if (expr.kind() == NodeKind::BinaryExpr) {
    collectNameRefsFromExpr(static_cast<const BinaryExpr&>(expr).left(), name, out);
    collectNameRefsFromExpr(static_cast<const BinaryExpr&>(expr).right(), name, out);
    return;
  }
  if (expr.kind() == NodeKind::UnaryExpr) {
    collectNameRefsFromExpr(static_cast<const UnaryExpr&>(expr).operand(), name, out);
    return;
  }
  if (expr.kind() == NodeKind::IndexExpr) {
    collectNameRefsFromExpr(static_cast<const IndexExpr&>(expr).object(), name, out);
    if (static_cast<const IndexExpr&>(expr).start() != nullptr) {
      collectNameRefsFromExpr(*static_cast<const IndexExpr&>(expr).start(), name, out);
    }
    if (static_cast<const IndexExpr&>(expr).stop() != nullptr) {
      collectNameRefsFromExpr(*static_cast<const IndexExpr&>(expr).stop(), name, out);
    }
    return;
  }
  if (expr.kind() == NodeKind::ListLiteral) {
    for (const std::unique_ptr<Expr>& item : static_cast<const ListLiteral&>(expr).elements()) {
      collectNameRefsFromExpr(*item, name, out);
    }
    return;
  }
  if (expr.kind() == NodeKind::ComprehensionExpr) {
    const auto& comp = static_cast<const ComprehensionExpr&>(expr);
    collectNameRefsFromExpr(comp.element(), name, out);
    collectNameRefsFromExpr(comp.iterable(), name, out);
    return;
  }
  if (expr.kind() == NodeKind::InterpolatedStringExpr) {
    for (const StringPart& part : static_cast<const InterpolatedStringExpr&>(expr).parts()) {
      if (part.value != nullptr) {
        collectNameRefsFromExpr(*part.value, name, out);
      }
    }
  }
  if (expr.kind() == NodeKind::TernaryExpr) {
    collectNameRefsFromExpr(static_cast<const TernaryExpr&>(expr).thenValue(), name, out);
    collectNameRefsFromExpr(static_cast<const TernaryExpr&>(expr).condition(), name, out);
    collectNameRefsFromExpr(static_cast<const TernaryExpr&>(expr).elseValue(), name, out);
  }
  if (expr.kind() == NodeKind::TupleExpr) {
    for (const std::unique_ptr<Expr>& item : static_cast<const TupleExpr&>(expr).elements()) {
      collectNameRefsFromExpr(*item, name, out);
    }
  }
}

void collectNameRefsFromStmt(const Stmt& stmt, std::string_view name, std::vector<SourceRange>& out) {
  switch (stmt.kind()) {
  case NodeKind::VarDecl:
    if (static_cast<const VarDecl&>(stmt).name() == name) {
      out.push_back(stmt.range());
    }
    if (static_cast<const VarDecl&>(stmt).init() != nullptr) {
      collectNameRefsFromExpr(*static_cast<const VarDecl&>(stmt).init(), name, out);
    }
    break;
  case NodeKind::AssignStmt:
    collectNameRefsFromExpr(static_cast<const AssignStmt&>(stmt).target(), name, out);
    collectNameRefsFromExpr(static_cast<const AssignStmt&>(stmt).value(), name, out);
    break;
  case NodeKind::ReturnStmt:
    if (static_cast<const ReturnStmt&>(stmt).value() != nullptr) {
      collectNameRefsFromExpr(*static_cast<const ReturnStmt&>(stmt).value(), name, out);
    }
    break;
  case NodeKind::ExprStmt:
    collectNameRefsFromExpr(static_cast<const ExprStmt&>(stmt).expression(), name, out);
    break;
  case NodeKind::IfStmt:
    for (const IfBranch& branch : static_cast<const IfStmt&>(stmt).branches()) {
      if (branch.condition != nullptr) {
        collectNameRefsFromExpr(*branch.condition, name, out);
      }
      for (const std::unique_ptr<Stmt>& bodyStmt : branch.body) {
        collectNameRefsFromStmt(*bodyStmt, name, out);
      }
    }
    break;
  case NodeKind::WhileStmt:
    collectNameRefsFromExpr(static_cast<const WhileStmt&>(stmt).condition(), name, out);
    for (const std::unique_ptr<Stmt>& bodyStmt : static_cast<const WhileStmt&>(stmt).body()) {
      collectNameRefsFromStmt(*bodyStmt, name, out);
    }
    break;
  case NodeKind::ForStmt:
    if (static_cast<const ForStmt&>(stmt).name() == name) {
      out.push_back(stmt.range());
    }
    collectNameRefsFromExpr(static_cast<const ForStmt&>(stmt).iterable(), name, out);
    for (const std::unique_ptr<Stmt>& bodyStmt : static_cast<const ForStmt&>(stmt).body()) {
      collectNameRefsFromStmt(*bodyStmt, name, out);
    }
    break;
  case NodeKind::AssertStmt:
    collectNameRefsFromExpr(static_cast<const AssertStmt&>(stmt).condition(), name, out);
    if (static_cast<const AssertStmt&>(stmt).message() != nullptr) {
      collectNameRefsFromExpr(*static_cast<const AssertStmt&>(stmt).message(), name, out);
    }
    break;
  case NodeKind::FunctionDef: {
    const auto& function = static_cast<const FunctionDef&>(stmt);
    if (function.name() == name) {
      out.push_back(function.range());
    }
    for (const std::unique_ptr<Stmt>& bodyStmt : function.body()) {
      collectNameRefsFromStmt(*bodyStmt, name, out);
    }
    break;
  }
  case NodeKind::ClassDef: {
    const auto& classDef = static_cast<const ClassDef&>(stmt);
    if (classDef.name() == name) {
      out.push_back(classDef.range());
    }
    for (const FieldDecl& field : classDef.fields()) {
      if (field.name == name) {
        out.push_back(field.range);
      }
    }
    for (const std::unique_ptr<FunctionDef>& method : classDef.methods()) {
      collectNameRefsFromStmt(*method, name, out);
    }
    break;
  }
  case NodeKind::EnumDef: {
    const auto& enumDef = static_cast<const EnumDef&>(stmt);
    if (enumDef.name() == name) {
      out.push_back(enumDef.range());
    }
    for (const EnumVariant& variant : enumDef.variants()) {
      if (variant.name == name) {
        out.push_back(variant.range);
      }
    }
    break;
  }
  case NodeKind::MacroDef: {
    const auto& def = static_cast<const MacroDef&>(stmt);
    if (def.name() == name) {
      out.push_back(macroNameRange(def));
    }
    for (const std::unique_ptr<Stmt>& bodyStmt : def.quoteBody()) {
      collectNameRefsFromStmt(*bodyStmt, name, out);
    }
    for (const MacroMatchArm& arm : def.matchArms()) {
      for (const std::unique_ptr<Stmt>& bodyStmt : arm.body) {
        collectNameRefsFromStmt(*bodyStmt, name, out);
      }
    }
    break;
  }
  case NodeKind::MacroInvokeStmt: {
    const auto& invoke = static_cast<const MacroInvokeStmt&>(stmt);
    if (invoke.name() == name) {
      out.push_back(identifierRange(invoke.range(), invoke.name()));
    }
    break;
  }
  case NodeKind::MatchStmt: {
    const auto& match = static_cast<const MatchStmt&>(stmt);
    collectNameRefsFromExpr(match.subject(), name, out);
    for (const MatchArm& arm : match.arms()) {
      if (arm.pattern != nullptr) {
        collectNameRefsFromExpr(*arm.pattern, name, out);
      }
      if (arm.guard != nullptr) {
        collectNameRefsFromExpr(*arm.guard, name, out);
      }
      for (const std::unique_ptr<Stmt>& bodyStmt : arm.body) {
        collectNameRefsFromStmt(*bodyStmt, name, out);
      }
    }
    break;
  }
  default:
    break;
  }
}

void collectNameRefs(const Node& root, std::string_view name, std::vector<SourceRange>& out) {
  if (root.kind() != NodeKind::Module) {
    return;
  }
  for (const std::unique_ptr<Stmt>& statement : static_cast<const Module&>(root).statements()) {
    if (statement->fromPrelude()) {
      continue;
    }
    collectNameRefsFromStmt(*statement, name, out);
  }
}

void collectCalls(const Node& root, std::vector<const CallExpr*>& out) {
  if (root.kind() != NodeKind::Module) {
    return;
  }
  for (const std::unique_ptr<Stmt>& statement : static_cast<const Module&>(root).statements()) {
    if (statement->fromPrelude()) {
      continue;
    }
    collectStmtCalls(*statement, out);
  }
}

namespace {

void collectMacroUsesFromExpr(const Expr& expr, std::vector<MacroUse>& out);
void collectMacroUsesFromStmt(const Stmt& stmt, std::vector<MacroUse>& out);

void pushMacroUse(const std::string& name, SourceRange range, MacroDelimiter delimiter,
                  std::vector<MacroUse>& out) {
  MacroUse use;
  use.name = name;
  use.range = range;
  use.nameRange = identifierRange(range, name);
  use.delimiter = delimiter;
  out.push_back(std::move(use));
}

void collectMacroUsesFromExpr(const Expr& expr, std::vector<MacroUse>& out) {
  if (expr.kind() == NodeKind::MacroInvokeExpr) {
    const auto& invoke = static_cast<const MacroInvokeExpr&>(expr);
    pushMacroUse(invoke.name(), invoke.range(), invoke.delimiter(), out);
    return;
  }
  switch (expr.kind()) {
  case NodeKind::CallExpr: {
    const auto& call = static_cast<const CallExpr&>(expr);
    collectMacroUsesFromExpr(call.callee(), out);
    for (const std::unique_ptr<Expr>& arg : call.arguments()) {
      collectMacroUsesFromExpr(*arg, out);
    }
    break;
  }
  case NodeKind::MemberExpr:
    collectMacroUsesFromExpr(static_cast<const MemberExpr&>(expr).object(), out);
    break;
  case NodeKind::BinaryExpr:
    collectMacroUsesFromExpr(static_cast<const BinaryExpr&>(expr).left(), out);
    collectMacroUsesFromExpr(static_cast<const BinaryExpr&>(expr).right(), out);
    break;
  case NodeKind::UnaryExpr:
    collectMacroUsesFromExpr(static_cast<const UnaryExpr&>(expr).operand(), out);
    break;
  case NodeKind::CastExpr:
    collectMacroUsesFromExpr(static_cast<const CastExpr&>(expr).value(), out);
    break;
  case NodeKind::IndexExpr: {
    const auto& index = static_cast<const IndexExpr&>(expr);
    collectMacroUsesFromExpr(index.object(), out);
    if (index.start() != nullptr) {
      collectMacroUsesFromExpr(*index.start(), out);
    }
    if (index.stop() != nullptr) {
      collectMacroUsesFromExpr(*index.stop(), out);
    }
    break;
  }
  case NodeKind::ListLiteral:
    for (const std::unique_ptr<Expr>& item : static_cast<const ListLiteral&>(expr).elements()) {
      collectMacroUsesFromExpr(*item, out);
    }
    break;
  case NodeKind::DictLiteral: {
    const auto& dict = static_cast<const DictLiteral&>(expr);
    for (std::size_t index = 0; index < dict.keys().size(); ++index) {
      collectMacroUsesFromExpr(*dict.keys()[index], out);
      collectMacroUsesFromExpr(*dict.values()[index], out);
    }
    break;
  }
  case NodeKind::ComprehensionExpr:
    collectMacroUsesFromExpr(static_cast<const ComprehensionExpr&>(expr).element(), out);
    collectMacroUsesFromExpr(static_cast<const ComprehensionExpr&>(expr).iterable(), out);
    break;
  case NodeKind::InterpolatedStringExpr:
    for (const StringPart& part : static_cast<const InterpolatedStringExpr&>(expr).parts()) {
      if (part.value != nullptr) {
        collectMacroUsesFromExpr(*part.value, out);
      }
    }
    break;
  case NodeKind::TernaryExpr: {
    const auto& ternary = static_cast<const TernaryExpr&>(expr);
    collectMacroUsesFromExpr(ternary.thenValue(), out);
    collectMacroUsesFromExpr(ternary.condition(), out);
    collectMacroUsesFromExpr(ternary.elseValue(), out);
    break;
  }
  case NodeKind::TupleExpr:
    for (const std::unique_ptr<Expr>& item : static_cast<const TupleExpr&>(expr).elements()) {
      collectMacroUsesFromExpr(*item, out);
    }
    break;
  default:
    break;
  }
}

void collectMacroUsesFromStmt(const Stmt& stmt, std::vector<MacroUse>& out) {
  if (stmt.kind() == NodeKind::MacroInvokeStmt) {
    const auto& invoke = static_cast<const MacroInvokeStmt&>(stmt);
    pushMacroUse(invoke.name(), invoke.range(), invoke.delimiter(), out);
    return;
  }
  switch (stmt.kind()) {
  case NodeKind::VarDecl:
    if (static_cast<const VarDecl&>(stmt).init() != nullptr) {
      collectMacroUsesFromExpr(*static_cast<const VarDecl&>(stmt).init(), out);
    }
    break;
  case NodeKind::AssignStmt:
    collectMacroUsesFromExpr(static_cast<const AssignStmt&>(stmt).target(), out);
    collectMacroUsesFromExpr(static_cast<const AssignStmt&>(stmt).value(), out);
    break;
  case NodeKind::ReturnStmt:
    if (static_cast<const ReturnStmt&>(stmt).value() != nullptr) {
      collectMacroUsesFromExpr(*static_cast<const ReturnStmt&>(stmt).value(), out);
    }
    break;
  case NodeKind::ExprStmt:
    collectMacroUsesFromExpr(static_cast<const ExprStmt&>(stmt).expression(), out);
    break;
  case NodeKind::IfStmt:
    for (const IfBranch& branch : static_cast<const IfStmt&>(stmt).branches()) {
      if (branch.condition != nullptr) {
        collectMacroUsesFromExpr(*branch.condition, out);
      }
      for (const std::unique_ptr<Stmt>& bodyStmt : branch.body) {
        collectMacroUsesFromStmt(*bodyStmt, out);
      }
    }
    break;
  case NodeKind::WhileStmt:
    collectMacroUsesFromExpr(static_cast<const WhileStmt&>(stmt).condition(), out);
    for (const std::unique_ptr<Stmt>& bodyStmt : static_cast<const WhileStmt&>(stmt).body()) {
      collectMacroUsesFromStmt(*bodyStmt, out);
    }
    break;
  case NodeKind::ForStmt:
    collectMacroUsesFromExpr(static_cast<const ForStmt&>(stmt).iterable(), out);
    for (const std::unique_ptr<Stmt>& bodyStmt : static_cast<const ForStmt&>(stmt).body()) {
      collectMacroUsesFromStmt(*bodyStmt, out);
    }
    break;
  case NodeKind::AssertStmt:
    collectMacroUsesFromExpr(static_cast<const AssertStmt&>(stmt).condition(), out);
    if (static_cast<const AssertStmt&>(stmt).message() != nullptr) {
      collectMacroUsesFromExpr(*static_cast<const AssertStmt&>(stmt).message(), out);
    }
    break;
  case NodeKind::RaiseStmt:
    if (static_cast<const RaiseStmt&>(stmt).value() != nullptr) {
      collectMacroUsesFromExpr(*static_cast<const RaiseStmt&>(stmt).value(), out);
    }
    break;
  case NodeKind::TryStmt: {
    const auto& tryStmt = static_cast<const TryStmt&>(stmt);
    for (const std::unique_ptr<Stmt>& bodyStmt : tryStmt.body()) {
      collectMacroUsesFromStmt(*bodyStmt, out);
    }
    for (const ExceptHandler& handler : tryStmt.handlers()) {
      for (const std::unique_ptr<Stmt>& bodyStmt : handler.body) {
        collectMacroUsesFromStmt(*bodyStmt, out);
      }
    }
    break;
  }
  case NodeKind::MatchStmt:
    collectMacroUsesFromExpr(static_cast<const MatchStmt&>(stmt).subject(), out);
    for (const MatchArm& arm : static_cast<const MatchStmt&>(stmt).arms()) {
      if (arm.pattern != nullptr) {
        collectMacroUsesFromExpr(*arm.pattern, out);
      }
      if (arm.guard != nullptr) {
        collectMacroUsesFromExpr(*arm.guard, out);
      }
      for (const std::unique_ptr<Stmt>& bodyStmt : arm.body) {
        collectMacroUsesFromStmt(*bodyStmt, out);
      }
    }
    break;
  case NodeKind::FunctionDef:
    for (const std::unique_ptr<Stmt>& bodyStmt : static_cast<const FunctionDef&>(stmt).body()) {
      collectMacroUsesFromStmt(*bodyStmt, out);
    }
    break;
  case NodeKind::ClassDef:
    for (const std::unique_ptr<FunctionDef>& method : static_cast<const ClassDef&>(stmt).methods()) {
      collectMacroUsesFromStmt(*method, out);
    }
    break;
  case NodeKind::DeferStmt:
    for (const std::unique_ptr<Stmt>& bodyStmt : static_cast<const DeferStmt&>(stmt).body()) {
      collectMacroUsesFromStmt(*bodyStmt, out);
    }
    break;
  default:
    break;
  }
}

}  // namespace

const MacroUse* findMacroUseAt(const std::vector<MacroUse>& uses, std::uint32_t offset) {
  const MacroUse* best = nullptr;
  std::uint32_t bestSpan = ~0u;
  for (const MacroUse& use : uses) {
    if (!rangeContains(use.nameRange, offset) && !rangeContains(use.range, offset)) {
      continue;
    }
    const std::uint32_t span = use.range.end.offset - use.range.start.offset;
    if (best == nullptr || span <= bestSpan) {
      best = &use;
      bestSpan = span;
    }
  }
  return best;
}

const MacroUse* findMacroNameAt(const std::vector<MacroUse>& uses, std::uint32_t offset) {
  const MacroUse* best = nullptr;
  std::uint32_t bestSpan = ~0u;
  for (const MacroUse& use : uses) {
    if (!rangeContains(use.nameRange, offset)) {
      continue;
    }
    const std::uint32_t span = use.nameRange.end.offset - use.nameRange.start.offset;
    if (best == nullptr || span <= bestSpan) {
      best = &use;
      bestSpan = span;
    }
  }
  return best;
}

void collectMacroUses(const Node& root, std::vector<MacroUse>& out) {
  if (root.kind() != NodeKind::Module) {
    return;
  }
  for (const std::unique_ptr<Stmt>& statement : static_cast<const Module&>(root).statements()) {
    if (statement->fromPrelude()) {
      continue;
    }
    collectMacroUsesFromStmt(*statement, out);
  }
}

std::string formatMacro(const MacroDef& def) {
  std::string text = "macro " + def.name();
  if (!def.params().empty()) {
    text += "(";
    for (std::size_t index = 0; index < def.params().size(); ++index) {
      if (index != 0) {
        text += ", ";
      }
      if (def.variadic() && index + 1 == def.params().size()) {
        text += "*";
      }
      text += def.params()[index];
    }
    text += ")";
  } else if (def.syntaxMode() == MacroSyntaxMode::Sere && def.matchArms().empty()) {
    text += "()";
  }
  if (def.syntaxMode() == MacroSyntaxMode::Raw) {
    text += "\nsyntax: raw";
  } else if (def.syntaxMode() == MacroSyntaxMode::Tokens) {
    text += "\nsyntax: tokens";
  } else if (def.syntaxMode() == MacroSyntaxMode::Pipeline) {
    text += "\nsyntax: pipeline";
  }
  if (def.interpolate() == MacroInterpolate::Brace) {
    text += "\ninterpolate: brace";
  } else if (def.interpolate() == MacroInterpolate::Dollar) {
    text += "\ninterpolate: dollar";
  }
  if (def.typed()) {
    text += "\ntyped: true";
  }
  if (!def.wrapper().empty()) {
    text += "\nwrapper: " + def.wrapper();
  }
  if (!def.matchArms().empty()) {
    text += "\nmatch: " + std::to_string(def.matchArms().size()) +
            (def.matchArms().size() == 1 ? " arm" : " arms");
  }
  return text;
}

std::string macroSnippet(const MacroDef& def) {
  if (def.syntaxMode() == MacroSyntaxMode::Raw || def.syntaxMode() == MacroSyntaxMode::Pipeline) {
    return def.name() + ":\n    $0";
  }
  if (def.params().empty() && !def.matchArms().empty()) {
    return def.name() + "!($0)";
  }
  if (def.params().empty()) {
    return def.name() + "!()";
  }
  std::string text = def.name() + "!(";
  for (std::size_t index = 0; index < def.params().size(); ++index) {
    if (index != 0) {
      text += ", ";
    }
    text += "${" + std::to_string(index + 1) + ":" + def.params()[index] + "}";
  }
  text += ")";
  return text;
}

SourceRange macroNameRange(const MacroDef& def) {
  if (def.nameRange().end.offset > def.nameRange().start.offset) {
    return def.nameRange();
  }
  SourceRange afterKeyword = def.range();
  constexpr std::uint32_t kMacroKeywordSize = 5;
  afterKeyword.start.offset += kMacroKeywordSize + 1;
  afterKeyword.start.column += kMacroKeywordSize + 1;
  return identifierRange(afterKeyword, def.name());
}

}  // namespace sere
