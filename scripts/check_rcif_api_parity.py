"""Compare the HTTP route surface of the 3088 Python backend and the 3090 plugin.

Usage: python scripts/check_rcif_api_parity.py
Exit code is 0 always; this reports drift rather than failing a build.
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
python_file = ROOT / "rcif-tools" / "workbench_server.py"
node_file = ROOT / "dsh-rcif" / "dist" / "index.js"

py_routes = {
    (method.upper(), path.removeprefix("/rcif-studio/api"))
    for method, path in re.findall(r'@app\.(get|post|put|delete)\("([^"]+)"', python_file.read_text(encoding="utf-8"))
}
node_routes = set()
for kind, path in re.findall(r"route\('(exact|prefix)', `\$\{API_PREFIX\}([^`]+)`", node_file.read_text(encoding="utf-8")):
    node_routes.add(("GET/POST" if kind == "prefix" else "ANY", path))

py_paths = {path for _, path in py_routes}
node_paths = {path for _, path in node_routes}

print(f"3088 routes: {len(py_routes)}")
print(f"3090 routes: {len(node_routes)}")
print()
print("only in 3088 (Python):")
for path in sorted(py_paths - node_paths):
    methods = ",".join(sorted(m for m, p in py_routes if p == path))
    print(f"  {methods:18} {path}")
print()
print("only in 3090 (plugin):")
for path in sorted(node_paths - py_paths):
    print(f"  {'ANY':18} {path}")
print()
print("shared paths:", len(py_paths & node_paths))
for path in sorted(py_paths & node_paths):
    py_methods = ",".join(sorted(m for m, p in py_routes if p == path))
    print(f"  {py_methods:18} {path}")
