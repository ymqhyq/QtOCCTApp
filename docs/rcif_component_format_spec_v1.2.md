# RCIF 构件数据格式与工程落地规范 v1.2（正式定稿基线版）

- **版本**：1.2.0 (Final Baseline)
- **状态**：正式研发基线（Approved Engineering Baseline）
- **日期**：2026-10-18
- **前序版本**：[v1.0](file:///D:/QtOCCTApp/docs/rcif_component_format_spec_v1.0.md) | [v1.1](file:///D:/QtOCCTApp/docs/rcif_component_format_spec_v1.1.md)
- **适用范围**：QtOCCTApp 构件库管理、参数化建模服务（`scripts-service`）、核心数据模型（`core-data-model`）、IFC 4.3 导出服务（`IfcExportService`）及工程包（`.rde`）

---

## 0. v1.2 相对 v1.1 的核心升级说明

1. **废除冗长的三元组 `bindings` 与重复的 `recipe.outputs`，统一收敛为 `semantics.shapes` 单表**：
   - 将原本分散在 `recipe.outputs`、`semantics.classification`、`semantics.subshapes`、`semantics.bindings` 四处的定义，合并为以 `shapeKey` 为键的面向对象字典 **`semantics.shapes`**；
   - 体（`Body`）、面（`SlopeFace`）、线（`ShoulderLine`、`ToeLine`）在 `shapes` 中平级对齐，每个几何部位的**中文标签、几何类型、基数约束（`cardinality`）、行业分类编码（`classCode`）、IFC 4.3 映射（`ifc`）、材质、属性绑定与二维制图角色（`drawingRole`）**就近内聚在同一对象下，JSON 体积缩减 60%，C++ 解析一次遍历即可完成。
2. **新增 `<shapeKey>/<instanceTag>` 同类型多几何区分与自动配对机制**：
   - 解决“双侧边坡（左/右两条路肩线与坡脚线）”、“多级边坡（动态 $N$ 级坡面与特征线）”及“群桩基础（动态 $M \times N$ 根桩）”下同类型语义几何的区分问题；
   - `component.json` 的 `shapes` 中每种语义槽位只定义一次（声明 `"cardinality": "1..*"`）；建模引擎输出 `"ShoulderLine/Left"`、`"ShoulderLine/Right"`（或直接输出数组自动展开为 `"ShoulderLine/1"`、`"ShoulderLine/2"`）；
   - 下游消费时按 `/` 切分：前缀 `<shapeKey>` 查表复用分类编码与 IFC 规则，后缀 `<instanceTag>` 区分个体并实现左右/分级二维示坡线的自动同名配对。
3. **强化构件族多预定义规格（`presets`）与 IFC 4.3 / 行业分类编码标准映射**：
   - 支持在 `parameters.presets` 中定义多个标准规格型号（共享同一 `.rcif` 构件族模板），支持简写参数字典与带规格元数据（`label`、`drawingCode`、`classCode`、`values`）的完整模式；
   - 明确局部几何（如路基边坡面 `SlopeFace`、路肩线 `ShoulderLine`、坡脚线 `ToeLine`）向 **IFC 4.3 (`IFC4X3_ADD2`)** 的 `IfcShapeAspect` / `IfcShapeRepresentation` 以及行业分类编码标准（如铁路 `CRBIM` / 国标 `GB/T 51269`）的标准导出规则。

---

## 1. 规范定位与核心架构原则

### 1.1 格式家族定义

本规范通过三层载体职责分离，解决“参数化可变性”、“原生几何高性能加载”与“跨内核/跨格式兼容”之间的矛盾：

| 扩展名 | 名称 | 容器/底层类型 | 核心职责与定位 |
| :--- | :--- | :--- | :--- |
| **`.rcif`** | **构件定义包**<br>(CBF Component) | ZIP 压缩包 | **构件库中的“族模板（Family）”**。存储参数 Schema、多规格型号表（Presets）、建模生成器配方、命名几何语义字典（`shapes`）及默认预览/缓存几何。发布后不可原地修改。 |
| **`.cbf`** | **原生几何缓存**<br>(OCAF Binary) | `BinXCAF` / `ACTBin` | **运行时高性能“几何快照”**。构件几何缓存统一采用 `BinXCAF` 方言，按内容哈希 `<cacheKey>.cbf` 寻址存储，仅承载 BRep 拓扑、命名子形状标签（`<shapeKey>` 或 `<shapeKey>/<instanceTag>`）和默认子形状材质分类。 |
| **`.rde`** | **项目交付包**<br>(Project Package) | ZIP 压缩包 | **工程项目容器**。存储主模型树 `master.cbf`（`ACTBin` 方言，含构件实例引用 + 型号/参数覆盖 + 空间布置）、二维图纸及去重几何缓存池（`geometry/<cacheKey>.cbf`）。 |

### 1.2 五大核心设计原则

1. **族模板、规格型号与实例缓存三级解耦**：
   - **族模板（`.rcif`）**：定义共用的参数集合、生成脚本和语义映射规则；
   - **规格型号（`presets`）**：在同一族模板内预置多组标准参数组合（如 `STD-8M`、`STD-12M`）；
   - **项目实例（Instance）**：仅存储“`componentId` + `revision` + 所选 `ModelNumber` 及参数覆盖（Overrides） + 空间布置（Placement）”，根据展开后的最终参数生成独立的 `<cacheKey>.cbf` 存入缓存池。
2. **文件极简内聚，禁止语义双写**：`.rcif` 包内仅设 **1 个唯一权威清单 `component.json`**，文件完整性统一由 `checksums.json` 负责；项目中的几何缓存通过主文档 [`adGeometricDef`](file:///D:/QtOCCTApp/core-data-model/schema/core-data-model.yaml#L42-L49) 记录参数与 `_shapeKeys` 映射，不在磁盘上生成伴随 JSON 文件，也不在 `BinXCAF` 内重复写入业务属性树。
3. **命名几何即语义锚点（`shapes` 单表模型）**：建模引擎输出的命名几何字典与 `component.json` 中的 `semantics.shapes` 严格对应，通过 `<shapeKey>/<instanceTag>` 两级命名天然支持单实例与动态多实例。
4. **代码级跨内核抽象优于静态 JSON 特征树**：采用 **Python 统一建模抽象接口（`cad_kernel_api`）** 驱动参数化重建，配合 C++ `IGeometryKernelAdapter` 适配 OCCT、中望（ZW3D）、华天（CrownCAD）等内核。
5. **交换格式按需惰性生成（Lazy Export）**：日常运行仅维护 `.cbf`（精确 BRep）与 `.glb`（显示网格）；高耗时的 `STEP AP242` 与 `IFC 4.3` 仅在跨内核迁移或归档交付时按需导出。

---

## 2. `.rcif` 构件定义包物理结构

每个 `.rcif` 为标准 Deflate ZIP 归档包，内部固定包含 **3~4 个基础文件**（内嵌脚本或外部导入构件可包含可选目录）：

```text
<family_id>_<revision>.rcif (ZIP 容器)
├── component.json          # [必须] 唯一权威清单（内聚身份、参数、型号表、配方、几何与语义）
├── geometry/
│   ├── default.cbf         # [可选] 默认参数下的 BinXCAF 原生几何（parametric/native-brep 构件推荐携带）
│   └── default.glb         # [可选] 仅当 identity.sourceMode == "mesh-only" 时作为权威工程网格
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
- **覆盖范围**：必须包含包内除 `checksums.json` 自身（及可选签名文件 `signatures.json`）以外的**所有文件**（若存在 `signatures.json`，签名内容必须同时覆盖 `checksums.json` 与 `component.json`）。
- **解压安全策略**：解压时拒绝任何包含 `..`、前导 `/`、`\` 或盘符 `:` 的路径；单文件大小上限、总解压上限与最大压缩比阈值由宿主应用配置（默认建议：`component.json` $\le 4\text{ MB}$，单构件包解压总大小 $\le 512\text{ MB}$）。

---

## 3. `component.json` 权威清单规范

`component.json` 采用 UTF-8 编码，由 **9 个顶级字段**组成：`format`、`formatVersion`、`schemaVersion`、`identity`、`environment`、`parameters`、`recipe`（仅 `sourceMode == "parametric"` 时必选，静态构件可省略）、`geometry`、`semantics`。

### 3.1 完整结构示例（以路基边坡 `SubgradeSlope` 为例）

```json
{
  "format": "cbf-component",
  "formatVersion": 1,
  "schemaVersion": "1.2.0",

  "identity": {
    "componentId": "0199f3c2-7d5a-7c3e-9b1a-2f4d6e8a0b11",
    "familyId": "SubgradeSlope",
    "revision": "1.0.0",
    "name": "路基边坡",
    "category": "Subgrade",
    "objectType": "SubgradeSlope",
    "sourceMode": "parametric",
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
      "STD-8M": {
        "label": "8米标准一级边坡",
        "drawingCode": "路基-2026-08",
        "values": { "Height": 8000.0, "SlopeRatio": 1.5 }
      },
      "STD-12M": {
        "label": "12米标准边坡",
        "drawingCode": "路基-2026-12",
        "values": { "Height": 12000.0, "SlopeRatio": 1.75 }
      }
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
    "apiVersion": "cad_kernel_api/1.0",
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
    }
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
    "codeSystem": "CRBIM-1002",
    "ifcClass": {
      "default": "IfcEarthworksElement",
      "byFunctionalType": {
        "路堤边坡": "IfcEarthworksFill",
        "路堑边坡": "IfcEarthworksCut"
      }
    },
    "classCode": {
      "code": "53-01.10.10",
      "name": "路基边坡"
    },
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
    "shapes": {
      "Body": {
        "label": "边坡本体",
        "geomType": "solid",
        "cardinality": "1",
        "role": "body",
        "classCode": { "code": "53-01.10.10", "name": "路基边坡本体" },
        "ifc": { "representation": "Body", "type": "Brep" },
        "material": { "category": "Concrete", "gradeParam": "StrengthGrade" },
        "properties": {
          "Pset_SlopeGeometry.Height": "$param.Height",
          "Pset_SlopeGeometry.Width": "$param.Width",
          "Qto_Body.NetVolume": "$measure.volume"
        }
      },
      "SlopeFace": {
        "label": "路基边坡面",
        "geomType": "face",
        "cardinality": "1..*",
        "role": "surface",
        "classCode": { "code": "53-01.20.01", "name": "路基边坡坡面" },
        "ifc": {
          "entity": "IfcShapeAspect",
          "representation": "Surface",
          "predefinedType": "SLOPE"
        },
        "properties": {
          "Pset_SlopeGeometry.SlopeArea": "$measure.area"
        }
      },
      "ShoulderLine": {
        "label": "路肩特征线",
        "geomType": "wire",
        "cardinality": "1..*",
        "role": "reference",
        "classCode": { "code": "53-01.90.01", "name": "路肩特征线" },
        "ifc": {
          "entity": "IfcShapeAspect",
          "representation": "Annotation",
          "type": "Curve3D",
          "curveType": "Breakline"
        },
        "drawingRole": "SlopeIndication.ShoulderLine"
      },
      "ToeLine": {
        "label": "坡脚特征线",
        "geomType": "wire",
        "cardinality": "1..*",
        "role": "reference",
        "classCode": { "code": "53-01.90.02", "name": "坡脚特征线" },
        "ifc": {
          "entity": "IfcShapeAspect",
          "representation": "Annotation",
          "type": "Curve3D",
          "curveType": "Breakline"
        },
        "drawingRole": "SlopeIndication.ToeLine"
      }
    }
  }
}
```

### 3.2 多预定义规格（`presets`）与五步参数求值流水线

同一构件族模板（`.rcif`）通过 `parameters.presets` 定义多个标准规格（例如不同高度或不同标准图集型号的边坡、桥墩、箱梁）。`presets` 的每个条目支持两种写法：
- **完整模式（推荐）**：`{ "label": "规格名称", "drawingCode": "标准图号", "classCode": "可选特定编码", "values": { "Height": 8000.0, ... } }`；
- **简写模式**：直接写参数键值字典 `{ "Height": 8000.0, "SlopeRatio": 1.5 }`。

为与 [core-data-model.yaml](file:///D:/QtOCCTApp/core-data-model/schema/core-data-model.yaml) 完全兼容，参数 `access` 固定为四种枚举（历史 `"output"` 自动归一化为 `"out"`）：

| `access` 类型 | 是否允许实例覆盖 | 求值时机 | 纳入 `cacheKey` | 说明 |
| :--- | :---: | :--- | :---: | :--- |
| **`input`** | 是 | 第 1~3 步（建模前） | 仅当在 `recipe.inputs` 中映射时纳入 | 标准输入参数（如 `Length`、`ModelNumber`） |
| **`inout`** | 是 | 第 2 步查表默认 + 第 3 步允许用户覆盖 + 第 5 步允许服务修正 | 仅当在 `recipe.inputs` 中映射时纳入 | 规格联动且可手动微调的参数（如墩高 `Height`） |
| **`derived`** | 否 | 第 4 步（建模前由客户端表达式 `expr` 求值） | 否（由输入决定） | 纯公式派生参数（如 `Width = Height * SlopeRatio`） |
| **`out`** | 否 | 第 5 步（建模服务执行后通过 JHB `args` 回写） | 否 | 依赖复杂几何求值或服务端查表得出的只读结果参数 |

**五步求值顺序**：
1. **默认值初始化**：读取 `schema[*].default`；
2. **预定义规格（`presets`）覆盖**：若 `ModelNumber` 命中 `presets[ModelNumber]`，用其 `values` 字典覆盖对应的 `inout` / `input` 参数；
3. **实例级参数覆盖**：应用项目实例的 `parameterOverrides`（仅作用于 `input` 与 `inout`）；
4. **前端派生计算与约束校验**：对 `access == "derived"` 的参数计算 `expr`，并校验 `constraints`；
5. **生成器执行与回写**：用第 1~4 步解析后的最终输入参数计算 `cacheKey`，未命中缓存时调用生成器，将返回的 `args` 按 `%.17g`（或 `std::to_chars`）无损回写至 `out` / `inout` 属性节点。

---

## 4. `semantics.shapes` 命名几何单表与多实例区分规范

### 4.1 命名几何三端一致原则

`semantics.shapes` 中的键（`Body`、`SlopeFace`、`ShoulderLine`、`ToeLine`）**既是建模引擎的输出清单，也是 `.cbf` 内部的子标签技术键，更是绑定 IFC 与分类编码的语义槽位**：
- `cardinality` 取值说明：
  - `"1"`：必须且只能输出 1 个该类型的几何对象；
  - `"1..*"`：必须输出至少 1 个，允许输出多个同类型几何对象；
  - `"0..*"` / `"0..1"`：可选输出。
- `properties` 值绑定简写语法：
  - `"$param.<ParamName>"`：直接绑定构件参数（如 `"$param.Height"`）；
  - `"$measure.<metric>"`：直接绑定当前几何对象的几何测量值（支持 `volume`、`area`、`length`、`bboxX`、`bboxY`、`bboxZ`）。

### 4.2 同类型多语义几何的区分与配对：`<shapeKey>/<instanceTag>` 两级命名规范

当一次建模输出多个同类型语义几何（如路基左右两条路肩线/坡脚线、多级边坡的多级坡面、群桩基础的多根桩体）时，采用斜杠 `/` 分隔的 **`<shapeKey>/<instanceTag>`** 两级命名规范：

```text
完整技术键 = <shapeKey> / <instanceTag>
               │              └── 实例区分与配对标签（如 Left, Right, Tier1, 1, 2...）
               └── 语义几何槽位（对应 component.json 中 semantics.shapes 的 Key）
```

#### （1）Python 建模脚本的三种合法输出方式
```python
# 方式 1：单实例（最常见，无 '/' 后缀）
outputs = {
    "Body": wedge_solid,
    "SlopeFace": slope_face,
    "ShoulderLine": shoulder_wire,
    "ToeLine": toe_wire
}

# 方式 2：数组自动编号（适用于分段路肩线、群桩等无须跨类型配对的多实例）
# 打包器自动将其展开为 "ShoulderLine/1", "ShoulderLine/2"
outputs = {
    "Body": wedge_solid,
    "ShoulderLine": [seg1_wire, seg2_wire],
    "ToeLine": [toe1_wire, toe2_wire]
}

# 方式 3：显式指定 <instanceTag>（适用于左右双侧边坡、多级边坡等需要配对的场景）
outputs = {
    "Body": full_subgrade_solid,
    # 左侧边坡组（共享实例标签 Left）
    "SlopeFace/Left": face_left,
    "ShoulderLine/Left": shoulder_left,
    "ToeLine/Left": toe_left,
    # 右侧边坡组（共享实例标签 Right）
    "SlopeFace/Right": face_right,
    "ShoulderLine/Right": shoulder_right,
    "ToeLine/Right": toe_right
}
```

#### （2）下游解析、IFC/分类编码映射与二维示坡线自动配对规则
当 C++ 客户端或导出器读取到子形状的技术键（如 `"ShoulderLine/Left"`）时，按 `/` 切分为 `shapeKey = "ShoulderLine"` 与 `instanceTag = "Left"`：
1. **语义与分类编码查表（按前缀 `<shapeKey>`）**：
   - 直接查找 `semantics.shapes["ShoulderLine"]`，自动继承行业分类编码 `53-01.90.01`（路肩特征线）、IFC 映射规则（`IfcShapeAspect`）和制图角色（`SlopeIndication.ShoulderLine`）。`component.json` 中**无需**为左右或多级边坡重复定义多个槽位。
2. **IFC 4.3 (`IFC4X3_ADD2`) 标准导出映射**：
   - **主实体（`role == "body"`）**：作为 `IfcEarthworksFill` / `IfcEarthworksCut` 的主几何表达 `IfcShapeRepresentation`（`RepresentationIdentifier = "Body"`），并关联构件级 `IfcClassificationReference`；
   - **局部几何特征面/线（`ifc.entity == "IfcShapeAspect"`，如 `SlopeFace/Left`、`ShoulderLine/Left`）**：为每个具体子形状生成独立的 `IfcShapeAspect`（`Name = "ShoulderLine/Left"`，归属于主构件的 `PartOfProductDefinitionShape`），内部挂载对应的 `IfcFaceSurface` 或 `IfcIndexedPolyCurve`，并为该 `IfcShapeAspect` 绑定 `53-01.90.01` 的 `IfcClassificationReference` 及属性集。
3. **二维示坡线自动同名配对（按后缀 `<instanceTag>`）**：
   - 当 [`adSlopeIndication`](file:///D:/QtOCCTApp/core-data-model/schema/core-data-model.yaml#L137-L148) 从目标三维构件提取特征线时，自动将具有**相同 `<instanceTag>`** 的 `ShoulderLine/<tag>` 与 `ToeLine/<tag>` 配成一对（如 `ShoulderLine/Left` 配对 `ToeLine/Left`，`ShoulderLine/Tier1` 配对 `ToeLine/Tier1`），分别生成每一侧或每一级坡面的二维示坡线齿线。

---

## 5. 原生几何缓存 `.cbf` 内部结构与跨重启持久化规范

### 5.1 `.cbf` 方言自描述与流式保存强制规范

1. **外部清单**：任何引用 `.cbf` 的 JSON 字段必须声明 `"payloadKind": "BinXCAF" | "ACTBin"`。
2. **内部自描述**：在保存 `.cbf` 时，在文档根标签 `0:1`（`doc->Main()`）写入 `TDataStd_Comment`（或 `TDataStd_AsciiString`）属性：
   - 构件几何缓存（`BinXCAF`）：`"CBF-Payload:BinXCAF;v1.2"`
   - 项目/图纸主文档（`ACTBin`）：`"CBF-Payload:ACTBin;v1.2"`
3. **【重要】流式保存强制约定**：OCCT 的 `BinXCAFDrivers` 默认注册扩展名为 `.xbf`，若直接向 `m_app->SaveAs(doc, path)` 传入 `.cbf` 或 `.tmp` 文件路径，OCCT 可能自动追加 `.xbf` 后缀导致原路径文件不存在（见 [`scripts-service/main.py`](file:///D:/QtOCCTApp/scripts-service/main.py#L308)）。因此，C++ 中保存 `.cbf` 或 `.tmp` 临时文件时，**必须统一使用 `std::ofstream` 二进制流重载 `m_app->SaveAs(doc, ofs)`**。

### 5.2 `BinXCAF` 中的命名几何锚点标签树

严禁将显示名称 `TDataStd_Name` 直接当作技术键使用。构件 `BinXCAF` 文档遵循以下层次布局（跨版本与跨文件解析以遍历匹配子 Label 的 `TDataStd_AsciiString` 技术键为唯一契约）：

```text
0:1 (Doc Main)
 ├── TDataStd_Comment: "CBF-Payload:BinXCAF;v1.2"
 └── 0:1:1 (XCAFDoc_ShapeTool Root)
      └── 0:1:1:1 [Component Root Compound]
           │  ├── TNaming_NamedShape: TopoDS_Compound (按 _shapeKeys 顺序包含各子形状)
           │  ├── TDataStd_Name: "路基边坡"                    <-- 显示名称 (可本地化)
           │  └── TDataStd_AsciiString: "<64字符hex cacheKey>" <-- 根形状缓存主键 (兼容现有代码)
           │
           ├── 0:1:1:1:1 [Sub-Shape 1: 主体实体]
           │    ├── TNaming_NamedShape: TopoDS_Solid
           │    ├── TDataStd_Name: "边坡本体"                  <-- UI 显示名称
           │    ├── TDataStd_AsciiString: "Body"               <-- [核心] 技术键 (<shapeKey>)
           │    └── XCAFDoc_VisMaterial / Color                <-- 子形状默认物理材质分类
           │
           ├── 0:1:1:1:2 [Sub-Shape 2: 坡面]
           │    ├── TNaming_NamedShape: TopoDS_Face
           │    ├── TDataStd_Name: "路基边坡面(左)"
           │    └── TDataStd_AsciiString: "SlopeFace/Left"     <-- [核心] 技术键 (<shapeKey>/<instanceTag>)
           │
           ├── 0:1:1:1:3 [Sub-Shape 3: 路肩特征线]
           │    ├── TNaming_NamedShape: TopoDS_Wire
           │    ├── TDataStd_Name: "路肩特征线(左)"
           │    └── TDataStd_AsciiString: "ShoulderLine/Left"  <-- [核心] 技术键 (<shapeKey>/<instanceTag>)
           │
           └── 0:1:1:1:4 [Sub-Shape 4: 坡脚特征线]
                ├── TNaming_NamedShape: TopoDS_Wire
                ├── TDataStd_Name: "坡脚特征线(左)"
                └── TDataStd_AsciiString: "ToeLine/Left"       <-- [核心] 技术键 (<shapeKey>/<instanceTag>)
```

### 5.3 `adGeometricDef` 磁盘缓存恢复时的 `_shapeKeys` 保序重建机制

在现有架构中，[`GeometryService::BuildGeometry`](file:///D:/QtOCCTApp/core-data-model/GeometryService.cpp#L690-L715) 会将生成的 `TopoDS_Shape` 存入主文档几何分区的 [`adGeometricDef`](file:///D:/QtOCCTApp/core-data-model/schema/core-data-model.yaml#L42-L49) 节点。由于 `adGeometricDef` 的 `Shape` 参数（`ActData_ShapeParameter`）只持久化 `TNaming_NamedShape` 而不包含 XCAF 子 Label 属性，遵循以下**保序重建规范**：

1. **生成与打包期**：`scripts-service` 与 [`brep_to_cbf.cpp`](file:///D:/QtOCCTApp/core-data-model/tools/brep_to_cbf.cpp) 按有序技术键列表（如 `["Body", "SlopeFace/Left", "ShoulderLine/Left", "ToeLine/Left"]`）构造顶层 `TopoDS_Compound`，并在 `protoLabel` 下按相同顺序创建子 Label 写入 `TDataStd_AsciiString`。
2. **写入 `adGeometricDef` 期**：在 [`GeometryService::CreateGeoDef`](file:///D:/QtOCCTApp/core-data-model/GeometryService.cpp#L411-L426) 中，将有序技术键数组 `"_shapeKeys"` 一并写入 `adGeometricDef->SetGeoParameter(...)` 的 JSON 中。
3. **重启恢复期**：当 `FindCachedGeoDef` 命中磁盘 `adGeometricDef` 时，通过 `TopoDS_Iterator` 遍历 `cachedGeoDef->GetShape()` 的直接子形状（OCCT 严格保证 `TopoDS_Compound` 子形状序列化前后顺序不变），对照 `GeoParameter` 中的 `_shapeKeys` 数组，在 `m_xcafDoc` 的 `protoLabel` 下完整恢复带 `TDataStd_AsciiString` 的子 Label。

---

## 6. 缓存键（`cacheKey`）计算规范与 JHB 协议 v1.2

### 6.1 确定性 `cacheKey` 计算公式（前后端与 Dapr 统一同源）

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
3. **前后端统一缓存键**：客户端计算出 `cacheKey` 后通过请求字段 `param_geo_id`（或 `cache_key`）传给 `scripts-service`，服务端直接以该 `cacheKey` 作为 Dapr 状态存储主键。
4. **几何与视觉材质参数边界**：`BinXCAF` 内置的 `XCAFDoc_VisMaterial` 仅代表子形状的默认物理材质分类（如混凝土、钢板、橡胶块）。若某个构件参数确实会改变生成几何体的默认视觉材质外观，构件作者必须将该参数列入 `recipe.inputs` 使其进入 `cacheKey`；纯力学指标（如混凝土 `C30` vs `C50`）不改变外观，不进入 `cacheKey`；实例级的颜色与透明度覆盖由主文档 [`adGeometry`](file:///D:/QtOCCTApp/core-data-model/schema/core-data-model.yaml#L50-L60) 在渲染时动态应用。

### 6.2 建模服务 JHB（JSON-Header-Binary）协议 v1.2

二进制流格式：`[4字节 uint32 Little-Endian JSON长度][UTF-8 JSON元数据][几何二进制载荷]`。
- **JSON 元数据契约（支持 `<shapeKey>/<instanceTag>` 多实例输出列表）**：
```json
{
  "protocolVersion": "1.2",
  "status": "ok",
  "error": null,
  "cache_key": "8f434346648f6b96df89dda901c5176b10a6d83961dd3c1ac88b59b2dc327aa4",
  "cache_key_version": 1,
  "generator_version": "1.2.0",
  "kernel": { "id": "occt", "version": "8.0.0", "adapterVersion": "1.0.0" },
  "payloadKind": "BinXCAF",
  "payloadFormat": "cbf",
  "shapeKeys": ["Body", "SlopeFace", "ShoulderLine", "ToeLine"],
  "args": {
    "Length": 20000.0,
    "Height": 8000.0,
    "SlopeRatio": 1.5,
    "SlopeArea": 288444102.037
  },
  "outputs": {
    "Body":         { "geomType": "solid", "volume": 960000000000.0, "bbox": [0, 0, 0, 20000, 12000, 8000] },
    "SlopeFace":    { "geomType": "face",  "area": 288444102.037 },
    "ShoulderLine": { "geomType": "wire",  "length": 20000.0 },
    "ToeLine":      { "geomType": "wire",  "length": 20000.0 }
  },
  "anchors": {
    "StartSection": { "origin": [0, 0, 0], "axis": [0, 0, 1], "refDirection": [1, 0, 0] },
    "EndSection":   { "origin": [20000, 0, 0], "axis": [0, 0, 1], "refDirection": [1, 0, 0] }
  }
}
```

---

## 7. 多源几何（IFC / GLB）与国产几何内核兼容方案

### 7.1 多源几何能力矩阵与实际导入判定

构件的 `exactBrep` 等能力必须以实际导入转换后的结果为准并记录在 `geometry.capabilities`（及 `source/import-report.json`）中：

| 来源模式 (`identity.sourceMode`) | 是否包含 `recipe` 节 | 权威几何载体 | `exactBrep` | 运行时准入与降级策略 |
| :--- | :---: | :--- | :---: | :--- |
| **`parametric`** | **必填** | `recipe.generator`（预烘焙于 `default.cbf`） | `true` | 支持变参重建、精确布尔、工程量计算及 2D 特征线提取。 |
| **`native-brep`** | 省略 | `geometry/default.cbf` | `true` | 静态精确 BRep，支持布尔与精确测量，不支持参数驱动变形。 |
| **`imported-ifc`** | 省略 | `source/original.ifc`（派生 `default.cbf`） | **视导入结果而定** | 若 IFC 实体为 SweptAreaSolid/BRep 则转为精确 BRep（`exactBrep=true`）；若原始 IFC 为 TriangulatedFaceSet 网格，则标记 `exactBrep=false`。 |
| **`mesh-only`** | 省略 | `geometry/default.glb` 或 `source/original.glb` | `false` | 权威几何为独立工程网格 `geometry/default.glb`（`display/preview.glb` 仅作缩略图预览）；**严禁参与精确 BRep 布尔运算与体积工程量统计**。 |

> **静态几何子形状选择器（Selector）规范**：当 `native-brep` 或 `imported-ifc` 构件未预置子形状名称时，允许在 `semantics.shapes.<ShapeKey>` 下直接附加基于**归一化包围盒坐标（Normalized BBox `[0.0, 1.0]`）**与法向极值的 `selector` 字段（例如 `"selector": { "normalRange": { "zMin": 0.1, "zMax": 0.95 } }`），严禁使用写死的绝对毫米坐标。

### 7.2 面向中望（ZW3D）、华天（CrownCAD）等国产内核的适配架构

通过 **`cad_kernel_api`（Python 建模层）** + **`IGeometryKernelAdapter`（C++ 运行时层）** 双端隔离具体几何内核：

```cpp
class IGeometryKernelAdapter {
public:
    virtual ~IGeometryKernelAdapter() = default;
    virtual KernelInfo info() const = 0;            // 返回 kernelId ("occt"|"zw3d"|"huatian"), version, nativeFormat
    virtual CapabilitySet capabilities() const = 0; // 返回当前内核支持的能力集合

    // 参数化求值与命名子形状解析（支持 <shapeKey>/<instanceTag>）
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

## 8. 现有代码库落地改造清单（研发实施路线）

### 阶段一：修复现有基础缺陷，打通 `<shapeKey>/<instanceTag>` 闭环（P0，立即执行）

1. **修复参数回写精度丢失**（[GeometryService.cpp](file:///D:/QtOCCTApp/core-data-model/GeometryService.cpp#L638-L645)）：
   - 将 `WriteBackParams` 中的 `std::to_string(val.get<double>())` 替换为 17 位无损浮点格式化（`std::snprintf(buf, sizeof(buf), "%.17g", val.get<double>())` 或 `std::to_chars`）。
2. **升级 `ComputeParamGeoID` 为 SHA-256（`%.12g` 规范化）并统一前后端缓存键**（[GeometryService.cpp](file:///D:/QtOCCTApp/core-data-model/GeometryService.cpp#L501-L509)、[scripts-service/main.py](file:///D:/QtOCCTApp/scripts-service/main.py#L100-L108)）：
   - 实现第 6.1 节的 `CacheKeyPayload` 规范化序列化（含 `%.12g` 与 `-0.0` 钳制）及 64 字符小写 Hex SHA-256 计算，服务端直接使用同一 `cacheKey`。
3. **支持多命名子形状（`<shapeKey>/<instanceTag>`）打包、合并与跨重启恢复**（[brep_to_cbf.cpp](file:///D:/QtOCCTApp/core-data-model/tools/brep_to_cbf.cpp)、[GeometryService.cpp](file:///D:/QtOCCTApp/core-data-model/GeometryService.cpp#L300-L381)）：
   - 升级 `scripts-service` 与 `brep_to_cbf.cpp`，支持读取 Python 脚本输出的 `outputs` 字典（及数组自动编号展开），按序组装 `TopoDS_Compound` 并在 `protoLabel` 子节点写入 `TDataStd_AsciiString`；
   - 在 `CreateGeoDef` 与 `FindCachedGeoDef` 中实现第 5.3 节的 `_shapeKeys` 保序持久化与重建；
   - 在 `GeometryService` 中新增按前缀/标签查询子形状接口，打通 [`adSlopeIndication`](file:///D:/QtOCCTApp/core-data-model/schema/core-data-model.yaml#L137-L148) 二维示坡线（含左右/多级配对）自动更新闭环。
4. **补齐 `ProjectManager::SaveAll()` 的流式原子替换保护**（[ProjectManager.cpp](file:///D:/QtOCCTApp/core-data-model/ProjectManager.cpp#L202-L218)）：
   - [ProjectPackage.cpp](file:///D:/QtOCCTApp/src/ProjectPackage.cpp#L1134) 已使用 `QSaveFile`，无需改动；
   - 针对 `ProjectManager::SaveAll()`，改为先通过 `std::ofstream` 二进制流调用 `m_app->SaveAs(doc, ofs)` 写入同目录 `<filename>.tmp`（避免 OCCT 自动追加后缀），流刷新关闭且状态为 `PCDM_SS_OK` 后，调用 `MoveFileExW(tempPath, targetPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)` 原子覆盖目标文件。

### 阶段二：落地 `.rcif` 构件包读写器、规格型号表与 IFC `IfcShapeAspect` 导出（P1，中期迭代）

1. **实现 `RcifPackageReader / Writer`**：支持 `.rcif` 包的打包、`checksums.json` 校验及 `component.json`（含 `semantics.shapes` 单表）解析。
2. **接入 `presets` 规格型号表与 `derived` 表达式求值**：在 `GeometryService::ExtractGeoParams` 中实现第 3.2 节的五步参数求值流水线。
3. **升级 `IfcExportService` 支持子形状导出**：根据 `semantics.shapes` 中的 `ifc` 与 `classCode` 定义，将 `SlopeFace`、`ShoulderLine`、`ToeLine` 导出为 `IfcShapeAspect` 并关联 `IfcClassificationReference`。

### 阶段三：多内核抽象与构件库检索服务（P2，长期演进）

1. **下沉 `OcctKernelAdapter`**：将 `GeometryService` 直接操作 `TopoDS_Shape` 的逻辑收敛到 `IGeometryKernelAdapter` 实现类中。
2. **构建 `catalog.sqlite` 本地构件库索引**：扫描 `.rcif` 包内的 `component.json` 提供按分类、功能类型和预定义规格型号的快速检索。
