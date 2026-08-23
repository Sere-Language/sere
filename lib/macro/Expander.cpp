/// @file Expander.cpp
/// Expands Sere macros: quote, match, raw interpolation, and pipeline syntax.

#include "sere/macro/Expander.h"

#include "sere/diag/DiagnosticEngine.h"
#include "sere/macro/Parse.h"
#include "sere/macro/Quote.h"

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <utility>

namespace sere {
namespace {

[[nodiscard]] std::string trimCopy(std::string_view text) {
  std::size_t start = 0;
  while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0) {
    ++start;
  }
  std::size_t end = text.size();
  while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
    --end;
  }
  return std::string(text.substr(start, end - start));
}

[[nodiscard]] std::vector<Token> innerTokens(const std::vector<Token>& tokens) {
  std::vector<Token> inner;
  for (const Token& token : tokens) {
    if (token.kind() == TokenKind::EndOfFile || token.kind() == TokenKind::Newline ||
        token.kind() == TokenKind::Indent || token.kind() == TokenKind::Dedent) {
      continue;
    }
    inner.push_back(token);
  }
  return inner;
}

[[nodiscard]] std::vector<Token> wrapDelimiter(const std::vector<Token>& inner,
                                               MacroDelimiter delimiter, SourceRange range) {
  TokenKind open = TokenKind::LParen;
  TokenKind close = TokenKind::RParen;
  if (delimiter == MacroDelimiter::BangBrace) {
    open = TokenKind::LBrace;
    close = TokenKind::RBrace;
  } else if (delimiter == MacroDelimiter::BangBracket) {
    open = TokenKind::LBracket;
    close = TokenKind::RBracket;
  }
  std::vector<Token> wrapped;
  wrapped.emplace_back(open, range, "");
  wrapped.insert(wrapped.end(), inner.begin(), inner.end());
  wrapped.emplace_back(close, range, "");
  return wrapped;
}

bool consumeExprTokens(const std::vector<Token>& tokens, std::size_t& index,
                       std::vector<Token>& captured) {
  int depth = 0;
  const std::size_t start = index;
  while (index < tokens.size()) {
    const TokenKind kind = tokens[index].kind();
    if (depth == 0 && (kind == TokenKind::Comma || kind == TokenKind::RParen ||
                       kind == TokenKind::RBracket || kind == TokenKind::RBrace ||
                       kind == TokenKind::FatArrow)) {
      break;
    }
    if (kind == TokenKind::LParen || kind == TokenKind::LBracket || kind == TokenKind::LBrace) {
      ++depth;
    } else if (kind == TokenKind::RParen || kind == TokenKind::RBracket ||
               kind == TokenKind::RBrace) {
      if (depth == 0) {
        break;
      }
      --depth;
    }
    captured.push_back(tokens[index]);
    ++index;
  }
  return index > start;
}

struct PatternAtom {
  enum class Kind { Literal, Meta, Repeat } kind = Kind::Literal;
  Token token{TokenKind::Unknown, SourceRange{}, {}};
  std::string metaName;
  std::string spec;
  std::vector<PatternAtom> inner;
  bool commaSeparated = false;
};

std::vector<PatternAtom> compilePattern(const std::vector<Token>& tokens, std::size_t& index,
                                        TokenKind stop) {
  std::vector<PatternAtom> atoms;
  while (index < tokens.size() && tokens[index].kind() != stop &&
         tokens[index].kind() != TokenKind::EndOfFile) {
    const Token& token = tokens[index];
    if (token.kind() == TokenKind::Dollar) {
      ++index;
      if (index < tokens.size() && tokens[index].kind() == TokenKind::LParen) {
        ++index;
        PatternAtom repeat;
        repeat.kind = PatternAtom::Kind::Repeat;
        repeat.inner = compilePattern(tokens, index, TokenKind::RParen);
        if (index < tokens.size() && tokens[index].kind() == TokenKind::RParen) {
          ++index;
        }
        if (index < tokens.size() && tokens[index].kind() == TokenKind::Comma) {
          repeat.commaSeparated = true;
          ++index;
        }
        if (index < tokens.size() && (tokens[index].kind() == TokenKind::Star ||
                                      tokens[index].kind() == TokenKind::Plus)) {
          ++index;
        }
        atoms.push_back(std::move(repeat));
        continue;
      }
      PatternAtom meta;
      meta.kind = PatternAtom::Kind::Meta;
      if (index < tokens.size() && tokens[index].kind() == TokenKind::Identifier) {
        meta.metaName = std::string(tokens[index].spelling());
        ++index;
      }
      meta.spec = "expr";
      if (index < tokens.size() && tokens[index].kind() == TokenKind::Colon) {
        ++index;
        if (index < tokens.size() && tokens[index].kind() == TokenKind::Identifier) {
          meta.spec = std::string(tokens[index].spelling());
          ++index;
        }
      }
      atoms.push_back(std::move(meta));
      continue;
    }
    PatternAtom literal;
    literal.kind = PatternAtom::Kind::Literal;
    literal.token = token;
    atoms.push_back(std::move(literal));
    ++index;
  }
  return atoms;
}

bool matchAtoms(const std::vector<PatternAtom>& atoms, const std::vector<Token>& tokens,
                std::size_t& index, MacroBindings& env, DiagnosticEngine& diagnostics);

bool matchMeta(const PatternAtom& meta, const std::vector<Token>& tokens, std::size_t& index,
               MacroBindings& env, DiagnosticEngine& diagnostics) {
  if (meta.spec == "ident") {
    if (index >= tokens.size() || tokens[index].kind() != TokenKind::Identifier) {
      return false;
    }
    auto name = std::make_unique<NameExpr>(tokens[index].range(),
                                           std::string(tokens[index].spelling()));
    env.exprs[meta.metaName].push_back(std::move(name));
    ++index;
    return true;
  }
  if (meta.spec == "literal") {
    if (index >= tokens.size()) {
      return false;
    }
    const Token& token = tokens[index];
    std::unique_ptr<Expr> expr;
    if (token.kind() == TokenKind::Integer) {
      const ParsedInteger parsed = parseIntegerToken(token.spelling());
      expr = std::make_unique<IntegerLiteral>(token.range(), parsed.value);
    } else if (token.kind() == TokenKind::Float) {
      std::string spelling(token.spelling());
      bool isF32 = false;
      if (!spelling.empty() && (spelling.back() == 'f' || spelling.back() == 'F')) {
        isF32 = true;
        spelling.pop_back();
      }
      expr = std::make_unique<FloatLiteral>(token.range(), std::strtod(spelling.c_str(), nullptr), isF32);
    } else if (token.kind() == TokenKind::String || token.kind() == TokenKind::Regex) {
      const DecodedString decoded = decodeStringToken(token.spelling());
      expr = std::make_unique<StringLiteral>(token.range(), decoded.value, decoded.regex);
    } else if (token.kind() == TokenKind::KeywordTrue || token.kind() == TokenKind::KeywordFalse) {
      expr = std::make_unique<BooleanLiteral>(token.range(),
                                              token.kind() == TokenKind::KeywordTrue);
    } else {
      return false;
    }
    env.exprs[meta.metaName].push_back(std::move(expr));
    ++index;
    return true;
  }
  std::vector<Token> captured;
  if (!consumeExprTokens(tokens, index, captured)) {
    return false;
  }
  std::unique_ptr<Expr> expr = parseSereExprFromTokens(diagnostics, captured);
  if (expr == nullptr) {
    return false;
  }
  env.exprs[meta.metaName].push_back(std::move(expr));
  return true;
}

bool matchAtoms(const std::vector<PatternAtom>& atoms, const std::vector<Token>& tokens,
                std::size_t& index, MacroBindings& env, DiagnosticEngine& diagnostics) {
  for (std::size_t atomIndex = 0; atomIndex < atoms.size(); ++atomIndex) {
    const PatternAtom& atom = atoms[atomIndex];
    if (atom.kind == PatternAtom::Kind::Literal) {
      if (index >= tokens.size() || tokens[index].kind() != atom.token.kind()) {
        return false;
      }
      if ((atom.token.kind() == TokenKind::Identifier || atom.token.kind() == TokenKind::Integer ||
           atom.token.kind() == TokenKind::String || atom.token.kind() == TokenKind::Regex) &&
          tokens[index].spelling() != atom.token.spelling()) {
        return false;
      }
      ++index;
      continue;
    }
    if (atom.kind == PatternAtom::Kind::Meta) {
      if (!matchMeta(atom, tokens, index, env, diagnostics)) {
        return false;
      }
      continue;
    }
    while (index < tokens.size()) {
      const std::size_t saved = index;
      MacroBindings inner;
      if (!matchAtoms(atom.inner, tokens, index, inner, diagnostics)) {
        index = saved;
        break;
      }
      for (auto& entry : inner.exprs) {
        for (std::unique_ptr<Expr>& expr : entry.second) {
          env.exprs[entry.first].push_back(std::move(expr));
        }
      }
      if (atom.commaSeparated) {
        if (index < tokens.size() && tokens[index].kind() == TokenKind::Comma) {
          ++index;
          continue;
        }
        break;
      }
    }
  }
  return true;
}

[[nodiscard]] std::unique_ptr<Expr> interpolateRaw(const std::string& text, SourceRange range,
                                                   MacroInterpolate mode,
                                                   DiagnosticEngine& diagnostics) {
  std::vector<StringPart> parts;
  std::string literal;
  std::size_t index = 0;
  const std::string_view view = text;
  while (index < view.size()) {
    if (mode == MacroInterpolate::Brace && view[index] == '{' && index + 1 < view.size() &&
        view[index + 1] == '{') {
      literal.push_back('{');
      index += 2;
      continue;
    }
    if (mode == MacroInterpolate::Brace && view[index] == '}' && index + 1 < view.size() &&
        view[index + 1] == '}') {
      literal.push_back('}');
      index += 2;
      continue;
    }
    if (mode == MacroInterpolate::Brace && view[index] == '{') {
      if (!literal.empty()) {
        StringPart part;
        part.literal = std::move(literal);
        parts.push_back(std::move(part));
        literal.clear();
      }
      ++index;
      const std::size_t start = index;
      int depth = 1;
      while (index < view.size() && depth > 0) {
        if (view[index] == '{') {
          ++depth;
        } else if (view[index] == '}') {
          --depth;
        }
        if (depth > 0) {
          ++index;
        }
      }
      std::unique_ptr<Expr> expr =
          parseSereExpr(diagnostics, view.substr(start, index - start), range.start);
      if (expr == nullptr) {
        return nullptr;
      }
      StringPart part;
      part.value = std::move(expr);
      parts.push_back(std::move(part));
      if (index < view.size()) {
        ++index;
      }
      continue;
    }
    if (mode == MacroInterpolate::Dollar && view[index] == '$' && index + 1 < view.size() &&
        (std::isalpha(static_cast<unsigned char>(view[index + 1])) != 0 ||
         view[index + 1] == '_')) {
      if (!literal.empty()) {
        StringPart part;
        part.literal = std::move(literal);
        parts.push_back(std::move(part));
        literal.clear();
      }
      ++index;
      const std::size_t start = index;
      while (index < view.size() &&
             (std::isalnum(static_cast<unsigned char>(view[index])) != 0 || view[index] == '_')) {
        ++index;
      }
      StringPart part;
      part.value = std::make_unique<NameExpr>(
          range, std::string(view.substr(start, index - start)));
      parts.push_back(std::move(part));
      continue;
    }
    literal.push_back(view[index]);
    ++index;
  }
  if (!literal.empty() || parts.empty()) {
    StringPart part;
    part.literal = std::move(literal);
    parts.push_back(std::move(part));
  }
  return std::make_unique<InterpolatedStringExpr>(range, std::move(parts));
}

[[nodiscard]] std::vector<std::string> splitPipeline(const std::string& text) {
  std::vector<std::string> parts;
  std::string current;
  for (std::size_t index = 0; index + 1 < text.size(); ++index) {
    if (text[index] == '|' && text[index + 1] == '>') {
      parts.push_back(trimCopy(current));
      current.clear();
      ++index;
      continue;
    }
    current.push_back(text[index]);
  }
  if (!text.empty()) {
    current.push_back(text.back());
  }
  const std::string last = trimCopy(current);
  if (!last.empty()) {
    parts.push_back(last);
  }
  return parts;
}

}  // namespace

bool MacroEnv::add(const MacroDef& def) {
  macros_[def.name()] = &def;
  return true;
}

const MacroDef* MacroEnv::find(const std::string& name) const {
  const auto found = macros_.find(name);
  if (found == macros_.end()) {
    return nullptr;
  }
  return found->second;
}

void MacroEnv::addModule(Module& module) {
  for (std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement->kind() == NodeKind::MacroDef) {
      add(static_cast<MacroDef&>(*statement));
    }
  }
}

MacroExpander::MacroExpander(DiagnosticEngine& diagnostics, MacroEnv& env, std::uint32_t fuel)
    : diagnostics_(&diagnostics), env_(&env), fuel_(fuel) {}

bool MacroExpander::expandModule(Module& module) {
  env_->addModule(module);
  return expandStmtList(module.statements());
}

bool MacroExpander::expandStmtList(std::vector<std::unique_ptr<Stmt>>& statements) {
  std::vector<std::unique_ptr<Stmt>> out;
  out.reserve(statements.size());
  for (std::unique_ptr<Stmt>& statement : statements) {
    if (statement == nullptr) {
      continue;
    }
    std::vector<std::unique_ptr<Stmt>> pieces;
    if (statement->kind() == NodeKind::MacroInvokeStmt) {
      pieces = expandInvokeStmt(static_cast<const MacroInvokeStmt&>(*statement));
      if (pieces.empty()) {
        out.push_back(std::move(statement));
        continue;
      }
    } else {
      pieces.push_back(std::move(statement));
    }
    for (std::unique_ptr<Stmt>& item : pieces) {
      if (item == nullptr) {
        continue;
      }
      if (item->kind() == NodeKind::ExprStmt &&
          static_cast<const ExprStmt&>(*item).expression().kind() == NodeKind::MacroInvokeExpr) {
        const auto& invoke =
            static_cast<const MacroInvokeExpr&>(static_cast<const ExprStmt&>(*item).expression());
        MacroInvokeStmt asStmt(invoke.range(), invoke.name(), invoke.delimiter(), invoke.rawText(),
                               invoke.rawRange(), invoke.tokens());
        std::vector<std::unique_ptr<Stmt>> expanded = expandInvokeStmt(asStmt);
        if (expanded.empty()) {
          out.push_back(std::move(item));
          continue;
        }
        for (std::unique_ptr<Stmt>& extra : expanded) {
          if (extra != nullptr && extra->kind() == NodeKind::ExprStmt) {
            std::unique_ptr<Expr> expr =
                expandExpr(static_cast<const ExprStmt&>(*extra).expression());
            if (expr != nullptr) {
              extra = std::make_unique<ExprStmt>(extra->range(), std::move(expr));
            }
          }
          if (extra != nullptr) {
            (void)expandInside(*extra);
            out.push_back(std::move(extra));
          }
        }
        continue;
      }
      if (item->kind() == NodeKind::ExprStmt) {
        std::unique_ptr<Expr> expr =
            expandExpr(static_cast<const ExprStmt&>(*item).expression());
        if (expr != nullptr) {
          item = std::make_unique<ExprStmt>(item->range(), std::move(expr));
        }
      } else if (item->kind() == NodeKind::VarDecl) {
        const auto& decl = static_cast<const VarDecl&>(*item);
        std::unique_ptr<Expr> init =
            decl.init() == nullptr ? nullptr : expandExpr(*decl.init());
        item = std::make_unique<VarDecl>(item->range(), decl.name(),
                                         decl.hasType() ? cloneTypeExpr(decl.type()) : nullptr,
                                         std::move(init), decl.isStatic(), decl.isConst());
      } else if (item->kind() == NodeKind::AssignStmt) {
        const auto& assign = static_cast<const AssignStmt&>(*item);
        item = std::make_unique<AssignStmt>(item->range(), expandExpr(assign.target()),
                                            expandExpr(assign.value()), assign.op());
      } else if (item->kind() == NodeKind::ReturnStmt) {
        const Expr* value = static_cast<const ReturnStmt&>(*item).value();
        std::unique_ptr<Expr> expanded = value == nullptr ? nullptr : expandExpr(*value);
        item = std::make_unique<ReturnStmt>(item->range(), std::move(expanded));
      }
      (void)expandInside(*item);
      out.push_back(std::move(item));
    }
  }
  statements = std::move(out);
  return true;
}

bool MacroExpander::expandInside(Stmt& statement) {
  switch (statement.kind()) {
  case NodeKind::FunctionDef:
    return expandStmtList(static_cast<FunctionDef&>(statement).body());
  case NodeKind::ClassDef:
    for (std::unique_ptr<FunctionDef>& method : static_cast<ClassDef&>(statement).methods()) {
      if (!expandStmtList(method->body())) {
        return false;
      }
    }
    return true;
  case NodeKind::EnumDef:
    for (std::unique_ptr<FunctionDef>& method : static_cast<EnumDef&>(statement).methods()) {
      if (!expandStmtList(method->body())) {
        return false;
      }
    }
    return true;
  case NodeKind::IfStmt:
    for (IfBranch& branch : static_cast<IfStmt&>(statement).branches()) {
      if (branch.condition != nullptr) {
        branch.condition = expandExpr(*branch.condition);
      }
      if (!expandStmtList(branch.body)) {
        return false;
      }
    }
    return true;
  case NodeKind::WhileStmt:
    if (!expandStmtList(static_cast<WhileStmt&>(statement).body())) {
      return false;
    }
    return true;
  case NodeKind::ForStmt:
    return expandStmtList(static_cast<ForStmt&>(statement).body());
  case NodeKind::MatchStmt: {
    auto& match = static_cast<MatchStmt&>(statement);
    std::unique_ptr<Expr> subject = expandExpr(match.subject());
    if (subject == nullptr) {
      return false;
    }
    match.setSubject(std::move(subject));
    for (MatchArm& arm : match.arms()) {
      if (arm.pattern != nullptr) {
        arm.pattern = expandExpr(*arm.pattern);
        if (arm.pattern == nullptr) {
          return false;
        }
      }
      if (arm.guard != nullptr) {
        arm.guard = expandExpr(*arm.guard);
        if (arm.guard == nullptr) {
          return false;
        }
      }
      if (!expandStmtList(arm.body)) {
        return false;
      }
    }
    return true;
  }
  default:
    return true;
  }
}

std::unique_ptr<Expr> MacroExpander::expandExpr(const Expr& expr) {
  if (fuel_ == 0) {
    diagnostics_->error(expr.range(), "macro expansion exceeded recursion limit");
    diagnostics_->help("break recursive macros or raise the expansion fuel limit");
    return nullptr;
  }
  if (expr.kind() == NodeKind::MacroInvokeExpr) {
    --fuel_;
    std::unique_ptr<Expr> expanded =
        expandInvokeExpr(static_cast<const MacroInvokeExpr&>(expr));
    if (expanded == nullptr) {
      return nullptr;
    }
    return expandExpr(*expanded);
  }
  switch (expr.kind()) {
  case NodeKind::CallExpr: {
    const auto& call = static_cast<const CallExpr&>(expr);
    std::unique_ptr<Expr> callee = expandExpr(call.callee());
    if (callee == nullptr) {
      return nullptr;
    }
    std::vector<std::unique_ptr<Expr>> args;
    for (const std::unique_ptr<Expr>& arg : call.arguments()) {
      if (arg == nullptr) {
        continue;
      }
      std::unique_ptr<Expr> expanded = expandExpr(*arg);
      if (expanded == nullptr) {
        return nullptr;
      }
      args.push_back(std::move(expanded));
    }
    std::vector<std::unique_ptr<TypeExpr>> typeArgs;
    for (const std::unique_ptr<TypeExpr>& arg : call.typeArgs()) {
      typeArgs.push_back(cloneTypeExpr(*arg));
    }
    std::vector<NamedArgument> keywordArgs;
    for (const NamedArgument& kw : call.keywordArguments()) {
      std::unique_ptr<Expr> expanded = expandExpr(*kw.value);
      if (expanded == nullptr) {
        return nullptr;
      }
      NamedArgument named;
      named.name = kw.name;
      named.value = std::move(expanded);
      keywordArgs.push_back(std::move(named));
    }
    return std::make_unique<CallExpr>(expr.range(), std::move(callee), std::move(typeArgs),
                                      std::move(args), std::move(keywordArgs));
  }
  case NodeKind::BinaryExpr: {
    const auto& binary = static_cast<const BinaryExpr&>(expr);
    return std::make_unique<BinaryExpr>(expr.range(), binary.op(), expandExpr(binary.left()),
                                        expandExpr(binary.right()));
  }
  case NodeKind::UnaryExpr: {
    const auto& unary = static_cast<const UnaryExpr&>(expr);
    return std::make_unique<UnaryExpr>(expr.range(), unary.op(), expandExpr(unary.operand()));
  }
  case NodeKind::MemberExpr: {
    const auto& member = static_cast<const MemberExpr&>(expr);
    return std::make_unique<MemberExpr>(expr.range(), expandExpr(member.object()), member.field());
  }
  case NodeKind::ListLiteral: {
    std::vector<std::unique_ptr<Expr>> elements;
    for (const std::unique_ptr<Expr>& item : static_cast<const ListLiteral&>(expr).elements()) {
      elements.push_back(expandExpr(*item));
    }
    return std::make_unique<ListLiteral>(expr.range(), std::move(elements));
  }
  case NodeKind::IndexExpr: {
    const auto& index = static_cast<const IndexExpr&>(expr);
    std::unique_ptr<Expr> start =
        index.start() == nullptr ? nullptr : expandExpr(*index.start());
    std::unique_ptr<Expr> stop = index.stop() == nullptr ? nullptr : expandExpr(*index.stop());
    return std::make_unique<IndexExpr>(expr.range(), expandExpr(index.object()), std::move(start),
                                       std::move(stop), index.isSlice());
  }
  case NodeKind::CastExpr: {
    const auto& cast = static_cast<const CastExpr&>(expr);
    return std::make_unique<CastExpr>(expr.range(), expandExpr(cast.value()),
                                      cloneTypeExpr(cast.target()));
  }
  case NodeKind::TernaryExpr: {
    const auto& ternary = static_cast<const TernaryExpr&>(expr);
    return std::make_unique<TernaryExpr>(expr.range(), expandExpr(ternary.thenValue()),
                                         expandExpr(ternary.condition()),
                                         expandExpr(ternary.elseValue()));
  }
  case NodeKind::InterpolatedStringExpr: {
    const auto& interpolated = static_cast<const InterpolatedStringExpr&>(expr);
    std::vector<StringPart> parts;
    for (const StringPart& part : interpolated.parts()) {
      StringPart copy;
      copy.literal = part.literal;
      if (part.value != nullptr) {
        copy.value = expandExpr(*part.value);
      }
      parts.push_back(std::move(copy));
    }
    return std::make_unique<InterpolatedStringExpr>(expr.range(), std::move(parts));
  }
  default:
    return cloneExpr(expr);
  }
}

std::vector<std::unique_ptr<Stmt>> MacroExpander::expandInvokeStmt(const MacroInvokeStmt& invoke) {
  const MacroDef* def = env_->find(invoke.name());
  if (def == nullptr) {
    diagnostics_->error(invoke.range(), "unknown macro '" + invoke.name() + "'");
    return {};
  }
  if (def->syntaxMode() == MacroSyntaxMode::Raw || def->syntaxMode() == MacroSyntaxMode::Pipeline ||
      !def->matchArms().empty()) {
    std::unique_ptr<Expr> expr = expandDef(*def, invoke.rawText(), invoke.rawRange(), invoke.tokens(),
                                           invoke.range(), invoke.delimiter());
    if (expr == nullptr) {
      return {};
    }
    std::vector<std::unique_ptr<Stmt>> stmts;
    stmts.push_back(std::make_unique<ExprStmt>(invoke.range(), std::move(expr)));
    return stmts;
  }
  MacroBindings env;
  std::vector<std::unique_ptr<Expr>> args =
      parseSereExprList(*diagnostics_, invoke.rawText(), invoke.rawRange().start);
  if (def->variadic() && !def->params().empty()) {
    env.exprs[def->params().front()] = std::move(args);
  } else if (args.size() == def->params().size()) {
    for (std::size_t index = 0; index < args.size(); ++index) {
      env.exprs[def->params()[index]].push_back(std::move(args[index]));
    }
  }
  if (def->typed() && !def->params().empty() && !env.exprs[def->params().front()].empty()) {
    std::vector<std::unique_ptr<Expr>> typeofArgs;
    typeofArgs.push_back(cloneExpr(*env.exprs[def->params().front()].front()));
    env.exprs["type"].push_back(std::make_unique<CallExpr>(
        invoke.range(), std::make_unique<NameExpr>(invoke.range(), "typeof"),
        std::vector<std::unique_ptr<TypeExpr>>{}, std::move(typeofArgs)));
  }
  std::vector<std::unique_ptr<Stmt>> body =
      substStmts(def->quoteBody(), env, nextMark_++, invoke.range());
  if (body.size() != 1 || body.front()->kind() != NodeKind::ExprStmt) {
    return body;
  }
  std::unique_ptr<Expr> expr = expandDef(*def, invoke.rawText(), invoke.rawRange(), invoke.tokens(),
                                         invoke.range(), invoke.delimiter());
  if (expr != nullptr) {
    body.clear();
    body.push_back(std::make_unique<ExprStmt>(invoke.range(), std::move(expr)));
  }
  return body;
}

std::unique_ptr<Expr> MacroExpander::expandInvokeExpr(const MacroInvokeExpr& invoke) {
  const MacroDef* def = env_->find(invoke.name());
  if (def == nullptr) {
    diagnostics_->error(invoke.range(), "unknown macro '" + invoke.name() + "'");
    return nullptr;
  }
  return expandDef(*def, invoke.rawText(), invoke.rawRange(), invoke.tokens(), invoke.range(),
                   invoke.delimiter());
}

std::unique_ptr<Expr> MacroExpander::expandDef(const MacroDef& def, const std::string& rawText,
                                               SourceRange rawRange, const std::vector<Token>& tokens,
                                               SourceRange callSite, MacroDelimiter delimiter) {
  if (def.syntaxMode() == MacroSyntaxMode::Pipeline) {
    return expandPipeline(rawText, callSite);
  }
  if (def.syntaxMode() == MacroSyntaxMode::Raw) {
    return expandRaw(def, rawText, rawRange, callSite);
  }
  if (!def.matchArms().empty()) {
    std::vector<Token> wrapped = wrapDelimiter(innerTokens(tokens), delimiter, callSite);
    return expandMatch(def, wrapped, callSite);
  }
  MacroBindings env;
  std::vector<std::unique_ptr<Expr>> args =
      parseSereExprList(*diagnostics_, rawText, rawRange.start);
  if (def.variadic()) {
    if (!def.params().empty()) {
      env.exprs[def.params().front()] = std::move(args);
    }
  } else {
    if (args.size() != def.params().size()) {
      diagnostics_->error(callSite, "macro '" + def.name() + "' argument count mismatch");
      return nullptr;
    }
    for (std::size_t index = 0; index < args.size(); ++index) {
      env.exprs[def.params()[index]].push_back(std::move(args[index]));
    }
  }
  if (def.typed() && !def.params().empty() && !env.exprs[def.params().front()].empty()) {
    std::vector<std::unique_ptr<Expr>> typeofArgs;
    typeofArgs.push_back(cloneExpr(*env.exprs[def.params().front()].front()));
    auto typeofCall = std::make_unique<CallExpr>(
        callSite, std::make_unique<NameExpr>(callSite, "typeof"),
        std::vector<std::unique_ptr<TypeExpr>>{}, std::move(typeofArgs));
    env.exprs["type"].push_back(std::move(typeofCall));
  }
  return expandQuote(def, std::move(env), callSite);
}

std::unique_ptr<Expr> MacroExpander::expandQuote(const MacroDef& def, MacroBindings env,
                                                 SourceRange callSite) {
  std::vector<std::unique_ptr<Stmt>> body =
      substStmts(def.quoteBody(), env, nextMark_++, callSite);
  if (body.size() == 1 && body.front()->kind() == NodeKind::ExprStmt) {
    return cloneExpr(static_cast<const ExprStmt&>(*body.front()).expression());
  }
  if (body.empty()) {
    diagnostics_->error(callSite, "macro '" + def.name() + "' produced no expression");
    return nullptr;
  }
  if (body.back()->kind() != NodeKind::ExprStmt) {
    diagnostics_->error(callSite, "macro '" + def.name() + "' produced a statement, not an expression");
    return nullptr;
  }
  return cloneExpr(static_cast<const ExprStmt&>(*body.back()).expression());
}

std::unique_ptr<Expr> MacroExpander::expandMatch(const MacroDef& def, const std::vector<Token>& tokens,
                                                 SourceRange callSite) {
  for (const MacroMatchArm& arm : def.matchArms()) {
    std::size_t patternIndex = 0;
    std::vector<PatternAtom> atoms = compilePattern(arm.pattern, patternIndex, TokenKind::EndOfFile);
    std::size_t tokenIndex = 0;
    MacroBindings env;
    if (!matchAtoms(atoms, tokens, tokenIndex, env, *diagnostics_)) {
      continue;
    }
    if (tokenIndex != tokens.size()) {
      continue;
    }
    std::vector<std::unique_ptr<Stmt>> body = substStmts(arm.body, env, nextMark_++, callSite);
    if (body.size() == 1 && body.front()->kind() == NodeKind::ExprStmt) {
      return cloneExpr(static_cast<const ExprStmt&>(*body.front()).expression());
    }
    if (!body.empty() && body.back()->kind() == NodeKind::ExprStmt) {
      return cloneExpr(static_cast<const ExprStmt&>(*body.back()).expression());
    }
  }
  diagnostics_->error(callSite, "no match arm for macro '" + def.name() + "'");
  diagnostics_->note(def.range(), "macro defined here");
  return nullptr;
}

std::unique_ptr<Expr> MacroExpander::expandRaw(const MacroDef& def, const std::string& rawText,
                                               SourceRange rawRange, SourceRange callSite) {
  (void)rawRange;
  MacroInterpolate mode = def.interpolate();
  if (mode == MacroInterpolate::None) {
    mode = MacroInterpolate::Brace;
  }
  std::unique_ptr<Expr> interpolated =
      interpolateRaw(rawText, callSite, mode, *diagnostics_);
  if (interpolated == nullptr) {
    return nullptr;
  }
  interpolated->setRange(callSite);
  return wrapResult(def, std::move(interpolated), callSite);
}

std::unique_ptr<Expr> MacroExpander::expandPipeline(const std::string& rawText,
                                                    SourceRange callSite) {
  const std::vector<std::string> stages = splitPipeline(rawText);
  if (stages.empty()) {
    diagnostics_->error(callSite, "pipeline macro requires at least one stage");
    return nullptr;
  }
  std::unique_ptr<Expr> acc = parseSereExpr(*diagnostics_, stages.front(), callSite.start);
  if (acc == nullptr) {
    return nullptr;
  }
  for (std::size_t index = 1; index < stages.size(); ++index) {
    std::unique_ptr<Expr> stage = parseSereExpr(*diagnostics_, stages[index], callSite.start);
    if (stage == nullptr) {
      return nullptr;
    }
    if (stage->kind() == NodeKind::CallExpr) {
      const auto& call = static_cast<const CallExpr&>(*stage);
      std::vector<std::unique_ptr<Expr>> args;
      for (const std::unique_ptr<Expr>& arg : call.arguments()) {
        args.push_back(cloneExpr(*arg));
      }
      args.push_back(std::move(acc));
      acc = std::make_unique<CallExpr>(callSite, cloneExpr(call.callee()),
                                       std::vector<std::unique_ptr<TypeExpr>>{}, std::move(args));
      continue;
    }
    if (stage->kind() == NodeKind::NameExpr) {
      std::vector<std::unique_ptr<Expr>> args;
      args.push_back(std::move(acc));
      acc = std::make_unique<CallExpr>(callSite, std::move(stage),
                                       std::vector<std::unique_ptr<TypeExpr>>{}, std::move(args));
      continue;
    }
    diagnostics_->error(callSite, "pipeline stage must be a name or call");
    return nullptr;
  }
  return acc;
}

std::unique_ptr<Expr> MacroExpander::wrapResult(const MacroDef& def, std::unique_ptr<Expr> value,
                                                SourceRange callSite) {
  if (def.wrapper().empty()) {
    return value;
  }
  std::vector<std::unique_ptr<Expr>> args;
  args.push_back(std::move(value));
  return std::make_unique<CallExpr>(callSite,
                                    std::make_unique<NameExpr>(callSite, def.wrapper()),
                                    std::vector<std::unique_ptr<TypeExpr>>{}, std::move(args));
}

}  // namespace sere
