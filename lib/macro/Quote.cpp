/// @file Quote.cpp
/// Clones AST nodes and substitutes macro splices.

#include "sere/macro/Quote.h"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace sere {
namespace {

[[nodiscard]] std::vector<std::unique_ptr<Expr>>
cloneExprs(const std::vector<std::unique_ptr<Expr>>& exprs) {
  std::vector<std::unique_ptr<Expr>> out;
  out.reserve(exprs.size());
  for (const std::unique_ptr<Expr>& expr : exprs) {
    out.push_back(cloneExpr(*expr));
  }
  return out;
}

[[nodiscard]] std::vector<std::unique_ptr<Stmt>>
cloneStmts(const std::vector<std::unique_ptr<Stmt>>& stmts) {
  std::vector<std::unique_ptr<Stmt>> out;
  out.reserve(stmts.size());
  for (const std::unique_ptr<Stmt>& stmt : stmts) {
    out.push_back(cloneStmt(*stmt));
  }
  return out;
}

void collectDeclNames(const Stmt& stmt, std::unordered_set<std::string>& names);
void collectDeclNames(const Expr& expr, std::unordered_set<std::string>& names);
void rewriteNames(Stmt& stmt, const std::unordered_map<std::string, std::string>& map);
void rewriteNames(Expr& expr, const std::unordered_map<std::string, std::string>& map);

void collectDeclNames(const Expr& expr, std::unordered_set<std::string>& names) {
  switch (expr.kind()) {
  case NodeKind::CallExpr:
    collectDeclNames(static_cast<const CallExpr&>(expr).callee(), names);
    for (const std::unique_ptr<Expr>& arg : static_cast<const CallExpr&>(expr).arguments()) {
      collectDeclNames(*arg, names);
    }
    break;
  case NodeKind::MemberExpr:
    collectDeclNames(static_cast<const MemberExpr&>(expr).object(), names);
    break;
  case NodeKind::BinaryExpr:
    collectDeclNames(static_cast<const BinaryExpr&>(expr).left(), names);
    collectDeclNames(static_cast<const BinaryExpr&>(expr).right(), names);
    break;
  case NodeKind::UnaryExpr:
    collectDeclNames(static_cast<const UnaryExpr&>(expr).operand(), names);
    break;
  case NodeKind::CastExpr:
    collectDeclNames(static_cast<const CastExpr&>(expr).value(), names);
    break;
  case NodeKind::ListLiteral:
    for (const std::unique_ptr<Expr>& item : static_cast<const ListLiteral&>(expr).elements()) {
      collectDeclNames(*item, names);
    }
    break;
  case NodeKind::TernaryExpr: {
    const auto& ternary = static_cast<const TernaryExpr&>(expr);
    collectDeclNames(ternary.thenValue(), names);
    collectDeclNames(ternary.condition(), names);
    collectDeclNames(ternary.elseValue(), names);
    break;
  }
  case NodeKind::ComprehensionExpr:
    names.insert(static_cast<const ComprehensionExpr&>(expr).name());
    collectDeclNames(static_cast<const ComprehensionExpr&>(expr).element(), names);
    collectDeclNames(static_cast<const ComprehensionExpr&>(expr).iterable(), names);
    break;
  case NodeKind::InterpolatedStringExpr:
    for (const StringPart& part : static_cast<const InterpolatedStringExpr&>(expr).parts()) {
      if (part.value != nullptr) {
        collectDeclNames(*part.value, names);
      }
    }
    break;
  default:
    break;
  }
}

void collectDeclNames(const Stmt& stmt, std::unordered_set<std::string>& names) {
  switch (stmt.kind()) {
  case NodeKind::VarDecl:
    names.insert(static_cast<const VarDecl&>(stmt).name());
    if (static_cast<const VarDecl&>(stmt).init() != nullptr) {
      collectDeclNames(*static_cast<const VarDecl&>(stmt).init(), names);
    }
    break;
  case NodeKind::AssignStmt:
    collectDeclNames(static_cast<const AssignStmt&>(stmt).target(), names);
    collectDeclNames(static_cast<const AssignStmt&>(stmt).value(), names);
    break;
  case NodeKind::ReturnStmt:
    if (static_cast<const ReturnStmt&>(stmt).value() != nullptr) {
      collectDeclNames(*static_cast<const ReturnStmt&>(stmt).value(), names);
    }
    break;
  case NodeKind::ExprStmt:
    collectDeclNames(static_cast<const ExprStmt&>(stmt).expression(), names);
    break;
  case NodeKind::IfStmt:
    for (const IfBranch& branch : static_cast<const IfStmt&>(stmt).branches()) {
      if (branch.condition != nullptr) {
        collectDeclNames(*branch.condition, names);
      }
      for (const std::unique_ptr<Stmt>& body : branch.body) {
        collectDeclNames(*body, names);
      }
    }
    break;
  case NodeKind::WhileStmt:
    collectDeclNames(static_cast<const WhileStmt&>(stmt).condition(), names);
    for (const std::unique_ptr<Stmt>& body : static_cast<const WhileStmt&>(stmt).body()) {
      collectDeclNames(*body, names);
    }
    break;
  case NodeKind::ForStmt:
    names.insert(static_cast<const ForStmt&>(stmt).name());
    collectDeclNames(static_cast<const ForStmt&>(stmt).iterable(), names);
    for (const std::unique_ptr<Stmt>& body : static_cast<const ForStmt&>(stmt).body()) {
      collectDeclNames(*body, names);
    }
    break;
  case NodeKind::MatchStmt: {
    const auto& match = static_cast<const MatchStmt&>(stmt);
    collectDeclNames(match.subject(), names);
    for (const MatchArm& arm : match.arms()) {
      if (arm.pattern != nullptr) {
        collectDeclNames(*arm.pattern, names);
      }
      if (arm.guard != nullptr) {
        collectDeclNames(*arm.guard, names);
      }
      for (const std::unique_ptr<Stmt>& body : arm.body) {
        collectDeclNames(*body, names);
      }
    }
    break;
  }
  default:
    break;
  }
}

void rewriteNames(Expr& expr, const std::unordered_map<std::string, std::string>& map) {
  switch (expr.kind()) {
  case NodeKind::NameExpr: {
    auto& name = static_cast<NameExpr&>(expr);
    const auto found = map.find(name.name());
    if (found != map.end()) {
      name.setName(found->second);
    }
    break;
  }
  case NodeKind::CallExpr:
    rewriteNames(static_cast<CallExpr&>(expr).callee(), map);
    for (const std::unique_ptr<Expr>& arg : static_cast<CallExpr&>(expr).arguments()) {
      rewriteNames(*arg, map);
    }
    break;
  case NodeKind::MemberExpr:
    rewriteNames(static_cast<MemberExpr&>(expr).object(), map);
    break;
  case NodeKind::BinaryExpr:
    rewriteNames(static_cast<BinaryExpr&>(expr).left(), map);
    rewriteNames(static_cast<BinaryExpr&>(expr).right(), map);
    break;
  case NodeKind::UnaryExpr:
    rewriteNames(static_cast<UnaryExpr&>(expr).operand(), map);
    break;
  case NodeKind::CastExpr:
    rewriteNames(static_cast<CastExpr&>(expr).value(), map);
    break;
  case NodeKind::ListLiteral:
    for (std::unique_ptr<Expr>& item : static_cast<ListLiteral&>(expr).elements()) {
      rewriteNames(*item, map);
    }
    break;
  case NodeKind::TernaryExpr: {
    auto& ternary = static_cast<TernaryExpr&>(expr);
    rewriteNames(ternary.thenValue(), map);
    rewriteNames(ternary.condition(), map);
    rewriteNames(ternary.elseValue(), map);
    break;
  }
  case NodeKind::ComprehensionExpr:
    rewriteNames(static_cast<ComprehensionExpr&>(expr).element(), map);
    rewriteNames(static_cast<ComprehensionExpr&>(expr).iterable(), map);
    break;
  case NodeKind::InterpolatedStringExpr:
    for (StringPart& part : static_cast<InterpolatedStringExpr&>(expr).parts()) {
      if (part.value != nullptr) {
        rewriteNames(*part.value, map);
      }
    }
    break;
  default:
    break;
  }
}

void rewriteNames(Stmt& stmt, const std::unordered_map<std::string, std::string>& map) {
  switch (stmt.kind()) {
  case NodeKind::VarDecl: {
    auto& decl = static_cast<VarDecl&>(stmt);
    const auto found = map.find(decl.name());
    if (found != map.end()) {
      decl.setName(found->second);
    }
    if (decl.init() != nullptr) {
      rewriteNames(const_cast<Expr&>(*decl.init()), map);
    }
    break;
  }
  case NodeKind::AssignStmt:
    rewriteNames(const_cast<Expr&>(static_cast<AssignStmt&>(stmt).target()), map);
    rewriteNames(const_cast<Expr&>(static_cast<AssignStmt&>(stmt).value()), map);
    break;
  case NodeKind::ReturnStmt:
    if (static_cast<const ReturnStmt&>(stmt).value() != nullptr) {
      rewriteNames(const_cast<Expr&>(*static_cast<const ReturnStmt&>(stmt).value()), map);
    }
    break;
  case NodeKind::ExprStmt:
    rewriteNames(const_cast<Expr&>(static_cast<ExprStmt&>(stmt).expression()), map);
    break;
  case NodeKind::IfStmt:
    for (IfBranch& branch : static_cast<IfStmt&>(stmt).branches()) {
      if (branch.condition != nullptr) {
        rewriteNames(*branch.condition, map);
      }
      for (std::unique_ptr<Stmt>& body : branch.body) {
        rewriteNames(*body, map);
      }
    }
    break;
  case NodeKind::WhileStmt:
    rewriteNames(static_cast<WhileStmt&>(stmt).condition(), map);
    for (std::unique_ptr<Stmt>& body : static_cast<WhileStmt&>(stmt).body()) {
      rewriteNames(*body, map);
    }
    break;
  case NodeKind::ForStmt:
    rewriteNames(static_cast<ForStmt&>(stmt).iterable(), map);
    for (std::unique_ptr<Stmt>& body : static_cast<ForStmt&>(stmt).body()) {
      rewriteNames(*body, map);
    }
    break;
  case NodeKind::MatchStmt: {
    auto& match = static_cast<MatchStmt&>(stmt);
    rewriteNames(match.subject(), map);
    for (MatchArm& arm : match.arms()) {
      if (arm.pattern != nullptr) {
        rewriteNames(*arm.pattern, map);
      }
      if (arm.guard != nullptr) {
        rewriteNames(*arm.guard, map);
      }
      for (std::unique_ptr<Stmt>& body : arm.body) {
        rewriteNames(*body, map);
      }
    }
    break;
  }
  default:
    break;
  }
}

[[nodiscard]] std::unique_ptr<Expr>
substOne(const Expr& tmpl, const MacroBindings& env, SourceRange callSite);

std::vector<std::unique_ptr<Expr>> substExprList(const std::vector<std::unique_ptr<Expr>>& exprs,
                                                 const MacroBindings& env,
                                                 SourceRange callSite) {
  std::vector<std::unique_ptr<Expr>> out;
  for (const std::unique_ptr<Expr>& expr : exprs) {
    if (expr->kind() == NodeKind::SpliceExpr) {
      const auto& splice = static_cast<const SpliceExpr&>(*expr);
      const auto found = env.exprs.find(splice.name());
      if (found != env.exprs.end() && (splice.isRepeat() || found->second.size() != 1)) {
        for (const std::unique_ptr<Expr>& bound : found->second) {
          out.push_back(cloneExpr(*bound));
        }
        continue;
      }
    }
    std::unique_ptr<Expr> replaced = substOne(*expr, env, callSite);
    if (replaced != nullptr) {
      out.push_back(std::move(replaced));
    }
  }
  return out;
}

std::unique_ptr<Expr> substOne(const Expr& tmpl, const MacroBindings& env, SourceRange callSite) {
  if (tmpl.kind() == NodeKind::SpliceExpr) {
    const auto& splice = static_cast<const SpliceExpr&>(tmpl);
    if (splice.name() == "input" && env.input != nullptr) {
      return cloneExpr(*env.input);
    }
    const auto found = env.exprs.find(splice.name());
    if (found == env.exprs.end() || found->second.empty()) {
      return cloneExpr(tmpl);
    }
    return cloneExpr(*found->second.front());
  }
  switch (tmpl.kind()) {
  case NodeKind::IntegerLiteral:
  case NodeKind::FloatLiteral:
  case NodeKind::StringLiteral:
  case NodeKind::BooleanLiteral:
  case NodeKind::NoneLiteral:
  case NodeKind::NameExpr:
  case NodeKind::MacroInvokeExpr:
    return cloneExpr(tmpl);
  case NodeKind::CallExpr: {
    const auto& call = static_cast<const CallExpr&>(tmpl);
    auto out = std::make_unique<CallExpr>(callSite,
                                          substOne(call.callee(), env, callSite),
                                          std::vector<std::unique_ptr<TypeExpr>>{},
                                          substExprList(call.arguments(), env, callSite));
    return out;
  }
  case NodeKind::MemberExpr: {
    const auto& member = static_cast<const MemberExpr&>(tmpl);
    return std::make_unique<MemberExpr>(
        callSite, substOne(member.object(), env, callSite), member.field());
  }
  case NodeKind::BinaryExpr: {
    const auto& binary = static_cast<const BinaryExpr&>(tmpl);
    return std::make_unique<BinaryExpr>(callSite,
                                        binary.op(),
                                        substOne(binary.left(), env, callSite),
                                        substOne(binary.right(), env, callSite));
  }
  case NodeKind::UnaryExpr: {
    const auto& unary = static_cast<const UnaryExpr&>(tmpl);
    return std::make_unique<UnaryExpr>(
        callSite, unary.op(), substOne(unary.operand(), env, callSite));
  }
  case NodeKind::CastExpr: {
    const auto& cast = static_cast<const CastExpr&>(tmpl);
    return std::make_unique<CastExpr>(
        callSite, substOne(cast.value(), env, callSite), cloneTypeExpr(cast.target()));
  }
  case NodeKind::IndexExpr: {
    const auto& index = static_cast<const IndexExpr&>(tmpl);
    std::unique_ptr<Expr> start =
        index.start() == nullptr ? nullptr : substOne(*index.start(), env, callSite);
    std::unique_ptr<Expr> stop =
        index.stop() == nullptr ? nullptr : substOne(*index.stop(), env, callSite);
    return std::make_unique<IndexExpr>(callSite,
                                       substOne(index.object(), env, callSite),
                                       std::move(start),
                                       std::move(stop),
                                       index.isSlice());
  }
  case NodeKind::ListLiteral:
    return std::make_unique<ListLiteral>(
        callSite, substExprList(static_cast<const ListLiteral&>(tmpl).elements(), env, callSite));
  case NodeKind::TupleExpr:
    return std::make_unique<TupleExpr>(
        callSite, substExprList(static_cast<const TupleExpr&>(tmpl).elements(), env, callSite));
  case NodeKind::TernaryExpr: {
    const auto& ternary = static_cast<const TernaryExpr&>(tmpl);
    return std::make_unique<TernaryExpr>(callSite,
                                         substOne(ternary.thenValue(), env, callSite),
                                         substOne(ternary.condition(), env, callSite),
                                         substOne(ternary.elseValue(), env, callSite));
  }
  case NodeKind::InterpolatedStringExpr: {
    const auto& interpolated = static_cast<const InterpolatedStringExpr&>(tmpl);
    std::vector<StringPart> parts;
    for (const StringPart& part : interpolated.parts()) {
      StringPart copy;
      copy.literal = part.literal;
      if (part.value != nullptr) {
        copy.value = substOne(*part.value, env, callSite);
      }
      parts.push_back(std::move(copy));
    }
    return std::make_unique<InterpolatedStringExpr>(callSite, std::move(parts));
  }
  default:
    return cloneExpr(tmpl);
  }
}

} // namespace

std::unique_ptr<TypeExpr> cloneTypeExpr(const TypeExpr& expr) {
  std::vector<std::unique_ptr<TypeExpr>> args;
  for (const std::unique_ptr<TypeExpr>& arg : expr.args()) {
    if (arg == nullptr) {
      args.push_back(nullptr);
      continue;
    }
    args.push_back(cloneTypeExpr(*arg));
  }
  return std::make_unique<TypeExpr>(expr.range(), expr.name(), std::move(args));
}

std::unique_ptr<Expr> cloneExpr(const Expr& expr) {
  switch (expr.kind()) {
  case NodeKind::IntegerLiteral:
    return std::make_unique<IntegerLiteral>(expr.range(),
                                            static_cast<const IntegerLiteral&>(expr).value());
  case NodeKind::FloatLiteral: {
    const auto& literal = static_cast<const FloatLiteral&>(expr);
    return std::make_unique<FloatLiteral>(expr.range(), literal.value(), literal.isF32());
  }
  case NodeKind::StringLiteral: {
    const auto& literal = static_cast<const StringLiteral&>(expr);
    return std::make_unique<StringLiteral>(expr.range(), literal.value(), literal.isRegex());
  }
  case NodeKind::BooleanLiteral:
    return std::make_unique<BooleanLiteral>(expr.range(),
                                            static_cast<const BooleanLiteral&>(expr).value());
  case NodeKind::NoneLiteral:
    return std::make_unique<NoneLiteral>(expr.range());
  case NodeKind::NameExpr:
    return std::make_unique<NameExpr>(expr.range(), static_cast<const NameExpr&>(expr).name());
  case NodeKind::SpliceExpr: {
    const auto& splice = static_cast<const SpliceExpr&>(expr);
    return std::make_unique<SpliceExpr>(
        expr.range(), splice.name(), splice.isRepeat(), splice.commaSeparated());
  }
  case NodeKind::CallExpr: {
    const auto& call = static_cast<const CallExpr&>(expr);
    std::vector<std::unique_ptr<TypeExpr>> typeArgs;
    for (const std::unique_ptr<TypeExpr>& arg : call.typeArgs()) {
      typeArgs.push_back(cloneTypeExpr(*arg));
    }
    std::vector<NamedArgument> keywordArgs;
    for (const NamedArgument& kw : call.keywordArguments()) {
      NamedArgument cloned;
      cloned.name = kw.name;
      cloned.value = cloneExpr(*kw.value);
      keywordArgs.push_back(std::move(cloned));
    }
    return std::make_unique<CallExpr>(expr.range(),
                                      cloneExpr(call.callee()),
                                      std::move(typeArgs),
                                      cloneExprs(call.arguments()),
                                      std::move(keywordArgs));
  }
  case NodeKind::MemberExpr: {
    const auto& member = static_cast<const MemberExpr&>(expr);
    return std::make_unique<MemberExpr>(expr.range(), cloneExpr(member.object()), member.field());
  }
  case NodeKind::BinaryExpr: {
    const auto& binary = static_cast<const BinaryExpr&>(expr);
    return std::make_unique<BinaryExpr>(
        expr.range(), binary.op(), cloneExpr(binary.left()), cloneExpr(binary.right()));
  }
  case NodeKind::UnaryExpr: {
    const auto& unary = static_cast<const UnaryExpr&>(expr);
    return std::make_unique<UnaryExpr>(expr.range(), unary.op(), cloneExpr(unary.operand()));
  }
  case NodeKind::AwaitExpr: {
    const auto& awaitExpr = static_cast<const AwaitExpr&>(expr);
    return std::make_unique<AwaitExpr>(expr.range(), cloneExpr(awaitExpr.operand()));
  }
  case NodeKind::CastExpr: {
    const auto& cast = static_cast<const CastExpr&>(expr);
    return std::make_unique<CastExpr>(
        expr.range(), cloneExpr(cast.value()), cloneTypeExpr(cast.target()));
  }
  case NodeKind::IndexExpr: {
    const auto& index = static_cast<const IndexExpr&>(expr);
    std::unique_ptr<Expr> start = index.start() == nullptr ? nullptr : cloneExpr(*index.start());
    std::unique_ptr<Expr> stop = index.stop() == nullptr ? nullptr : cloneExpr(*index.stop());
    return std::make_unique<IndexExpr>(expr.range(),
                                       cloneExpr(index.object()),
                                       std::move(start),
                                       std::move(stop),
                                       index.isSlice());
  }
  case NodeKind::ListLiteral:
    return std::make_unique<ListLiteral>(
        expr.range(), cloneExprs(static_cast<const ListLiteral&>(expr).elements()));
  case NodeKind::DictLiteral: {
    const auto& dict = static_cast<const DictLiteral&>(expr);
    return std::make_unique<DictLiteral>(
        expr.range(), cloneExprs(dict.keys()), cloneExprs(dict.values()));
  }
  case NodeKind::ComprehensionExpr: {
    const auto& comp = static_cast<const ComprehensionExpr&>(expr);
    return std::make_unique<ComprehensionExpr>(
        expr.range(), cloneExpr(comp.element()), comp.name(), cloneExpr(comp.iterable()));
  }
  case NodeKind::TernaryExpr: {
    const auto& ternary = static_cast<const TernaryExpr&>(expr);
    return std::make_unique<TernaryExpr>(expr.range(),
                                         cloneExpr(ternary.thenValue()),
                                         cloneExpr(ternary.condition()),
                                         cloneExpr(ternary.elseValue()));
  }
  case NodeKind::TupleExpr:
    return std::make_unique<TupleExpr>(expr.range(),
                                       cloneExprs(static_cast<const TupleExpr&>(expr).elements()));
  case NodeKind::WalrusExpr: {
    const auto& walrus = static_cast<const WalrusExpr&>(expr);
    return std::make_unique<WalrusExpr>(expr.range(), walrus.name(), cloneExpr(walrus.value()));
  }
  case NodeKind::LambdaExpr: {
    const auto& lambda = static_cast<const LambdaExpr&>(expr);
    std::vector<ParamDecl> params;
    for (const ParamDecl& param : lambda.params()) {
      ParamDecl copy;
      copy.name = param.name;
      copy.range = param.range;
      if (param.type != nullptr) {
        copy.type = cloneTypeExpr(*param.type);
      }
      if (param.defaultValue != nullptr) {
        copy.defaultValue = cloneExpr(*param.defaultValue);
      }
      params.push_back(std::move(copy));
    }
    std::unique_ptr<TypeExpr> ret =
        lambda.returnType() == nullptr ? nullptr : cloneTypeExpr(*lambda.returnType());
    auto copy = std::make_unique<LambdaExpr>(
        expr.range(), std::move(params), cloneExpr(lambda.body()), std::move(ret));
    copy->setLlvmName(lambda.llvmName());
    return copy;
  }
  case NodeKind::InterpolatedStringExpr: {
    const auto& interpolated = static_cast<const InterpolatedStringExpr&>(expr);
    std::vector<StringPart> parts;
    for (const StringPart& part : interpolated.parts()) {
      StringPart copy;
      copy.literal = part.literal;
      if (part.value != nullptr) {
        copy.value = cloneExpr(*part.value);
      }
      parts.push_back(std::move(copy));
    }
    return std::make_unique<InterpolatedStringExpr>(expr.range(), std::move(parts));
  }
  case NodeKind::MacroInvokeExpr: {
    const auto& invoke = static_cast<const MacroInvokeExpr&>(expr);
    return std::make_unique<MacroInvokeExpr>(expr.range(),
                                             invoke.name(),
                                             invoke.delimiter(),
                                             invoke.rawText(),
                                             invoke.rawRange(),
                                             invoke.tokens());
  }
  default:
    return std::make_unique<NameExpr>(expr.range(), "<uncloned>");
  }
}

std::unique_ptr<Stmt> cloneStmt(const Stmt& stmt) {
  switch (stmt.kind()) {
  case NodeKind::ExprStmt:
    return std::make_unique<ExprStmt>(stmt.range(),
                                      cloneExpr(static_cast<const ExprStmt&>(stmt).expression()));
  case NodeKind::VarDecl: {
    const auto& decl = static_cast<const VarDecl&>(stmt);
    std::unique_ptr<Expr> init = decl.init() == nullptr ? nullptr : cloneExpr(*decl.init());
    std::unique_ptr<TypeExpr> type = decl.hasType() ? cloneTypeExpr(decl.type()) : nullptr;
    return std::make_unique<VarDecl>(stmt.range(),
                                     decl.name(),
                                     std::move(type),
                                     std::move(init),
                                     decl.isStatic(),
                                     decl.isConst());
  }
  case NodeKind::AssignStmt: {
    const auto& assign = static_cast<const AssignStmt&>(stmt);
    auto copy = std::make_unique<AssignStmt>(
        stmt.range(), cloneExpr(assign.target()), cloneExpr(assign.value()), assign.op());
    copy->setNameAlias(assign.isNameAlias());
    return copy;
  }
  case NodeKind::ReturnStmt: {
    const auto& ret = static_cast<const ReturnStmt&>(stmt);
    std::unique_ptr<Expr> value = ret.value() == nullptr ? nullptr : cloneExpr(*ret.value());
    return std::make_unique<ReturnStmt>(stmt.range(), std::move(value));
  }
  case NodeKind::PassStmt:
    return std::make_unique<PassStmt>(stmt.range());
  case NodeKind::BreakStmt:
    return std::make_unique<BreakStmt>(stmt.range());
  case NodeKind::ContinueStmt:
    return std::make_unique<ContinueStmt>(stmt.range());
  case NodeKind::IfStmt: {
    std::vector<IfBranch> branches;
    for (const IfBranch& branch : static_cast<const IfStmt&>(stmt).branches()) {
      IfBranch copy;
      copy.range = branch.range;
      copy.condition = branch.condition == nullptr ? nullptr : cloneExpr(*branch.condition);
      copy.body = cloneStmts(branch.body);
      branches.push_back(std::move(copy));
    }
    return std::make_unique<IfStmt>(stmt.range(), std::move(branches));
  }
  case NodeKind::WhileStmt: {
    const auto& loop = static_cast<const WhileStmt&>(stmt);
    return std::make_unique<WhileStmt>(
        stmt.range(), cloneExpr(loop.condition()), cloneStmts(loop.body()));
  }
  case NodeKind::ForStmt: {
    const auto& loop = static_cast<const ForStmt&>(stmt);
    return std::make_unique<ForStmt>(
        stmt.range(), loop.name(), cloneExpr(loop.iterable()), cloneStmts(loop.body()));
  }
  case NodeKind::AssertStmt: {
    const auto& assertion = static_cast<const AssertStmt&>(stmt);
    std::unique_ptr<Expr> message =
        assertion.message() == nullptr ? nullptr : cloneExpr(*assertion.message());
    return std::make_unique<AssertStmt>(
        stmt.range(), cloneExpr(assertion.condition()), std::move(message));
  }
  case NodeKind::MatchStmt: {
    const auto& match = static_cast<const MatchStmt&>(stmt);
    std::vector<MatchArm> arms;
    for (const MatchArm& arm : match.arms()) {
      MatchArm copy;
      copy.range = arm.range;
      copy.pattern = arm.pattern == nullptr ? nullptr : cloneExpr(*arm.pattern);
      copy.guard = arm.guard == nullptr ? nullptr : cloneExpr(*arm.guard);
      copy.body = cloneStmts(arm.body);
      arms.push_back(std::move(copy));
    }
    return std::make_unique<MatchStmt>(stmt.range(), cloneExpr(match.subject()), std::move(arms));
  }
  case NodeKind::MacroInvokeStmt: {
    const auto& invoke = static_cast<const MacroInvokeStmt&>(stmt);
    return std::make_unique<MacroInvokeStmt>(stmt.range(),
                                             invoke.name(),
                                             invoke.delimiter(),
                                             invoke.rawText(),
                                             invoke.rawRange(),
                                             invoke.tokens());
  }
  case NodeKind::DelStmt:
    return std::make_unique<DelStmt>(stmt.range(),
                                     cloneExpr(static_cast<const DelStmt&>(stmt).target()));
  case NodeKind::DeferStmt:
    return std::make_unique<DeferStmt>(stmt.range(),
                                       cloneStmts(static_cast<const DeferStmt&>(stmt).body()));
  case NodeKind::WithStmt: {
    const auto& withStmt = static_cast<const WithStmt&>(stmt);
    return std::make_unique<WithStmt>(
        stmt.range(), cloneExpr(withStmt.context()), withStmt.name(), cloneStmts(withStmt.body()));
  }
  default:
    return std::make_unique<PassStmt>(stmt.range());
  }
}

std::unique_ptr<Expr>
substExpr(const Expr& tmpl, const MacroBindings& env, std::uint32_t mark, SourceRange callSite) {
  (void)mark;
  return substOne(tmpl, env, callSite);
}

std::vector<std::unique_ptr<Stmt>> substStmts(const std::vector<std::unique_ptr<Stmt>>& body,
                                              const MacroBindings& env,
                                              std::uint32_t mark,
                                              SourceRange callSite) {
  std::vector<std::unique_ptr<Stmt>> out;
  for (const std::unique_ptr<Stmt>& stmt : body) {
    std::unique_ptr<Stmt> cloned = cloneStmt(*stmt);
    applyHygiene(*cloned, mark);
    if (cloned->kind() == NodeKind::ExprStmt) {
      auto expr = substOne(static_cast<const ExprStmt&>(*cloned).expression(), env, callSite);
      out.push_back(std::make_unique<ExprStmt>(callSite, std::move(expr)));
      continue;
    }
    if (cloned->kind() == NodeKind::VarDecl) {
      const auto& decl = static_cast<const VarDecl&>(*cloned);
      std::unique_ptr<Expr> init =
          decl.init() == nullptr ? nullptr : substOne(*decl.init(), env, callSite);
      std::unique_ptr<TypeExpr> type = decl.hasType() ? cloneTypeExpr(decl.type()) : nullptr;
      out.push_back(std::make_unique<VarDecl>(callSite,
                                              decl.name(),
                                              std::move(type),
                                              std::move(init),
                                              decl.isStatic(),
                                              decl.isConst()));
      continue;
    }
    if (cloned->kind() == NodeKind::ReturnStmt) {
      const Expr* value = static_cast<const ReturnStmt&>(*cloned).value();
      std::unique_ptr<Expr> subst = value == nullptr ? nullptr : substOne(*value, env, callSite);
      out.push_back(std::make_unique<ReturnStmt>(callSite, std::move(subst)));
      continue;
    }
    if (cloned->kind() == NodeKind::MatchStmt) {
      auto& match = static_cast<MatchStmt&>(*cloned);
      match.setSubject(substOne(match.subject(), env, callSite));
      for (MatchArm& arm : match.arms()) {
        if (arm.pattern != nullptr) {
          arm.pattern = substOne(*arm.pattern, env, callSite);
        }
        if (arm.guard != nullptr) {
          arm.guard = substOne(*arm.guard, env, callSite);
        }
        arm.body = substStmts(arm.body, env, mark, callSite);
      }
      out.push_back(std::move(cloned));
      continue;
    }
    out.push_back(std::move(cloned));
  }
  return out;
}

void applyHygiene(Expr& expr, std::uint32_t mark) {
  std::unordered_set<std::string> names;
  collectDeclNames(expr, names);
  std::unordered_map<std::string, std::string> map;
  for (const std::string& name : names) {
    map[name] = "__m" + std::to_string(mark) + "_" + name;
  }
  rewriteNames(expr, map);
}

void applyHygiene(Stmt& stmt, std::uint32_t mark) {
  std::unordered_set<std::string> names;
  collectDeclNames(stmt, names);
  std::unordered_map<std::string, std::string> map;
  for (const std::string& name : names) {
    map[name] = "__m" + std::to_string(mark) + "_" + name;
  }
  if (stmt.kind() == NodeKind::VarDecl) {
    // VarDecl name is not rewritable via NameExpr; rebuild is handled by substStmts.
  }
  rewriteNames(stmt, map);
}

} // namespace sere
