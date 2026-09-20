/// @file DocFormat.cpp
/// Docstring parsing and Markdown rendering for hover, completion, and
/// signature help.

#include "sere/lsp/DocFormat.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace sere {

namespace {

/// Separates a documented name from its description.
constexpr const char* kDescriptionDash = " — ";

[[nodiscard]] std::string lowerTitle(std::string_view text) {
  std::string lowered;
  lowered.reserve(text.size());
  for (const char ch : text) {
    lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
  }
  return lowered;
}

[[nodiscard]] std::string trim(std::string_view text) {
  std::size_t begin = 0;
  std::size_t end = text.size();
  while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }
  while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return std::string(text.substr(begin, end - begin));
}

/// The kinds of section a docstring may open.
enum class SectionKind { None, Params, Returns, Raises, Code, Prose };

/// Titles a section may use. Keeping the vocabulary closed means a sentence that
/// happens to contain a colon stays prose, and the set is what the language
/// reference documents.
[[nodiscard]] SectionKind sectionKind(std::string_view title) {
  if (title == "arg" || title == "args" || title == "argument" || title == "arguments" ||
      title == "param" || title == "params" || title == "parameter" ||
      title == "parameters") {
    return SectionKind::Params;
  }
  if (title == "return" || title == "returns" || title == "result") {
    return SectionKind::Returns;
  }
  if (title == "raise" || title == "raises" || title == "throw" || title == "throws") {
    return SectionKind::Raises;
  }
  if (title == "code" || title == "example" || title == "examples" || title == "usage") {
    return SectionKind::Code;
  }
  if (title == "note" || title == "notes" || title == "warning" || title == "warnings" ||
      title == "attention" || title == "tip" || title == "tips" || title == "yields" ||
      title == "attribute" || title == "attributes" || title == "todo" ||
      title == "deprecated" || title == "since" || title == "references" ||
      title == "see also" || title == "seealso") {
    return SectionKind::Prose;
  }
  return SectionKind::None;
}

/// `Title: rest` on an unindented line of its own starts a section. The title must
/// come from the documented vocabulary and be typed as one to three words, so a
/// sentence that happens to contain a colon stays prose.
[[nodiscard]] bool sectionHeader(const std::string& line, std::string& title, std::string& rest) {
  if (line.empty() || std::isspace(static_cast<unsigned char>(line.front()))) {
    return false;
  }
  const std::size_t colon = line.find(':');
  if (colon == std::string::npos) {
    return false;
  }
  const std::string name = line.substr(0, colon);
  if (name.empty() || name.size() > 16) {
    return false;
  }
  for (const char ch : name) {
    if (!std::isalpha(static_cast<unsigned char>(ch)) && ch != ' ') {
      return false;
    }
  }
  if (sectionKind(lowerTitle(name)) == SectionKind::None) {
    return false;
  }
  title = name;
  rest = trim(std::string_view(line).substr(colon + 1));
  return true;
}

/// Splits a raw docstring into lines with the indentation it inherits from its
/// own line removed, so a section header is the first thing on its line and the
/// entries of that section sit one level under it. The first line follows the
/// opening quotes and carries no indentation of its own, so only the lines after
/// it share the body's indentation.
[[nodiscard]] std::vector<std::string> dedent(std::string_view text) {
  std::vector<std::string> lines;
  std::string current;
  for (const char ch : text) {
    if (ch == '\r') {
      continue;
    }
    if (ch == '\n') {
      lines.push_back(current);
      current.clear();
      continue;
    }
    current.push_back(ch);
  }
  lines.push_back(current);
  lines.front() = trim(lines.front());
  std::size_t indent = std::string::npos;
  for (std::size_t index = 1; index < lines.size(); ++index) {
    const std::size_t first = lines[index].find_first_not_of(" \t");
    if (first == std::string::npos) {
      continue;
    }
    indent = std::min(indent, first);
  }
  if (indent == std::string::npos || indent == 0) {
    return lines;
  }
  for (std::size_t index = 1; index < lines.size(); ++index) {
    std::string& line = lines[index];
    line = line.size() <= indent ? std::string{} : line.substr(indent);
  }
  return lines;
}

/// A documented entry is `name: text`, with the type optionally parenthesised
/// after the name. A line that names nothing continues the previous entry.
[[nodiscard]] bool entryLine(const std::string& line, std::string& name, std::string& text) {
  const std::string trimmed = trim(line);
  const std::size_t colon = trimmed.find(':');
  if (colon == std::string::npos || colon == 0) {
    return false;
  }
  std::string label = trimmed.substr(0, colon);
  const std::size_t paren = label.find('(');
  if (paren != std::string::npos) {
    label = label.substr(0, paren);
  }
  label = trim(label);
  if (label.empty() || label.find(' ') != std::string::npos) {
    return false;
  }
  name = label;
  text = trim(std::string_view(trimmed).substr(colon + 1));
  return true;
}

/// Splits the entries of an `Args`-style section, folding the indented
/// continuation lines of one entry into its description.
void collectEntries(const std::vector<std::string>& lines,
                    std::vector<std::pair<std::string, std::string>>& entries) {
  for (const std::string& line : lines) {
    std::string name;
    std::string text;
    if (entryLine(line, name, text)) {
      entries.emplace_back(std::move(name), std::move(text));
      continue;
    }
    if (entries.empty()) {
      continue;
    }
    const std::string trimmed = trim(line);
    if (trimmed.empty()) {
      continue;
    }
    std::string& description = entries.back().second;
    if (!description.empty()) {
      description += " ";
    }
    description += trimmed;
  }
}

/// Joins the paragraphs of a section body, keeping the blank line between them
/// and folding the wrapped lines of each paragraph into one.
[[nodiscard]] std::string joinParagraphs(const std::vector<std::string>& lines) {
  std::string text;
  bool paragraphStart = true;
  for (const std::string& line : lines) {
    const std::string trimmed = trim(line);
    if (trimmed.empty()) {
      paragraphStart = true;
      continue;
    }
    if (paragraphStart) {
      if (!text.empty()) {
        text += "\n\n";
      }
      text += trimmed;
      paragraphStart = false;
      continue;
    }
    text += " " + trimmed;
  }
  return text;
}

[[nodiscard]] std::string joinLines(const std::vector<std::string>& lines) {
  std::string text;
  for (const std::string& line : lines) {
    if (!text.empty()) {
      text += "\n";
    }
    text += line;
  }
  return text;
}

/// A code section is indented under its header, which the fence already marks, so
/// the block's shared indentation is removed and its blank edges dropped.
[[nodiscard]] std::string joinCodeLines(std::vector<std::string> lines) {
  while (!lines.empty() && trim(lines.front()).empty()) {
    lines.erase(lines.begin());
  }
  while (!lines.empty() && trim(lines.back()).empty()) {
    lines.pop_back();
  }
  std::size_t indent = std::string::npos;
  for (const std::string& line : lines) {
    const std::size_t first = line.find_first_not_of(" \t");
    if (first == std::string::npos) {
      continue;
    }
    indent = indent == std::string::npos ? first : std::min(indent, first);
  }
  if (indent != std::string::npos && indent != 0) {
    for (std::string& line : lines) {
      line = line.size() <= indent ? std::string{} : line.substr(indent);
    }
  }
  return joinLines(lines);
}

/// A bullet list of `name — description` entries.
[[nodiscard]] std::string entryList(
    const std::vector<std::pair<std::string, std::string>>& entries) {
  std::string text;
  for (const auto& [name, description] : entries) {
    text += "\n- `";
    text += name;
    text += "`";
    if (!description.empty()) {
      text += kDescriptionDash;
      text += description;
    }
  }
  return text;
}

void appendSection(std::string& text, std::string_view title) {
  if (!text.empty()) {
    text += "\n\n";
  }
  text += "**";
  text += title;
  text += "**";
}

/// The description, returns, raises, and the author's own sections, in the order
/// a hover reads them. The argument list is not part of this: a callable lists
/// the parameters its signature declares, and a plain declaration lists the ones
/// its docstring documents.
[[nodiscard]] std::string proseBlocks(const DocComment& doc) {
  std::string text;
  if (!doc.summary.empty()) {
    text = doc.summary;
  }
  if (!doc.body.empty()) {
    if (!text.empty()) {
      text += "\n\n";
    }
    text += doc.body;
  }
  if (!doc.returns.empty()) {
    appendSection(text, "Returns");
    text += ": ";
    text += doc.returns;
  }
  if (!doc.raises.empty()) {
    appendSection(text, "Raises");
    text += entryList(doc.raises);
  }
  for (const DocSection& section : doc.sections) {
    appendSection(text, section.title);
    if (section.text.empty()) {
      continue;
    }
    text += "\n";
    if (section.code) {
      text += "```sere\n";
      text += section.text;
      text += "\n```";
    } else {
      text += section.text;
    }
  }
  return text;
}

} // namespace

bool DocComment::empty() const {
  return summary.empty() && body.empty() && params.empty() && returns.empty() && raises.empty() &&
         sections.empty();
}

DocComment parseDocstring(std::string_view docstring) {
  DocComment comment;
  struct Block {
    std::string title;
    std::string rest;
    std::vector<std::string> lines;
  };
  // Everything before the first section header is prose: its first paragraph is
  // the summary, the paragraphs after it are the body.
  std::vector<std::string> prose;
  std::vector<Block> blocks;
  for (const std::string& line : dedent(docstring)) {
    std::string title;
    std::string rest;
    if (sectionHeader(line, title, rest)) {
      Block block;
      block.title = std::move(title);
      block.rest = std::move(rest);
      blocks.push_back(std::move(block));
      continue;
    }
    if (blocks.empty()) {
      prose.push_back(line);
    } else {
      blocks.back().lines.push_back(line);
    }
  }
  bool paragraphStart = true;
  for (const std::string& line : prose) {
    const std::string trimmed = trim(line);
    if (trimmed.empty()) {
      paragraphStart = true;
      continue;
    }
    if (paragraphStart) {
      if (comment.summary.empty()) {
        comment.summary = trimmed;
      } else {
        if (!comment.body.empty()) {
          comment.body += "\n\n";
        }
        comment.body += trimmed;
      }
      paragraphStart = false;
      continue;
    }
    std::string& target = comment.body.empty() ? comment.summary : comment.body;
    target += " " + trimmed;
  }
  for (const Block& block : blocks) {
    const std::string lowered = lowerTitle(block.title);
    std::vector<std::string> body = block.lines;
    if (!block.rest.empty()) {
      body.insert(body.begin(), block.rest);
    }
    switch (sectionKind(lowered)) {
    case SectionKind::Params:
      collectEntries(body, comment.params);
      break;
    case SectionKind::Returns:
      comment.returns = joinParagraphs(body);
      break;
    case SectionKind::Raises:
      collectEntries(body, comment.raises);
      break;
    case SectionKind::Code:
    case SectionKind::Prose: {
      DocSection section;
      section.title = block.title;
      section.code = sectionKind(lowered) == SectionKind::Code;
      section.text = section.code ? joinCodeLines(body) : joinParagraphs(body);
      comment.sections.push_back(std::move(section));
      break;
    }
    case SectionKind::None:
      break;
    }
  }
  return comment;
}

std::string renderDocMarkdown(const DocComment& doc) {
  if (doc.params.empty()) {
    return proseBlocks(doc);
  }
  // A declaration without a signature still lists its arguments, ahead of the
  // returns and raises prose. Arguments come first here because the caller has no
  // parameter list of its own to show them in.
  std::string text;
  if (!doc.summary.empty()) {
    text = doc.summary;
  }
  if (!doc.body.empty()) {
    if (!text.empty()) {
      text += "\n\n";
    }
    text += doc.body;
  }
  appendSection(text, "Arguments");
  text += entryList(doc.params);
  if (!doc.returns.empty()) {
    appendSection(text, "Returns");
    text += ": " + doc.returns;
  }
  if (!doc.raises.empty()) {
    appendSection(text, "Raises");
    text += entryList(doc.raises);
  }
  for (const DocSection& section : doc.sections) {
    appendSection(text, section.title);
    if (section.text.empty()) {
      continue;
    }
    text += "\n";
    if (section.code) {
      text += "```sere\n" + section.text + "\n```";
    } else {
      text += section.text;
    }
  }
  return text;
}

std::string renderCallableMarkdown(const DocSignature& signature, const DocComment& doc) {
  std::vector<std::pair<std::string, std::string>> entries;
  for (std::size_t index = 0; index < signature.params.size(); ++index) {
    const std::string& name = signature.params[index];
    const std::string& type = index < signature.types.size() ? signature.types[index] : "";
    std::string description;
    for (const auto& [documented, text] : doc.params) {
      if (documented == name) {
        description = text;
        break;
      }
    }
    std::string label = name;
    if (!type.empty()) {
      label += " (" + type + ")";
    }
    entries.emplace_back(std::move(label), std::move(description));
  }
  // A docstring may document a parameter the signature no longer takes, and the
  // text still belongs in the hover rather than being dropped.
  for (const auto& [name, text] : doc.params) {
    const bool known = std::any_of(signature.params.begin(), signature.params.end(),
                                   [&](const std::string& param) { return param == name; });
    if (!known) {
      entries.emplace_back(name, text);
    }
  }
  std::string text;
  if (!signature.text.empty()) {
    text = "```sere\n" + signature.text + "\n```";
  }
  const std::string prose = proseBlocks(doc);
  if (!prose.empty()) {
    if (!text.empty()) {
      text += "\n\n";
    }
    text += prose;
  }
  if (!entries.empty()) {
    appendSection(text, "Arguments");
    text += entryList(entries);
  }
  return text;
}

std::string renderDeclarationMarkdown(std::string_view declaration, const DocComment& doc) {
  std::string text;
  if (!declaration.empty()) {
    text = "```sere\n";
    text.append(declaration);
    text += "\n```";
  }
  const std::string described = renderDocMarkdown(doc);
  if (described.empty()) {
    return text;
  }
  if (!text.empty()) {
    text += "\n\n";
  }
  text += described;
  return text;
}

} // namespace sere
