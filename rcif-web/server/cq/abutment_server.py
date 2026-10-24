"""Persistent CadQuery worker used by the web dev server.

Protocol: one JSON object per line on stdin, one JSON object per line on stdout.
Keeping this process alive avoids re-importing CadQuery/OCP for every slider
change or preset switch.
"""

from __future__ import annotations

import json
import sys
import traceback

import cadquery  # noqa: F401  (paid once for the process lifetime)

from abutment_worker import build_payload


def main() -> int:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    for line in sys.stdin:
        if not line.strip():
            continue
        try:
            payload = json.loads(line)
            result = build_payload(payload)
            sys.stdout.write(json.dumps(result, ensure_ascii=False) + "\n")
        except Exception as error:  # return the failure to the Node caller
            sys.stdout.write(
                json.dumps(
                    {"error": str(error), "traceback": traceback.format_exc(limit=3)},
                    ensure_ascii=False,
                )
                + "\n"
            )
        sys.stdout.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
