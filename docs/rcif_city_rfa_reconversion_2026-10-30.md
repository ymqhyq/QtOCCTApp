# 城轨 RFA 构件批量重转换记录（2026-10-30）

- 工作区：`铁路构件智能助手/城轨rfa转换`
- 输入：`inputs/` 下 14 个 Revit 族（`.rfa`）
- 输出：`out/<FamilyId>_1.0.0.rcif`（覆盖旧包，旧包备份在 `out/_backup_pre_reconvert_20261030/`）
- 转换器：`rcif-tools/rfa2rcif.py`（2026-10-30 版本，含 GRep B-Rep 解码与扶梯修复）
- 运行环境：`rcif-web/.venv-cq`（Python 3.12.12，CadQuery 2.8.0，olefile 0.47）
- 机读报告：`out/_conversion_report_20261030.json`（含每个包的 SHA-256、包体校验结果）

## 转换结果

| 输入 RFA | FamilyId | 模板 | 类别 | 型号 | 部件 | 体积 m³ | 包大小 |
| :--- | :--- | :--- | :--- | ---: | ---: | ---: | ---: |
| AHU - 卧式 - 顶出后送式 - 2000-9000 CMH.rfa | CityAHU | ahu | MechanicalEquipment | 14 | 14 | 9.642 | 413,798 |
| JZ-固定窗.rfa | CityFixedWindow | door | Window | 1 | 15 | 0.077 | 1,158,683 |
| 全程水处理设备.rfa | CityWaterTreatment | universal | MechanicalEquipment | 1 | 21 | 0.435 | 408,789 |
| 分集水器 - 4 接口.rfa | CityManifold4 | manifold | MechanicalEquipment | 1 | 20 | 0.241 | 503,447 |
| 卷帘门.rfa | CityRollerDoor | door | Door | 3 | 6 | 0.033 | 795,927 |
| 双平开窗扇，带贴面.rfa | CityCasementWindow | door | Window | 3 | 6 | 0.025 | 764,627 |
| 可曲挠橡胶头.rfa | CityRubberJoint | rubber_joint | PipeAccessory | 1 | 3 | 0.015 | 371,331 |
| 玻璃单开门弧拱双侧窗.rfa | CityGlassDoor | door | Door | 1 | 6 | 0.032 | 790,733 |
| 离心泵 - 卧式.rfa | CityPump | pump | MechanicalEquipment | 1 | 10 | 0.069 | 483,884 |
| 管道消声器.rfa | CityDuctSilencer | silencer | MechanicalEquipment | 5 | 9 | 0.111 | 358,762 |
| 膨胀水箱-01.rfa | CityExpansionTank | tank | MechanicalEquipment | 12 | 6 | 0.074 | 423,708 |
| 螺旋旋转式冷水机组.rfa | CityChiller | universal | MechanicalEquipment | 1 | 30 | 11.852 | 647,731 |
| 金属外壳片式消声器.rfa | CityMetalSilencer | silencer | MechanicalEquipment | 1 | 9 | 0.111 | 451,087 |
| 风道消声器.rfa | CityDuctSilencer2 | silencer | MechanicalEquipment | 1 | 9 | 0.111 | 419,880 |

14/14 转换成功，无报错。

## 包体校验

对每个 `.rcif` 均执行：

- ZIP CRC 校验通过；
- `checksums.json` 中每个条目的 SHA-256 与包内实际字节一致；
- `geometry/default.step` 以 `ISO-10303-21` 开头，`display/preview.glb` 以 `glTF` 魔数开头；
- `component.json` 的 `identity.familyId`/`revision`/`name` 与文件名、会话记录一致；
- 包内保留 `source/<原文件名>.rfa`、`source/partatom.xml`、`source/grep.json` 与 `source/grep-candidates.obj`。

`qa.grep.brepDecoded` 现在为 `true`（例如 CityChiller：6 页、194 marker、42 实体、289 面、760 边），
但几何仍由 PartAtom 参数 + CadQuery 类别模板重建，并非 Revit 精确 B-Rep。

## 与重转换前旧包的差异

多数构件（AHU、分集水器、水泵、水箱、橡胶头、三种消声器）的模板、部件数与体积与旧包完全一致，
说明转换结果可重复。存在差异的构件：

| 构件 | 旧包 | 新包 | 说明 |
| :--- | :--- | :--- | :--- |
| CityFixedWindow | 8 部件 / 0.486 m³ | 15 部件 / 0.077 m³ | 门窗模板改为薄框 + 玻璃 + 竖梃，体积更接近真实窗扇 |
| CityRollerDoor | 8 / 0.061 | 6 / 0.033 | 同上 |
| CityCasementWindow | 8 / 0.135 | 6 / 0.025 | 同上 |
| CityGlassDoor | 8 / 0.167 | 6 / 0.032 | 同上 |
| CityChiller | 30 / 5.828 | 30 / 11.852 | universal 模板的原语分类改变，部分曲面实体改按 `revolve` 圆柱生成，出现超包围盒的叠体积（需复核） |
| CityWaterTreatment | 21 / 0.419 | 21 / 0.435 | 基本一致 |

## 已知边界（未在本次变更）

1. `CityChiller`、`CityWaterTreatment` 没有专属几何模板，走 `universal` GRep AABB/圆柱近似；
   部件互相重叠，体积总和可超过包围盒体积（CityChiller 11.852 m³ > 包围盒 9.66 m³），只能作占位几何。
2. 三种消声器（管道 / 金属外壳 / 风道）输出尺寸完全相同（1280×880×500 mm，0.111 m³），
   说明模板未读取各 RFA 的实际类型尺寸，而是套用默认参数。
3. `qa.note` 声明"GRep B-Rep 已完整解析"，指证据层面已解码，不代表生成的 STEP 与 Revit 几何等价；
   `qa.sourceGeometry.requiresProfessionalReview` 仍为 `true`。

## 复现命令

```powershell
$py = 'D:\QtOCCTApp\rcif-web\.venv-cq\Scripts\python.exe'
& $py rcif-tools/rfa2rcif.py `
  '铁路构件智能助手/城轨rfa转换/inputs/离心泵 - 卧式.rfa' `
  '铁路构件智能助手/城轨rfa转换/out/CityPump_1.0.0.rcif' `
  --family-id CityPump --name CityPump --revision 1.0.0
```

批量转换需要把 `TEMP` 指向工作区内可写目录；本次使用
`铁路构件智能助手/城轨rfa转换/out/_tmp`（沙箱对系统临时目录的写权限受限）。
