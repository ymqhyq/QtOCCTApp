from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "rcif-tools"))

from rfa_grep import (  # noqa: E402
    find_gzip_pages,
    read_rfa_grep,
    strip_revit_64k_blocks,
)

PUMP = ROOT / "构件库-城轨rfa" / "离心泵 - 卧式.rfa"
PIER_15 = ROOT / "构件库rfa" / "桥Pier_15.rfa"


class GRepParserTest(unittest.TestCase):
    def test_page_scanner_uses_gzip_magic(self) -> None:
        raw = b"\x00" * 44 + b"\x1f\x8b\x08" + b"\xAA" * 10 + b"\x1f\x8b\x08" + b"\xBB" * 5
        self.assertEqual(find_gzip_pages(raw), [(44, 57), (57, 65)])

    def test_strip_revit_64k_blocks_removes_353_byte_framing(self) -> None:
        chunk1 = b"\x11" * 64896
        framing = b"\xFF" * 353
        chunk2 = b"\x22" * 1024
        self.assertEqual(strip_revit_64k_blocks(chunk1 + framing + chunk2), chunk1 + chunk2)

    @unittest.skipUnless(PUMP.exists(), "City rail RFA library is not present")
    def test_pump_complete_grep_and_brep_decoding(self) -> None:
        result = read_rfa_grep(str(PUMP))
        summary = result.summary()

        # 1. Partition & GZIP framing verification (100% checksum pass across all 16 pages)
        self.assertEqual(summary["partition"], "Partitions/0")
        self.assertEqual(summary["pageCount"], 16)
        self.assertEqual(summary["checksumOkCount"], 16)
        self.assertTrue(all(page["checksumOk"] for page in summary["pages"]))

        # 2. Formats/Latest schema & complete GElement FIFO deserialization
        self.assertEqual(summary["schemaClassCount"], 3792)
        self.assertTrue(summary["brepDecoded"])
        self.assertEqual(summary["gElementCount"], 75)
        self.assertEqual(summary["gElementExactCount"], 75)

        # 3. B-Rep topology & analytical surface verification
        self.assertEqual(summary["brepSolidCount"], 39)
        self.assertEqual(summary["faceCount"], 186)
        self.assertEqual(summary["edgeCount"], 391)
        self.assertEqual(summary["loopCount"], 194)
        self.assertEqual(summary["closedLoopCount"], 194)
        self.assertEqual(summary["surfaceCounts"], {"Plane": 122, "CylSurf": 64})
        self.assertEqual(summary["curveCount"], 165)
        self.assertLess(summary["maxAdjacentFaceEdgeGapMm"], 1e-6)

        # Check the boolean-joined FamilyGeomCombination pump solid (eid=1712260)
        joined = next(g for g in result.gelements if g.element_id == 1712260)
        self.assertEqual(joined.element_class, "FamilyGeomCombination")
        self.assertTrue(joined.exact_parse)
        self.assertEqual(len(joined.solids), 1)
        solid = joined.solids[0]
        self.assertEqual(solid.face_count, 44)
        self.assertEqual(solid.edge_count, 107)
        self.assertEqual(solid.loop_count, 48)
        self.assertEqual(solid.closed_loop_count, 48)
        self.assertEqual(solid.surface_counts, {"Plane": 28, "CylSurf": 16})
        self.assertEqual(solid.bbox_mm, [[0.0, 0.0, 0.0], [406.0, 878.0, 483.0]])

        # 4. 3D mesh & curve Wavefront OBJ export
        with tempfile.TemporaryDirectory() as tmp:
            obj = Path(tmp) / "grep.obj"
            count = result.write_obj(str(obj))
            self.assertGreater(count, 4000)
            text = obj.read_text(encoding="utf-8")
            self.assertIn("decoded Revit GRep B-Rep mesh", text)
            self.assertGreater(text.count("\nv "), 4000)
            self.assertGreater(text.count("\nf "), 2000)
            self.assertGreater(text.count("\nl "), 100)



    @unittest.skipUnless(PIER_15.exists(), "Pier 15 RFA is not present")
    def test_pier15_revit2008_schema_and_partition_decoding(self) -> None:
        result = read_rfa_grep(str(PIER_15))
        summary = result.summary()
        self.assertEqual(summary["partition"], "Partitions/28")
        self.assertGreater(summary["schemaClassCount"], 2000)
        self.assertEqual(summary["pageCount"], 1)
        self.assertTrue(summary["pages"][0]["checksumOk"])


if __name__ == "__main__":
    unittest.main()
