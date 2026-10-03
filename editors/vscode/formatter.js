"use strict";

const OPEN_TO_CLOSE = { "(": ")", "[": "]", "{": "}" };
const OPERATORS = [
  "<<=", ">>=", "**=", "//=", "|=", "&=", "^=", "|>", "==", "!=", "<=", ">=", "->",
  "+=", "-=", "*=", "/=", "%=", "**", "//", "<<", ">>", "|", "&", "^", "+", "-",
  "*", "/", "%", "=", "<", ">",
];

function indentationWidth(line) {
  let width = 0;
  for (const character of line) {
    if (character === " ") width += 1;
    else if (character === "\t") width += 4 - (width % 4);
    else break;
  }
  return width;
}

function scanLine(line, state, indent) {
  let index = 0;
  let code = "";
  while (index < line.length) {
    const character = line[index];
    if (state.quote) {
      if (state.triple && line.startsWith(state.quote.repeat(3), index)) {
        state.quote = "";
        state.triple = false;
        index += 3;
      } else if (!state.triple && character === "\\") {
        index += 2;
      } else if (!state.triple && character === state.quote) {
        state.quote = "";
        index += 1;
      } else {
        index += 1;
      }
      continue;
    }
    if (character === "#") break;
    if (character === "'" || character === '"' || character === "`") {
      state.quote = character;
      state.triple = character !== "`" && line.startsWith(character.repeat(3), index);
      index += state.triple ? 3 : 1;
      continue;
    }
    code += character;
    if (OPEN_TO_CLOSE[character]) {
      state.brackets.push({ character, indent });
    } else if (")] }".includes(character) && character !== " ") {
      const top = state.brackets[state.brackets.length - 1];
      if (top && OPEN_TO_CLOSE[top.character] === character) state.brackets.pop();
    }
    index += 1;
  }
  if (state.quote && !state.triple) state.quote = "";
  return code.trimEnd();
}

function formatLine(line) {
  let result = "";
  let quote = "";
  let triple = false;
  let squareDepth = 0;
  let index = 0;

  while (index < line.length) {
    const character = line[index];
    if (quote) {
      result += character;
      if (triple && line.startsWith(quote.repeat(3), index)) {
        result += line.slice(index + 1, index + 3);
        index += 3;
        quote = "";
        triple = false;
      } else if (!triple && character === "\\") {
        result += line[index + 1] || "";
        index += 2;
      } else {
        if (!triple && character === quote) quote = "";
        index += 1;
      }
      continue;
    }
    if (character === "#") {
      const prefix = result.trimEnd();
      return (prefix && !prefix.endsWith(" ") ? prefix + " " : prefix) + line.slice(index).trimEnd();
    }
    if (character === "'" || character === '"' || character === "`") {
      quote = character;
      triple = character !== "`" && line.startsWith(character.repeat(3), index);
      result += triple ? line.slice(index, index + 3) : character;
      index += triple ? 3 : 1;
      continue;
    }
    if (character === "[" || character === "(" || character === "{") {
      result = result.trimEnd() + character;
      if (character === "[") squareDepth += 1;
      index += 1;
      while (line[index] === " " || line[index] === "\t") index += 1;
      continue;
    }
    if (character === "]" || character === ")" || character === "}") {
      result = result.trimEnd() + character;
      if (character === "]") squareDepth = Math.max(0, squareDepth - 1);
      index += 1;
      continue;
    }
    if (character === ".") {
      result = result.trimEnd() + character;
      index += 1;
      while (line[index] === " " || line[index] === "\t") index += 1;
      continue;
    }
    if (character === "," || character === ":") {
      result = result.trimEnd() + character;
      index += 1;
      while (line[index] === " " || line[index] === "\t") index += 1;
      const next = line[index];
      if ((character === "," || squareDepth === 0) && next && !"#)]}".includes(next)) result += " ";
      continue;
    }
    if (character === " " || character === "\t") {
      if (result.length > 0 && result[result.length - 1] !== " ") result += " ";
      index += 1;
      continue;
    }

    const operator = OPERATORS.find((candidate) => line.startsWith(candidate, index));
    if (!operator) {
      result += character;
      index += 1;
      continue;
    }
    const trimmed = result.trimEnd();
    const previous = trimmed.slice(-1);
    const previousWord = /([A-Za-z_]\w*)$/.exec(trimmed);
    const separatedUnary = /\s$/.test(result) && previousWord && ["return", "raise", "yield"].includes(previousWord[1]);
    const unary = ["+", "-", "*", "&"].includes(operator) &&
      (!previous || "([{,:=+-*/%|&^".includes(previous) || separatedUnary);
    const leadingWordOperator = ["+", "-", "*", "&"].includes(operator) &&
      (!previous || /[A-Za-z_]/.test(previous)) && !separatedUnary;
    if (operator === "->") result = result.trimEnd() + " -> ";
    else if (unary && separatedUnary) result = trimmed + " " + operator;
    else if (unary && /\\s$/.test(result)) result += operator;
    else if (unary) result = result.trimEnd() + operator;
    else if (leadingWordOperator) result = result.trimEnd() + " " + operator + " ";
    else result = result.trimEnd() + " " + operator + " ";
    index += operator.length;
    while (line[index] === " " || line[index] === "\t") index += 1;
  }
  return result.trimEnd();
}

function formatSere(text) {
  const newline = text.includes("\r\n") ? "\r\n" : "\n";
  const sourceLines = text.replace(/\r\n/g, "\n").split("\n");
  const state = { quote: "", triple: false, brackets: [] };
  const indentColumns = [0];
  const output = [];
  let pendingSuite = false;
  let suiteIndent = 0;

  for (const sourceLine of sourceLines) {
    if (sourceLine.trim().length === 0 && !state.triple) {
      output.push({ text: "", literal: false });
      continue;
    }

    const wasInTriple = state.triple;
    const continuation = state.brackets.length > 0;
    const sourceIndent = indentationWidth(sourceLine);
    const content = sourceLine.trimStart();
    let indent;

    if (wasInTriple) {
      indent = 0;
    } else if (continuation) {
      const openerIndent = state.brackets[state.brackets.length - 1].indent;
      indent = /^[)\]}]/.test(content) ? openerIndent : openerIndent + 4;
    } else {
      if (pendingSuite) {
        const parent = indentColumns[indentColumns.length - 1];
        indentColumns.push(sourceIndent > suiteIndent ? sourceIndent : parent + 1);
        pendingSuite = false;
      } else {
        while (indentColumns.length > 1 && sourceIndent < indentColumns[indentColumns.length - 1]) {
          indentColumns.pop();
        }
      }
      indent = (indentColumns.length - 1) * 4;
    }

    const code = scanLine(sourceLine, state, indent);
    const formatted = wasInTriple ? sourceLine : " ".repeat(indent) + formatLine(content);
    output.push({ text: formatted, literal: wasInTriple });
    if (!wasInTriple && !continuation && state.brackets.length === 0 && code.endsWith(":")) {
      pendingSuite = true;
      suiteIndent = sourceIndent;
    }
  }

  const spaced = [];
  for (const item of output) {
    const line = item.text;
    if (line.trim().length === 0) {
      if (item.literal) spaced.push(item);
      else if (spaced.length > 0 && spaced[spaced.length - 1].text !== "") spaced.push(item);
      continue;
    }
    if (/^\s*(def|class|struct|enum|macro)\b/.test(line)) {
      let previous = spaced.length - 1;
      while (previous >= 0 && spaced[previous].text === "") previous -= 1;
      const prior = previous >= 0 ? spaced[previous].text.trimStart() : "";
      const topLevel = indentationWidth(line) === 0;
      const sameBlock = previous >= 0 && indentationWidth(spaced[previous].text) === indentationWidth(line);
      if (previous >= 0 && !prior.startsWith("@") && !prior.startsWith("#") && (topLevel || sameBlock)) {
        const blankLines = topLevel ? 2 : 1;
        while (spaced.length - previous - 1 < blankLines) spaced.push({ text: "", literal: false });
      }
    }
    spaced.push(item);
  }

  while (spaced.length > 0 && spaced[spaced.length - 1].text === "") spaced.pop();
  return spaced.length > 0 ? spaced.map((item) => item.text).join(newline) + newline : "";
}

module.exports = { formatSere };
