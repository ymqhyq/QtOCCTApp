# RCIF 构件数据格式与工程落地规范 v1.4.2 — Round 3 复核终审报告

- **审核方**：DeepSeek
- **被审规范**：[rcif_component_format_spec_v1.4.2.md](file:///D:/QtOCCTApp/docs/rcif_component_format_spec_v1.4.2.md)（更新版，2026-10-19 09:20）
- **前序基线**：[rcif_component_format_spec_v1.4.1.md](file:///D:/QtOCCTApp/docs/rcif_component_format_spec_v1.4.1.md)
- **架构护栏**：[review_ledger.md](file:///D:/QtOCCTApp/docs/review_ledger.md) 中 G1~G4
- **状态机**：[review_loop_state.json](file:///D:/QtOCCTApp/docs/review_loop_state.json)
- **审核时间**：2026-10-19T09:25:40+08:00

---

## 0. 本轮审核结论摘要与状态表

### 0.1 总体结论

更新后的 v1.4.2 对 Round 2 的 B4、M6、m5、m6、m7 做出了**真实闭环**，且本轮 DeepSeek 对关键哈希做了独立复算：

- **B4/m2 已彻底闭环**：
  - §6.2 的 `generator.py` 代码块按 UTF-8 + LF 保存为 **1,486 字节**，实算 SHA-256 为 `0c034d0fe7643a9ffcfffb1d79ec9ce2363a0e79baa0ca599cb99cd259f1eb0d`，与 `recipe.digest`、`CacheKeyPayload.digest` 一致；
  - `PierBody` 的 `CacheKeyPayload` 实算 SHA-256 为 `8361c4b4b7916b48c16a2687f3d73f1fb3ccf1f850f12e201333f77257697405`，与 `geometry.defaultCacheKey`（L179）、测试向量 4（L323）、JHB `cache_key`（L359）三处一致。
  - 这不再是“改一个期望值迁就实现”，而是“源码 → digest → payload → cacheKey → JHB”全链路一致。
- **M6 已闭环**：`acceptFormats` 五行矩阵覆盖 `cbf-jhb@1.4`、`cbf-binxcaf`、`step-ap242`、`gltf-2.0` 与 406，旧客户端（含 glTF 查看器）有明确回退。
- **m5 已闭环**：`edgeToShape` 长度 = `edgesCount`，边线拾取链路与面拾取对称。
- **m6 已闭环**：明确 `LineSegments2`/`LineMaterial` + `material.resolution` DPI 更新，不再依赖被驱动限制的 `LineBasicMaterial.linewidth`。
- **m7 已闭环**：NOTE 明确 `cbf-jhb@1.4` 为 1.4.x 主次版本族、`protocolVersion 1.4.2` 为精确补丁版本，语义自洽。

本轮仅剩 **2 个 `MINOR`**（多值 `acceptFormats` 优先级、`edgeToShape` 示例全部指向 wire），**0 个 `BLOCKER`、0 个 `MAJOR`**。按收敛规则，**签发 `APPROVED`**。

### 0.2 状态表

| 项目 | 结果 |
| :--- | :--- |
| 当前轮次 | Round 3 / 最大 10 轮 |
| 被审版本 | v1.4.2（09:20 更新版） |
| 上轮问题 | 1 `BLOCKER` + 1 `MAJOR` + 3 `MINOR` |
| 上轮已闭环 | B4、M6、m5、m6、m7、m2 |
| 本轮 `BLOCKER` | **0** |
| 本轮 `MAJOR` | **0** |
| 本轮 `MINOR` | 2（m8、m10） |
| 本轮 `INFO` | 2 |
| 本轮裁决 | **`APPROVED`** |
| 下一回合 | `DONE`（闭环结束） |

### 0.3 收敛轨迹

| 轮次 | 被审版本 | BLOCKER | MAJOR | 裁决 |
| :---: | :---: | :---: | :---: | :--- |
| Round 1 | v1.4 | 4 | 5 | REVISE |
| Round 2 | v1.4.1 | 1 | 1 | REVISE |
| **Round 3** | **v1.4.2（更新版）** | **0** | **0** | **APPROVED** |

---

## 1. 上轮问题闭环核验

### 1.1 逐项核验表

| 编号 | 上轮级别 | Gemini 处置 | DeepSeek 核验结论 | 证据/说明 |
| :--- | :---: | :---: | :--- | :--- |
| **B4** | `BLOCKER` | `ACCEPTED` | **已闭环** | 更新版 §6.2 脚本 1,486 字节，SHA-256 = `0c034d...`；§5.1 向量 4 实算 = `8361c4...`；`defaultCacheKey`（L179）、JHB `cache_key`（L359）同值。 |
| **M6** | `MAJOR` | `ACCEPTED` | **已闭环** | §5.2(2)（L343-L349）五行矩阵含 `cbf-jhb@1.4`、`cbf-binxcaf`、`step-ap242`、`gltf-2.0`、406。 |
| **m5** | `MINOR` | `ACCEPTED` | **已闭环** | `edgeToShape`（L369）长度 28 = `edgesCount`；§7.3（L493）边拾取反查链路成立。 |
| **m6** | `MINOR` | `ACCEPTED` | **已闭环** | §7.1（L482）明确 `LineSegments2`/`LineMaterial`、`material.resolution` 按 DPR 刷新、线宽 1.5~2.0。 |
| **m7** | `MINOR` | `ACCEPTED` | **已闭环** | §5.2(2) NOTE（L351-L352）明确 `@1.4` 为主次版本族、`protocolVersion 1.4.2` 为补丁版本。 |
| **m2** | `MINOR` | `ACCEPTED` | **已闭环** | `recipe.digest=0c034d...` 与 §6.2 脚本实算一致；示例包可通过门禁 3。 |

### 1.2 全链路实算证据

**（1）脚本摘要**

```text
§6.2 generator.py：UTF-8 + LF，1,486 字节
SHA-256 = 0c034d0fe7643a9ffcfffb1d79ec9ce2363a0e79baa0ca599cb99cd259f1eb0d
recipe.digest = 0c034d0fe7643a9ffcfffb1d79ec9ce2363a0e79baa0ca599cb99cd259f1eb0d  ✅
```

**（2）PierBody CacheKeyPayload**

```text
{"cacheKeyVersion":1,"digest":"0c034d0fe7643a9ffcfffb1d79ec9ce2363a0e79baa0ca599cb99cd259f1eb0d","engine":"build123d","inputParams":{"CrossSlope":2,"PierHeight":12000,"Slope":35,"TopLength":6400,"TopWidth":2400},"kernelId":"occt","kernelVersion":"8.0.0","source":"generators/generator.py","tolerance":{"angular":0.001,"linear":0.01},"units":{"angle":"rad","length":"mm"},"version":"1.4.2"}

SHA-256 = 8361c4b4b7916b48c16a2687f3d73f1fb3ccf1f850f12e201333f77257697405
geometry.defaultCacheKey = 8361c4...  ✅
JHB cache_key = 8361c4...             ✅
Vector 4 expected = 8361c4...         ✅
```

B4/m2 正式闭环；这是本轮批准的关键依据。

---

## 2. 本轮剩余问题（均为 MINOR，不阻断）

### 2.1 协议协商细节

#### m8【MINOR】多值 `acceptFormats` 的服务端优先级未定义

- 表（L343-L349）用“包含/仅包含”描述分支；若客户端发送多个格式（如 `["gltf-2.0","cbf-binxcaf"]` 或 `["cbf-binxcaf","step-ap242"]`），未定义服务端选择哪一个。
- **建议**：给出明确优先级，例如 `cbf-jhb@1.4.2 > step-ap242 > cbf-binxcaf > gltf-2.0`，或按 HTTP `q` 值/数组顺序协商；无交集返回 406。属于内容协商细节，计 `MINOR`。

### 2.2 示例完整性

#### m10【MINOR】`edgeToShape` 示例 28 条边全部指向 `PierContourWire/1`

- 头部 `edgeToShape`（L369）28 项全部为 `2`，即全部边归 `shapeKeys[2] = PierContourWire/1`。
- 若 `edges` 只提取具名 wire 的折线，这是正确的；若也包含 `PierBodySolid` 的 B-Rep 边，则存在误归属。建议在 §5.2 明确“edges 的提取范围是具名 wire 特征线，不含 solid 的全部拓扑边”，或给出 `[0,0,...,2,2]` 的混合映射示例。计 `MINOR`。

---

## 3. 过度设计自检

1. 本轮修复只涉及真实摘要、版本语义、边线映射与粗线实现；没有新增服务/中间件，没有把外部语义写回 `component.json`。
2. `edgeToShape` 只增加一个 28 项紧凑数组；`shapes[*].parent` 只增加一个可选字段；`acceptFormats` 只是协商矩阵；均符合 G1~G4 的极简原则。
3. 未将“专业 CAD 级”“零拷贝”等表述计入阻断项；未要求引入装配 BOM、权限中心、Kafka、服务网格等重型组件。
4. 未把远期三期规划、命名偏好、章节结构计入阻断项。

---

## 4. 裁决块

### 4.1 问题统计

| 级别 | 数量 | 编号 |
| :--- | :---: | :--- |
| `BLOCKER` | **0** | — |
| `MAJOR` | **0** | — |
| `MINOR` | 2 | m8、m10 |
| `INFO` | 2 | 见 §4.3 |

### 4.2 通过结论

- **数据契约**：JHB buffer 布局、`edgeOffsets`、`faceToShape`、`edgeToShape` 自洽；`CacheKeyPayload` 与四条向量（含 `PierBody`）实算一致；`defaultCacheKey`、JHB `cache_key`、`recipe.digest` 全链路一致。
- **兼容性**：`acceptFormats` 覆盖 JHB/BinXCAF/STEP/glTF/406，旧客户端有明确回退。
- **Code-CAD 安全**：`show()` 仅限 `__main__`、Worker 用 `importlib` 调 `entry`、容器非 root/断网/只读/限额，AST 作为纵深防御。
- **Web 交互**：矢量黑边、Stencil 加盖、`parent` 部件树、面/边拾取、测距均有数据与实现路径。
- **护栏**：§0.1 明确继承 v1.3.2 的 G1~G4、三级映射、真值表、`selector`、逐子形状 bbox；未出现外部映射回写构件包或前端公式引擎。

### 4.3 `INFO` 备忘（不计入阻断）

- **INFO-1**：v1.4.2 的“源码 → digest → payload → cacheKey”自检链是正确的闭环方式，建议后续示例都沿用。
- **INFO-2**：审核时 `review_ledger.md` 仍未追加 v1.4/v1.4.1/v1.4.2 的处置记录；建议归档时补齐，使“账本—规范—状态机”一致。该流程项不影响技术通过结论。

### 4.4 最终裁决

```json
{
  "verdict": "APPROVED",
  "blockerCount": 0,
  "majorCount": 0,
  "minorCount": 2,
  "infoCount": 2
}
```

**终审签署**：v1.4.2（09:20 更新版）已消除全部 `BLOCKER` 与 `MAJOR`；剩余 2 项 `MINOR` 为多值协商优先级与示例边线映射的说明性加固，不阻塞编码/实施。RCIF v1.4.2 正式通过 DeepSeek 审核。
