/// @file TokenTree.cpp
/// Groups delimiter pairs into nested token trees.

#include "sere/macro/TokenTree.h"

#include <string>

namespace sere {
namespace {

[[nodiscard]] TokenKind closerFor(TokenKind opener) {
  switch (opener) {
  case TokenKind::LParen:
    return TokenKind::RParen;
  case TokenKind::LBracket:
    return TokenKind::RBracket;
  case TokenKind::LBrace:
    return TokenKind::RBrace;
  case TokenKind::Indent:
    return TokenKind::Dedent;
  default:
    return TokenKind::Unknown;
  }
}

void parseSeq(const std::vector<Token>& tokens, std::size_t& index, TokenKind closer,
              std::vector<TokenTree>& out) {
  while (index < tokens.size()) {
    const Token& token = tokens[index];
    if (token.kind() == closer || token.kind() == TokenKind::EndOfFile) {
      return;
    }
    const TokenKind match = closerFor(token.kind());
    if (match != TokenKind::Unknown) {
      TokenTree group;
      group.kind = TokenTreeKind::Delimited;
      group.opener = token.kind();
      group.closer = match;
      ++index;
      parseSeq(tokens, index, match, group.children);
      if (index < tokens.size() && tokens[index].kind() == match) {
        ++index;
      }
      out.push_back(std::move(group));
      continue;
    }
    TokenTree leaf;
    leaf.kind = TokenTreeKind::Leaf;
    leaf.leaf = token;
    out.push_back(std::move(leaf));
    ++index;
  }
}

void flattenSeq(const std::vector<TokenTree>& trees, std::vector<Token>& out) {
  for (const TokenTree& tree : trees) {
    if (tree.kind == TokenTreeKind::Leaf) {
      out.push_back(tree.leaf);
      continue;
    }
    out.emplace_back(tree.opener, SourceRange{}, std::string());
    flattenSeq(tree.children, out);
    out.emplace_back(tree.closer, SourceRange{}, std::string());
  }
}

}  // namespace

std::vector<TokenTree> buildTokenTrees(const std::vector<Token>& tokens) {
  std::vector<TokenTree> trees;
  std::size_t index = 0;
  parseSeq(tokens, index, TokenKind::EndOfFile, trees);
  return trees;
}

std::vector<Token> flattenTokenTrees(const std::vector<TokenTree>& trees) {
  std::vector<Token> tokens;
  flattenSeq(trees, tokens);
  return tokens;
}

}  // namespace sere
