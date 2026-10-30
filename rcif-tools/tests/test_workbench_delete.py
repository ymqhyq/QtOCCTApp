from __future__ import annotations

import asyncio
import json
import shutil
import unittest
import uuid
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "rcif-tools"))

from fastapi import HTTPException  # noqa: E402

import workbench_server as ws  # noqa: E402


class FakeRequest:
    def __init__(self, body):
        self._body = body

    async def json(self):
        return self._body


class DeleteRouteTest(unittest.TestCase):
    """调试后端也必须支持工作台的“删除会话 / 删除工作区”。"""

    def setUp(self) -> None:
        self.root = Path(__file__).resolve().parent / f".tmp_delete_{uuid.uuid4().hex[:8]}"
        self.root.mkdir(parents=True)
        self._old_root = ws.ASSISTANT_ROOT
        self._old_package = ws.CURRENT_PACKAGE
        ws.ASSISTANT_ROOT = str(self.root)
        ws.CURRENT_PACKAGE = None
        self.workspace = self.root / "demo_ws"
        (self.workspace / ".rcif" / "sessions").mkdir(parents=True)
        self.meta = self.workspace / ".rcif" / "sessions" / "session-abc.json"
        self.meta.write_text(
            json.dumps({"sessionId": "session-abc", "componentName": "Demo"}, ensure_ascii=False),
            encoding="utf-8",
        )

    def tearDown(self) -> None:
        ws.ASSISTANT_ROOT = self._old_root
        ws.CURRENT_PACKAGE = self._old_package
        shutil.rmtree(self.root, ignore_errors=True)

    def test_session_delete_archives_and_hides(self) -> None:
        result = asyncio.run(ws.delete_session(FakeRequest({"sessionId": "session-abc"})))
        self.assertTrue(result["ok"])
        self.assertTrue(result["archived"])
        self.assertFalse(self.meta.exists())
        archived = list((self.workspace / ".rcif" / "archive" / "sessions").glob("session-abc.*.json"))
        self.assertEqual(len(archived), 1)

    def test_session_delete_unknown_session_is_404(self) -> None:
        with self.assertRaises(HTTPException) as ctx:
            asyncio.run(ws.delete_session(FakeRequest({"sessionId": "session-none"})))
        self.assertEqual(ctx.exception.status_code, 404)

    def test_session_delete_rejects_invalid_ids(self) -> None:
        for bad in ("../escape", "a/b", "", "bad id"):
            with self.assertRaises(HTTPException) as ctx:
                asyncio.run(ws.delete_session(FakeRequest({"sessionId": bad})))
            self.assertEqual(ctx.exception.status_code, 422, bad)

    def test_session_meta_title_follows_component_name(self) -> None:
        meta = ws.write_session_meta("session-abc", {"componentName": "简支梁 32m"})
        self.assertEqual(meta["title"], "简支梁 32m")
        self.assertEqual(meta["componentName"], "简支梁 32m")
        explicit = ws.write_session_meta(
            "session-abc", {"componentName": "简支梁 32m", "title": "自定义会话名"}
        )
        self.assertEqual(explicit["title"], "自定义会话名")

    def test_workspace_delete_removes_directory(self) -> None:
        result = asyncio.run(
            ws.delete_workspace(FakeRequest({"workspaceId": "demo_ws", "deleteFiles": True}))
        )
        self.assertTrue(result["filesDeleted"])
        self.assertFalse(self.workspace.exists())

    def test_workspace_delete_guards(self) -> None:
        for bad in ("..", "../demo_ws", "", "demo/ws"):
            with self.assertRaises(HTTPException) as ctx:
                asyncio.run(
                    ws.delete_workspace(FakeRequest({"workspaceId": bad, "deleteFiles": True}))
                )
            self.assertEqual(ctx.exception.status_code, 422, bad)
        with self.assertRaises(HTTPException) as ctx:
            asyncio.run(
                ws.delete_workspace(FakeRequest({"workspaceId": "missing", "deleteFiles": True}))
            )
        self.assertEqual(ctx.exception.status_code, 404)


if __name__ == "__main__":
    unittest.main()
