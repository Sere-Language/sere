/// @file SemanticTokens.cpp
/// Classifies lexer tokens and symbols for LSP semantic highlighting.

#include "sere/lsp/SemanticTokens.h"

#include "sere/diag/DiagnosticEngine.h"
#include "sere/driver/Frontend.h"
#include "sere/lex/Lexer.h"
#include "sere/lex/Token.h"
#include "sere/lex/TokenKind.h"
#include "sere/sema/TypeChecker.h"
#include "sere/source/SourceManager.h"
#include "sere/types/Intrinsic.h"

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <vector>

namespace sere {
namespace {

[[nodiscard]] bool isSkipped(TokenKind kind) {
  return kind == TokenKind::Newline || kind == TokenKind::Indent || kind == TokenKind::Dedent ||
         kind == TokenKind::EndOfFile || kind == TokenKind::Comment;
}

[[nodiscard]] TokenKind previousKind(const std::vector<Token>& tokens, std::size_t index) {
  while (index > 0) {
    --index;
    if (!isSkipped(tokens[index].kind())) {
      return tokens[index].kind();
    }
  }
  return TokenKind::Unknown;
}

[[nodiscard]] bool isModifierKeyword(TokenKind kind) {
  return kind == TokenKind::KeywordStatic || kind == TokenKind::KeywordConst ||
         kind == TokenKind::KeywordExtern;
}

[[nodiscard]] bool isLanguageKeyword(TokenKind kind) {
  return kind >= TokenKind::KeywordFalse && kind <= TokenKind::KeywordWith;
}

[[nodiscard]] bool isBuiltinType(std::string_view name) {
  return name == "void" || name == "bool" || name == "i8" || name == "i16" || name == "i32" ||
         name == "i64" || name == "u8" || name == "u16" || name == "u32" || name == "u64" ||
         name == "f32" || name == "f64" || name == "str" || name == "regex" || name == "byte" ||
         name == "Iterator" || name == "never" || name == "Unique" || name == "Shared" || name == "Ptr" ||
         name == "list" || name == "array" || name == "dict" || name == "Callable" ||
         name == "Function" || name == "Class" || name == "Type";
}

[[nodiscard]] bool isCompilerIntrinsic(std::string_view name) {
  // Derive from the intrinsic registry so this list cannot drift from the
  // intrinsics actually registered by the frontend.
  const auto isDeclared = [name](const IntrinsicInfo& info) {
    return info.declared && info.name == name;
  };
  return std::ranges::any_of(allIntrinsics(), isDeclared);
}

[[nodiscard]] bool isOperatorToken(TokenKind kind) {
  switch (kind) {
  case TokenKind::Plus:
  case TokenKind::PlusPlus:
  case TokenKind::PlusEqual:
  case TokenKind::Minus:
  case TokenKind::MinusMinus:
  case TokenKind::MinusEqual:
  case TokenKind::Star:
  case TokenKind::StarStar:
  case TokenKind::StarEqual:
  case TokenKind::StarStarEqual:
  case TokenKind::Slash:
  case TokenKind::SlashSlash:
  case TokenKind::SlashEqual:
  case TokenKind::SlashSlashEqual:
  case TokenKind::Percent:
  case TokenKind::PercentEqual:
  case TokenKind::Amp:
  case TokenKind::AmpEqual:
  case TokenKind::Caret:
  case TokenKind::CaretEqual:
  case TokenKind::Tilde:
  case TokenKind::Pipe:
  case TokenKind::PipeEqual:
  case TokenKind::EqualEqual:
  case TokenKind::BangEqual:
  case TokenKind::Less:
  case TokenKind::LessLess:
  case TokenKind::LessEqual:
  case TokenKind::LessLessEqual:
  case TokenKind::Greater:
  case TokenKind::GreaterGreater:
  case TokenKind::GreaterEqual:
  case TokenKind::GreaterGreaterEqual:
  case TokenKind::Bang:
  case TokenKind::FatArrow:
  case TokenKind::Arrow:
  case TokenKind::Dollar:
    return true;
  default:
    return false;
  }
}

void pushToken(std::vector<SemanticToken>& out, SourceRange range, SemanticType type,
               std::uint32_t modifiers) {
  if (range.start.line == 0 || range.end.line != range.start.line) {
    return;
  }
  if (range.end.offset <= range.start.offset) {
    return;
  }
  SemanticToken token;
  token.line = range.start.line - 1;
  token.column = range.start.column == 0 ? 0 : range.start.column - 1;
  token.length = range.end.offset - range.start.offset;
  token.type = static_cast<std::uint32_t>(type);
  token.modifiers = modifiers;
  out.push_back(token);
}

[[nodiscard]] int typeFromKind(std::string_view kind) {
  if (kind == "class") {
    return static_cast<int>(SemanticType::Class);
  }
  if (kind == "enum") {
    return static_cast<int>(SemanticType::Enum);
  }
  if (kind == "struct") {
    return static_cast<int>(SemanticType::Struct);
  }
  if (kind == "enumMember") {
    return static_cast<int>(SemanticType::EnumMember);
  }
  if (kind == "field") {
    return static_cast<int>(SemanticType::Property);
  }
  if (kind == "function") {
    return static_cast<int>(SemanticType::Function);
  }
  if (kind == "method") {
    return static_cast<int>(SemanticType::Method);
  }
  if (kind == "macro") {
    return static_cast<int>(SemanticType::Macro);
  }
  if (kind == "type") {
    return static_cast<int>(SemanticType::Type);
  }
  if (kind == "module") {
    return static_cast<int>(SemanticType::Namespace);
  }
  if (kind == "parameter") {
    return static_cast<int>(SemanticType::Parameter);
  }
  if (kind == "variable") {
    return static_cast<int>(SemanticType::Variable);
  }
  return -1;
}

[[nodiscard]] int lookupName(const TypeChecker* checker, std::string_view name, bool member) {
  if (checker == nullptr) {
    return -1;
  }
  int found = -1;
  for (const SemanticSymbol& symbol : checker->symbols()) {
    if (symbol.name != name) {
      continue;
    }
    const int type = typeFromKind(symbol.kind);
    if (type < 0) {
      continue;
    }
    if (member && (symbol.kind == "enumMember" || symbol.kind == "method" ||
                   symbol.kind == "field")) {
      return type;
    }
    found = type;
  }
  return found;
}

[[nodiscard]] int declarationType(TokenKind prev) {
  if (prev == TokenKind::KeywordClass) {
    return static_cast<int>(SemanticType::Class);
  }
  if (prev == TokenKind::KeywordStruct) {
    return static_cast<int>(SemanticType::Struct);
  }
  if (prev == TokenKind::KeywordEnum) {
    return static_cast<int>(SemanticType::Enum);
  }
  if (prev == TokenKind::KeywordType) {
    return static_cast<int>(SemanticType::Type);
  }
  if (prev == TokenKind::KeywordMacro) {
    return static_cast<int>(SemanticType::Macro);
  }
  if (prev == TokenKind::KeywordDef) {
    return static_cast<int>(SemanticType::Function);
  }
  return -1;
}

void emitIdentifier(const std::vector<Token>& tokens, std::size_t index, const TypeChecker* checker,
                    std::vector<SemanticToken>& out) {
  const Token& token = tokens[index];
  const std::string_view name = token.spelling();
  const TokenKind prev = previousKind(tokens, index);
  int type = declarationType(prev);
  std::uint32_t mods = type >= 0 ? static_cast<std::uint32_t>(SemanticMod::Declaration) : 0;
  if (prev == TokenKind::KeywordDef) {
    const int looked = lookupName(checker, name, false);
    if (looked >= 0) {
      type = looked;
    }
  } else if (prev == TokenKind::Dot) {
    type = lookupName(checker, name, true);
    if (type < 0) {
      type = static_cast<int>(SemanticType::Property);
    }
  } else if (type < 0) {
    type = lookupName(checker, name, false);
  }
  if (name == "self") {
    type = static_cast<int>(SemanticType::Parameter);
  }
  if (name == "_") {
    type = static_cast<int>(SemanticType::Variable);
    mods |= static_cast<std::uint32_t>(SemanticMod::Readonly);
  }
  if (name == "quote") {
    type = static_cast<int>(SemanticType::Keyword);
  }
  if (prev == TokenKind::Colon &&
      (name == "expr" || name == "ident" || name == "literal" || name == "tt" ||
       name == "token" || name == "stmt" || name == "block")) {
    type = static_cast<int>(SemanticType::TypeParameter);
  }
  if (prev == TokenKind::KeywordCase || prev == TokenKind::KeywordMatch) {
    const int looked = lookupName(checker, name, false);
    if (looked >= 0) {
      type = looked;
    }
  }
  const TokenKind next =
      index + 1 < tokens.size() ? tokens[index + 1].kind() : TokenKind::Unknown;
  if (next == TokenKind::Bang) {
    const int macroType = lookupName(checker, name, false);
    if (macroType == static_cast<int>(SemanticType::Macro)) {
      type = macroType;
    }
  }
  if (isBuiltinType(name)) {
    type = static_cast<int>(SemanticType::Type);
    mods |= static_cast<std::uint32_t>(SemanticMod::DefaultLibrary);
  } else if (isCompilerIntrinsic(name)) {
    type = static_cast<int>(SemanticType::Function);
    mods |= static_cast<std::uint32_t>(SemanticMod::DefaultLibrary);
  } else if (type >= 0 && checker != nullptr) {
    for (const SemanticSymbol& symbol : checker->symbols()) {
      if (symbol.name == name && !symbol.navigable &&
          (symbol.kind == "function" || symbol.kind == "macro" || symbol.kind == "class" ||
           symbol.kind == "type")) {
        mods |= static_cast<std::uint32_t>(SemanticMod::DefaultLibrary);
        break;
      }
    }
  }
  if (type >= 0) {
    pushToken(out, token.range(), static_cast<SemanticType>(type), mods);
  }
}

std::size_t emitDecorator(const std::vector<Token>& tokens, std::size_t index,
                          std::vector<SemanticToken>& out) {
  const Token& at = tokens[index];
  if (index + 1 >= tokens.size() || tokens[index + 1].kind() != TokenKind::Identifier) {
    pushToken(out, at.range(), SemanticType::Decorator, 0);
    return index;
  }
  const Token& name = tokens[index + 1];
  SourceRange range = at.range();
  range.end = name.range().end;
  if (name.range().start.offset == at.range().end.offset) {
    pushToken(out, range, SemanticType::Decorator, 0);
    return index + 1;
  }
  pushToken(out, at.range(), SemanticType::Decorator, 0);
  pushToken(out, name.range(), SemanticType::Decorator, 0);
  return index + 1;
}

void emitLiteral(const Token& token, std::vector<SemanticToken>& out) {
  if (token.kind() == TokenKind::Integer || token.kind() == TokenKind::Float) {
    pushToken(out, token.range(), SemanticType::Number, 0);
  } else if (token.kind() == TokenKind::String || token.kind() == TokenKind::FString) {
    pushToken(out, token.range(), SemanticType::String, 0);
  } else if (token.kind() == TokenKind::Regex) {
    pushToken(out, token.range(), SemanticType::Regexp, 0);
  } else if (isOperatorToken(token.kind())) {
    pushToken(out, token.range(), SemanticType::Operator, 0);
  }
}

}  // namespace

void collectSemanticTokens(Frontend& frontend, std::vector<SemanticToken>& out) {
  if (frontend.source() == nullptr) {
    return;
  }
  DiagnosticEngine unused;
  Lexer lexer(*frontend.source(), unused);
  const std::vector<Token> tokens = lexer.tokenizeAll();
  const TypeChecker* checker = frontend.checker();
  for (std::size_t index = 0; index < tokens.size(); ++index) {
    const Token& token = tokens[index];
    if (isSkipped(token.kind())) {
      continue;
    }
    if (isModifierKeyword(token.kind())) {
      pushToken(out, token.range(), SemanticType::Modifier, 0);
      continue;
    }
    if (isLanguageKeyword(token.kind())) {
      pushToken(out, token.range(), SemanticType::Keyword, 0);
      continue;
    }
    if (token.kind() == TokenKind::At) {
      index = emitDecorator(tokens, index, out);
      continue;
    }
    if (token.kind() == TokenKind::Identifier) {
      emitIdentifier(tokens, index, checker, out);
      continue;
    }
    emitLiteral(token, out);
  }
}

void encodeSemanticTokens(std::vector<SemanticToken> tokens, std::vector<std::int64_t>& data) {
  std::sort(tokens.begin(), tokens.end(), [](const SemanticToken& left, const SemanticToken& right) {
    return left.line < right.line || (left.line == right.line && left.column < right.column);
  });
  std::uint32_t prevLine = 0;
  std::uint32_t prevCol = 0;
  for (const SemanticToken& token : tokens) {
    const std::uint32_t deltaLine = token.line - prevLine;
    const std::uint32_t deltaStart = deltaLine == 0 ? token.column - prevCol : token.column;
    data.push_back(static_cast<std::int64_t>(deltaLine));
    data.push_back(static_cast<std::int64_t>(deltaStart));
    data.push_back(static_cast<std::int64_t>(token.length));
    data.push_back(static_cast<std::int64_t>(token.type));
    data.push_back(static_cast<std::int64_t>(token.modifiers));
    prevLine = token.line;
    prevCol = token.column;
  }
}

}  // namespace sere
