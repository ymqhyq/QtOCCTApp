"""One-shot CadQuery STEP exporter for the web download endpoint."""

from __future__ import annotations

import json
import sys
import tempfile
from pathlib import Path

import cadquery as cq

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from abutment_worker import load_generator


def main() -> int:
    payload = json.loads(sys.stdin.read() or "{}")
    params = payload.get("params") or {}
    generator = load_generator(str(payload.get("generatorPath") or ""))
    builder = getattr(generator, "build_abutment", None) or getattr(generator, "build", None)
    if builder is None:
        raise RuntimeError("生成器没有 build_abutment/build 入口")
    shape = builder(params)
    with tempfile.NamedTemporaryFile(suffix=".step", delete=False) as handle:
        output = Path(handle.name)
    try:
        cq.exporters.export(shape, str(output))
        sys.stdout.buffer.write(output.read_bytes())
    finally:
        output.unlink(missing_ok=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
