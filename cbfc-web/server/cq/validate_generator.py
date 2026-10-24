"""AST safety gate for uploaded CadQuery generators.

This is not a sandbox. It rejects the obvious network/process/filesystem
execution surface before a generator is written and executed by the worker.
Production deployments must additionally run the worker in a container with
no network and a read-only filesystem.
"""

from __future__ import annotations

import ast
import json
import sys

ALLOWED_IMPORTS = {
    "__future__",
    "cadquery",
    "collections",
    "dataclasses",
    "enum",
    "math",
    "typing",
    "ocp_vscode",
}
FORBIDDEN_NAMES = {
    "__import__",
    "breakpoint",
    "compile",
    "eval",
    "exec",
    "getattr",
    "globals",
    "input",
    "locals",
    "open",
    "setattr",
    "delattr",
}
FORBIDDEN_ATTRS = {"__subclasses__", "__globals__", "__builtins__", "__code__"}


def validate(source: str) -> list[str]:
    errors: list[str] = []
    if len(source.encode("utf-8")) > 256_000:
        return ["generator.py 超过 256 KB 限制"]
    try:
        tree = ast.parse(source)
    except SyntaxError as error:
        return [f"Python 语法错误: {error.msg} (line {error.lineno})"]
    for node in ast.walk(tree):
        if isinstance(node, (ast.Import, ast.ImportFrom)):
            names = [alias.name.split(".")[0] for alias in node.names]
            if isinstance(node, ast.ImportFrom) and node.module:
                names = [node.module.split(".")[0]]
            for name in names:
                if name not in ALLOWED_IMPORTS:
                    errors.append(f"不允许的导入: {name} (line {node.lineno})")
        elif isinstance(node, ast.Call):
            if isinstance(node.func, ast.Name) and node.func.id in FORBIDDEN_NAMES:
                errors.append(f"不允许的调用: {node.func.id} (line {node.lineno})")
            if isinstance(node.func, ast.Attribute) and node.func.attr in FORBIDDEN_ATTRS:
                errors.append(f"不允许的属性访问: {node.func.attr} (line {node.lineno})")
        elif isinstance(node, ast.Attribute):
            if node.attr in FORBIDDEN_ATTRS:
                errors.append(f"不允许的属性访问: {node.attr} (line {node.lineno})")
    return sorted(set(errors))


def main() -> int:
    source = sys.stdin.read()
    errors = validate(source)
    sys.stdout.write(json.dumps({"ok": not errors, "errors": errors}, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
