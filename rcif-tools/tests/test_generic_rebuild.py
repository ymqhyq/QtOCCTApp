"""Generic draft parameter/generator rebuild contract test."""

from __future__ import annotations

import os
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "rcif-tools"))

import generic_rebuild  # noqa: E402
import package_api  # noqa: E402
import pdf2rcif  # noqa: E402

PDF = ROOT / "rcif-tools" / "out" / "_test_rebuild_input.pdf"
BASE = ROOT / "rcif-tools" / "out" / "_test_rebuild_base.rcif"
NEW = ROOT / "rcif-tools" / "out" / "_test_rebuild_new.rcif"


class GenericRebuildTest(unittest.TestCase):
    def setUp(self) -> None:
        import pymupdf

        PDF.parent.mkdir(parents=True, exist_ok=True)
        doc = pymupdf.open()
        page = doc.new_page()
        page.insert_text((72, 100), "Bridge pier")
        page.insert_text(
            (72, 140),
            "height 8000 mm width 3000 mm length 2000 mm diameter 1200 mm",
        )
        doc.save(str(PDF))
        doc.close()
        for path in (BASE, NEW):
            if path.exists():
                path.unlink()
        pdf2rcif.convert_pdf_to_rcif(str(PDF), str(BASE), family_id="RebuildPier", revision="0.1.0")

    def tearDown(self) -> None:
        for path in (PDF, BASE, NEW):
            if path.exists():
                path.unlink()

    def test_parameter_rebuild_changes_geometry_and_revision(self) -> None:
        before = package_api.read_package(str(BASE))["metadata"]
        pkg, info = generic_rebuild.rebuild(
            str(BASE),
            str(NEW),
            params={"Height": 9000, "Width": 3500},
            revision="0.1.1",
            note="unit test",
        )
        self.assertTrue(os.path.isfile(pkg))
        self.assertEqual(info["revision"], "0.1.1")
        self.assertGreater(info["volumeM3"], before["volumeM3"])
        self.assertGreater(info["bounds"]["z"][1], before["bounds"]["z"][1])
        after = package_api.read_package(pkg)
        self.assertEqual(after["metadata"]["revision"], "0.1.1")
        preset = after["component"]["parameters"]["presets"][0]["params"]
        self.assertEqual(preset["Height"], 9000.0)
        self.assertEqual(preset["Width"], 3500.0)
        self.assertTrue(after["metadata"]["preview"]["available"])

    def test_generator_source_rebuild(self) -> None:
        import zipfile

        with zipfile.ZipFile(BASE) as archive:
            source = archive.read("generators/generator.py").decode("utf-8")
        source = source.replace("max(300.0, H * 0.08)", "500.0")
        pkg, info = generic_rebuild.rebuild(
            str(BASE),
            str(NEW),
            params={},
            source_text=source,
            revision="0.1.2",
            note="unit test source",
        )
        self.assertTrue(os.path.isfile(pkg))
        self.assertTrue(info["sourceChanged"])
        after = package_api.read_package(pkg)["metadata"]
        self.assertEqual(after["revision"], "0.1.2")
        self.assertTrue(after["preview"]["available"])


if __name__ == "__main__":
    unittest.main()
