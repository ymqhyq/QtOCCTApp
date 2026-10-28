"""转换器契约测试：IFC -> RCIF（mesh-only）。

STEP / PDF 转换分别依赖大模型与 6 道门禁（20–60s），放在 E2E 套件中；
这里锁定 IFC 的单位、部位、GLB 与包完整性。
"""

from __future__ import annotations

import os
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "rcif-tools"))

import ifc2rcif  # noqa: E402
import package_api  # noqa: E402
import step2rcif  # noqa: E402

SAMPLE_IFC = ROOT / "piles.ifc"
FACETED_STEP = ROOT / "Abutment.step"
OUTPUT = ROOT / "rcif-tools" / "out" / "_test_ifc_contract.rcif"
STEP_OUTPUT = ROOT / "rcif-tools" / "out" / "_test_step_faceted.rcif"


class IfcConverterTest(unittest.TestCase):
    def setUp(self) -> None:
        if not SAMPLE_IFC.is_file():
            self.skipTest("缺少 piles.ifc 样例")
        OUTPUT.parent.mkdir(parents=True, exist_ok=True)
        if OUTPUT.exists():
            OUTPUT.unlink()

    def tearDown(self) -> None:
        if OUTPUT.exists():
            OUTPUT.unlink()

    def test_ifc_to_rcif_package(self) -> None:
        pkg, info = ifc2rcif.convert_ifc_to_rcif(
            str(SAMPLE_IFC), str(OUTPUT), family_id="PilesContract", revision="1.0.0"
        )
        self.assertTrue(os.path.isfile(pkg))
        self.assertGreater(info["partCount"], 0)
        self.assertGreater(info["volumeM3"], 0.0)

        meta = package_api.read_package(pkg)
        self.assertEqual(meta["metadata"]["familyId"], "PilesContract")
        self.assertEqual(meta["metadata"]["sourceMode"], "mesh-only")
        self.assertFalse(meta["metadata"]["exactBrep"])
        self.assertTrue(meta["metadata"]["preview"]["available"])
        self.assertTrue(meta["checksums"]["verified"])
        self.assertGreater(meta["metadata"]["shapeCount"], 0)

        # IFC 常见单位是 mm；纠偏后桩长应达到米级（>1000mm），而不是 20mm
        z = meta["metadata"]["bounds"]["z"]
        self.assertGreater(abs(z[1] - z[0]), 1000.0)
        self.assertEqual(meta["metadata"]["shapes"][0]["geomType"], "shell")


class FacetedStepFallbackTest(unittest.TestCase):
    """供应商 FACETED_BREP 的 STEP 不能因 OCP 读成空 shell 而失败。"""

    def setUp(self) -> None:
        if not FACETED_STEP.is_file():
            self.skipTest("缺少 Abutment.step 样例")
        STEP_OUTPUT.parent.mkdir(parents=True, exist_ok=True)
        if STEP_OUTPUT.exists():
            STEP_OUTPUT.unlink()

    def tearDown(self) -> None:
        if STEP_OUTPUT.exists():
            STEP_OUTPUT.unlink()

    def test_faceted_brep_rebuilds_solid(self) -> None:
        pkg, info = step2rcif.convert_step_to_rcif(
            input_step_path=str(FACETED_STEP),
            output_dir=str(STEP_OUTPUT),
            family_id="FacetedAbutment",
            revision="1.0.0",
        )
        self.assertTrue(os.path.isfile(pkg))
        self.assertGreater(info["geo_info"]["volume_m3"], 0.0)
        meta = package_api.read_package(pkg)
        self.assertEqual(meta["metadata"]["sourceMode"], "native-brep")
        self.assertTrue(meta["metadata"]["exactBrep"])
        self.assertGreater(meta["metadata"]["bounds"]["x"][1] - meta["metadata"]["bounds"]["x"][0], 1000.0)
        self.assertTrue(meta["checksums"]["verified"])


if __name__ == "__main__":
    unittest.main()
