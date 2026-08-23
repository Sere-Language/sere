/// @file SemanticTokens.h
/// LSP semantic-token legend and collection for Sere highlighting.

#pragma once

#include <cstdint>
#include <vector>

namespace sere {

class Frontend;

struct SemanticToken {
  std::uint32_t line = 0;
  std::uint32_t column = 0;
  std::uint32_t length = 0;
  std::uint32_t type = 0;
  std::uint32_t modifiers = 0;
};

enum class SemanticType : std::uint32_t {
  Namespace = 0,
  Type,
  Class,
  Enum,
  Struct,
  TypeParameter,
  Parameter,
  Variable,
  Property,
  EnumMember,
  Function,
  Method,
  Macro,
  Keyword,
  Modifier,
  String,
  Number,
  Regexp,
  Operator,
  Decorator,
};

enum class SemanticMod : std::uint32_t {
  Declaration = 1u << 0,
  Definition = 1u << 1,
  Readonly = 1u << 2,
  Static = 1u << 3,
  Abstract = 1u << 4,
  DefaultLibrary = 1u << 5,
};

inline constexpr const char* kSemanticTokenTypeNames[] = {
    "namespace", "type",          "class",    "enum",     "struct",  "typeParameter",
    "parameter", "variable",      "property", "enumMember", "function", "method",
    "macro",     "keyword",       "modifier", "string",   "number",  "regexp",
    "operator",  "decorator",
};

inline constexpr const char* kSemanticTokenModifierNames[] = {
    "declaration", "definition", "readonly", "static", "abstract", "defaultLibrary",
};

void collectSemanticTokens(Frontend& frontend, std::vector<SemanticToken>& out);
void encodeSemanticTokens(std::vector<SemanticToken> tokens, std::vector<std::int64_t>& data);

}  // namespace sere
