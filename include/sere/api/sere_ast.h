/// @file sere_ast.h
/// C ABI for parsing Sere source into an inspectable syntax tree.
///
/// The stdlib `ast` module wraps this API: a program that imports `ast` links
/// `sere_front`, which carries the compiler's lexer, parser, and syntax AST.
/// Handles are read-only; `ast.sere` builds mutable Sere nodes from them, which
/// is where traversal, transformation, and unparsing happen.
///
/// Ownership: every handle belongs to the tree it came from and stays valid
/// until `sere_ast_free`. Handles are never freed individually.
///
/// Strings are different: they are interned for the life of the process, so a
/// caller may free the tree and keep using the text it read out of it. That is
/// what a Sere `str` needs — the code generator turns the trailing
/// `out_data`/`out_len` parameters into a pointer plus a length and copies
/// nothing, so the characters have to outlive the call by as long as the program
/// holds the string. Each distinct word is stored once, which also makes a
/// repeated identifier or field name share a single copy.
///
/// Conventions match the rest of the runtime: text comes back as a pointer plus
/// a length, so a Sere `str` return maps directly onto the trailing out
/// parameters and the code that generates the FFI builds the string for us.

#ifndef SERE_API_AST_H
#define SERE_API_AST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SereAstTree SereAstTree;
typedef struct SereAstNode SereAstNode;

/// Kinds of value a field holds. The numbering is part of the ABI: `ast.sere`
/// maps each kind to the accessor it calls.
enum {
  /// The field has no value at all, as in a `pass` statement.
  SERE_AST_VALUE_NONE = 0,
  /// A single child node; a null handle means `None`.
  SERE_AST_VALUE_NODE = 1,
  /// A list of child nodes.
  SERE_AST_VALUE_NODES = 2,
  /// Text: a name, identifier, field, module path, docstring, or operator word.
  SERE_AST_VALUE_TEXT = 3,
  /// A 64-bit integer (a literal's value).
  SERE_AST_VALUE_INT = 4,
  /// A double (a float literal's value).
  SERE_AST_VALUE_FLOAT = 5,
  /// A boolean flag.
  SERE_AST_VALUE_BOOL = 6,
  /// An operator or modifier keyword such as `add`, `not`, or `vararg`.
  SERE_AST_VALUE_TOKEN = 7,
  /// A list of strings (decorator names, imported names, module path parts).
  SERE_AST_VALUE_TEXTS = 8,
};

/// Node categories, so tooling can tell statements from expressions.
enum {
  SERE_AST_CATEGORY_MODULE = 0,
  SERE_AST_CATEGORY_STMT = 1,
  SERE_AST_CATEGORY_EXPR = 2,
  SERE_AST_CATEGORY_TYPE = 3,
  /// Syntax that is not itself a node: a parameter, case arm, handler, keyword
  /// argument, interpolated part, class field, or enum variant.
  SERE_AST_CATEGORY_HELPER = 4,
};

/// Parse modes.
enum {
  SERE_AST_MODE_MODULE = 0,
  SERE_AST_MODE_EXPRESSION = 1,
};

/// Parse `source` (not null-terminated; `length` bytes of UTF-8) into a tree.
///
/// Returns null when the source does not parse. The rendered diagnostics are
/// then available from `sere_ast_last_error` until the next parse. `filename`
/// is used for diagnostics only and may be null or empty.
SereAstTree* sere_ast_parse(const char* source, int64_t length, const char* filename, int32_t mode);

/// Free a tree and every handle it handed out.
void sere_ast_free(SereAstTree* tree);

/// Diagnostics of the most recent failed parse, owned by the bridge.
void sere_ast_last_error(const char** out_data, int64_t* out_len);

/// The root node: the module, or the single expression of an expression parse.
const SereAstNode* sere_ast_root(const SereAstTree* tree);

/// The source text the tree was parsed from.
void sere_ast_source(const SereAstTree* tree, const char** out_data, int64_t* out_len);

/// The filename the tree was parsed with.
void sere_ast_filename(const SereAstTree* tree, const char** out_data, int64_t* out_len);

// --- node introspection ----------------------------------------------------

/// Public kind name: "Module", "FunctionDef", "BinOp", "Constant", ...
void sere_ast_kind(const SereAstNode* node, const char** out_data, int64_t* out_len);

/// Native `NodeKind` ordinal, stable for one compiler build. `ast.sere` uses it
/// to recognise the literal flavors the parse produced.
int32_t sere_ast_kind_id(const SereAstNode* node);

/// One of the `SERE_AST_CATEGORY_*` values.
int32_t sere_ast_category(const SereAstNode* node);

/// Number of inspectable fields.
int64_t sere_ast_field_count(const SereAstNode* node);

/// Name of the field at `index`; empty when the index is out of range.
void sere_ast_field_name(const SereAstNode* node, int64_t index, const char** out_data,
                         int64_t* out_len);

/// One of the `SERE_AST_VALUE_*` values, or NONE for an out-of-range index.
int32_t sere_ast_field_kind(const SereAstNode* node, int64_t index);

/// Index of the named field, or -1 when the kind has no such field.
int64_t sere_ast_field_index(const SereAstNode* node, const char* name, int64_t length);

// --- field values ----------------------------------------------------------

/// A NODE field. Returns null when the field is `None`.
const SereAstNode* sere_ast_field_node(const SereAstNode* node, int64_t index);

/// Length of a NODES field.
int64_t sere_ast_field_node_count(const SereAstNode* node, int64_t index);

/// One element of a NODES field.
const SereAstNode* sere_ast_field_node_at(const SereAstNode* node, int64_t index, int64_t item);

/// A TEXT or TOKEN field; empty for any other kind.
void sere_ast_field_text(const SereAstNode* node, int64_t index, const char** out_data,
                         int64_t* out_len);

/// Length of a TEXTS field.
int64_t sere_ast_field_text_count(const SereAstNode* node, int64_t index);

/// One element of a TEXTS field.
void sere_ast_field_text_at(const SereAstNode* node, int64_t index, int64_t item,
                            const char** out_data, int64_t* out_len);

/// An INT field.
int64_t sere_ast_field_int(const SereAstNode* node, int64_t index);

/// A BOOL field.
int32_t sere_ast_field_bool(const SereAstNode* node, int64_t index);

/// A FLOAT field.
double sere_ast_field_float(const SereAstNode* node, int64_t index);

/// Source range: 1-based lines and 0-based columns, as diagnostics report them.
int64_t sere_ast_range_start_line(const SereAstNode* node);
int64_t sere_ast_range_start_column(const SereAstNode* node);
int64_t sere_ast_range_end_line(const SereAstNode* node);
int64_t sere_ast_range_end_column(const SereAstNode* node);

/// How many kinds the bridge knows, for validating manually built nodes.
int64_t sere_ast_kind_count(void);
void sere_ast_kind_name_at(int64_t index, const char** out_data, int64_t* out_len);

/// The field names a kind declares, so `ast.sere` can check a node it built.
int64_t sere_ast_field_count_of_kind(const char* kind_name, int64_t length);
void sere_ast_field_name_of_kind(const char* kind_name, int64_t length, int64_t index,
                                 const char** out_data, int64_t* out_len);

/// Category of a kind by name, or SERE_AST_CATEGORY_HELPER for an unknown kind.
int32_t sere_ast_kind_category(const char* kind_name, int64_t length);

#ifdef __cplusplus
}
#endif

#endif // SERE_API_AST_H
