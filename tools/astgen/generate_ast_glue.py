"""Emit the mechanical parts of stdlib/ast.sere.

The public syntax kinds and their dispatch follow the bridge table in
lib/api/AstBridge.cpp one-for-one, so this script prints the blocks that must
stay in step with it:

  * the kind list every tool iterates,
  * NodeVisitor.visit's dispatch chain,
  * the visit_<Kind> stubs a visitor overrides.

Run it from the repository root and paste the output into stdlib/ast.sere:

    py tools/astgen/generate_ast_glue.py
"""

from __future__ import annotations

# Public kind names, in bridge table order (statements first, then expressions,
# then helpers) so dump and generated source read naturally.
STATEMENT_KINDS = [
    "Import", "AnnAssign", "Assign", "AugAssign", "Expr", "Pass", "Break",
    "Continue", "Return", "Yield", "If", "While", "For", "Assert", "Raise",
    "Try", "Match", "Delete", "Defer", "With", "FunctionDef", "ClassDef",
    "EnumDef", "TypeAlias", "MacroDef",
]

EXPRESSION_KINDS = [
    "Constant", "Name", "Attribute", "Subscript", "Call", "BinOp", "UnaryOp",
    "BoolOp", "Compare", "Cast", "Await", "IfExp", "NamedExpr", "Lambda", "Do",
    "List", "Tuple", "Dict", "Comprehension", "JoinedStr", "Splice",
    "MacroInvoke", "Type",
]

HELPER_KINDS = [
    "Param", "Keyword", "IfBranch", "Handler", "MatchCase", "Field", "Variant",
    "PayloadField", "StringPart", "MacroArm",
]

OPERATOR_KINDS = [
    "Add", "Sub", "Mult", "Div", "FloorDiv", "Mod", "Pow", "BitAnd", "BitOr",
    "BitXor", "LShift", "RShift", "MatMult", "And", "Or", "Not", "Invert",
    "USub", "UAdd", "Eq", "NotEq", "Lt", "LtE", "Gt", "GtE", "Is", "IsNot",
    "In", "NotIn", "PreInc", "PreDec", "PostInc", "PostDec", "Deref", "AddrOf",
]

ALL_KINDS = OPERATOR_KINDS + STATEMENT_KINDS + EXPRESSION_KINDS + HELPER_KINDS


def kind_list() -> str:
    lines = ["KIND_NAMES: list[str] = []", "", "", "def _register_kinds() -> void:"]
    lines.append('    """Record every kind a visitor can dispatch on."""')
    for kind in ALL_KINDS:
        lines.append(f'    KIND_NAMES.append("{kind}")')
    lines.append("")
    lines.append("")
    lines.append("_register_kinds()")
    return "\n".join(lines)


def visitor_dispatch(indent: str) -> str:
    lines = []
    for index, kind in enumerate(ALL_KINDS):
        keyword = "if" if index == 0 else "elif"
        lines.append(f'{indent}{keyword} node.kind == "{kind}":')
        lines.append(f"{indent}    self.visit_{kind}(node)")
        lines.append(f"{indent}    return")
    lines.append(f"{indent}self.generic_visit(node)")
    return "\n".join(lines)


def visitor_stubs(indent: str, return_type: str) -> str:
    lines = []
    for kind in ALL_KINDS:
        lines.append(f'{indent}def visit_{kind}(self, node: AST) -> {return_type}:')
        lines.append(f'{indent}    """Visit a {kind} node; override to handle it."""')
        lines.append(f"{indent}    return self.generic_visit(node)")
    return "\n".join(lines)


if __name__ == "__main__":
    print("==== KIND_NAMES ====")
    print(kind_list())
    print()
    print("==== NodeVisitor.visit body ====")
    print(visitor_dispatch("        "))
    print()
    print("==== NodeVisitor stubs ====")
    print(visitor_stubs("    ", "void"))
    print()
    print("==== NodeTransformer stubs ====")
    print(visitor_stubs("    ", "AST"))
