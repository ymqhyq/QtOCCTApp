"""Unregistered PDF -> generic RCIF draft contract test."""

from __future__ import annotations

import os
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "rcif-tools"))

import package_api  # noqa: E402
import pdf2rcif  # noqa: E402

PDF = ROOT / "rcif-tools" / "out" / "_test_generic_input.pdf"
OUTPUT = ROOT / "rcif-tools" / "out" / "_test_generic_draft.rcif"


class GenericPdfTest(unittest.TestCase):
    def setUp(self) -> None:
        import pymupdf

        PDF.parent.mkdir(parents=True, exist_ok=True)
        doc = pymupdf.open()
        page = doc.new_page()
        page.insert_text((72, 100), "铁路桥墩标准图", fontsize=18, fontname="china-s")
        page.insert_text(
            (72, 140),
            "墩身高度 8000mm 宽度 3000mm 长度 2000mm 直径 1200mm",
            fontsize=12,
            fontname="china-s",
        )
        doc.save(str(PDF))
        doc.close()
        if OUTPUT.exists():
            OUTPUT.unlink()

    def tearDown(self) -> None:
        for path in (PDF, OUTPUT):
            if path.exists():
                path.unlink()

    def test_unregistered_pdf_builds_reviewable_draft(self) -> None:
        pkg, info = pdf2rcif.convert_pdf_to_rcif(
            str(PDF), str(OUTPUT), family_id="GenericPier", revision="0.1.0"
        )
        self.assertTrue(os.path.isfile(pkg))
        self.assertEqual(info["extractionMode"], "generic_heuristic_draft")
        self.assertEqual(info["genericType"], "pier")
        self.assertGreaterEqual(info["confidence"], 0.7)

        meta = package_api.read_package(pkg)
        md = meta["metadata"]
        self.assertEqual(md["familyId"], "GenericPier")
        self.assertEqual(md["sourceMode"], "parametric")
        self.assertTrue(md["exactBrep"])
        self.assertGreater(md["shapeCount"], 0)
        self.assertGreater(md["volumeM3"], 0.0)
        self.assertTrue(md["preview"]["available"])
        self.assertTrue(meta["checksums"]["verified"])
        self.assertEqual(md["qa"]["extractionMode"], "generic_heuristic_draft")
        self.assertEqual(md["qa"]["genericType"], "pier")


if __name__ == "__main__":
    unittest.main()
