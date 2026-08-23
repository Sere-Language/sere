/// @file lex_basic.cpp
/// Checks that the lexer emits Python-style tokens for print("hi").

#include "sere/diag/DiagnosticEngine.h"
#include "sere/lex/Lexer.h"
#include "sere/source/SourceManager.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

int fail(const char* message) {
  std::cerr << "lex_basic: " << message << '\n';
  return 1;
}

}  // namespace

int main() {
  const std::string text = "print(f\"hi {x}\")\n";
  sere::DiagnosticEngine diagnostics;
  sere::SourceManager source("lex_basic.sere", text);
  sere::Lexer lexer(source, diagnostics);
  const std::vector<sere::Token> tokens = lexer.tokenizeAll();
  if (diagnostics.hasErrors()) {
    diagnostics.printAll(source);
    return fail("lexer reported errors");
  }
  if (tokens.size() < 5) {
    return fail("expected at least 5 tokens");
  }
  if (tokens[0].kind() != sere::TokenKind::Identifier || tokens[0].spelling() != "print") {
    return fail("expected identifier print");
  }
  if (tokens[1].kind() != sere::TokenKind::LParen) {
    return fail("expected '('");
  }
  if (tokens[2].kind() != sere::TokenKind::FString) {
    return fail("expected f-string");
  }
  if (tokens[3].kind() != sere::TokenKind::RParen) {
    return fail("expected ')'");
  }
  if (tokens.back().kind() != sere::TokenKind::EndOfFile) {
    return fail("expected end of file");
  }

  const std::string bomText = "\xEF\xBB\xBFprint()\n";
  sere::DiagnosticEngine bomDiagnostics;
  sere::SourceManager bomSource("lex_bom.sere", bomText);
  sere::Lexer bomLexer(bomSource, bomDiagnostics);
  const std::vector<sere::Token> bomTokens = bomLexer.tokenizeAll();
  if (bomDiagnostics.hasErrors() || bomTokens.empty() ||
      bomTokens[0].kind() != sere::TokenKind::Identifier || bomTokens[0].spelling() != "print") {
    return fail("UTF-8 BOM should be ignored");
  }

  const std::string floatText = "1.0 2. 3e2 4.5e-1 .5 1.0f 1f\n";
  sere::DiagnosticEngine floatDiagnostics;
  sere::SourceManager floatSource("lex_float.sere", floatText);
  sere::Lexer floatLexer(floatSource, floatDiagnostics);
  const std::vector<sere::Token> floatTokens = floatLexer.tokenizeAll();
  if (floatDiagnostics.hasErrors()) {
    floatDiagnostics.printAll(floatSource);
    return fail("float lexer reported errors");
  }
  int floatCount = 0;
  for (const sere::Token& token : floatTokens) {
    if (token.kind() == sere::TokenKind::Float) {
      ++floatCount;
    }
  }
  if (floatCount != 7) {
    return fail("expected 7 float tokens");
  }

  const std::string quoteText = "'hi' '''ab''' `a+` \"\"\n";
  sere::DiagnosticEngine quoteDiagnostics;
  sere::SourceManager quoteSource("lex_quotes.sere", quoteText);
  sere::Lexer quoteLexer(quoteSource, quoteDiagnostics);
  const std::vector<sere::Token> quoteTokens = quoteLexer.tokenizeAll();
  if (quoteDiagnostics.hasErrors()) {
    quoteDiagnostics.printAll(quoteSource);
    return fail("quote lexer reported errors");
  }
  bool sawSingle = false;
  bool sawTriple = false;
  bool sawRegex = false;
  bool sawEmpty = false;
  for (const sere::Token& token : quoteTokens) {
    if (token.kind() == sere::TokenKind::String && token.spelling() == "'hi'") {
      sawSingle = true;
    }
    if (token.kind() == sere::TokenKind::String && token.spelling() == "'''ab'''") {
      sawTriple = true;
    }
    if (token.kind() == sere::TokenKind::Regex && token.spelling() == "`a+`") {
      sawRegex = true;
    }
    if (token.kind() == sere::TokenKind::String && token.spelling() == "\"\"") {
      sawEmpty = true;
    }
  }
  if (!sawSingle || !sawTriple || !sawRegex || !sawEmpty) {
    return fail("expected single, triple, regex, and empty string tokens");
  }

  const sere::DecodedString single = sere::decodeStringToken("'hi\\n'");
  if (single.regex || single.value != "hi\n") {
    return fail("single-quoted escapes should decode");
  }
  const sere::DecodedString triple = sere::decodeStringToken("\"\"\"a\nb\"\"\"");
  if (triple.regex || triple.value != "a\nb") {
    return fail("triple-quoted strings should keep newlines");
  }
  const sere::DecodedString regex = sere::decodeStringToken("`\\d+`");
  if (!regex.regex || regex.value != "\\d+") {
    return fail("regex literals should keep pattern escapes");
  }

  const std::string radixText = "0x0a 0XA 0b1010 0o17 1_000 0xFF_FF\n";
  sere::DiagnosticEngine radixDiagnostics;
  sere::SourceManager radixSource("lex_radix.sere", radixText);
  sere::Lexer radixLexer(radixSource, radixDiagnostics);
  const std::vector<sere::Token> radixTokens = radixLexer.tokenizeAll();
  if (radixDiagnostics.hasErrors()) {
    radixDiagnostics.printAll(radixSource);
    return fail("radix lexer reported errors");
  }
  int integerCount = 0;
  for (const sere::Token& token : radixTokens) {
    if (token.kind() == sere::TokenKind::Integer) {
      ++integerCount;
    }
  }
  if (integerCount != 6) {
    return fail("expected 6 integer tokens for radix and grouped literals");
  }
  const sere::ParsedInteger hex = sere::parseIntegerToken("0x0a");
  const sere::ParsedInteger hexUpper = sere::parseIntegerToken("0XA");
  const sere::ParsedInteger binary = sere::parseIntegerToken("0b1010");
  const sere::ParsedInteger octal = sere::parseIntegerToken("0o17");
  const sere::ParsedInteger grouped = sere::parseIntegerToken("1_000");
  const sere::ParsedInteger hexGroup = sere::parseIntegerToken("0xFF_FF");
  if (hex.status != sere::IntegerParseStatus::Ok || hex.value != 10) {
    return fail("0x0a should parse as 10");
  }
  if (hexUpper.status != sere::IntegerParseStatus::Ok || hexUpper.value != 10) {
    return fail("0XA should parse as 10");
  }
  if (binary.status != sere::IntegerParseStatus::Ok || binary.value != 10) {
    return fail("0b1010 should parse as 10");
  }
  if (octal.status != sere::IntegerParseStatus::Ok || octal.value != 15) {
    return fail("0o17 should parse as 15");
  }
  if (grouped.status != sere::IntegerParseStatus::Ok || grouped.value != 1000) {
    return fail("1_000 should parse as 1000");
  }
  if (hexGroup.status != sere::IntegerParseStatus::Ok || hexGroup.value != 65535) {
    return fail("0xFF_FF should parse as 65535");
  }
  return 0;
}
