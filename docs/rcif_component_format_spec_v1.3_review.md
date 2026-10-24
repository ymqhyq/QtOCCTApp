# RCIF v1.3.1 审核报告（Round 3 · DEEPSEEK · 终审）

- **审核对象**：[rcif_component_format_spec_v1.3.1.md](rcif_component_format_spec_v1.3.1.md)
- **审核依据**：[review_ledger.md](review_ledger.md)（护栏 G1~G4 + Round 1/2 处置账本）
- **轮次**：Round 3
- **裁决**：**APPROVED** — 0 BLOCKER / 0 MAJOR

---

## 0. 终审结论

Round 2 提出的 **2 BLOCKER + 2 MAJOR 已全部实质消除**，且消除方式经本机实测复核，非仅文字声明：

| 编号 | 问题 | 本轮实测复核结果 | 判定 |
| :--- | :--- | :--- | :---: |
| **B1** | 算量换算硬编码 `mm→m` | 已改为 `factor(n) = (u_src/u_tgt)^n`，两侧单位必须命中枚举 `{mm,cm,dm,m}`，未知单位直接报 `error`（L403） | ✅ **CLOSED** |
| **B2** | `DeterministicJSON` 未规定 Unicode 转义 | 新增规则 4（UTF-8 原生直出、禁 `\uXXXX`）+ 向量 3；**实算哈希逐字节命中** | ✅ **CLOSED** |
| **M1** | 同类型子形状错位不可检出 | `_shapeKeys` 扩展为 `{key, type, bbox}`，恢复时比 `Bnd_Box` 6 极值（容差 `1e-2` mm） | ✅ **CLOSED** |
| **M2** | `scripts-service/cq_script/` 与 `Wedge.py`/`Pier.py` 不存在 | 已改为仓库根 `cq_script/`，并点名 6 个**实测存在**的脚本 | ✅ **CLOSED** |

三项 `INFO`（`Surface3D`、请求侧 `acceptFormats`、`ports` 与 `defaultCacheKey` 关联）亦已顺带修复。

---

## 1. 四项 BLOCKER/MAJOR 闭环复核（逐条实测）

### B1 算量单位换算 —— 已改为双侧动态因子
L403 现行规定：
- `environment.units.length` 与 `quantityUnits.length` **必须**属于 `{"mm":0.001, "cm":0.01, "dm":0.1, "m":1.0}`；
- 未知单位在包校验或导出期**直接报 `error`**（不再静默）；
- `factor(n) = (u_src / u_tgt)^n`，`n = 1/2/3` 对应长度/面积/体积。

**验算**：
| 场景 | 因子 | 结果 |
| :--- | :--- | :--- |
| `mm → m`，体积（n=3） | `(0.001/1.0)^3 = 1e-9` | 与旧规则一致 ✔（向后兼容） |
| `cm → m`，面积（n=2） | `(0.01/1.0)^2 = 1e-4` | 正确（旧规则会错用 1e-6） |
| `m → m` | `1` | 正确（旧规则会错除 1e3~1e9） |

结论：原先"非 mm 构件算量静默 10³~10⁹ 倍错误"的通道**已被关闭**，且退化路径改为显式 `error`。

### B2 Unicode 转义 —— 规则 + 向量均已落地
L441 新增规则 4：键名与字符串值以 UTF-8 原始字节直出，禁 `\uXXXX`，明确 Python `ensure_ascii=False` 与 C++ `nlohmann::dump()` 一致；仅 `\"`、`\\`、`U+0000–U+001F` 转义。

L449-451 向量 3 **本机实算复核**：
```
输入 : {"Layout":"梅花形"}
字节 : 7b 22 4c 61 79 6f 75 74 22 3a 22 e6 a2 85 e8 8a b1 e5 bd a2 22 7d   (22 字节)
实算 : 1135ce65b3a41eb7ab8e979d812eb41d730b8e0227d76c9dac782cb32497d647
声明 : 1135ce65b3a41eb7ab8e979d812eb41d730b8e0227d76c9dac782cb32497d647   ✔ 命中
```
反向对照（证明该规则确实约束了实现）：
```
若误用 ensure_ascii=True → {"Layout":"\u6885\u82b1\u5f62"} → 47779228714036dacdd2fcabeb461ff5a51362c8005ead8839c9bda2cbcdc9b0
```
两个哈希完全不同，证明规则 4 是**有区分力**的必要约束，而非装饰性条文。跨语言 `cacheKey` 不一致的通道**已关闭**。

### M1 同类型子形状防错位 —— 已用生成期 bbox 闭环
L543/L547 现行规定：
- `_shapeKeys` 条目为 `{"key", "type", "bbox": [xmin,ymin,zmin,xmax,ymax,zmax]}`；
- 恢复时除数量与 `ShapeType()` 外，逐项比对 `Bnd_Box` 6 个极值，容差 `1e-2` mm；不符即判缓存失效。

**为什么该方案满足 G3（零序列化开销）**：bbox 并非在加载期新算的昂贵指标，而是 §5.2 JHB `outputs` 在**生成期已返回**的元数据（L475-478），随 `_shapeKeys` 一并落盘。加载期只在**已反序列化并驻留内存**的子形状上做 6 次 double 比较，不触碰 BRep 序列化、不做 SHA-256——G3 明令禁止的两项开销均未引入。

`ShoulderLine/Left_1`（bbox `[0,0,8000,20000,0,8000]`）与 `ToeLine/Left_1`（bbox `[0,12000,0,20000,12000,0]`）在示例中已可区分，机制有效。

### M2 脚本路径 —— 已改为真实路径与真实文件
L110、L238、L245 现均指向仓库根 `cq_script/`，并写明解析规则 `<repo>/cq_script/<ModelType>.py`（与 [`main.py:224`](../scripts-service/main.py#L224) 一致）。

**文件系统实测**：
```
D:\QtOCCTApp\cq_script\Pile.py            True
D:\QtOCCTApp\cq_script\PierBody.py        True
D:\QtOCCTApp\cq_script\PierTray.py        True
D:\QtOCCTApp\cq_script\BridgePier2.py     True
D:\QtOCCTApp\cq_script\Girder.py          True
D:\QtOCCTApp\cq_script\PileCap.py         True
```
`subgrade_slope.py` 已正确标注为"规划新增脚本"，不再冒充既有资产。`service://` 解析不再产生不存在路径，**代码落地阻塞已解除**。

---

## 2. 回归检查（确认本轮修订未引入新缺陷）

| 检查项 | 结果 |
| :--- | :--- |
| 向量 1 复算 | `51a01f79…0cb0c` ✔ 与声明一致 |
| 向量 2 复算 | `0fee2637…afac0` ✔ 与声明一致 |
| 向量 3 复算 | `1135ce65…97647` ✔ 与声明一致 |
| `geometry.defaultCacheKey`（L178）与向量 2 | 完全一致 ✔ |
| 规则编号连续性 | 现为"五条规则"，1–5 编号连续，无"四条"残留 ✔ |
| `DeterministicJSON` 四类要素覆盖 | 键序/空白（规则 1）、浮点（规则 2）、缺失与外部几何（规则 3）、字符串转义（规则 4）、向量（规则 5）——**已完备** ✔ |
| `IFC` 映射属性名 | `IfcShapeAspect.Description`、`IfcShapeRepresentation.RepresentationIdentifier/Type` 均为真实属性；`Surface3D` 为标准 `RepresentationType` 取值 ✔ |
| `_shapeKeys` 写入位置 | 写入 `GeoParameter` 的 JSON，**不进入 `cacheKey`**（键只由 `recipe.inputs` 决定），无哈希污染 ✔ |
| 脚本解析路径 | 与 `main.py:224` 实际逻辑一致 ✔ |

---

## 3. 遗留 INFO（不构成阻塞条件，建议后续版本顺手处理）

| 编号 | 内容 | 说明 |
| :--- | :--- | :--- |
| **INFO-A** | 向量 3 文案称"21 字节 UTF-8"，实测为 **22 字节**（`{`+`"Layout"`+`:`+`"`+9+`"`+`}` = 22） | 十六进制串与期望哈希**均正确**，仅字节计数笔误；任何 JSON 解析器产出的是正确字节，不影响互操作。建议改为 22。 |
| **INFO-B** | 向量 2 的 `recipe.digest` 仍为 `e3b0c442…b855`（实测即 SHA-256("")） | 作为占位自洽；建议换非空示例值，避免实现方误以为允许留空。 |
| **INFO-C** | `_shapeKeys` 的 bbox 校验在严格复杂度意义上是对已加载子形状的有界遍历，而非数学意义上的 $O(1)$ | 相对 BRep 序列化可忽略（数量级差 3~4 个数量级），且已满足 G3 的禁止项。若未来追求**绝对零遍历**，可演进为"每个子形状各自持有独立 `TNaming` 属性"，从结构上消除顺序假设。属演进建议。 |
| **INFO-D** | `semanticProfileHint` 为 `@1.3`，profile 内 `version` 为 `1.3.1` | 语义兼容（同 minor）；建议注明"hint 仅锁主次版本，补丁版本可漂移"。 |
| **INFO-E** | 示例中 `SlopeFace/Left_1` 的 bbox 与 `Body` 相同 | 机制靠 `type` 已可区分，不影响正确性；若给一个多级边坡示例（两个 `SlopeFace` bbox 不同），能更直观展示 bbox 的区分力。 |

---

## 4. 护栏合规声明（G1~G4 自检）

| 护栏 | 本报告是否违反 | 说明 |
| :--- | :---: | :--- |
| **G1 极简构件包 + 外部映射** | 否 | 本轮未要求新增任何 `component.json` 字段；B1/B2 均为**既有字段的语义澄清**，M1 的 bbox 只存在于运行时 `GeoParameter` 元数据，M2 为文档路径修正。 |
| **G2 计算逻辑内聚引擎** | 否 | 未引入表达式/AST/伴随文件；bbox 由引擎经 JHB `outputs` 提供。 |
| **G3 拒绝冗余双写与性能劣化** | 否 | 明确判定 bbox 校验不构成"序列化 BRep 重算 SHA-256"；`schemaVersion`/`shapes` 哈希的驳回继续被接受。 |
| **G4 收敛判定标准** | 是（严格遵守） | 本轮 0 BLOCKER / 0 MAJOR；5 条 INFO 全部属于措辞、演进建议或示例观感，按 G4 不得阻塞。 |

---

## 5. 固定裁决块

```
ROUND              : 3
VERDICT            : APPROVED
blockerCount       : 0
majorCount         : 0
infoCount          : 5

CLOSED_THIS_ROUND:
  B1  算量换算改为双侧动态因子 + 单位白名单 + 未知单位报错        (L403)
  B2  DeterministicJSON 规则 4（UTF-8 原生直出）+ 向量 3 实算命中  (L441, L449-451)
  M1  _shapeKeys 扩展为 {key, type, bbox} + Bnd_Box 容差校验      (L543, L547)
  M2  cq_script/ 真实路径 + 6 个实测存在的脚本 + 规划脚本标注      (L110, L238, L245)

VERIFIED_BY_EXECUTION:
  vector 1 → 51a01f79a9ca9470a9d4373b8f06ab39e92a849bed2035ee58d92f7bc640cb0c
  vector 2 → 0fee26374b7a518d8098da4ad6c1ad52716d9f33f5bf183fe0f75f90f4bafac0
  vector 3 → 1135ce65b3a41eb7ab8e979d812eb41d730b8e0227d76c9dac782cb32497d647
  (反例 ensure_ascii=True → 47779228…c9b0，证明规则有区分力)

REMAINING INFO (non-blocking):
  INFO-A 向量 3 字节数 21 → 应为 22
  INFO-B digest 占位为 SHA-256("")
  INFO-C bbox 校验非严格 O(1)，可演进为逐子形状独立 TNaming
  INFO-D hint @1.3 vs profile 1.3.1 的补丁漂移说明
  INFO-E 示例 bbox 区分力展示

FINAL: v1.3.1 可发布为正式标准基线（Approved Specification Baseline）。
```

**发布后建议**：按附录 B 的 P0 顺序执行 1→2→3→4；其中 P0-2（前端/服务端 `DeterministicJSON` 实现）应直接用上述三条向量做 CI 断言，任何一端实现漂移都会被立即捕获。
