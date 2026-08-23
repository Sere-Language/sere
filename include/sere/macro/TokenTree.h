/// @file TokenTree.h
/// Nested token groups for declarative macro matching.

#pragma once

#include "sere/lex/Token.h"

#include <cstddef>
#include <vector>

namespace sere {

enum class TokenTreeKind {
  Leaf,
  Delimited,
};

struct TokenTree {
  TokenTreeKind kind = TokenTreeKind::Leaf;
  Token leaf{TokenKind::Unknown, SourceRange{}, {}};
  TokenKind opener = TokenKind::Unknown;
  TokenKind closer = TokenKind::Unknown;
  std::vector<TokenTree> children{};
};

/// Groups `()`, `[]`, `{}`, and Indent/Dedent pairs into nested trees.
[[nodiscard]] std::vector<TokenTree> buildTokenTrees(const std::vector<Token>& tokens);

/// Flattens trees back to a token sequence, preserving delimiter tokens.
[[nodiscard]] std::vector<Token> flattenTokenTrees(const std::vector<TokenTree>& trees);

}  // namespace sere
