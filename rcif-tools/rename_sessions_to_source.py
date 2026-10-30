"""Rename every workbench conversation title to its source file stem.

Dry run by default; pass --apply to write changes.
"""

from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ASSISTANT_ROOT = ROOT / "铁路构件智能助手"


def file_stem(value: str) -> str:
    name = str(value or "").replace("\\", "/").split("/")[-1]
    stem, ext = os.path.splitext(name)
    return stem.strip() if ext else name.strip()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()

    changes = []
    skipped = []
    for meta_path in sorted(ASSISTANT_ROOT.glob("*/.rcif/sessions/*.json")):
        try:
            data = json.loads(meta_path.read_text(encoding="utf-8"))
        except (OSError, ValueError) as error:
            skipped.append((meta_path, f"unreadable: {error}"))
            continue
        workspace = meta_path.parent.parent.parent.name
        old_title = str(data.get("title") or "")
        stem = file_stem(data.get("inputFile") or "")
        if not stem:
            skipped.append((meta_path, "no inputFile"))
            continue
        if old_title == stem:
            skipped.append((meta_path, "already named after source file"))
            continue
        data["title"] = stem
        changes.append((meta_path, workspace, old_title, stem))
        if args.apply:
            temp = meta_path.with_suffix(meta_path.suffix + ".tmp")
            temp.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")
            os.replace(temp, meta_path)

    print(f"{'APPLY' if args.apply else 'DRY-RUN'}: {len(changes)} change(s), {len(skipped)} skipped")
    for meta_path, workspace, old_title, stem in changes:
        print(f"  [{workspace}] {meta_path.name}: {old_title!r} -> {stem!r}")
    for meta_path, reason in skipped:
        print(f"  skip {meta_path.parent.parent.parent.name}/{meta_path.name}: {reason}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
