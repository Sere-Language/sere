/// @file DocFormat.h
/// Parses declaration docstrings and renders them as Markdown for the editor.
///
/// A docstring is the string literal that opens a function, class, or enum body.
/// The format is Google-style: a summary, free prose, then sections whose entries
/// are indented under a `Name:` header.
///
/// ```sere
/// def serve_beer(name: str, age: i32) -> Result[str, str]:
///     """Serve a beer when the guest is old enough.
///
///     Longer prose follows the summary paragraph.
///
///     Args:
///         name: Who is being served.
///         age: The guest's age in years.
///
///     Returns:
///         Ok with a message when served, Err with the reason otherwise.
///
///     Raises:
///         ValueError: When age is negative.
///
///     Example:
///         print(serve_beer("Ada", 30))
///     """
/// ```
///
/// `Args`, `Returns`, and `Raises` are recognised and rendered as their own
/// sections; any other `Title:` header keeps its title and text, so a docstring
/// can add `Notes:` or `See also:` without the language server knowing it.

#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sere {

/// A callable's signature, split so the parameters a docstring documents can be
/// listed with the types the declaration actually takes.
struct DocSignature {
  /// Signature line shown in a fenced block, e.g.
  /// `def serve_beer(name: str, age: i32) -> Result[str, str]`.
  std::string text;
  /// Parameter names, in declaration order.
  std::vector<std::string> params;
  /// Parameter types, parallel to `params`; an entry may be empty.
  std::vector<std::string> types;
  /// Declared result type, empty when the callable returns nothing.
  std::string returnType;
};

/// One section of a docstring that is not `Args`, `Returns`, or `Raises`.
struct DocSection {
  std::string title;
  std::string text;
  /// Rendered as a code block rather than as prose.
  bool code = false;
};

/// A parsed docstring.
struct DocComment {
  /// First paragraph, shown on its own under the signature.
  std::string summary;
  /// Prose between the summary and the first section.
  std::string body;
  /// `name -> description` from `Args`, in the order the docstring lists them.
  std::vector<std::pair<std::string, std::string>> params;
  /// Text after `Returns`.
  std::string returns;
  /// `error -> description` from `Raises`.
  std::vector<std::pair<std::string, std::string>> raises;
  /// Every other `Title:` section, in the order they appear.
  std::vector<DocSection> sections;

  [[nodiscard]] bool empty() const;
};

/// Splits a raw docstring into its summary, sections, and entries. A string with
/// no sections yields a summary and body only, which is what an undocumented
/// declaration's prose collapses to.
[[nodiscard]] DocComment parseDocstring(std::string_view docstring);

/// Markdown for a documented callable: the signature in a `sere` fence, then the
/// description, arguments, returns, raises, and any other section.
[[nodiscard]] std::string renderCallableMarkdown(const DocSignature& signature,
                                                 const DocComment& doc);

/// Markdown for a value or a type: the declaration line, then the docstring's
/// prose and sections. `declaration` is fenced like a signature.
[[nodiscard]] std::string renderDeclarationMarkdown(std::string_view declaration,
                                                    const DocComment& doc);

/// Markdown for a docstring alone, for completion items whose `detail` already
/// shows the declaration.
[[nodiscard]] std::string renderDocMarkdown(const DocComment& doc);

} // namespace sere
