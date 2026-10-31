# RCIF 构件创作闭环工作流：实现说明与使用指南

> **2026-10-26 结构升级**：工作台已改为“固定根目录 → 工作区 → 会话”模型，左侧为工作区/会话树，新增“当前会话”页；原左侧 7 功能区改为顶部标签，底部文件路径输入已移除。“打开智能助手”与多会话双向切换见 [铁路构件智能助手工作区/会话模型](rcif_assistant_workspace_model_2026-10-26.md)。本文保留转换、3D、批注、入库的实现细节。

- **日期**：2026-10-26
- **范围**：`dsh-rcif`（Host 插件 + 客户端会话桥接 + 八功能区工作台）、`rcif-tools`（PDF/STEP/IFC 转换、包检查、入库）、`rcif-web`（构件库导入/审核）
- **目标**：把“对话 → 转换 → 工作台审阅 → 反馈 → 重建 → 入库”做成可运行闭环，并保证多会话下工作台内容与 DSH 会话一一对应。

---

## 1. 用户视角的完整流程

```text
① 对话输入：“把 xx.pdf / xx.step / xx.ifc 转换为构件”
      Agent 选工具：rcif_ingest_pdf / rcif_ingest_step / rcif_ingest_ifc
      （也可在工作台底部“新建/转换构件”里发起）
                         │
                         ▼
② 生成 .rcif → 绑定到发起工具调用的会话 → 工作台 2.5s 内自动加载
      七个功能区：元数据 / 3D 视图 / 构件参数 / 构建脚本 /
                  命名部位 / 型号列表 / 构件入库
                         │
                         ▼
③ 审阅与反馈
      - 点 3D 部位 → HUD + 命名部位联动
      - 读 generators/generator.py，点行号选行写批注
      - 截图 3D（自动附加缩略图）
      - 自然语言意见
      统一 POST /api/feedback → sessionController.prompt
                         │
                         ▼
④ Agent 收到“当前会话 + 当前构件包 + 意见 + 批注 + 截图路径”
      → 修改参数/生成器 → rcif_compile_package 或 rcif_ingest_*
                         │
                         ▼
⑤ 新包绑定回同一会话 → 工作台自动刷新 → 重复 ②–④
                         │
                         ▼
⑥ 构件入库：写入 out/library 清单；构件库在线时创建族 + 审核任务
```

---

## 2. 工作台七个功能区

| 功能区 | 内容 | 数据来源 |
| :--- | :--- | :--- |
| **元数据** | 族/名称/版本/来源模式/引擎/精确 B-Rep/方量/包围盒/参数-型号-部位计数/包 SHA-256/文件清单/checksums/provenance/抽取模式/changelog | `component.json` + `checksums.json` + `.provenance.json` |
| **3D 视图** | 真实 `display/preview.glb`、等轴测/正视/侧视/俯视、棱线、部位拾取、HUD、截图反馈 | 包内 GLB（Z-up，转到 three 的 Y-up 显示） |
| **构件参数** | 参数 Schema 表 + 型号预设 JSON | `component.parameters` |
| **构建脚本** | 包内 `generators/generator.py`，行号选择与批注；原生构件显示“无构建脚本” | 包内 generator.py |
| **命名部位** | shapes 表：键/node/标签/语义码/几何类型/材质/可见性/定位 | `component.shapes` |
| **型号列表** | 49 类型号表：型号/标签/图号/参数数/cacheKey/跳转 3D/参数 | `component.parameters.presets` |
| **构件入库** | 包校验摘要 + 入库说明 + 写入清单 + 提交构件库 | `/api/library/ingest` |

底部常驻“向智能助手反馈”栏：自然语言、截图、新建/转换面板。

---

## 3. 转换语义（诚实边界）

| 输入 | 工具 | sourceMode | 能力 | 说明 |
| :--- | :--- | :--- | :--- | :--- |
| STEP | `rcif_ingest_step` → `step2rcif.py` | `native-brep` | 精确 B-Rep、命名部位、GLB | 无参数化变参；需参数化时另写 generator |
| IFC | `rcif_ingest_ifc` → `ifc2rcif.py` | `mesh-only` | 命名部位 + 三角网格 + 方量 | ifcopenshell 世界坐标、单位纠偏、法向不一致时体积保守估计 |
| PDF | `rcif_ingest_pdf` → `pdf2rcif.py` | `parametric` | 图纸真值 + 桥台模板生成器 + 49 型号草稿 | `extractionMode=canonical_rules_inferred`，**不是 OCR**，必须人工复核 |

PDF 草稿包的元数据会写入：

```json
"qa": {
  "extractionMode": "canonical_rules_inferred",
  "pdfFingerprint": "4332A-I Verified",
  "pageCount": 1,
  "sourcePdfSha256": "..."
}
```

---

## 4. 后端接口

Host 插件（`dsh-rcif/dist/index.js` + `dist/lib/*`）：

| 方法 | 路径 | 说明 |
| :--- | :--- | :--- |
| GET | `/rcif-studio/api/state` | 指定 sessionId 的当前包、能力、反馈历史（2.5s 轮询） |
| GET | `/rcif-studio/api/sessions` | DSH 会话目录：ID/标题/是否已有构件包 |
| GET | `/rcif-studio/api/assistant` | 智能助手地址与运行状态 |
| POST | `/rcif-studio/api/assistant/launch` | 调试后端下启动 DSH 实例（3090） |
| GET | `/rcif-studio/api/package` | 解析 .rcif 元数据/文件/校验/预览/脚本 |
| GET | `/rcif-studio/api/package/preview.glb` | 包内 GLB |
| GET | `/rcif-studio/api/package/source` | 包内生成器源码 |
| POST | `/rcif-studio/api/ingest` | PDF/STEP/IFC → RCIF，自动绑定会话 |
| POST | `/rcif-studio/api/command` | 工作台自然语言 → Agent |
| POST | `/rcif-studio/api/feedback` | 批注/截图/意见 → Agent（截图存 `out/feedback`） |
| POST | `/rcif-studio/api/library/ingest` | 入库清单 + 构件库 import |
| GET | `/rcif-studio/api/init-token` | owner Cookie + capToken 握手 |

FastAPI 调试后端（`rcif-tools/workbench_server.py`，3088）实现同一组只读/转换接口；
`command`/`feedback` 会如实返回 `delivered:false`，因为调试后端没有 Agent 会话。

### 4.1 包解析实现

- Node：`dist/lib/zip.js`（无依赖 ZIP 读取、Zip64/路径穿越/大小限制）+ `dist/lib/package.js`（component/checksums/GLB/source 摘要，按 mtime+size 缓存）。
- Python：`rcif-tools/package_api.py`（FastAPI 与测试共用，字段契约一致）。
- 包内 checksums 会逐文件重算 SHA-256；不匹配在“元数据/入库”页明确标红。

### 4.2 构建脚本进入包内

`pack_rcif_cli.py` 会把 `generators/` 一并复制进 `.rcif` 并纳入 `checksums.json`，
因此“构建脚本”页能直接读到真实生成器源码；PDF 草稿包因此也具备可审阅脚本。

### 4.3 多会话与工作台对应性

工作台不再维护单一 `currentPackagePath`，而是 **按 DSH sessionId 绑定构件包**：

```text
sessionPackages: Map<sessionId, { packagePath, updatedAt }>
```

保证对应性的四层机制：

1. **会话身份**：工作台 URL 固定携带 `?sessionId=...`；所有 `/state`、`/package`、
   `/feedback`、`/library/ingest` 请求都带同一 sessionId。
2. **包归属**：`/state?sessionId=X` 只返回 X 的包；X 的工具调用（`rcif_ingest_*`、
   `rcif_open_package`、`rcif_compile_package`）通过 `exec.agent.session.id` 写入 X，
   不会影响其他会话。
3. **产物隔离**：默认输出改为
   `rcif-tools/out/sessions/<sessionId>/<family>_<revision>.rcif`，
   避免同名族在不同会话间互相覆盖。
4. **UI 绑定**：工作台顶部“会话”下拉框从 `/api/sessions` 读取 DSH 会话；
   切换会话会清空当前 3D/元数据并重新握手，URL 同步更新。首次打开优先绑定
   “已有构件包的会话”，否则绑定最近的 `session-...` 会话；不会绑定子代理 UUID 目录。

“打开智能助手”按钮：

- DSH 后端：打开当前 DSH 实例（`assistantUrl`），工作台会话 ID 可一键复制核对；
- FastAPI 调试后端：若 DSH 未运行，会调用 `/api/assistant/launch` 启动
  `dsh --profile rcif-studio --port 3090` 后再打开。

**已知限制**：当前 DSH Web UI 没有暴露按 sessionId 的 URL 深链（前端 bundle 中未发现
`pushState`/`sessionId` 路由），因此“跳转”只能打开助手实例，具体会话仍需在助手会话列表中
选择同一 ID/标题。若要做到一键直达，需要追加一个 DSH 客户端 UI 插件（会话 header 按钮 +
conversation slot），这属于下一阶段。

![会话绑定与打开智能助手](rcif_workbench_sessions_2026-10-26.png)

---

## 5. 反馈与重建

工作台发送的 prompt 形态：

```text
【RCIF 工作台反馈 · annotation】
当前构件包：rcif-tools/out/sessions/<sessionId>/Abutment_1.0.0.rcif
用户意见：垫石顺桥向缩进改为 1300mm
代码批注：
<user_code_annotation_data>
[{"file":"generators/generator.py","lines":[40,43],"comment":"..."}]
</user_code_annotation_data>
3D 截图：rcif-tools/out/feedback/shot-....png（可用 read_image 查看）
请据此修改参数或 generators/generator.py，重新生成 .rcif；完成后工作台会自动刷新。
```

- 送达：插件调用 `sessionController.prompt({sessionId, requestId, mode:'followup', content:[text]})`；
- 会话正忙：返回 409；会话 ID 不对：`delivered:false` 并在 UI 显示原因；
- Agent 重建后 `rcif_compile_package` / `rcif_ingest_*` 内部调用 `setCurrentPackage`，
  工作台轮询 `/state` 自动刷新，无需手动复制路径。

---

## 6. 构件入库

1. 工作台“构件入库”页校验包 SHA/checksums/provenance；
2. 始终写入 `rcif-tools/out/library/<family>_<revision>_<timestamp>/`：
   `.rcif` + `.provenance.json` + `submission.json`；
3. 若 `http://127.0.0.1:5178` 在线：
   - 插件以 demo `asset-admin` 登录；
   - `POST /api/v1/components/import-rcif`；
   - 构件库创建/更新族，状态 `reviewing`，并生成审核任务（`RV-...`）；
4. 构件库不在线：返回 `mode:"inbox"`，清单可直接重试，不丢数据。

`import-rcif` 端点（`rcif-web/server/api.ts`）会把包内 component.json 映射为族：
identity、sourceMode、schema、presets、shapes、cacheKey；首次导入会克隆模板族作为网格占位。

---

## 7. 启动方式

```powershell
# 1) DSH 原生（推荐：Agent 反馈 + 签名包 + 入库）
启动助手.bat
# 打开 http://127.0.0.1:3090/rcif-studio/workbench/

# 2) 旧调试后端（仅离线：转换/查看；正式功能以 3090 为准）
启动工作台.bat
# 打开 http://127.0.0.1:3088/

# 3) 构件库（入库送审的目标）
start-rcif.bat
# 打开 http://127.0.0.1:5178/
```

同步插件到 profile：

```powershell
# 打包 + 备份 + 安装 + 哈希校验
.\scripts\sync_rcif_plugin.ps1
# 只校验当前 profile 副本是否与仓库一致
.\scripts\sync_rcif_plugin.ps1 -Check
```

---

## 8. 验证记录

| 验证 | 结果 |
| :--- | :--- |
| 宿主契约测试 | 19/19 PASS（路由/工具/鉴权/包解析/ingest/反馈/入库/工作区会话/上传/PDF 限制/发送） |
| Python 契约测试 | 23/23 OK（跨语言向量、可复现归档、Ed25519、IFC 转换、FACETED_BREP STEP 回退、通用 PDF 草稿/重建、GATE-01 安全拒绝、RFA/GRep 解析） |
| rcif-web vitest | 39/39 PASS |
| STEP 转换 | `梁.step` → `BoxGirderTest_1.0.0.rcif` 成功 |
| IFC 转换 | `bridge.ifc` → 27 个命名部位、体积 3309.443 m³、bounds 95.75m |
| PDF 转换 | 测试图 → `PdfUi_0.1.0.rcif`：49 型号、8 参数、8 部位、包内 generator.py |
| 工作台 UI（headless Chrome） | 7 个功能区；16 个会话可选；自动绑定最近用户会话；3D 27 部位；49 型号；截图 270KB；0 JS 异常 |
| 入库 | 构件库生成 `RV-2026-324`（PdfUi，reviewing，49 型号，8 shapes） |
| FastAPI 调试接口 | state/package/preview/source/ingest/library/sessions/assistant 实测通过 |

---

## 9. 已知限制

1. **PDF 不是 OCR**：图号指纹 + 规范规则推断 + 桥台模板几何；`extractionMode` 已如实标注，必须人工复核。
2. **IFC 是 mesh-only**：不做 B-Rep 重建，不承诺参数化变参。
3. **构件库网格占位**：`import-rcif` 首次导入克隆模板网格，3D 详情暂不读取包内 GLB。
4. **原生 React Tab 未实现**：仍通过 `ui-sidebar-browser + iframe` 打开；客户端插件需要独立打包与真实启动验证。
5. **助手会话深链缺失**：当前只能打开助手实例，会话由工作台下拉框按 ID/标题对齐；一键直达需客户端插件。
6. **调试 FastAPI 无 Agent**：`command`/`feedback` 返回 `delivered:false`，这是刻意的诚实行为；且调试后端仍是单包状态，只有 DSH 后端支持按会话隔离。
7. **截图以文件路径送达**：Agent 需用 `read_image` 读取；未走附件 API，以降低对 DSH 附件接口的耦合。
