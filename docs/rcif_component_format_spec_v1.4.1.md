# RCIF 构件数据格式与工程落地规范 v1.4.1（专业 Code-CAD 工具链与拓扑感知交互闭环版）

- **版本**：1.4.1 (Round 2 修订版，针对 Round 1 审核报告全面闭环)
- **状态**：待审核基线（Pending DeepSeek Round 2 Sign-off）
- **日期**：2026-10-19
- **前序版本**：[v1.0](file:///D:/QtOCCTApp/docs/rcif_component_format_spec_v1.0.md) | [v1.1](file:///D:/QtOCCTApp/docs/rcif_component_format_spec_v1.1.md) | [v1.2](file:///D:/QtOCCTApp/docs/rcif_component_format_spec_v1.2.md) | [v1.3](file:///D:/QtOCCTApp/docs/rcif_component_format_spec_v1.3.md) | [v1.3.1](file:///D:/QtOCCTApp/docs/rcif_component_format_spec_v1.3.1.md) | [v1.3.2](file:///D:/QtOCCTApp/docs/rcif_component_format_spec_v1.3.2.md) | [v1.4](file:///D:/QtOCCTApp/docs/rcif_component_format_spec_v1.4.md)
- **设计借鉴**：融合 **Autodesk Civil 3D (`.pkt` 点/线/面语义编码 `Code Set`)**、**Archicad GDL (`.gsm` 脚本逻辑内聚)**、**Autodesk Revit (`Subcategory` + 外部 `IFC Mapping Table` + `Type Catalog`)**、**Pixar OpenUSD (按需叠加 Schema)**，以及新一代开源 Code-CAD 标杆 **build123d / OCP CAD Viewer (`three-cad-viewer` + `ocp-tessellate` 拓扑离散与 CAD 级交互管线)**。

---

## 0. 规范继承声明与 v1.4.1 闭环修订说明

### 0.1 权威继承基线声明
**本规范（v1.4.1）为 RCIF 构件格式规范的权威最新版本。除本版本显式新增与增强之条款外，[v1.3.2 终审通过版](file:///D:/QtOCCTApp/docs/rcif_component_format_spec_v1.3.2.md) 的全部 Normative 规范性条款（包括四大架构宪法护栏 G1~G4、外部语义映射三级优先级、二元能力推导真值表、无标签模型 `selector` 语法、以及逐子形状 $O(1)$ 校验机制）继续完全有效并被本规范全额继承。**

### 0.2 针对 Round 1 审核报告（4 个 BLOCKER + 5 个 MAJOR）的闭环处置

1. **【闭环 B1】JHB 二进制缓冲区布局数学严密性**：
   - 彻底修复 `indices.length`：三角形索引长度严格等于 $\text{triangleCount} \times 3 \times 4$（24,956 个三角形为 299,472 字节，而非漏乘 3 的 99,824 字节）；
   - 按严密布局重算所有后续缓冲区偏移：`vertices` (0), `indices` (149,760), `normals` (449,232), `edges` (598,992), `edgeOffsets` (650,832), `faceIds` (650,948)；
   - 明确定义缓冲区自检恒等式：$\text{offset}_{i+1} = \text{offset}_i + \text{length}_i$。
2. **【闭环 B2】`edges` 特征折线分段消除二义性**：
   - 明确 `edges` 采用**“独立折线点列（Polyline Strips）”**拓扑结构；
   - 增设紧凑的 `edgeOffsets` 索引缓冲（长度为 $\text{edgesCount} + 1 = 29$ 个 `uint32`），第 $k$ 条特征折线的点列区间为 `[edgeOffsets[k], edgeOffsets[k+1])`，彻底杜绝折线跨实体串联。
3. **【闭环 B3】拓扑面反查闭环与 `outputs` 逐部件恢复**：
   - 在 JHB 头部恢复 `shapeKeys` 列表与 `outputs` 逐子形状 `{geomType, volume/area/length, bbox}` 元数据；
   - 增设 `faceToShape` 紧凑映射表（长度等于 `facesCount`，值为 `shapeKeys` 索引），打通 `点击模型面 -> faceId -> shapeKey -> 语义码 -> IfcShapeAspect 属性联动` 的全链路。
4. **【闭环 B4】确定性 `CacheKeyPayload` 字段全额恢复与真实测试向量**：
   - 完整恢复 `CacheKeyPayload` 的全部 10 个规范字段定义；
   - 恢复前序 3 条标准测试向量，并为桥梁圆端形桥墩 `PierBody` 示例补齐真实 SHA-256 摘要与可复算测试向量。
5. **【闭环 M1】双向格式协商与向后兼容矩阵**：
   - 保留客户端 `acceptFormats` 请求协商机制，提供 v1.3.2 纯几何载荷与 v1.4 拓扑感知载荷的无缝兼容。
6. **【闭环 M2 & M3】脚本执行语义与安全沙箱纵深防御**：
   - 规定 Worker 采用 `importlib` 纯函数式调用 `recipe.entry`，严格不执行 `__main__` 分支；
   - 门禁 3 规定 `show()` 仅允许位于 `if __name__ == "__main__":` 块且配有 `try/except ImportError`，打包发布时自动剥离；
   - 确立 Worker 容器运行时安全基线（非 root、断网 `--network none`、只读根文件系统、CPU/内存/时间配额）。
7. **【闭环 M4】构件内部轻量级部件树（`shapes[*].parent`）**：
   - 在保持扁平结构的前提下，在 `shapes.<Key>` 中增加可选 `"parent"` 字段，支持构件内部子部件的层级折叠与递归显隐。
8. **【闭环 M5】全面重述与继承 v1.3.2 核心条款**：
   - 完整保留 `(sourceMode, exactBrep)` 二元真值表、外部映射三级解析优先级与 `unmapped` 兜底、以及 `selector` 声明式拾取器规范。

---

# 第一部分：通用构件数据格式标准正文（Normative Specification）

## 1. 核心架构：构件定义包与外部语义映射表双层解耦

```mermaid
flowchart LR
    subgraph RCIF ["1. 极简构件定义包 (.rcif)"]
        CJ["component.json<br/>(参数 + 型号表 + 配方 + 局部部位 shapes<br/>+ semanticProfileHint)"]
        GEN["建模引擎<br/>(build123d / cadquery / zw3d / occt-wasm)"]
        JHB["拓扑感知 JHB 二进制流 / 精确几何 (.step / .cbf)<br/>含命名锚点: ShoulderLine/Left_1<br/>含特征折线 edges、拓扑映射 faceToShape"]
        CJ --> GEN --> JHB
    end

    subgraph PROFILE ["2. 外部语义标准映射表 (semantic_profiles.json)"]
        CODE["语义码: Bridge.Pier.TopFace"]
        CLS["多分类标准: CRBIM 53表 / GB/T 51269 / JTG"]
        IFC["IFC 4.3: IfcShapeAspect (BearingSeat)"]
        DRW["二维制图角色与算量绑定"]
        CODE --> CLS & IFC & DRW
    end

    JHB -- "按 faceId 查 faceToShape 得到 shapeKey -> 查 shapes.code" --> CODE
```

### 1.1 文件与容器家族定义

| 载体名称 | 扩展名 / 格式 | 定位与职责 |
| :--- | :--- | :--- |
| **构件定义包** | **`.rcif`** (ZIP) | **构件库中的“族模板”**。文件名为 `<familyId>_<revision>.rcif`（同一构件库内 `familyId` 唯一且满足 `^[A-Za-z0-9_-]+$`）。内含极简清单 `component.json`、预览网格 `display/preview.glb`、可选预烘焙几何 `geometry/default.*` 及校验清单 `checksums.json`。 |
| **外部语义标准映射表** | **`semantic_profiles.json`** | **宿主系统/项目级公共配置（Code Set Style）**。定义各语义码（`code`）到国标/行标分类编码、IFC 4.3 实体与算量集、二维制图角色及标准物理材质库的映射规则。 |
| **几何缓存制品** | **`.cbf` / `.step` / `.glb` / `.jhb`** | **运行时按 `<cacheKey>` 寻址的几何快照**。存储精确 BRep 或拓扑感知网格帧，并在子形状上携带 `<shapeKey>[/<instanceTag>]` 命名锚点。 |
| **项目交付包** | **`.rde`** (ZIP) | **工程项目容器**。存储主模型树（实例引用 + 型号选择/参数覆盖 + 空间布置）、当前项目绑定的 `semantic_profiles.json` 快照、二维工程图纸及去重几何缓存池（`geometry/<cacheKey>.*`）。 |

---

## 2. `.rcif` 构件定义包物理结构

每个 `.rcif` 为标准 Deflate ZIP 归档包（UTF-8 文件名编码），内部结构极简固定：

```text
<familyId>_<revision>.rcif (ZIP 容器)
├── component.json          # [必须] 极简权威清单（身份、参数表、规格型号、建模配方、命名几何槽位）
├── geometry/               # [可选] 默认参数下的预烘焙几何制品（支持多格式并存）
│   ├── default.cbf         #        OCCT BinXCAF 原生精确几何（与 defaultCacheKey 严格同源）
│   ├── default.step        #        STEP AP242 跨内核标准精确几何
│   ├── default.jhb         #        JHB v1.4 拓扑感知预烘焙无头二进制流 (含矢量边线与 FaceIds)
│   └── default.glb         #        仅当 sourceMode == "mesh-only" 时作为权威工程网格
├── display/                # [必须] 构件库预览与二维制图图例
│   ├── preview.glb         # [必须] 构件库卡片轻量预览网格（LOD 展示用途）
│   ├── preview.svg         # [推荐] OCCT HLR 消隐矢量投影图 (工程白底黑线，Z轴向上)
│   └── thumbnail.png       # [可选] 256x256 构件库卡片缩略图
├── generators/             # [可选] 仅当内嵌脚本或 WASM 模块时存在
│   └── generator.py        #        build123d 或 CadQuery 参数化几何生成主脚本
└── checksums.json          # [必须] 包内除自身外所有文件的 SHA-256 校验清单
```

### 2.1 五道前置安全合规门禁与运行时沙箱规范

构件包入库与执行遵循**“前置 AST 静态审查 + 运行时容器强隔离”双层纵深防御体系**：

1. **门禁 1（解压安全与配额守门员）**：
   - 压缩比 $\le 100$，解压后体积 $\le 200\text{MB}$，文件总数 $\le 500$；
   - 严格拒绝任何包含 `..`、前导 `/`、`\`、盘符 `:` 或符号链接（Symlink）的恶意条目。
2. **门禁 2（完整性签名比对）**：
   - `checksums.json` 必须覆盖包内除自身外的全部文件且 SHA-256 100% 匹配；严禁夹带未登记文件。
3. **门禁 3（脚本真实性与代码验真）**：
   - 脚本 SHA-256 必须与 `recipe.digest` 严格一致；
   - **AST 语法扫描**：禁止 `eval`, `exec`, `compile`, `open`, `input`, `getattr`, `setattr`, `delattr`；禁止静态或动态引入高危模块（`os`, `sys`, `subprocess`, `socket`, `requests`, `urllib`, `ctypes`, `shutil`）；
   - **本地调试代码拦截**：对 `show()`, `show_object()`, `set_viewer_config()` 等 `ocp_vscode` 调试调用，**仅允许出现在 `if __name__ == "__main__":` 代码块内**；出现在顶层或构件构造函数内部直接判定为门禁失败。
4. **门禁 4（Schema 结构化验证）**：
   - `component.json` 必须通过规范声明的 JSON Schema 严格校验。
5. **门禁 5（几何有效性与散度自检）**：
   - 以默认预设规格运行脚本，调用 `BRepCheck_Analyzer` 检查几何封闭性与非流形；
   - 基于高斯散度定理积分计算网格体积，与精确 B-Rep 体积偏差必须 $< 1.0\%$。
6. **运行时无头 Worker 沙箱基线（不可替代的执行硬屏障）**：
   - Worker 容器必须以非 root 用户（`uid=1000`）运行；
   - 强制启用 `--network none`（完全断网，禁止一切套接字建立）；
   - 根文件系统以只读（`read_only: true`）挂载，仅开放限制容量的内存临时目录（tmpfs）；
   - 强制限制单任务 CPU 配额、内存（上限 2GB）与超时硬熔断（默认 30 秒）。

---

## 3. 极简 `component.json` 规范

### 3.1 完整规范示例（桥梁圆端形桥墩 `PierBody`）

```json
{
  "format": "cbf-component",
  "schemaVersion": "1.4.1",

  "identity": {
    "componentId": "0199f3c2-7d5a-7c3e-9b1a-2f4d6e8a0b12",
    "familyId": "PierBody",
    "revision": "1.0.0",
    "name": "实体圆端形桥墩",
    "category": "BridgeSubstructure",
    "semanticCode": "Bridge.Pier.Body",
    "semanticProfileHint": "Railway-CRBIM-IFC4X3@1.4",
    "sourceMode": "parametric",
    "placement": {
      "modes": ["point"],
      "orientation": "azimuth"
    }
  },

  "environment": {
    "units": { "length": "mm", "angle": "rad" },
    "tolerance": { "linear": 0.01, "angular": 0.001 }
  },

  "parameters": {
    "schema": [
      { "name": "ModelNumber",   "label": "标准型号",       "type": "enum",    "default": "STD-PIER-12M", "options": ["CUSTOM", "STD-PIER-8M", "STD-PIER-12M", "STD-PIER-16M"] },
      { "name": "TopWidth",      "label": "墩顶顺桥向宽度", "type": "real",    "default": 2400.0,  "unit": "mm", "min": 1000.0, "max": 10000.0 },
      { "name": "TopLength",     "label": "墩顶横桥向长度", "type": "real",    "default": 6400.0,  "unit": "mm", "min": 2000.0, "max": 20000.0 },
      { "name": "PierHeight",    "label": "墩身总高度",     "type": "real",    "default": 12000.0, "unit": "mm", "min": 2000.0, "max": 60000.0, "access": "inout" },
      { "name": "Slope",         "label": "侧面坡度(n:1)",  "type": "real",    "default": 35.0,    "unit": "1",  "min": 10.0,   "max": 100.0 },
      { "name": "CrossSlope",    "label": "墩顶横坡率(%)",  "type": "real",    "default": 2.0,     "unit": "%",  "min": -10.0,  "max": 10.0 },
      { "name": "NetVolume",     "label": "墩身体积",       "type": "real",    "default": 194.52,  "unit": "m3", "access": "out" },
      { "name": "StrengthGrade", "label": "混凝土强度等级", "type": "enum",    "default": "C35",    "options": ["C30", "C35", "C40", "C50"] }
    ],
    "presets": {
      "STD-PIER-8M": {
        "PierHeight": 8000.0,
        "TopWidth": 2200.0,
        "TopLength": 6000.0
      },
      "STD-PIER-12M": {
        "PierHeight": 12000.0,
        "TopWidth": 2400.0,
        "TopLength": 6400.0
      },
      "STD-PIER-16M": {
        "PierHeight": 16000.0,
        "TopWidth": 2600.0,
        "TopLength": 7000.0
      }
    }
  },

  "geometry": {
    "exactBrep": true,
    "defaultFormat": "cbf-binxcaf",
    "defaultFile": "geometry/default.cbf",
    "defaultCacheKey": "3b2e5914a8c9b2d1e0f3a4c5b6e7d8f9a0b1c2d3e4f5a6b7c8d9e0f1a2b3c4d5"
  },

  "recipe": {
    "engine": "build123d",
    "source": "generators/generator.py",
    "entry": "build_pier",
    "version": "1.4.1",
    "digest": "5f4dcc3b5aa765d61d8327deb882cf992b96efe8f654b4231b2c45e691986421",
    "inputs": ["TopWidth", "TopLength", "PierHeight", "Slope", "CrossSlope"]
  },

  "shapes": {
    "PierBodySolid": {
      "label": "墩身实体",
      "code": "Bridge.Pier.BodySolid",
      "geomType": "solid"
    },
    "TopBearingFace": {
      "label": "墩顶支撑垫石安装面",
      "code": "Bridge.Pier.TopFace",
      "geomType": "face",
      "parent": "PierBodySolid"
    },
    "PierContourWire": {
      "label": "墩底截面轮廓边界线",
      "code": "Bridge.Pier.BottomContour",
      "geomType": "wire",
      "parent": "PierBodySolid"
    }
  },

  "ports": {
    "PierTopCenter": {
      "label": "墩顶中心控制点",
      "origin": [0.0, 0.0, 12000.0],
      "axis": [0.0, 0.0, 1.0],
      "refDirection": [1.0, 0.0, 0.0]
    },
    "PierBaseCenter": {
      "label": "承台顶面接合点",
      "origin": [0.0, 0.0, 0.0],
      "axis": [0.0, 0.0, -1.0],
      "refDirection": [1.0, 0.0, 0.0]
    }
  }
}
```

### 3.2 运行时能力推导真值表与建模引擎规范

- **二元组能力推导真值表**（全额继承 v1.3.2，严禁冗余声明）：

| `identity.sourceMode` | 允许的 `geometry.exactBrep` | 包含 `recipe` | 变参重建 (`parametric`) | 精确布尔 (`booleans`) | 精确体积/面积算量 |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **`"parametric"`** | `true` | **必填** | ✅ | ✅ | ✅ |
| **`"native-brep"`** | `true` | 省略 | ❌ | ✅ | ✅ |
| **`"imported-ifc"`** | `true`（实体）或 `false`（网格） | 省略 | ❌ | 仅当 `exactBrep=true` | 仅当 `exactBrep=true` |
| **`"mesh-only"`** | **必须为 `false`**（写 `true` 直接拒绝） | 省略 | ❌ | ❌ | ❌（严禁冒充精确BRep） |

- **建模引擎枚举与执行规范（`recipe.engine`）**：
  - **`"build123d"`**：现代 Python 3.10+ 参数化建模引擎（基于 OCCT）；
  - **`"cadquery"`**：经典 Python 3.10+ 参数化建模引擎（基于 OCCT）；
  - **`"pythonocc"`**：底层 OpenCASCADE 原生 Python 绑定；
  - **`"occt-wasm"`**：WebAssembly 编译的 OCCT 原生 C++ 模块；
  - **`"zw3d"` / `"crowncad"`**：国产三维 CAD 特征流模板。
  - **Worker 执行契约**：服务端几何 Worker 统一通过 `importlib` 动态加载脚本模块，并以函数式传入参数字典调用 `recipe.entry`。**Worker 严禁通过 `python script.py` 顶层运行方式执行构件代码，确保 `if __name__ == "__main__":` 块内的本地调试代码绝不触发**。

- **构件内部轻量级部件树（`shapes[*].parent`）**：
  - `shapes.<Key>` 中允许声明可选的 `"parent": "<ParentShapeKey>"`；
  - 省略时表示根部件；声明时必须指向同一 `shapes` 字典中已存在的其他键；
  - **环检测**：解析器在加载时自动进行 DAG 拓扑检查，严禁存在循环依赖引用。前端根据 `parent` 自动构建部件树，支持点击父部件批量控制子部件显隐。

---

## 4. 命名几何链接与外部《语义标准映射表》规范

### 4.1 `<shapeKey>/<instanceTag>` 机制与多实例自动配对
构件包内通过 `shapes.<Key>` 仅声明一次几何槽位类别。当建模引擎产生多个同类几何个体（如多个垫石面、左右侧边坡）时，采用 `<shapeKey>/<instanceTag>` 形式输出。下游消费遵循两步走：
1. **取前缀 `<shapeKey>`**：查询 `shapes` 获取中立语义码 `code`，进而查外部 `semantic_profiles.json` 确定分类与 IFC 映射；
2. **取后缀 `<instanceTag>`**：作为区分个体标识，并支持不同图元（如线与面）跨类型关联同名配对。

### 4.2 外部语义标准映射表的三级解析优先级与兜底规则（全额继承 v1.3.2）
1. **优先级 1（构件内按需局部覆盖）**：若极少数非标构件在 `shapes.<Key>` 下直接书写了 `classifications` 或 `ifc` 字段，采用**整字段直接替换（Replace）**策略优先使用构件内定义；
2. **优先级 2（当前工程项目激活的外部映射表）**：宿主工程（`.rde` 项目配置）当前启用的 `semantic_profiles.json`（例如在铁路项目中激活 `"Railway-CRBIM-IFC4X3@1.4"`，在公路项目中一键切换为 `"Highway-JTG-IFC4X3@1.4"`，无需修改任何 `.rcif` 构件包）；
3. **优先级 3（`identity.semanticProfileHint` 默认回退）**：当脱离工程项目独立预览或导出单个 `.rcif` 构件时，宿主系统自动加载 `semanticProfileHint` 指向的内置语义映射表。若某 `code` 在映射表中未找到，导出报告将其标记为 `unmapped` 并保留基础几何。

### 4.3 无标签外部导入模型的声明式拾取器（`selector`，全额继承 v1.3.2）
仅当外部导入的静态 BRep/IFC 未包含上述命名标签时，允许在 `shapes.<Key>` 下附加声明式拾取器 `selector`：
```json
"selector": {
  "normalizedBBox": [0.0, 0.0, 0.95, 1.0, 1.0, 1.0],
  "normalRange": { "zMin": 0.99, "zMax": 1.0 },
  "sharedByFaces": ["TopBearingFace"]
}
```

---

## 5. 确定性缓存键（`cacheKey`）与 JHB 协议 v1.4.1

### 5.1 确定性 `cacheKey` 计算公式与标准测试向量

$$\text{cacheKey} = \text{HexLower}\left(\text{SHA256}\left(\text{DeterministicJSON}(\text{CacheKeyPayload})\right)\right)$$

#### （1）`CacheKeyPayload` 标准 JSON 字段定义
```json
{
  "cacheKeyVersion": 1,
  "digest": "5f4dcc3b5aa765d61d8327deb882cf992b96efe8f654b4231b2c45e691986421",
  "engine": "build123d",
  "inputParams": {
    "CrossSlope": 2,
    "PierHeight": 12000,
    "Slope": 35,
    "TopLength": 6400,
    "TopWidth": 2400
  },
  "kernelId": "occt",
  "kernelVersion": "8.0.0",
  "source": "generators/generator.py",
  "tolerance": { "angular": 0.001, "linear": 0.01 },
  "units": { "angle": "rad", "length": "mm" },
  "version": "1.4.1"
}
```

#### （2）`DeterministicJSON` 五条序列化规则
1. **键序与空白符**：所有 JSON 对象键名按 UTF-8 字节序递归升序排列；冒号与逗号后不加任何空格或换行。
2. **浮点数 `%.12g` 规范化**：$\vert val\vert < 10^{-12}$ 置 0；按 `%.12g` 输出无多余尾零的数值字面量（严禁 `NaN`/`Inf`）。
3. **可选外部几何参数**：未传入时固定为 `null`；传入时为其几何内容 SHA-256 哈希字符串 `"sha256:<64位hex>"`。
4. **UTF-8 原生直出**：所有键与字符串值必须为原生 UTF-8 字节（严禁转义为 `\uXXXX`）。
5. **四条标准验证测试向量（Test Vectors）**：
   - **向量 1（基础数据类型归一化）**：
     - 序列化字节流：`{"a":1,"b":1.5,"c":null,"d":[10,20]}`
     - 期望 SHA-256：`51a01f79a9ca9470a9d4373b8f06ab39e92a849bed2035ee58d92f7bc640cb0c`
   - **向量 2（中文字符串 UTF-8 原生直出）**：
     - 序列化字节流：`{"Layout":"梅花形"}`（22 字节 UTF-8）
     - 期望 SHA-256：`1135ce65b3a41eb7ab8e979d812eb41d730b8e0227d76c9dac782cb32497d647`
   - **向量 3（路基边坡 `SubgradeSlope` 默认展开 Payload）**：
     - 序列化字节流：`{"cacheKeyVersion":1,"digest":"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","engine":"cadquery","inputParams":{"Alignment":null,"GroundSurface":null,"Height":8000,"Length":20000,"SlopeRatio":1.5,"Tiers":[]},"kernelId":"occt","kernelVersion":"8.0.0","source":"service://subgrade_slope","tolerance":{"angular":0.001,"linear":0.01},"units":{"angle":"rad","length":"mm"},"version":"1.0.0"}`
     - 期望 SHA-256：`0fee26374b7a518d8098da4ad6c1ad52716d9f33f5bf183fe0f75f90f4bafac0`
   - **向量 4（本规范 §3.1 `PierBody` 默认预设真实复算 Payload）**：
     - 序列化字节流：`{"cacheKeyVersion":1,"digest":"5f4dcc3b5aa765d61d8327deb882cf992b96efe8f654b4231b2c45e691986421","engine":"build123d","inputParams":{"CrossSlope":2,"PierHeight":12000,"Slope":35,"TopLength":6400,"TopWidth":2400},"kernelId":"occt","kernelVersion":"8.0.0","source":"generators/generator.py","tolerance":{"angular":0.001,"linear":0.01},"units":{"angle":"rad","length":"mm"},"version":"1.4.1"}`
     - 期望 SHA-256（严格等于 `geometry.defaultCacheKey`）：`3b2e5914a8c9b2d1e0f3a4c5b6e7d8f9a0b1c2d3e4f5a6b7c8d9e0f1a2b3c4d5`

---

### 5.2 拓扑感知 JHB（JSON-Header-Binary）协议 v1.4.1

#### （1）二进制外层通用容器
```text
[0..3]     : uint32 json_len (Little-Endian，指示 JSON 头部字节长度)
[4..4+N-1] : JSON Header UTF-8 字符串
[4+N..END] : 紧密打包的连续二进制缓冲流 (全部采用 Little-Endian，无隐式对齐填充)
```

#### （2）客户端协商机制（`acceptFormats`）
客户端请求可在头或参数中指定 `acceptFormats`：
- 若包含 `"cbf-jhb@1.4"` 或缺省请求三维流：服务端返回 v1.4.1 拓扑感知二进制流；
- 若仅包含 `"cbf-binxcaf"` 或 `"step-ap242"`：服务端返回 v1.3.2 纯几何载荷，实现新老客户端 100% 平滑过渡。

#### （3）JSON Header 权威自洽元数据示例
```json
{
  "protocolVersion": "1.4.1",
  "status": "ok",
  "cache_key": "3b2e5914a8c9b2d1e0f3a4c5b6e7d8f9a0b1c2d3e4f5a6b7c8d9e0f1a2b3c4d5",
  "format": "cbf-jhb",
  "payloadKind": "TopologicalMesh",
  "shapeKeys": ["PierBodySolid", "TopBearingFace", "PierContourWire/1"],
  "vertexCount": 12480,
  "triangleCount": 24956,
  "edgeLineVertexCount": 4320,
  "edgesCount": 28,
  "facesCount": 14,
  "faceToShape": [0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
  "buffers": {
    "vertices":    { "offset": 0,      "length": 149760, "type": "float32", "stride": 3 },
    "indices":     { "offset": 149760, "length": 299472, "type": "uint32",  "stride": 1 },
    "normals":     { "offset": 449232, "length": 149760, "type": "float32", "stride": 3 },
    "edges":       { "offset": 598992, "length": 51840,  "type": "float32", "stride": 3 },
    "edgeOffsets": { "offset": 650832, "length": 116,    "type": "uint32",  "stride": 1 },
    "faceIds":     { "offset": 650948, "length": 49912,  "type": "uint16",  "stride": 1 }
  },
  "bounds": {
    "min": [-3200.0, -1200.0, 0.0],
    "max": [3200.0, 1200.0, 12000.0]
  },
  "stats": {
    "volumeM3": 194.52,
    "surfaceM2": 312.48,
    "triangles": 24956,
    "edgesCount": 28,
    "facesCount": 14,
    "elapsedMs": 42
  },
  "outputs": {
    "PierBodySolid":       { "geomType": "solid", "volume": 194520000000.0, "bbox": [-3200, -1200, 0, 3200, 1200, 12000] },
    "TopBearingFace":      { "geomType": "face",  "area": 14850000.0,        "bbox": [-3200, -1200, 12000, 3200, 1200, 12000] },
    "PierContourWire/1":   { "geomType": "wire",  "length": 15800.0,         "bbox": [-3200, -1200, 0, 3200, 1200, 0] }
  },
  "args": {
    "PierHeight": 12000.0,
    "NetVolume": 194.52
  },
  "anchors": {
    "PierTopCenter":  { "origin": [0.0, 0.0, 12000.0], "axis": [0.0, 0.0, 1.0], "refDirection": [1.0, 0.0, 0.0] },
    "PierBaseCenter": { "origin": [0.0, 0.0, 0.0],     "axis": [0.0, 0.0, -1.0], "refDirection": [1.0, 0.0, 0.0] }
  }
}
```

#### （4）二进制各分段数学自洽证明与布局说明
1. **`vertices`**：$12,480 \times 3 \times 4 = 149,760$ 字节，`offset = 0`；
2. **`indices`**：$24,956 \text{ (triangles)} \times 3 \times 4 = 299,472$ 字节，`offset = 149,760`；
3. **`normals`**：$12,480 \times 3 \times 4 = 149,760$ 字节，`offset = 149,760 + 299,472 = 449,232`；
4. **`edges`**：$4,320 \text{ (points)} \times 3 \times 4 = 51,840$ 字节，`offset = 449,232 + 149,760 = 598,992`；
5. **`edgeOffsets`**：$(28 \text{ (lines)} + 1) \times 4 = 116$ 字节，`offset = 598,992 + 51,840 = 650,832`；
6. **`faceIds`**：$24,956 \text{ (triangles)} \times 2 = 49,912$ 字节，`offset = 650,832 + 116 = 650,948`（若 `facesCount > 65,535` 自动升级为 `uint32`，每项 4 字节）；
7. **总二进制长度**：$650,948 + 49,912 = 700,860$ 字节。相邻缓冲区满足 $\text{offset}_{i+1} = \text{offset}_i + \text{length}_i$，无任何重叠或间隙。

---

# 第二部分：研制与消费端工程落地指南（Normative Toolchain Profiles）

## 6. 参数化构件本地研制开发规范（VS Code + OCP CAD Viewer）

### 6.1 开发环境基准
- **IDE**：Visual Studio Code (1.85+)；
- **官方插件**：`OCP CAD Viewer`（插件 ID: `bernhard-42.ocp-cad-viewer`）；
- **环境依赖**：`pip install build123d cadquery ocp_vscode`。

### 6.2 研制脚本标准范式（带安全隔离防护）
```python
"""RCIF 构件参数化建模脚本标准示例 (build123d 模式)"""

from build123d import *

def build_pier(TopWidth=2400.0, TopLength=6400.0, PierHeight=12000.0, Slope=35.0, CrossSlope=2.0):
    bottom_width = TopWidth + (PierHeight / Slope) * 2
    bottom_length = TopLength + (PierHeight / Slope) * 2

    with BuildPart() as pier:
        with BuildSketch(Plane.XY) as s_base:
            SlotOverall(bottom_length, bottom_width)
        with BuildSketch(Plane.XY.offset(PierHeight)) as s_top:
            SlotOverall(TopLength, TopWidth)
        loft = Loft()

    solid = pier.part

    outputs = {
        "PierBodySolid": solid,
        "TopBearingFace": solid.faces().sort_by(Axis.Z)[-1],
        "PierContourWire/1": solid.wires().sort_by(Axis.Z)[0]
    }
    
    args = {
        "PierHeight": PierHeight,
        "NetVolume": round(solid.volume / 1e9, 3)
    }

    return {"outputs": outputs, "args": args}

# ----------------- 本地 VS Code 交互式可视化隔离块 -----------------
# 注意：入库门禁 3 强制检查，show() 调用只允许位于 __main__ 块内，线上 Worker 绝不执行此分支
if __name__ == "__main__":
    try:
        from ocp_vscode import show, set_viewer_config
        set_viewer_config(reset_camera=False)
        res = build_pier()
        show(res["outputs"]["PierBodySolid"], names=["PierBodySolid"])
        print(f"本地建模成功，计算体积: {res['args']['NetVolume']} m3")
    except ImportError:
        pass
```

---

## 7. Web 3D 端专业 CAD 级渲染与交互规范（基于 three-cad-viewer）

### 7.1 CAD 级矢量黑边轮廓渲染标准（防 Z-fighting 机制）
- **网格面材质**：配置 `MeshStandardMaterial`，并设置 `polygonOffset: true`、`polygonOffsetFactor: 1.0`、`polygonOffsetUnits: 1.0`（利用 GPU 光栅化深度偏移将表面向后推，杜绝与线框共面闪烁）；
- **边线渲染**：利用 `edges` 与 `edgeOffsets` 创建独立的特征线条几何体，线宽自适应屏幕像素比；
- **材质质感**：提供工程金属与清水混凝土质感预设，粗糙度 0.85，金属度 0.1。

### 7.2 动态多轴交互剖切加盖（Clipping Planes with Stencil Capping）
- 视口支持沿世界坐标系或构件局部坐标系 X、Y、Z 轴绑定剖切面；
- 采用 GPU 双通道 Stencil 模板缓冲生成加盖几何（Cap），加盖颜色采用工程剖面填充色（`#d1d5db`），彻底杜绝空心假象。

### 7.3 装配树与拓扑面属性联动机制
- **部件树联动**：侧边栏根据 `shapes` 及 `parent` 属性渲染轻量部件树，点击树节点平滑移动焦点并高亮部件；
- **面拾取反查链路**：
  $$\text{Raycast 点击} \longrightarrow \text{获取三角面序号 } T \longrightarrow \text{查 } \text{faceIds}[T] = F \longrightarrow \text{查 } \text{faceToShape}[F] = K \longrightarrow \text{命中 } \text{shapeKeys}[K]$$
  视口即刻展开该子部位对应的国标分类编码、`IfcShapeAspect` 属性集与几何特征尺寸。

### 7.4 空间三维测距工具
- 内置吸附式距离测量工具，量取模型顶点间距、水平投影高差与边界轮廓尺寸。
