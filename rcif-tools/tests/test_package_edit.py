"""Contract-safe package edit tests."""

from __future__ import annotations

import hashlib
import json
import os
import sys
import unittest
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "rcif-tools"))

import package_api  # noqa: E402
import package_edit  # noqa: E402
import pdf2rcif  # noqa: E402

PDF = ROOT / "rcif-tools" / "out" / "_test_edit_input.pdf"
BASE = ROOT / "rcif-tools" / "out" / "_test_edit_base.rcif"
EDITED = ROOT / "rcif-tools" / "out" / "_test_edit_identity.rcif"
REBUILT = ROOT / "rcif-tools" / "out" / "_test_edit_rebuild.rcif"


class PackageEditTest(unittest.TestCase):
    def setUp(self) -> None:
        import pymupdf

        PDF.parent.mkdir(parents=True, exist_ok=True)
        doc = pymupdf.open()
        page = doc.new_page()
        page.insert_text((72, 100), "Bridge pier")
        page.insert_text((72, 140), "height 8000 mm width 3000 mm length 2000 mm diameter 1200 mm")
        doc.save(str(PDF))
        doc.close()
        for path in (BASE, EDITED, REBUILT):
            if path.exists():
                path.unlink()
        pdf2rcif.convert_pdf_to_rcif(str(PDF), str(BASE), family_id="EditPier", revision="0.1.0")

    def tearDown(self) -> None:
        for path in (PDF, BASE, EDITED, REBUILT):
            if path.exists():
                path.unlink()

    def test_identity_edit_creates_new_revision_and_checksums(self) -> None:
        pkg, info = package_edit.edit(
            str(BASE),
            [{"op": "identity.update", "value": {"name": "编辑后的桥墩", "applicableSpan": ["32m简支箱梁"]}}],
            str(EDITED),
            note="identity test",
            actor="test",
        )
        self.assertTrue(os.path.isfile(pkg))
        self.assertEqual(info["revision"], "0.1.1")
        with zipfile.ZipFile(pkg) as archive:
            component = json.loads(archive.read("component.json"))
        self.assertEqual(component["identity"]["name"], "编辑后的桥墩")
        self.assertEqual(component["identity"]["applicableSpan"], ["32m简支箱梁"])
        meta = package_api.read_package(pkg)
        self.assertEqual(meta["metadata"]["packageSha256"], hashlib.sha256(Path(pkg).read_bytes()).hexdigest())
        self.assertNotIn("packageSha256", meta["metadata"].get("qa", {}))

    def test_parameter_edit_rebuilds_geometry_and_keeps_semantic_shapes(self) -> None:
        with zipfile.ZipFile(BASE) as archive:
            component = json.loads(archive.read("component.json"))
        preset = component["parameters"]["presets"][0]
        preset["params"]["Height"] = 9500
        pkg, info = package_edit.edit(
            str(BASE),
            [{"op": "parameters.presets.upsert", "value": preset}],
            str(REBUILT),
            note="parameter rebuild test",
            actor="test",
        )
        self.assertTrue(os.path.isfile(pkg))
        self.assertTrue(info["rebuild"])
        self.assertEqual(info["revision"], "0.1.1")
        meta = package_api.read_package(pkg)
        self.assertTrue(meta["metadata"]["preview"]["available"])
        self.assertGreater(meta["metadata"]["bounds"]["z"][1], 9000)

    def test_parameters_replace_updates_schema_and_presets(self) -> None:
        with zipfile.ZipFile(BASE) as archive:
            component = json.loads(archive.read("component.json"))
        parameters = component["parameters"]
        parameters["schema"].append({
            "name": "NoteValue",
            "label": "附加说明值",
            "type": "real",
            "default": 1.0,
            "access": "in",
        })
        parameters["presets"][0]["params"]["NoteValue"] = 2.0
        pkg, info = package_edit.edit(
            str(BASE),
            [{"op": "parameters.replace", "value": parameters}],
            str(REBUILT),
            note="parameters replace test",
            actor="test",
        )
        self.assertTrue(info["rebuild"])
        meta = package_api.read_package(pkg)
        self.assertEqual(meta["metadata"]["parameterCount"], 9)
        self.assertEqual(meta["metadata"]["presetCount"], 1)
        self.assertIn("NoteValue", meta["component"]["parameters"]["schema"][-1]["name"])


if __name__ == "__main__":
    unittest.main()
