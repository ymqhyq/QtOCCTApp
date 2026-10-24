# RCIF 构件数据格式与工程落地规范 v1.1（正式定稿基线版）

- **版本**：1.1.0 (Final Baseline)
- **状态**：正式研发基线（Approved Engineering Baseline）
- **日期**：2026-10-17
- **前序版本**：[v1.0](file:///D:/QtOCCTApp/docs/rcif_component_format_spec_v1.0.md)
- **适用范围**：QtOCCTApp 构件库管理、参数化建模服务（`scripts-service`）、核心数据模型（`core-data-model`）及工程包（`.rde`）

---

## 0. v1.1 最终定稿修订说明

1. **修正 `cacheKey` 格式与浮点精度**：去掉路径不安全的 `sha256:` 前缀，统一使用 **64 个小写十六进制字符（256-bit SHA-256）** 以兼容 Windows 文件系统；浮点数规范化采用 **`%.12g` + 零值钳制（消除 `-0.0`）**，并包含 `cacheKeyVersion: 1`，兼顾亚微米级几何精度与 IEEE 754 尾数抗噪；删除冗余的 `parameterHash`，前后端与 Dapr 缓存统一使用单一 `cacheKey`。
2. **明确 `cacheKey` 与视觉材质的职责边界**：`BinXCAF` 内置材质仅表达子形状的默认物理材质分类（如混凝土、钢材、橡胶）；若某参数会改变几何体的默认视觉材质外观，必须将其列入 `recipe.inputs` 纳入 `cacheKey`；实例级调色与透明度覆盖由主文档 [`adGeometry`](file:///D:/QtOCCTApp/core-data-model/schema/core-data-model.yaml#L50-L60) 节点动态叠加。
3. **对齐 `access` 参数流向枚举**：与现有 [core-data-model.yaml](file:///D:/QtOCCTApp/core-data-model/schema/core-data-model.yaml) 保持严格一致，定义为 `input | inout | out | derived`（生成前表达式求值用 `derived`，生成后由服务端回写用 `out`）。
4. **打通 `shapeKey` 跨重启持久化与多输出传递闭环**：
   - 在 `BinXCAF` 子形状 Label 上使用 `TDataStd_AsciiString` 存储 `shapeKey`，保留 `TDataStd_Name` 供界面本地化展示；
   - 采用**“保序 `TopoDS_Compound` + `_shapeKeys` 序号映射”**机制，解决主文档 [`adGeometricDef`](file:///D:/QtOCCTApp/core-data-model/schema/core-data-model.yaml#L42-L49)（仅存 `TNaming_NamedShape`）在工程重启恢复后丢失 XCAF 子 Label `shapeKey` 的问题。
5. **明确 `.cbf` 方言自描述与多内核 JHB 载荷标识**：在 JSON 清单与 JHB 协议头中显式声明 `payloadKind` 与 `payloadFormat`，并在 `.cbf` 的 `0:1` 根标签写入方言标识。
6. **区分 `mesh-only` 权威网格与预览网格**：`mesh-only` 构件使用独立的 `geometry/default.glb`（或 `source/original.glb`）作为权威工程网格，`display/preview.glb` 仅用于构件库轻量卡片预览。
7. **结构化端口坐标引用与内置对象属性说明**：端口动态坐标采用结构化引用 `[{ "ref": "Length" }, 0.0, 0.0]` 免除数组字符串解析；明确分类条件中的 `$object.FunctionalType` 来源于 [`adObject`](file:///D:/QtOCCTApp/core-data-model/schema/core-data-model.yaml#L80-L98) 基类内置属性。
8. **规避 OCCT `SaveAs` 扩展名篡改陷阱**：明确使用 C++ `std::ofstream` 流式接口 `m_app->SaveAs(doc, ostream)` 写入 `.tmp` 临时文件后再调用 `MoveFileExW` 原子替换，防止 OCCT 因后缀非 `.cbf`/`.xbf` 自动追加后缀导致替换失败。

---

## 1. 规范定位与核心架构原则

### 1.1 格式家族定义

本规范通过三层载体职责分离，解决“参数化可变性”、“原生几何高性能加载”与“跨内核/跨格式兼容”之间的矛盾：

| 扩展名 | 名称 | 容器/底层类型 | 核心职责与定位 |
| :--- | :--- | :--- | :--- |
| **`.rcif`** | **构件定义包**<br>(CBF Component) | ZIP 压缩包 | **构件库中的“族模板（Family）”**。存储参数 Schema、规格型号表（Presets）、建模生成器配方、语义绑定契约及默认参数下的预览与可选默认几何。发布后不可原地修改。 |
| **`.cbf`** | **原生几何缓存**<br>(OCAF Binary) | `BinXCAF` / `ACTBin` | **运行时高性能“几何快照”**。构件几何缓存统一采用 `BinXCAF` 方言，按内容哈希 `<cacheKey>.cbf` 寻址存储，仅承载 BRep 拓扑、命名子形状（`shapeKey`）和默认子形状材质分类。 |
| **`.rde`** | **项目交付包**<br>(Project Package) | ZIP 压缩包 | **工程项目容器**。存储主模型树 `master.cbf`（`ACTBin` 方言，含构件实例引用 + 参数覆盖 + 空间布置）、二维图纸及去重几何缓存池（`geometry/<cacheKey>.cbf`）。 |

### 1.2 五大核心设计原则

1. **族模板与实例缓存彻底解耦**：`.rcif` 构件包定义“参数规则与生成配方”；项目实例仅存储“`componentId` + `revision` + 参数覆盖值（Overrides） + 空间布置（Placement）”。实例生成的 `.cbf` 存入项目包或本地缓存池，绝不回写构件库。
2. **文件极简内聚，禁止语义双写**：`.rcif` 包内仅设 **1 个唯一权威清单 `component.json`**，包内文件完整性统一由 `checksums.json` 负责（不在 `component.json` 内重复抄写文件哈希）；对于项目中的几何缓存，其参数与 `_shapeKeys` 映射直接记录在主文档的 [`adGeometricDef`](file:///D:/QtOCCTApp/core-data-model/schema/core-data-model.yaml#L42-L49) 节点中，不在磁盘上生成碎片化的伴随 JSON 文件，也不在 `BinXCAF` 内部重复写入业务属性树。
3. **语义与 2D 联动基于稳定技术键（`shapeKey`）**：三维实体、参考线（路肩线/坡脚线）、装配端口均通过生成器显式输出的 `shapeKey` 标识，彻底消除对易变 `TDF_Label` 路径（如 `0:1:4:1`）或拓扑面序号的依赖。
4. **代码级跨内核抽象优于静态 JSON 特征树**：采用 **Python 统一建模抽象接口（`cad_kernel_api`）** 驱动参数化重建，配合 C++ `IGeometryKernelAdapter` 适配 OCCT、中望（ZW3D）、华天（CrownCAD）等内核，不自研静态 JSON 特征历史树。
5. **交换格式按需惰性生成（Lazy Export）**：日常运行仅维护 `.cbf`（精确 BRep）与 `.glb`（显示网格）；高耗时的 `STEP AP242` 仅在跨内核迁移或归档交付时按需导出。

---

## 2. `.rcif` 构件定义包物理结构

每个 `.rcif` 为标准 Deflate ZIP 归档包，内部固定包含 **3~4 个基础文件**（内嵌脚本或外部导入构件可包含可选目录）：

```text
<family_id>_<revision>.rcif (ZIP 容器)
├── component.json          # [必须] 唯一权威清单（9 个顶级域，内聚身份、参数、型号表、配方、几何与语义）
├── geometry/
│   ├── default.cbf         # [可选] 默认参数下的 BinXCAF 原生几何（parametric/native-brep 构件推荐携带）
│   └── default.glb         # [可选] 仅当 recipe.mode == "mesh-only" 时作为权威工程网格
├── display/
│   ├── preview.glb         # [必须] 构件库卡片轻量预览网格（LOD 展示用途，不作为工程计算权威）
│   └── thumbnail.png       # [可选] 256x256 构件库卡片缩略图
├── generators/             # [可选] 仅当 recipe.generator.resolver == "embedded" 时存放包内 Python 脚本
│   └── <script_name>.py
├── source/                 # [可选] 仅当构件来源于外部 IFC / GLB / STEP 时保留原始文件与导入报告
│   ├── original.<ext>
│   └── import-report.json
└── checksums.json          # [必须] 包内除 checksums.json 与 signatures.json 外所有文件的 SHA-256 清单
```

### 2.1 `checksums.json` 格式与安全约束

```json
{
  "algorithm": "sha256",
  "files": {
    "component.json": "3a7bd3e2360a3d29eea436fcfb7e44c735d117c42d1c1835420b6b9942dd4f1b",
    "geometry/default.cbf": "8f434346648f6b96df89dda901c5176b10a6d83961dd3c1ac88b59b2dc327aa4",
    "display/preview.glb": "4b227777d4dd1fc61c6f884f48641d02b4d121d3fd328cb08b5531fcacdabf8a"
  }
}
```
- **覆盖范围**：必须包含包内除 `checksums.json` 自身（及可选签名文件 `signatures.json`）以外的**所有文件**（若存在可选签名文件 `signatures.json`，签名内容必须同时覆盖 `checksums.json` 与 `component.json`）。
- **解压安全策略**：解压时拒绝任何包含 `..`、前导 `/`、`\` 或盘符 `:` 的路径；单文件大小上限、总解压上限与最大压缩比阈值由宿主应用配置（默认建议：`component.json` $\le 4\text{ MB}$，单构件包解压总大小 $\le 512\text{ MB}$）。

---

## 3. `component.json` 权威清单规范

`component.json` 采用 UTF-8 编码，固定由 **9 个顶级字段**组成：`format`、`formatVersion`、`schemaVersion`、`identity`、`environment`、`parameters`、`recipe`、`geometry`、`semantics`。

### 3.1 完整结构示例（以路基边坡 `SubgradeSlope` 为例，几何指标严格对齐数学公式）

```json
{
  "format": "cbf-component",
  "formatVersion": 1,
  "schemaVersion": "1.1.0",

  "identity": {
    "componentId": "0199f3c2-7d5a-7c3e-9b1a-2f4d6e8a0b11",
    "familyId": "SubgradeSlope",
    "revision": "1.0.0",
    "name": "路基边坡",
    "category": "Subgrade",
    "objectType": "SubgradeSlope",
    "supportedFunctionalTypes": ["路堤边坡", "路堑边坡"],
    "status": "published",
    "placementSupport": {
      "modes": ["point", "curve"],
      "lengthPolicy": "fixedOrCurveDriven"
    }
  },

  "environment": {
    "units": { "length": "mm", "angle": "rad", "mass": "kg" },
    "coordinateSystem": {
      "origin": [0.0, 0.0, 0.0],
      "axis": [0.0, 0.0, 1.0],
      "refDirection": [1.0, 0.0, 0.0],
      "handedness": "right"
    },
    "tolerance": { "linear": 0.01, "angular": 0.001 }
  },

  "parameters": {
    "schema": [
      {
        "name": "ModelNumber",
        "label": "标准型号",
        "type": "enum",
        "options": ["CUSTOM", "STD-8M", "STD-12M"],
        "default": "STD-8M",
        "access": "input",
        "group": "Pset_SlopeGeometry"
      },
      {
        "name": "Length",
        "label": "边坡长度",
        "type": "real",
        "unit": "mm",
        "default": 20000.0,
        "min": 1000.0,
        "max": 200000.0,
        "access": "input",
        "group": "Pset_SlopeGeometry"
      },
      {
        "name": "Height",
        "label": "边坡高度",
        "type": "real",
        "unit": "mm",
        "default": 8000.0,
        "min": 100.0,
        "max": 50000.0,
        "access": "inout",
        "group": "Pset_SlopeGeometry"
      },
      {
        "name": "SlopeRatio",
        "label": "坡率(1:m)",
        "type": "real",
        "unit": "1",
        "default": 1.5,
        "min": 0.1,
        "max": 5.0,
        "access": "inout",
        "group": "Pset_SlopeGeometry"
      },
      {
        "name": "Width",
        "label": "坡面水平投影宽",
        "type": "real",
        "unit": "mm",
        "default": 12000.0,
        "access": "derived",
        "expr": "Height * SlopeRatio",
        "group": "Pset_SlopeGeometry"
      },
      {
        "name": "SlopeArea",
        "label": "坡面斜面积",
        "type": "real",
        "unit": "mm2",
        "default": 288444102.037,
        "access": "out",
        "group": "Pset_SlopeGeometry"
      },
      {
        "name": "StrengthGrade",
        "label": "混凝土强度等级",
        "type": "enum",
        "options": ["C30", "C35", "C40", "C50"],
        "default": "C30",
        "access": "input",
        "group": "Pset_MaterialConcrete"
      }
    ],
    "presets": {
      "STD-8M":  { "Height": 8000.0,  "SlopeRatio": 1.5 },
      "STD-12M": { "Height": 12000.0, "SlopeRatio": 1.75 }
    },
    "constraints": [
      {
        "id": "c_min_length",
        "expr": "Length >= 2000.0",
        "severity": "error",
        "message": "边坡长度不能小于最小示坡线间距 (2000mm)"
      }
    ]
  },

  "recipe": {
    "mode": "parametric",
    "authority": "generator",
    "apiVersion": "cad_kernel_api/1.0",
    "cacheKeyVersion": 1,
    "canonicalExport": "lazy",
    "deterministic": true,
    "timeoutSeconds": 60,
    "sandbox": true,
    "generator": {
      "id": "SubgradeSlope",
      "version": "1.2.0",
      "resolver": "scripts-service",
      "entryPoint": "subgrade_slope",
      "codeHash": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
    },
    "inputs": {
      "Length": "Length",
      "Height": "Height",
      "SlopeRatio": "SlopeRatio"
    },
    "kernelSupport": {
      "supported": ["occt", "zw3d", "huatian"],
      "requiredCapabilities": ["exactBrep", "booleans"]
    },
    "outputs": [
      { "shapeKey": "Body",         "role": "body",      "geomType": "solid", "required": true },
      { "shapeKey": "ShoulderLine", "role": "reference", "geomType": "wire",  "required": true },
      { "shapeKey": "ToeLine",      "role": "reference", "geomType": "wire",  "required": true }
    ]
  },

  "geometry": {
    "capabilities": {
      "exactBrep": true,
      "booleans": true,
      "parametric": true,
      "mesh": true
    },
    "defaultArtifact": {
      "cacheKey": "8f434346648f6b96df89dda901c5176b10a6d83961dd3c1ac88b59b2dc327aa4",
      "cacheKeyVersion": 1,
      "generatorVersion": "1.2.0",
      "kernel": { "id": "occt", "version": "8.0.0", "adapterVersion": "1.0.0" },
      "nativeFile": "geometry/default.cbf",
      "payloadKind": "BinXCAF",
      "displayFile": "display/preview.glb",
      "metrics": {
        "volume": 960000000000.0,
        "area": 784444102.037,
        "bbox": [0.0, 0.0, 0.0, 20000.0, 12000.0, 8000.0],
        "watertight": true
      }
    }
  },

  "semantics": {
    "classification": [
      { "system": "IFC", "code": "IfcEarthworksFill", "condition": "$object.FunctionalType == '路堤边坡'" },
      { "system": "IFC", "code": "IfcEarthworksCut",  "condition": "$object.FunctionalType == '路堑边坡'" }
    ],
    "materials": [
      {
        "materialId": "mat_concrete",
        "category": "Concrete",
        "gradeParam": "StrengthGrade"
      }
    ],
    "ports": [
      {
        "portId": "StartSection",
        "domain": "alignment",
        "origin": [0.0, 0.0, 0.0],
        "unit": "mm",
        "axis": [0.0, 0.0, 1.0],
        "refDirection": [1.0, 0.0, 0.0]
      },
      {
        "portId": "EndSection",
        "domain": "alignment",
        "origin": [{ "ref": "Length" }, 0.0, 0.0],
        "unit": "mm",
        "axis": [0.0, 0.0, 1.0],
        "refDirection": [1.0, 0.0, 0.0]
      }
    ],
    "bindings": [
      {
        "id": "bind_height",
        "subject": { "kind": "shape", "ref": "Body" },
        "predicate": { "kind": "property", "ref": "Pset_SlopeGeometry.Height" },
        "value": { "kind": "parameter", "ref": "Height" }
      },
      {
        "id": "bind_material",
        "subject": { "kind": "shape", "ref": "Body" },
        "predicate": { "kind": "material", "ref": "mat_concrete" },
        "value": { "kind": "parameter", "ref": "StrengthGrade" }
      },
      {
        "id": "bind_volume",
        "subject": { "kind": "shape", "ref": "Body" },
        "predicate": { "kind": "quantity", "ref": "Qto_Body.NetVolume" },
        "value": { "kind": "geometryMeasure", "ref": "Body", "measure": "volume", "unit": "mm3" }
      },
      {
        "id": "bind_shoulder",
        "subject": { "kind": "shape", "ref": "ShoulderLine" },
        "predicate": { "kind": "drawingRole", "ref": "SlopeIndication.ShoulderLine" },
        "value": { "kind": "literal", "value": "ShoulderLine" }
      },
      {
        "id": "bind_toe",
        "subject": { "kind": "shape", "ref": "ToeLine" },
        "predicate": { "kind": "drawingRole", "ref": "SlopeIndication.ToeLine" },
        "value": { "kind": "literal", "value": "ToeLine" }
      }
    ]
  }
}
```

### 3.2 参数求值流水线（`input` / `inout` / `derived` / `out`）

为与 [core-data-model.yaml](file:///D:/QtOCCTApp/core-data-model/schema/core-data-model.yaml) 完全兼容，参数 `access` 固定为四种枚举（加载器如遇到历史 `"output"` 自动归一化为 `"out"`）：

| `access` 类型 | 是否允许实例覆盖 | 求值时机 | 纳入 `cacheKey` | 说明 |
| :--- | :---: | :--- | :---: | :--- |
| **`input`** | 是 | 第 1~3 步（建模前） | 仅当在 `recipe.inputs` 中映射时纳入 | 标准输入参数（如 `Length`、`ModelNumber`） |
| **`inout`** | 是 | 第 2 步查表默认 + 第 3 步允许用户覆盖 + 第 5 步允许服务修正 | 仅当在 `recipe.inputs` 中映射时纳入 | 规格联动且可手动微调的参数（如墩高 `Height`） |
| **`derived`** | 否 | 第 4 步（建模前由客户端表达式 `expr` 求值） | 否（由输入决定） | 纯公式派生参数（如 `Width = Height * SlopeRatio`） |
| **`out`** | 否 | 第 5 步（建模服务执行后通过 JHB `args` 回写） | 否 | 依赖复杂几何求值或服务端查表得出的只读结果参数 |

**五步求值顺序**：
1. **默认值初始化**：读取 `schema[*].default`；
2. **型号表覆盖**：若 `ModelNumber` 命中 `presets[ModelNumber]`，用预设字典覆盖对应的 `inout` / `input` 参数（注：若 `ModelNumber` 仅用于查表展开 `presets` 而不直接改变生成器拓扑分支，无需放入 `recipe.inputs`；若生成器内部直接读取 `ModelNumber` 分支建模，则必须放入 `recipe.inputs`）；
3. **实例覆盖**：应用项目实例的 `parameterOverrides`（仅作用于 `input` 与 `inout`）；
4. **前端派生计算**：对 `access == "derived"` 的参数计算 `expr`，并校验 `constraints`；
5. **生成器执行与回写**：用第 1~4 步解析后的最终输入参数计算 `cacheKey`，未命中缓存时调用生成器，将返回的 `args` 按 `%.17g`（或 `std::to_chars`）无损回写至 `out` / `inout` 属性节点。

> **内置对象上下文（`$object.*`）说明**：在 `semantics.classification[*].condition` 中，除引用 `parameters.schema` 中的参数外，允许通过 `$object.ObjectType`、`$object.FunctionalType`、`$object.StructuralType` 引用 [`adObject`](file:///D:/QtOCCTApp/core-data-model/schema/core-data-model.yaml#L80-L98) 实例的基础分类属性。

### 3.3 生成器解析模式与 `defaultArtifact` 可选性

- **`recipe.generator.resolver`**：
  - `"scripts-service"`（默认，外部服务解析）：构件包内不含脚本，通过 `generator.id` + `version` 请求建模服务执行，服务端返回实际执行的脚本 `codeHash` 供校验。
  - `"embedded"`（包内自包含脚本）：脚本位于 `.rcif` 包内 `generators/<entryPoint>.py`，必须纳入 `checksums.json`，且仅在沙箱策略允许时执行。
- **`geometry.defaultArtifact`（可选字段）**：当构件包仅包含 `display/preview.glb` 而不携带预编译 `geometry/default.cbf` 时，`defaultArtifact` 可设为 `null`。若非空，加载时需校验 `defaultArtifact.cacheKey` 与当前默认参数算出的 `cacheKey` 一致，否则视为过期缓存（`stale`）并在首次实例化时重新生成。

### 3.4 项目实例引用与布置结构（支持 `point` 与 `curve`）

在项目模型（`.rde`）中，构件实例根据 `placementSupport.modes` 选择具体布置模式：

```json
// 模式 A：点式刚体布置 (point) —— 适用于桥墩、承台、支座等
{
  "instanceId": "0199f3c5-1122-7000-8000-000000000001",
  "componentId": "0199f3c2-7d5a-7c3e-9b1a-2f4d6e8a0b11",
  "revision": "1.0.0",
  "parameterOverrides": { "ModelNumber": "STD-8M", "Height": 9500.0 },
  "placement": {
    "mode": "point",
    "origin": [1000.0, 2000.0, 0.0],
    "axis": [0.0, 0.0, 1.0],
    "refDirection": [1.0, 0.0, 0.0]
  }
}

// 模式 B：路线曲线驱动布置 (curve) —— 适用于路基边坡、箱梁等线性构件
{
  "instanceId": "0199f3c5-1122-7000-8000-000000000002",
  "componentId": "0199f3c2-7d5a-7c3e-9b1a-2f4d6e8a0b11",
  "revision": "1.0.0",
  "parameterOverrides": { "ModelNumber": "STD-12M" },
  "placement": {
    "mode": "curve",
    "alignmentRef": "alignment-mainline-01",
    "stationUnit": "mm",
    "stationStart": 1200000.0,
    "stationEnd": 1220000.0,
    "lateralOffset": 0.0,
    "verticalOffset": 0.0,
    "crossfall": 0.0,
    "orientationRule": "tangent"
  }
}
```

---

## 4. 原生几何缓存 `.cbf` 内部结构与跨重启持久化规范

### 4.1 `.cbf` 方言自描述与流式保存强制规范

1. **外部清单**：任何引用 `.cbf` 的 JSON 字段必须声明 `"payloadKind": "BinXCAF" | "ACTBin"`。
2. **内部自描述**：在保存 `.cbf` 时，在文档根标签 `0:1`（`doc->Main()`）写入 `TDataStd_Comment`（或 `TDataStd_AsciiString`）属性：
   - 构件几何缓存（`BinXCAF`）：`"CBF-Payload:BinXCAF;v1.1"`
   - 项目/图纸主文档（`ACTBin`）：`"CBF-Payload:ACTBin;v1.1"`
3. **【重要】流式保存强制约定**：OCCT 的 `BinXCAFDrivers` 默认注册扩展名为 `.xbf`，若直接向 `m_app->SaveAs(doc, path)` 传入 `.cbf` 或 `.tmp` 文件路径，OCCT 可能自动追加 `.xbf` 后缀导致原路径文件不存在（见 [`scripts-service/main.py`](file:///D:/QtOCCTApp/scripts-service/main.py#L308)）。因此，C++ 中保存 `.cbf` 或 `.tmp` 临时文件时，**必须统一使用 `std::ofstream` 二进制流重载 `m_app->SaveAs(doc, ofs)`**。

### 4.2 `BinXCAF` 中的命名几何锚点（`shapeKey`）标签树

严禁将显示名称 `TDataStd_Name` 直接当作技术键 `shapeKey` 使用。构件 `BinXCAF` 文档建议遵循以下层次布局（**注：具体 Label 路径如 `0:1:1:1:1` 仅为推荐布局，跨版本与跨文件解析必须以匹配子 Label 的 `TDataStd_AsciiString` 技术键 `shapeKey` 为唯一契约**）：

```text
0:1 (Doc Main)
 ├── TDataStd_Comment: "CBF-Payload:BinXCAF;v1.1"
 └── 0:1:1 (XCAFDoc_ShapeTool Root)
      └── 0:1:1:1 [Component Root Compound]
           │  ├── TNaming_NamedShape: TopoDS_Compound (按 outputs 顺序包含各子形状)
           │  ├── TDataStd_Name: "路基边坡"                    <-- 显示名称 (可本地化)
           │  └── TDataStd_AsciiString: "<64字符hex cacheKey>" <-- 根形状缓存主键 (兼容现有代码)
           │
           ├── 0:1:1:1:1 [Sub-Shape 1: 主体实体]
           │    ├── TNaming_NamedShape: TopoDS_Solid / TopoDS_Shell
           │    ├── TDataStd_Name: "边坡主体"                  <-- UI 显示名称
           │    ├── TDataStd_AsciiString: "Body"               <-- [核心] 稳定技术键 shapeKey
           │    └── XCAFDoc_VisMaterial / Color                <-- 子形状默认物理材质分类
           │
           ├── 0:1:1:1:2 [Sub-Shape 2: 路肩特征线]
           │    ├── TNaming_NamedShape: TopoDS_Wire / TopoDS_Edge
           │    ├── TDataStd_Name: "路肩特征线"
           │    └── TDataStd_AsciiString: "ShoulderLine"       <-- [核心] 稳定技术键 shapeKey
           │
           └── 0:1:1:1:3 [Sub-Shape 3: 坡脚特征线]
                ├── TNaming_NamedShape: TopoDS_Wire / TopoDS_Edge
                ├── TDataStd_Name: "坡脚特征线"
                └── TDataStd_AsciiString: "ToeLine"            <-- [核心] 稳定技术键 shapeKey
```

### 4.3 `adGeometricDef` 磁盘缓存恢复时的 `shapeKey` 保序重建机制

在现有架构中，[`GeometryService::BuildGeometry`](file:///D:/QtOCCTApp/core-data-model/GeometryService.cpp#L690-L715) 会将生成的 `TopoDS_Shape` 存入主文档几何分区的 [`adGeometricDef`](file:///D:/QtOCCTApp/core-data-model/schema/core-data-model.yaml#L42-L49) 节点。由于 `adGeometricDef` 的 `Shape` 参数（`ActData_ShapeParameter`）只持久化 `TNaming_NamedShape` 而不包含 XCAF 子 Label 属性，必须遵循以下**保序重建规范**：

1. **生成与打包期**：Python 生成器脚本定义字典 `outputs = {"Body": solid, "ShoulderLine": wire1, "ToeLine": wire2}`（若仅定义 `result` 变量，自动包装为 `{"Body": result}`）。`scripts-service` 与 [`brep_to_cbf.cpp`](file:///D:/QtOCCTApp/core-data-model/tools/brep_to_cbf.cpp) 按有序键列表构造顶层 `TopoDS_Compound`，并在 `protoLabel` 下按相同顺序创建子 Label 写入 `TDataStd_AsciiString(shapeKey)`。
2. **写入 `adGeometricDef` 期**：在 [`GeometryService::CreateGeoDef`](file:///D:/QtOCCTApp/core-data-model/GeometryService.cpp#L411-L426) 中，将有序键数组 `"_shapeKeys": ["Body", "ShoulderLine", "ToeLine"]` 一并写入 `adGeometricDef->SetGeoParameter(...)` 的 JSON 中。
3. **重启恢复期**：当 `FindCachedGeoDef` 命中磁盘 `adGeometricDef` 时，通过 `TopoDS_Iterator` 遍历 `cachedGeoDef->GetShape()` 的直接子形状（OCCT 严格保证 `TopoDS_Compound` 子形状序列化前后顺序不变），对照 `GeoParameter` 中的 `_shapeKeys` 数组，在 `m_xcafDoc` 的 `protoLabel` 下完整恢复带 `TDataStd_AsciiString(shapeKey)` 的子 Label。

---

## 5. 缓存键（`cacheKey`）计算规范与 JHB 协议 v1.1

### 5.1 确定性 `cacheKey` 计算公式（前后端与 Dapr 统一同源）

`cacheKey` 统一定义为 **256 位 SHA-256 哈希的 64 个纯小写十六进制字符表示（不含 `sha256:` 前缀）**，可直接安全用作 Windows/Linux 文件名（如 `geometry/8f434346...aa4.cbf`）：

$$\text{cacheKey} = \text{HexLower}\left(\text{SHA256}\left(\text{CanonicalJSON}(\text{CacheKeyPayload})\right)\right)$$

`CacheKeyPayload` 结构如下：
```json
{
  "cacheKeyVersion": 1,
  "codeHash": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
  "generatorId": "SubgradeSlope",
  "generatorVersion": "1.2.0",
  "inputParams": {
    "Height": 8000,
    "Length": 20000,
    "SlopeRatio": 1.5
  },
  "kernelId": "occt",
  "kernelVersion": "8.0.0",
  "tolerance": { "angular": 0.001, "linear": 0.01 },
  "units": { "angle": "rad", "length": "mm" }
}
```

**浮点数规范化与材质职责准则**：
1. **零值钳制**：若 `std::abs(val) < 1e-12`，统一置为 `0.0`（消除 `-0.0` 与浮点计算残差）。
2. **`%.12g` 缓存键格式化**：计算 `cacheKey` 时，所有 `real` 参数按 `std::snprintf(buf, sizeof(buf), "%.12g", val)` 格式化后输出为 JSON Number（注：参数回写 `WriteBackParams` 时使用 `%.17g` 或 `std::to_chars` 保持双精度完整往返；而计算几何 `cacheKey` 时采用 `%.12g`，在 100 米尺度下分辨率达 $0.1\ \mu\text{m}$，既远高于 `0.01 mm` 工程公差，又能彻底屏蔽 IEEE 754 第 16~17 位尾数噪声）。
3. **前后端统一缓存键**：客户端计算出 `cacheKey` 后通过请求字段 `param_geo_id`（或 `cache_key`）传给 `scripts-service`，服务端直接以该 `cacheKey` 作为 Dapr 状态存储主键，废除 [`scripts-service/main.py`](file:///D:/QtOCCTApp/scripts-service/main.py#L100-L108) 中未规范化浮点数的独立 `compute_cache_key` 函数。
4. **几何与视觉材质参数边界**：
   - `inputParams` 收录 `recipe.inputs` 中声明的所有经过 `presets` 和 `parameterOverrides` 解析后的最终输入参数（及 `curve` 模式下的基准曲线哈希）。
   - `BinXCAF` 内置的 `XCAFDoc_VisMaterial` 仅代表子形状的默认物理材质分类（如混凝土、钢板、橡胶块）。若某个构件参数确实会改变生成几何体的默认视觉材质外观，构件作者必须将该参数列入 `recipe.inputs` 使其进入 `cacheKey`；反之，纯力学指标（如混凝土 `C30` vs `C50`）不改变外观，不进入 `cacheKey`；实例级的颜色与透明度覆盖由主文档 [`adGeometry`](file:///D:/QtOCCTApp/core-data-model/schema/core-data-model.yaml#L50-L60) 在渲染时动态应用。

### 5.2 建模服务 JHB（JSON-Header-Binary）协议 v1.1

二进制流格式：`[4字节 uint32 Little-Endian JSON长度][UTF-8 JSON元数据][几何二进制载荷]`。
- **大小限制**：默认 JSON 元数据上限 `16 MB`，二进制载荷上限由宿主应用配置（默认 `256 MB`，特大型桥梁/路基模型可在服务端配置中上调）。
- **JSON 元数据契约（支持多内核载荷声明与 `_shapeKeys` 传递）**：
```json
{
  "protocolVersion": "1.1",
  "status": "ok",
  "error": null,
  "cache_key": "8f434346648f6b96df89dda901c5176b10a6d83961dd3c1ac88b59b2dc327aa4",
  "cache_key_version": 1,
  "generator_version": "1.2.0",
  "kernel": { "id": "occt", "version": "8.0.0", "adapterVersion": "1.0.0" },
  "payloadKind": "BinXCAF",
  "payloadFormat": "cbf",
  "shapeKeys": ["Body", "ShoulderLine", "ToeLine"],
  "args": {
    "Length": 20000.0,
    "Height": 8000.0,
    "SlopeRatio": 1.5,
    "SlopeArea": 288444102.037
  },
  "outputs": {
    "Body": { "geomType": "solid", "volume": 960000000000.0, "bbox": [0, 0, 0, 20000, 12000, 8000] },
    "ShoulderLine": { "geomType": "wire", "length": 20000.0 },
    "ToeLine": { "geomType": "wire", "length": 20000.0 }
  },
  "anchors": {
    "StartSection": { "origin": [0, 0, 0], "axis": [0, 0, 1], "refDirection": [1, 0, 0] },
    "EndSection": { "origin": [20000, 0, 0], "axis": [0, 0, 1], "refDirection": [1, 0, 0] }
  }
}
```

---

## 6. 多源几何（IFC / GLB）与国产几何内核兼容方案

### 6.1 多源几何能力矩阵与实际导入判定

构件的 `exactBrep` 等能力必须以实际导入转换后的结果为准并记录在 `geometry.capabilities`（及 `source/import-report.json`）中：

| 来源模式 (`recipe.mode`) | 权威几何文件 (`authority`) | `exactBrep` | `parametric` | 运行时准入与降级策略 |
| :--- | :--- | :---: | :---: | :--- |
| **`parametric`** | `generator`（缓存于 `default.cbf`） | `true` | `true` | 支持变参重建、精确布尔、工程量计算及 2D 特征线提取。 |
| **`native-brep`** | `geometry/default.cbf` | `true` | `false` | 静态精确 BRep，支持布尔与精确测量，不支持参数驱动变形。 |
| **`imported-ifc`** | `source/original.ifc`（派生 `default.cbf`） | **视导入结果而定** | `false` | 若 IFC 实体为 SweptAreaSolid/BRep 则转为精确 BRep（`exactBrep=true`）；若原始 IFC 为 TriangulatedFaceSet 网格，则标记 `exactBrep=false`。 |
| **`mesh-only`** | `geometry/default.glb` 或 `source/original.glb` | `false` | `false` | 权威几何为独立工程网格 `geometry/default.glb`（`display/preview.glb` 仅作缩略图预览）；**严禁参与精确 BRep 布尔运算与体积工程量统计**。 |

> **静态几何子形状选择器（Selector）规范**：当 `native-brep` 或 `imported-ifc` 构件未预置子形状名称时，允许在 `bindings` 中使用基于**归一化包围盒坐标（Normalized BBox `[0.0, 1.0]`）**与法向极值的选择器（例如 `"filter": { "type": "face", "normal": [0,0,1], "normalizedCentroidZ": { "min": 0.98 } }`），严禁使用写死的绝对毫米坐标。

### 6.2 面向中望（ZW3D）、华天（CrownCAD）等国产内核的适配架构

通过 **`cad_kernel_api`（Python 建模层）** + **`IGeometryKernelAdapter`（C++ 运行时层）** 双端隔离具体几何内核，内核原生文件扩展名由各自 Adapter 动态声明：

```cpp
class IGeometryKernelAdapter {
public:
    virtual ~IGeometryKernelAdapter() = default;
    virtual KernelInfo info() const = 0;            // 返回 kernelId ("occt"|"zw3d"|"huatian"), version, nativeFormat
    virtual CapabilitySet capabilities() const = 0; // 返回当前内核支持的能力集合

    // 参数化求值与命名子形状解析
    virtual bool evaluate(const RecipeSpec& recipe, const nlohmann::json& inputParams, std::string& outCacheKey) = 0;
    virtual bool resolveShapeKeys(const std::string& cacheKey, std::vector<std::string>& outShapeKeys) = 0;

    // 加载与保存内核原生几何缓存
    virtual bool loadNativeCache(const std::string& cacheKey, const std::string& filePath) = 0;
    virtual bool saveNativeCache(const std::string& cacheKey, const std::string& filePath) = 0;

    // 按 (cacheKey, shapeKey) 提取几何测量指标与离散渲染网格，向业务层屏蔽 TopoDS_Shape
    virtual bool queryShapeMetrics(const std::string& cacheKey, const std::string& shapeKey, ShapeMetrics& out) = 0;
    virtual bool tessellate(const std::string& cacheKey, double linearDeflection, MeshData& outMesh) = 0;

    // 跨内核惰性 STEP AP242 导入导出
    virtual bool exportStepCanonical(const std::string& cacheKey, const std::string& stepPath) = 0;
    virtual bool importStepCanonical(const std::string& stepPath, const std::string& outCacheKey) = 0;
};
```

---

## 7. 现有代码库落地改造清单（研发实施路线）

### 阶段一：修复现有基础缺陷，打通 `shapeKey` 闭环（P0，立即执行）

1. **修复参数回写精度丢失**（[GeometryService.cpp](file:///D:/QtOCCTApp/core-data-model/GeometryService.cpp#L638-L645)）：
   - 将 `WriteBackParams` 中的 `std::to_string(val.get<double>())` 替换为 17 位无损浮点格式化（`std::snprintf(buf, sizeof(buf), "%.17g", val.get<double>())` 或 `std::to_chars`）。
2. **升级 `ComputeParamGeoID` 为 SHA-256（`%.12g` 规范化）并统一前后端缓存键**（[GeometryService.cpp](file:///D:/QtOCCTApp/core-data-model/GeometryService.cpp#L501-L509)、[scripts-service/main.py](file:///D:/QtOCCTApp/scripts-service/main.py#L100-L108)）：
   - 实现第 5.1 节的 `CacheKeyPayload` 规范化序列化（含 `%.12g` 与 `-0.0` 钳制）及 64 字符小写 Hex SHA-256 计算，服务端直接使用同一 `cacheKey`。
3. **支持多命名子形状（`shapeKey`）打包、合并与跨重启恢复**（[brep_to_cbf.cpp](file:///D:/QtOCCTApp/core-data-model/tools/brep_to_cbf.cpp)、[GeometryService.cpp](file:///D:/QtOCCTApp/core-data-model/GeometryService.cpp#L300-L381)）：
   - 升级 `scripts-service` 与 `brep_to_cbf.cpp`，支持读取 Python 脚本输出的 `outputs` 字典，按序组装 `TopoDS_Compound` 并在 `protoLabel` 子节点写入 `TDataStd_AsciiString(shapeKey)`；
   - 在 `CreateGeoDef` 与 `FindCachedGeoDef` 中实现第 4.3 节的 `_shapeKeys` 保序持久化与重建；
   - 在 `GeometryService` 中新增 `GetSubShapeByKey(const std::string& cacheKey, const std::string& shapeKey)`，打通 [`adSlopeIndication`](file:///D:/QtOCCTApp/core-data-model/schema/core-data-model.yaml#L137-L148) 二维示坡线自动更新闭环。
4. **补齐 `ProjectManager::SaveAll()` 的流式原子替换保护**（[ProjectManager.cpp](file:///D:/QtOCCTApp/core-data-model/ProjectManager.cpp#L202-L218)）：
   - [ProjectPackage.cpp](file:///D:/QtOCCTApp/src/ProjectPackage.cpp#L1134) 已使用 `QSaveFile`，无需改动；
   - 针对 `ProjectManager::SaveAll()`，改为先通过 `std::ofstream` 二进制流调用 `m_app->SaveAs(doc, ofs)` 写入同目录 `<filename>.tmp`（避免 OCCT 自动追加后缀），流刷新关闭且状态为 `PCDM_SS_OK` 后，调用 `MoveFileExW(tempPath, targetPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)` 原子覆盖目标文件。

### 阶段二：落地 `.rcif` 构件包读写器与型号表驱动（P1，中期迭代）

1. **实现 `RcifPackageReader / Writer`**：支持 `.rcif` 包的打包、`checksums.json` 校验及 `component.json` 解析。
2. **接入 `presets` 规格型号表与 `derived` 表达式求值**：在 `GeometryService::ExtractGeoParams` 中实现第 3.2 节的五步参数求值流水线。
3. **完善 `.rde` 工程包 `manifest.json`**：在 `ProjectPackage.cpp` 中填入 `schemaVersion`、`coordinateSystem` 及每个 `.cbf` 条目的 `payloadKind`（`ACTBin` / `BinXCAF`）。

### 阶段三：多内核抽象与构件库检索服务（P2，长期演进）

1. **下沉 `OcctKernelAdapter`**：将 `GeometryService` 直接操作 `TopoDS_Shape` 的逻辑收敛到 `IGeometryKernelAdapter` 实现类中。
2. **构建 `catalog.sqlite` 本地构件库索引**：扫描 `.rcif` 包内的 `component.json` 提供按分类、功能类型和规格型号的快速检索。
