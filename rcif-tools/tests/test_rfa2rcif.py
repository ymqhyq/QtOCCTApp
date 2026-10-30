"""RFA -> RCIF converter tests using the bundled Revit family library."""

from __future__ import annotations

import json
import os
import sys
import unittest
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "rcif-tools"))

import rfa2rcif  # noqa: E402

LIB = ROOT / "构件库rfa"
CITY_LIB = ROOT / "构件库-城轨rfa"
COLUMN = LIB / "冷弯空心型钢 - 圆形柱.rfa"
DOOR = LIB / "双面嵌板连窗玻璃门 1.rfa"
PUMP = CITY_LIB / "离心泵 - 卧式.rfa"
MANIFOLD = CITY_LIB / "分集水器 - 4 接口.rfa"
MANIFOLD_6 = LIB / "分集水器 - 6 接口.rfa"
RUBBER = CITY_LIB / "可曲挠橡胶头.rfa"
AHU = CITY_LIB / "AHU - 卧式 - 顶出后送式 - 2000-9000 CMH.rfa"
CHILLER = CITY_LIB / "螺旋旋转式冷水机组.rfa"
WATER_TREATMENT = CITY_LIB / "全程水处理设备.rfa"
YJK_STAIR = LIB / "YJK-双跑楼梯.rfa"
PIER_07 = LIB / "桥Pier_07.rfa"
PIER_15 = LIB / "桥Pier_15.rfa"
ESCALATOR = LIB / "自动扶梯_30度角自动扶梯_R.rfa"
ELEVATOR = LIB / "无机房电梯_双门.rfa"
OUT = ROOT / "rcif-tools" / "out" / "_test_rfa_column.rcif"


@unittest.skipUnless(COLUMN.exists(), "RFA family library is not present")
class RfaConversionTest(unittest.TestCase):
    def tearDown(self) -> None:
        if OUT.exists():
            OUT.unlink()

    def test_column_family_becomes_parametric_rcif(self) -> None:
        info = rfa2rcif.convert(str(COLUMN), str(OUT), family_id="RFAColumn", revision="1.0.0")
        self.assertTrue(os.path.isfile(OUT))
        self.assertEqual(info["template"], "column")
        self.assertEqual(info["revitVersion"], "2018")
        self.assertGreater(info["typeCount"], 1)
        with zipfile.ZipFile(OUT) as archive:
            component = json.loads(archive.read("component.json"))
            generator = archive.read("generators/generator.py").decode("utf-8")
            self.assertTrue(archive.read("source/partatom.xml"))
            self.assertTrue(archive.read("source/" + COLUMN.name))
            self.assertTrue(archive.read("display/preview.glb"))
            self.assertTrue(archive.read("geometry/default.step"))
        self.assertEqual(component["identity"]["sourceMode"], "parametric")
        self.assertEqual(component["environment"]["units"], {"length": "mm", "angle": "rad"})
        self.assertEqual(component["qa"]["extractionMode"], "rfa_partatom_heuristic")
        self.assertEqual(component["qa"]["template"], "column")
        self.assertIn('TEMPLATE = "column"', generator)
        self.assertGreater(len(component["parameters"]["schema"]), 3)
        self.assertGreater(len(component["parameters"]["presets"]), 1)
        names = {field["name"] for field in component["parameters"]["schema"]}
        self.assertIn("Diameter", names)
        self.assertIn("Thickness", names)
        self.assertIn("Height", names)

    @unittest.skipUnless(DOOR.exists(), "RFA family library is not present")
    def test_door_family_uses_door_template(self) -> None:
        out = ROOT / "rcif-tools" / "out" / "_test_rfa_door.rcif"
        info = rfa2rcif.convert(str(DOOR), str(out), family_id="RFADoor", revision="1.0.0")
        try:
            self.assertEqual(info["template"], "door")
            self.assertEqual(info["category"], "Door")
            self.assertGreater(info["typeCount"], 0)
            self.assertEqual(info["shapeCount"], 15)
            self.assertAlmostEqual(info["bounds"]["x"][0], -1650.0, delta=1.0)
            self.assertAlmostEqual(info["bounds"]["x"][1], 1650.0, delta=1.0)
            self.assertAlmostEqual(info["bounds"]["z"][1], 2100.0, delta=1.0)
            with zipfile.ZipFile(out) as archive:
                component = json.loads(archive.read("component.json").decode("utf-8"))
            shapes = component["shapes"]
            self.assertIn("OuterFrame", shapes)
            self.assertIn("DoorLeafLeft", shapes)
            self.assertIn("DoorLeafRight", shapes)
            self.assertIn("GlassDoorLeft", shapes)
            self.assertIn("HandleOuterLeft", shapes)
            self.assertIn("GlazingBarsDoorLeft", shapes)
            self.assertEqual(shapes["OuterFrame"]["material"], "Aluminum")
            self.assertEqual(shapes["GlassDoorLeft"]["material"], "Glass")
            self.assertEqual(shapes["HandleOuterLeft"]["material"], "StainlessSteel")
            self.assertEqual(shapes["GlazingBarsDoorLeft"]["material"], "Brass")
            ports = component["ports"]
            self.assertIn("Origin", ports)
            self.assertIn("OpeningCenter", ports)
            self.assertIn("HingeLeft", ports)
            self.assertIn("HingeRight", ports)
        finally:
            if out.exists():
                out.unlink()

    @unittest.skipUnless(PUMP.exists(), "City rail RFA library is not present")
    def test_pump_metadata_and_geometry_bindings(self) -> None:
        out = ROOT / "rcif-tools" / "out" / "_test_rfa_pump.rcif"
        try:
            rfa2rcif.convert(str(PUMP), str(out), family_id="CityPump", revision="1.0.0")
            with zipfile.ZipFile(out) as archive:
                component = json.loads(archive.read("component.json"))
            self.assertEqual(component["identity"]["category"], "MechanicalEquipment")
            self.assertEqual(component["identity"]["sourceMode"], "parametric")
            self.assertEqual(component["environment"]["tolerance"]["linear"], 0.01)
            self.assertIn("SuctionOuterRadius", component["recipe"]["inputs"])
            self.assertIn("DischargeNominalDiameter", component["recipe"]["inputs"])
            self.assertNotIn("PumpMaterial", component["recipe"]["inputs"])
            self.assertEqual(component["qa"]["sourceGeometry"]["fidelity"], "heuristic-reconstruction")
            self.assertEqual(component["qa"]["grep"]["status"], "parsed")
            self.assertGreater(component["qa"]["grep"]["grepPageCount"], 0)
            self.assertTrue(component["qa"]["grep"]["brepDecoded"])
            self.assertEqual(component["qa"]["grep"]["gElementCount"], 75)
            self.assertEqual(component["qa"]["grep"]["brepSolidCount"], 39)
            self.assertEqual(component["qa"]["grep"]["faceCount"], 186)
            self.assertEqual(component["qa"]["grep"]["edgeCount"], 391)
            with zipfile.ZipFile(out) as archive:
                self.assertIn("source/grep.json", archive.namelist())
                self.assertIn("source/grep-candidates.obj", archive.namelist())
            self.assertEqual(component["shapes"]["PumpCasing"]["material"], "Steel")
            self.assertNotEqual(component["shapes"]["DriveShaft"]["material"], component["shapes"]["BasePlate"]["material"])
            self.assertEqual(
                list(component["shapes"].keys()),
                [
                    "BasePlate", "PumpCasing", "PumpHub", "DriveShaft", "Motor",
                    "MotorPedestal", "DischargeNozzle", "DischargeFlange",
                    "SuctionFlange", "SuctionNozzle",
                ],
            )
            self.assertEqual(component["geometry"]["bounds"], {"x": [0.0, 406.0], "y": [0.0, 878.0], "z": [0.0, 483.0]})
            self.assertEqual(component["ports"]["Suction"]["origin"], [203.0, 15.0, 272.0])
            self.assertEqual(component["ports"]["Discharge"]["origin"], [203.0, 91.0, 483.0])
        finally:
            if out.exists():
                out.unlink()

    @unittest.skipUnless(MANIFOLD.exists(), "City rail RFA library is not present")
    def test_manifold_port_count_comes_from_title(self) -> None:
        out = ROOT / "rcif-tools" / "out" / "_test_rfa_manifold.rcif"
        try:
            info = rfa2rcif.convert(str(MANIFOLD), str(out), family_id="CityManifold4", revision="1.0.0")
            self.assertEqual(info["shapeCount"], 20)
            with zipfile.ZipFile(out) as archive:
                component = json.loads(archive.read("component.json"))
            ports = next(field for field in component["parameters"]["schema"] if field["name"] == "PortCount")
            self.assertEqual(ports["default"], 4)
            self.assertIn("Port_1", component["ports"])
            self.assertIn("Port_4", component["ports"])
            self.assertIn("Thermometer", component["ports"])
            self.assertIn("Drain", component["ports"])
        finally:
            if out.exists():
                out.unlink()

    @unittest.skipUnless(MANIFOLD_6.exists(), "RFA library is not present")
    def test_manifold_6_ports_parametric_matching_step(self) -> None:
        out = ROOT / "rcif-tools" / "out" / "_test_rfa_manifold_6.rcif"
        try:
            info = rfa2rcif.convert(str(MANIFOLD_6), str(out), family_id="RFAManifold", revision="1.0.0")
            self.assertEqual(info["shapeCount"], 24)
            self.assertEqual(info["template"], "manifold")
            with zipfile.ZipFile(out) as archive:
                component = json.loads(archive.read("component.json"))
            ports = next(field for field in component["parameters"]["schema"] if field["name"] == "PortCount")
            self.assertEqual(ports["default"], 6)
            # Verify 6 branch ports exist
            for i in range(1, 7):
                self.assertIn(f"Port_{i}", component["ports"])
            self.assertIn("Thermometer", component["ports"])
            self.assertIn("PressureGauge", component["ports"])
            self.assertIn("Drain", component["ports"])
            self.assertIn("Support_Left", component["ports"])
            self.assertIn("Support_Right", component["ports"])
            # Verify materials
            shapes = component["shapes"]
            self.assertEqual(shapes["ManifoldBody"]["material"], "CarbonSteel")
            self.assertEqual(shapes["Flange_1"]["material"], "Steel")
            self.assertEqual(shapes["ThermometerNozzle"]["material"], "StainlessSteel")
            self.assertEqual(shapes["SupportLeft"]["material"], "StructuralSteel")
            self.assertEqual(shapes["WoodBlockLeft"]["material"], "Wood")
        finally:
            if out.exists():
                out.unlink()

    @unittest.skipUnless(RUBBER.exists(), "City rail RFA library is not present")
    def test_rubber_joint_parametric_reconstruction(self) -> None:
        out = ROOT / "rcif-tools" / "out" / "_test_rfa_rubber.rcif"
        try:
            info = rfa2rcif.convert(str(RUBBER), str(out), family_id="CityRubberJoint", revision="1.0.0")
            self.assertEqual(info["template"], "rubber_joint")
            self.assertEqual(info["category"], "PipeAccessory")
            with zipfile.ZipFile(out) as archive:
                component = json.loads(archive.read("component.json"))
            self.assertEqual(component["identity"]["familyId"], "CityRubberJoint")
            self.assertEqual(component["identity"]["category"], "PipeAccessory")
            self.assertEqual(
                list(component["shapes"].keys()),
                ["FlangeLeft", "FlangeRight", "RubberArch"],
            )
            self.assertEqual(component["shapes"]["FlangeLeft"]["material"], "DuctileIron")
            self.assertEqual(component["shapes"]["FlangeRight"]["material"], "DuctileIron")
            self.assertEqual(component["shapes"]["RubberArch"]["material"], "Rubber")
            self.assertAlmostEqual(component["geometry"]["bounds"]["x"][0], -153.0, places=1)
            self.assertAlmostEqual(component["geometry"]["bounds"]["x"][1], 153.0, places=1)
            self.assertEqual(component["ports"]["Inlet"]["origin"], [-153.0, 0.0, 0.0])
            self.assertEqual(component["ports"]["Outlet"]["origin"], [153.0, 0.0, 0.0])
        finally:
            if out.exists():
                out.unlink()

    @unittest.skipUnless(AHU.exists(), "City rail RFA library is not present")
    def test_ahu_parametric_reconstruction(self) -> None:
        out = ROOT / "rcif-tools" / "out" / "_test_rfa_ahu.rcif"
        try:
            info = rfa2rcif.convert(str(AHU), str(out), family_id="CityAHU", revision="1.0.0")
            self.assertEqual(info["template"], "ahu")
            self.assertEqual(info["category"], "MechanicalEquipment")
            self.assertEqual(info["typeCount"], 14)
            self.assertEqual(info["shapeCount"], 14)
            self.assertGreater(info["volumeM3"], 1.0)
            with zipfile.ZipFile(out) as archive:
                component = json.loads(archive.read("component.json"))
            self.assertEqual(component["identity"]["familyId"], "CityAHU")
            self.assertEqual(component["identity"]["category"], "MechanicalEquipment")
            self.assertEqual(component["qa"]["template"], "ahu")
            self.assertEqual(component["qa"]["typeCount"], 14)
            expected_shapes = [
                "BaseRailLeft", "BaseRailRight", "CabinetBody",
                "SupplyAirDuct", "SupplyAirFlange", "ReturnAirDuct", "ReturnAirFlange",
                "WaterInletNozzle", "WaterInletFlange", "WaterOutletNozzle", "WaterOutletFlange",
                "DrainNozzle", "AccessPanelFront", "AccessPanelBack",
            ]
            self.assertEqual(list(component["shapes"].keys()), expected_shapes)
            self.assertEqual(component["shapes"]["CabinetBody"]["material"], "GalvanizedSteel")
            self.assertEqual(component["shapes"]["BaseRailLeft"]["material"], "Steel")
            self.assertEqual(component["shapes"]["WaterInletNozzle"]["material"], "Copper")
            self.assertEqual(component["shapes"]["DrainNozzle"]["material"], "PVC")
            self.assertEqual(component["shapes"]["AccessPanelFront"]["material"], "Aluminum")
            self.assertIn("SupplyAir", component["ports"])
            self.assertIn("ReturnAir", component["ports"])
            self.assertIn("WaterInlet", component["ports"])
            self.assertIn("WaterOutlet", component["ports"])
            self.assertIn("Drain", component["ports"])
            self.assertIn("SupplyAirWidth", component["recipe"]["inputs"])
            self.assertIn("ReturnAirHeight", component["recipe"]["inputs"])
            self.assertIn("WaterInletRadius", component["recipe"]["inputs"])
            self.assertIn("BaseHeight", component["recipe"]["inputs"])
        finally:
            if out.exists():
                out.unlink()

    @unittest.skipUnless(CHILLER.exists(), "City rail RFA library is not present")
    def test_universal_chiller_conversion(self) -> None:
        out = ROOT / "rcif-tools" / "out" / "_test_rfa_chiller.rcif"
        try:
            info = rfa2rcif.convert(str(CHILLER), str(out), family_id="CityChiller", revision="1.0.0")
            self.assertEqual(info["template"], "universal")
            self.assertEqual(info["category"], "MechanicalEquipment")
            self.assertGreaterEqual(info["shapeCount"], 25)
            self.assertGreater(info["volumeM3"], 2.0)

            with zipfile.ZipFile(out) as archive:
                component = json.loads(archive.read("component.json"))
                generator_code = archive.read("generators/generator.py").decode("utf-8")
                self.assertIn("component.json", archive.namelist())
                self.assertIn("checksums.json", archive.namelist())
                self.assertIn("display/preview.glb", archive.namelist())
                self.assertIn("geometry/default.step", archive.namelist())

            self.assertEqual(component["identity"]["familyId"], "CityChiller")
            self.assertEqual(component["qa"]["template"], "universal")
            self.assertGreaterEqual(len(component["shapes"]), 25)

            # 验证各部件具有智能语义命名，绝非单一包围盒
            shape_names = list(component["shapes"].keys())
            has_base = any(name.startswith("BaseFrame") for name in shape_names)
            has_cyl = any(name.startswith("MainCylinder") for name in shape_names)
            self.assertTrue(has_base, "Universal pipeline should detect base frame components")
            self.assertTrue(has_cyl, "Universal pipeline should detect cylinder primitives")

            # 验证所有材质均为机电设备工业材质（非 C35 混凝土）
            for name, meta in component["shapes"].items():
                self.assertNotEqual(meta["material"], "Concrete", f"{name} should not be Concrete")
                self.assertIn(meta["material"], ["Steel", "CarbonSteel", "StructuralSteel", "CastIron", "Aluminum", "Copper", "Rubber", "GalvanizedSteel"])

            # 验证端口自动推断
            self.assertIn("Base", component["ports"])
            self.assertTrue(len(component["ports"]) >= 1)

            # 验证生成器可执行性与实体有效性
            ns: dict = {}
            exec(generator_code, ns)
            self.assertIn("build_parts", ns)
            defaults = {f["name"]: f["default"] for f in component["parameters"]["schema"]}
            parts = ns["build_parts"](defaults)
            self.assertGreaterEqual(len(parts), 25)
            for part_name, solid in parts.items():
                s = solid.val() if hasattr(solid, "val") else solid
                self.assertTrue(s.isValid(), f"Part {part_name} must be a valid CadQuery Solid")
        finally:
            if out.exists():
                out.unlink()

    @unittest.skipUnless(WATER_TREATMENT.exists(), "City rail RFA library is not present")
    def test_universal_water_treatment_conversion(self) -> None:
        out = ROOT / "rcif-tools" / "out" / "_test_rfa_water_treatment.rcif"
        try:
            info = rfa2rcif.convert(str(WATER_TREATMENT), str(out), family_id="CityWaterTreatment", revision="1.0.0")
            self.assertEqual(info["template"], "universal")
            self.assertEqual(info["category"], "MechanicalEquipment")
            self.assertGreaterEqual(info["shapeCount"], 15)
            self.assertGreater(info["volumeM3"], 0.1)

            with zipfile.ZipFile(out) as archive:
                component = json.loads(archive.read("component.json"))
                generator_code = archive.read("generators/generator.py").decode("utf-8")

            self.assertEqual(component["identity"]["familyId"], "CityWaterTreatment")
            self.assertEqual(component["qa"]["template"], "universal")
            self.assertGreaterEqual(len(component["shapes"]), 15)

            # 验证圆柱罐体与支架部件均存在
            shape_names = list(component["shapes"].keys())
            self.assertTrue(any("Cylinder" in name for name in shape_names), "Must detect tank cylinder")
            self.assertTrue(any("Frame" in name or "Base" in name for name in shape_names), "Must detect frame/base")

            # 验证端口包含底座及管口
            self.assertIn("Base", component["ports"])
            self.assertGreaterEqual(len(component["ports"]), 2)

            # 验证生成器可执行性与每个部件拓扑有效
            ns: dict = {}
            exec(generator_code, ns)
            defaults = {f["name"]: f["default"] for f in component["parameters"]["schema"]}
            parts = ns["build_parts"](defaults)
            self.assertGreaterEqual(len(parts), 15)
            for part_name, solid in parts.items():
                s = solid.val() if hasattr(solid, "val") else solid
                self.assertTrue(s.isValid(), f"Part {part_name} must be a valid CadQuery Solid")
        finally:
            if out.exists():
                out.unlink()

    @unittest.skipUnless(YJK_STAIR.exists(), "YJK stair RFA is not present")
    def test_yjk_stair_conversion(self) -> None:
        out = ROOT / "rcif-tools" / "out" / "_test_rfa_yjk.rcif"
        try:
            info = rfa2rcif.convert(str(YJK_STAIR), str(out), family_id="YJK", revision="1.0.0")
            self.assertEqual(info["template"], "stair")
            self.assertEqual(info["category"], "Stairs")
            self.assertEqual(info["shapeCount"], 13)
            self.assertGreater(info["volumeM3"], 5.0)
            with zipfile.ZipFile(out) as archive:
                comp = json.loads(archive.read("component.json"))
                gen_code = archive.read("generators/generator.py").decode("utf-8")
            self.assertEqual(len(comp["shapes"]), 13)
            expected_shapes = [
                "LandingBottom", "Flight1_Run", "LandingMid", "LandingBeamMid",
                "Flight2_Run", "LandingTop", "FloorBeamTop",
                "Handrail1_Outer", "Handrail2_Outer", "Handrail_Inner",
                "Post_Bottom", "Post_Mid", "Post_Top",
            ]
            self.assertEqual(list(comp["shapes"].keys()), expected_shapes)
            self.assertEqual(comp["shapes"]["Flight1_Run"]["material"], "Concrete")
            self.assertEqual(comp["shapes"]["Handrail1_Outer"]["material"], "StainlessSteel")
            self.assertIn("Bottom_Start", comp["ports"])
            self.assertIn("Mid_Landing", comp["ports"])
            self.assertIn("Top_Exit", comp["ports"])
            ns: dict = {}
            exec(gen_code, ns)
            defaults = {f["name"]: f["default"] for f in comp["parameters"]["schema"]}
            parts = ns["build_parts"](defaults)
            self.assertEqual(len(parts), 13)
            for part_name, solid in parts.items():
                s = solid.val() if hasattr(solid, "val") else solid
                self.assertTrue(s.isValid(), f"Part {part_name} must be a valid CadQuery Solid")
        finally:
            if out.exists():
                out.unlink()

    @unittest.skipUnless(PIER_07.exists(), "Bridge pier 07 RFA is not present")
    def test_bridge_pier_07_conversion(self) -> None:
        out = ROOT / "rcif-tools" / "out" / "_test_rfa_pier07.rcif"
        try:
            info = rfa2rcif.convert(str(PIER_07), str(out), family_id="BridgePier07", revision="1.0.0")
            self.assertEqual(info["template"], "bridge_pier")
            self.assertEqual(info["category"], "BridgePier")
            self.assertEqual(info["shapeCount"], 24)
            self.assertAlmostEqual(info["volumeM3"], 69.42, delta=0.05)
            self.assertAlmostEqual(info["bounds"]["x"][0], -10270.0, delta=1.0)
            self.assertAlmostEqual(info["bounds"]["x"][1], 10270.0, delta=1.0)
            self.assertAlmostEqual(info["bounds"]["y"][0], -700.0, delta=1.0)
            self.assertAlmostEqual(info["bounds"]["y"][1], 700.0, delta=1.0)
            self.assertAlmostEqual(info["bounds"]["z"][0], 0.0, delta=1.0)
            self.assertAlmostEqual(info["bounds"]["z"][1], 11207.0, delta=1.0)
            with zipfile.ZipFile(out) as archive:
                comp = json.loads(archive.read("component.json"))
                gen_code = archive.read("generators/generator.py").decode("utf-8")
            self.assertEqual(len(comp["shapes"]), 24)
            self.assertIn("CappingBeam", comp["shapes"])
            self.assertIn("PierColumnLeft", comp["shapes"])
            self.assertIn("PierColumnCenter", comp["shapes"])
            self.assertIn("PierColumnRight", comp["shapes"])
            self.assertIn("BedStone_01", comp["shapes"])
            self.assertIn("Bearing_01", comp["shapes"])
            for name, meta in comp["shapes"].items():
                if "Bearing" in name:
                    self.assertEqual(meta["material"], "StructuralSteel")
                else:
                    self.assertEqual(meta["material"], "Concrete")
            self.assertIn("Bottom_Center", comp["ports"])
            self.assertIn("Column_Left", comp["ports"])
            self.assertIn("Column_Center", comp["ports"])
            self.assertIn("Column_Right", comp["ports"])
            self.assertIn("Bearing_01", comp["ports"])
            self.assertIn("Bearing_10", comp["ports"])
            ns: dict = {}
            exec(gen_code, ns)
            defaults = {f["name"]: f["default"] for f in comp["parameters"]["schema"]}
            parts = ns["build_parts"](defaults)
            self.assertEqual(len(parts), 24)
            for part_name, solid in parts.items():
                s = solid.val() if hasattr(solid, "val") else solid
                self.assertTrue(s.isValid(), f"Part {part_name} must be a valid CadQuery Solid")
        finally:
            if out.exists():
                out.unlink()

    @unittest.skipUnless(PIER_15.exists(), "Bridge pier 15 RFA is not present")
    def test_bridge_pier_15_conversion(self) -> None:
        out = ROOT / "rcif-tools" / "out" / "_test_rfa_pier15.rcif"
        try:
            info = rfa2rcif.convert(str(PIER_15), str(out), family_id="BridgePier15", revision="1.0.0")
            self.assertEqual(info["template"], "portal_pier")
            self.assertEqual(info["category"], "BridgePier")
            self.assertEqual(info["shapeCount"], 8)
            self.assertAlmostEqual(info["bounds"]["x"][0], -2250.0, delta=1.0)
            self.assertAlmostEqual(info["bounds"]["x"][1], 2250.0, delta=1.0)
            self.assertAlmostEqual(info["bounds"]["y"][0], -500.0, delta=1.0)
            self.assertAlmostEqual(info["bounds"]["y"][1], 500.0, delta=1.0)
            self.assertAlmostEqual(info["bounds"]["z"][0], 0.0, delta=1.0)
            self.assertAlmostEqual(info["bounds"]["z"][1], 4072.0, delta=1.0)
            self.assertGreaterEqual(info["volumeM3"], 6.8)
            self.assertLessEqual(info["volumeM3"], 7.3)
            with zipfile.ZipFile(out) as archive:
                comp = json.loads(archive.read("component.json"))
                self.assertTrue(archive.read("geometry/default.step"))
                self.assertTrue(archive.read("display/preview.glb"))
            self.assertEqual(len(comp["shapes"]), 8)
            self.assertIn("PierLegRight", comp["shapes"])
            self.assertIn("PierLegLeft", comp["shapes"])
            self.assertIn("TieBeamLower", comp["shapes"])
            self.assertIn("TieBeamUpper", comp["shapes"])
            self.assertIn("PlinthLeftLower", comp["shapes"])
            self.assertIn("PlinthRightLower", comp["shapes"])
        finally:
            if out.exists():
                out.unlink()

    @unittest.skipUnless(ELEVATOR.exists(), "Elevator RFA is not present")
    def test_elevator_conversion(self) -> None:
        out = ROOT / "rcif-tools" / "out" / "_test_rfa_elevator.rcif"
        try:
            info = rfa2rcif.convert(str(ELEVATOR), str(out), family_id="RFAElevator", revision="1.0.0")
            self.assertEqual(info["template"], "elevator")
            self.assertEqual(info["shapeCount"], 10)
            self.assertEqual(info["typeCount"], 4)
            self.assertAlmostEqual(info["bounds"]["x"][0], -805.0, delta=1.0)
            self.assertAlmostEqual(info["bounds"]["x"][1], 915.0, delta=1.0)
            self.assertAlmostEqual(info["bounds"]["y"][0], -825.0, delta=1.0)
            self.assertAlmostEqual(info["bounds"]["y"][1], 975.0, delta=1.0)
            self.assertAlmostEqual(info["bounds"]["z"][0], -1400.0, delta=1.0)
            self.assertAlmostEqual(info["bounds"]["z"][1], 8000.0, delta=1.0)
            self.assertAlmostEqual(info["volumeM3"], 1.119, delta=0.05)

            with zipfile.ZipFile(out) as archive:
                comp = json.loads(archive.read("component.json"))
                gen_code = archive.read("generators/generator.py").decode("utf-8")
                self.assertTrue(archive.read("geometry/default.step"))
                self.assertTrue(archive.read("display/preview.glb"))

            self.assertEqual(len(comp["shapes"]), 10)
            self.assertIn("CarBody", comp["shapes"])
            self.assertIn("DoorFrontLeft", comp["shapes"])
            self.assertIn("DoorRearLeft", comp["shapes"])
            self.assertIn("BasePlinth", comp["shapes"])
            self.assertIn("SupportBeamFront", comp["shapes"])
            self.assertIn("GuideRailTop", comp["shapes"])

            self.assertEqual(comp["shapes"]["CarBody"]["material"], "StainlessSteel")
            self.assertEqual(comp["shapes"]["SupportBeamFront"]["material"], "CarbonSteel")
            self.assertEqual(comp["shapes"]["GuideRailTop"]["material"], "AlloySteel")

            # 验证定位端口
            ports = comp["ports"]
            self.assertIn("Origin", ports)
            self.assertIn("CarCenter", ports)
            self.assertIn("FrontEntrance", ports)
            self.assertIn("RearEntrance", ports)
            self.assertIn("PitBase", ports)
            self.assertIn("OverheadTop", ports)

            # 验证多型号全覆盖
            ns: dict = {}
            exec(gen_code, ns)
            presets = comp["parameters"]["presets"]
            self.assertEqual(len(presets), 4)
            widths = {}
            for pr in presets:
                m_num = pr["modelNumber"]
                parts = ns["build_parts"](pr["params"])
                self.assertEqual(len(parts), 10)
                for part_name, solid in parts.items():
                    s = solid.val() if hasattr(solid, "val") else solid
                    self.assertTrue(s.isValid(), f"Preset {m_num} part {part_name} must be valid")
                widths[m_num] = max((s.val() if hasattr(s, "val") else s).BoundingBox().xmax for s in parts.values()) -                                 min((s.val() if hasattr(s, "val") else s).BoundingBox().xmin for s in parts.values())

            self.assertAlmostEqual(widths["P11"], 1720.0, delta=1.0)
            self.assertAlmostEqual(widths["P8"], 1470.0, delta=1.0)
            self.assertAlmostEqual(widths["P14_宽"], 1970.0, delta=1.0)
        finally:
            if out.exists():
                out.unlink()

    @unittest.skipUnless(ESCALATOR.exists(), "Escalator RFA is not present")
    def test_escalator_conversion(self) -> None:
        out = ROOT / "rcif-tools" / "out" / "_test_rfa_escalator.rcif"
        try:
            info = rfa2rcif.convert(str(ESCALATOR), str(out), family_id="Escalator_30Deg_R", revision="1.0.0")
            self.assertEqual(info["template"], "escalator")
            self.assertEqual(info["category"], "SpecialEquipment")
            self.assertEqual(info["shapeCount"], 32)
            self.assertEqual(info["typeCount"], 3)
            self.assertAlmostEqual(info["bounds"]["x"][0], 0.0, delta=1.0)
            self.assertAlmostEqual(info["bounds"]["x"][1], 12123.2, delta=1.0)
            self.assertAlmostEqual(info["bounds"]["y"][0], -575.0, delta=1.0)
            self.assertAlmostEqual(info["bounds"]["y"][1], 575.0, delta=1.0)
            self.assertAlmostEqual(info["bounds"]["z"][0], -1100.0, delta=1.0)
            self.assertAlmostEqual(info["bounds"]["z"][1], 4950.0, delta=1.0)
            self.assertGreaterEqual(info["volumeM3"], 25.0)
            self.assertLessEqual(info["volumeM3"], 30.0)

            with zipfile.ZipFile(out) as archive:
                comp = json.loads(archive.read("component.json"))
                gen_code = archive.read("generators/generator.py").decode("utf-8")
                self.assertTrue(archive.read("geometry/default.step"))
                self.assertTrue(archive.read("display/preview.glb"))

            self.assertEqual(len(comp["shapes"]), 32)
            self.assertIn("TrussMain", comp["shapes"])
            self.assertIn("GlassBalustradeLeft", comp["shapes"])
            self.assertIn("HandrailLeft", comp["shapes"])
            self.assertIn("DeckingSkirtLeft", comp["shapes"])
            self.assertIn("Step_01", comp["shapes"])
            self.assertEqual(comp["shapes"]["GlassBalustradeLeft"]["material"], "Glass")
            self.assertEqual(comp["shapes"]["HandrailLeft"]["material"], "Rubber")
            self.assertEqual(comp["shapes"]["TrussMain"]["material"], "StructuralSteel")

            # 验证端口
            ports = comp["ports"]
            self.assertIn("Origin", ports)
            self.assertIn("LowerEntrance", ports)
            self.assertIn("UpperExit", ports)
            self.assertIn("LowerSupport", ports)
            self.assertIn("UpperSupport", ports)

            # 验证全部预设型号多变参驱动
            ns: dict = {}
            exec(gen_code, ns)
            presets = comp["parameters"]["presets"]
            self.assertEqual(len(presets), 3)
            preset_widths = {}
            for pr in presets:
                m_num = pr["modelNumber"]
                parts = ns["build_parts"](pr["params"])
                self.assertEqual(len(parts), 32)
                for part_name, solid in parts.items():
                    s = solid.val() if hasattr(solid, "val") else solid
                    self.assertTrue(s.isValid(), f"Preset {m_num} part {part_name} must be valid")
                y_span = max((s.val() if hasattr(s, "val") else s).BoundingBox().ymax for s in parts.values()) -                          min((s.val() if hasattr(s, "val") else s).BoundingBox().ymin for s in parts.values())
                preset_widths[m_num] = y_span

            self.assertAlmostEqual(preset_widths["S600"], 1150.0, delta=1.0)
            self.assertAlmostEqual(preset_widths["S800"], 1350.0, delta=1.0)
            self.assertAlmostEqual(preset_widths["S1000"], 1550.0, delta=1.0)
            # 验证两端半圆圆角与平滑过渡特征
            s600_parts = ns["build_parts"](presets[0]["params"])
            glass_s = s600_parts["GlassBalustradeLeft"].val() if hasattr(s600_parts["GlassBalustradeLeft"], "val") else s600_parts["GlassBalustradeLeft"]
            glass_bb = glass_s.BoundingBox()
            self.assertAlmostEqual(glass_bb.xmin, 665.0, delta=2.0)
            self.assertAlmostEqual(glass_bb.xmax, 10908.2, delta=2.0)
            hr_s = s600_parts["HandrailLeft"].val() if hasattr(s600_parts["HandrailLeft"], "val") else s600_parts["HandrailLeft"]
            hr_bb = hr_s.BoundingBox()
            self.assertAlmostEqual(hr_bb.xmin, 630.0, delta=2.0)
            self.assertAlmostEqual(hr_bb.xmax, 10943.2, delta=2.0)
            skirt_s = s600_parts["DeckingSkirtLeft"].val() if hasattr(s600_parts["DeckingSkirtLeft"], "val") else s600_parts["DeckingSkirtLeft"]
            # 验证底座端头凸圆角 (凸圆角体积为 ~6.308e8，凹圆角体积仅为 ~6.238e8)
            self.assertGreater(skirt_s.Volume(), 6.28e8)
        finally:
            if out.exists():
                out.unlink()

    def test_output_collision_and_determinism(self) -> None:
        with self.assertRaises(ValueError):
            rfa2rcif.convert(str(COLUMN), str(COLUMN), family_id="RFAColumn")
        first = ROOT / "rcif-tools" / "out" / "_test_rfa_det1.rcif"
        second = ROOT / "rcif-tools" / "out" / "_test_rfa_det2.rcif"
        try:
            rfa2rcif.convert(str(COLUMN), str(first), family_id="RFAColumn", revision="1.0.0")
            rfa2rcif.convert(str(COLUMN), str(second), family_id="RFAColumn", revision="1.0.0")
            self.assertEqual(first.read_bytes(), second.read_bytes())
        finally:
            for path in (first, second):
                if path.exists():
                    path.unlink()


if __name__ == "__main__":
    unittest.main()
