// Unit tests for the docstring parser and its Markdown rendering.
//
// These cover the pieces the language server puts in front of the editor: the
// summary/section split, the argument list merged with a signature, and the
// fenced declaration. A regression in any of them shows up as broken hover,
// completion, or signature help.

#include "sere/lsp/DocFormat.h"

#include <iostream>
#include <string>

namespace {

int g_failures = 0;

void expect(bool condition, const std::string& what) {
  if (condition) {
    return;
  }
  std::cerr << "FAIL: " << what << "\n";
  ++g_failures;
}

void expectContains(const std::string& haystack,
                    const std::string& needle,
                    const std::string& what) {
  if (haystack.find(needle) != std::string::npos) {
    return;
  }
  std::cerr << "FAIL: " << what << "\n  missing: " << needle << "\n  in:\n"
            << haystack << "\n";
  ++g_failures;
}

void expectNotContains(const std::string& haystack,
                       const std::string& needle,
                       const std::string& what) {
  if (haystack.find(needle) == std::string::npos) {
    return;
  }
  std::cerr << "FAIL: " << what << "\n  unexpected: " << needle << "\n";
  ++g_failures;
}

const char* kServeBeer = R"(Serve a beer when the guest is old enough.

The drink is only poured after the guest's age has been checked.

Args:
    name: Who is being served.
    age (i32): The guest's age in years.

Returns:
    Ok with a message when served, Err otherwise.

Raises:
    ValueError: When the age is negative.

Example:
    print(serve_beer("Ada", 30))
)";

void testParseSections() {
  const sere::DocComment doc = sere::parseDocstring(kServeBeer);
  expect(doc.summary == "Serve a beer when the guest is old enough.", "summary paragraph");
  expect(doc.body.find("only poured") != std::string::npos, "body paragraph");
  expect(doc.params.size() == 2, "two documented parameters");
  if (doc.params.size() == 2) {
    expect(doc.params[0].first == "name", "first parameter name");
    expect(doc.params[0].second == "Who is being served.", "first parameter text");
    expect(doc.params[1].first == "age", "second parameter name");
    expect(doc.params[1].second == "The guest's age in years.", "type suffix is dropped");
  }
  expect(doc.returns.find("Ok with a message") != std::string::npos, "returns text");
  expect(doc.raises.size() == 1 && doc.raises[0].first == "ValueError", "raises entry");
  expect(doc.sections.size() == 1 && doc.sections[0].title == "Example" &&
             doc.sections[0].code,
         "example section is a code block");
}

void testProseOnlyDocstringHasNoSections() {
  const sere::DocComment doc = sere::parseDocstring("Just a sentence with a colon: here.");
  expect(doc.summary == "Just a sentence with a colon: here.", "unknown title stays prose");
  expect(doc.params.empty() && doc.returns.empty() && doc.sections.empty(),
         "prose-only docstring opens no section");
}

void testRenderCallableMergesSignature() {
  sere::DocSignature signature;
  signature.text = "def serve_beer(name: str, age: i32) -> Result[str, str]";
  signature.params = {"name", "age"};
  signature.types = {"str", "i32"};
  signature.returnType = "Result[str, str]";

  const std::string markdown =
      sere::renderCallableMarkdown(signature, sere::parseDocstring(kServeBeer));
  expectContains(markdown, "```sere", "signature is fenced");
  expectContains(markdown, "def serve_beer(name: str, age: i32) -> Result[str, str]",
                 "signature text is shown");
  // Types come from the signature, so the bullet shows `name (str)` even though
  // the docstring only wrote `name`.
  expectContains(markdown, "- `name (str)` — Who is being served.", "argument bullet keeps type");
  expectContains(markdown, "- `age (i32)` — The guest's age in years.",
                 "argument bullet uses the documented type");
  expectContains(markdown, "**Returns**", "returns section");
  expectContains(markdown, "**Raises**", "raises section");
}

void testRenderCallableKeepsUndocumentedSignatureParam() {
  sere::DocSignature signature;
  signature.text = "def f(a: i32, b: i32) -> void";
  signature.params = {"a", "b"};
  signature.types = {"i32", "i32"};

  const std::string markdown = sere::renderCallableMarkdown(
      signature, sere::parseDocstring("Summary.\n\nArgs:\n    a: first\n"));
  expectContains(markdown, "- `a (i32)` — first", "documented argument");
  expectContains(markdown, "- `b (i32)`", "undocumented signature parameter is still listed");
}

void testRenderDeclarationFencesAndDocuments() {
  const std::string markdown =
      sere::renderDeclarationMarkdown("class Guest:", sere::parseDocstring("A person."));
  expectContains(markdown, "```sere\nclass Guest:\n```", "declaration is fenced");
  expectContains(markdown, "A person.", "declaration prose");
  // A per-member hover must never splice member prose into the fence: the fence
  // has to close before any prose or bullet list begins.
  const std::size_t fenceEnd = markdown.find("```", markdown.find("```") + 3);
  expect(fenceEnd != std::string::npos, "declaration fence closes");
  expect(markdown.find("**Arguments**") == std::string::npos,
         "declaration-only docstring adds no argument list");
}

void testEmptyDocstring() {
  expect(sere::parseDocstring("").empty(), "empty docstring parses empty");
  expect(sere::parseDocstring("   \n  \n").empty(), "blank docstring parses empty");
}

} // namespace

int main() {
  testParseSections();
  testProseOnlyDocstringHasNoSections();
  testRenderCallableMergesSignature();
  testRenderCallableKeepsUndocumentedSignatureParam();
  testRenderDeclarationFencesAndDocuments();
  testEmptyDocstring();

  if (g_failures != 0) {
    std::cerr << g_failures << " docstring check(s) failed\n";
    return 1;
  }
  std::cout << "docstring checks passed\n";
  return 0;
}
