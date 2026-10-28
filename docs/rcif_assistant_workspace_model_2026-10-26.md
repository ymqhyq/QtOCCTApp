# “铁路构件智能助手”工作区 / 会话模型改造说明

- **日期**：2026-10-26
- **范围**：DSH Host 插件 `dsh-rcif`、客户端会话桥接、FastAPI 调试后端、工作台 UI
- **目标**：用 **固定工作目录 → 工作区 → 会话** 统一工作台与智能助手的数据模型，并让两边切换会话时保持同步。

---

## 1. 固定工作目录

```text
D:\QtOCCTApp\铁路构件智能助手\
├─ 桥台工作区\                    ← DSH Workspace（工作区）
│  ├─ .rcif\sessions\<sessionId>.json
│  ├─ inputs\上传测试.ifc
│  └─ out\UploadTest_1.0.0.rcif
├─ TestWorkspace\
│  ├─ .rcif\sessions\<sessionId>.json
│  ├─ inputs\piles.ifc
│  └─ out\PilesDemo_1.0.0.rcif
└─ <新建工作区>\
```

- 根目录固定为 `D:\QtOCCTApp\铁路构件智能助手`，由 Host 插件启动时创建。
- 每个**工作区**是根目录下的一个子目录，通过 DSH `workspaceController.create({ path })` 注册。
- 每个**会话**通过 DSH `sessionController.create({ workspaceId })` 创建，自动绑定对应工作区。
- 可用环境变量 `RCIF_ASSISTANT_ROOT` 或插件配置 `assistantRoot` 覆盖根目录。

工作区注册与 Session 归属都复用 DSH 自己的注册表，因此助手侧和工作台侧看到的是**同一套工作区与会话**，不另建一套 session 数据库。

---

## 2. 工作台布局

```text
┌─────────────────────────────────────────────────────────────┐
│ 铁路构件智能助手   [构件包] [版本]   复制会话ID  打开助手  状态 │
├───────────────┬─────────────────────────────────────────────┤
│ 工作区与会话   │ 当前会话 │ 元数据 │ 3D │ 参数 │ 脚本 │ 部位 │ 型号 │ 入库 │
│ ├ 桥台工作区  │─────────────────────────────────────────────│
│ │ ├ 构件会话… │                                             │
│ │ └ 构件会话… │              主视图（当前标签页）              │
│ └ TestWorkspace│                                             │
│   ├ …          │                                             │
│ [+ 工作区]     │                                             │
│ [+ 当前工作区新会话]                                          │
├───────────────┴─────────────────────────────────────────────┤
│ 💬 向智能助手反馈：自然语言 / 截图 / 代码批注                  │
└─────────────────────────────────────────────────────────────┘
```

### 2.1 左侧“工作区与会话”

- 列出固定根目录下所有 DSH 工作区；
- 每个工作区展开后列出它的会话；
- 会话显示：标题 / `构件名称 · 短 ID` / `● 已绑定构件`；
- 点击会话 = 切换当前工作台上下文；
- “＋工作区”会在根目录创建子目录 + 注册 DSH 工作区，并自动创建一个会话；
- “＋当前工作区新会话”在选中工作区创建 DSH 会话并自动切换。

### 2.2 顶部功能区

原左侧 7 个功能按钮改为顶部横向标签，并新增 **“当前会话”**：

`当前会话 / 元数据 / 3D 视图 / 构件参数 / 构建脚本 / 命名部位 / 型号列表 / 构件入库`

原底部“文件路径 / 族 ID / 版本 / 输出路径”输入框已全部移除。

### 2.3 “当前会话”页

负责一个会话的输入与产物信息：

| 字段 | 说明 |
| :--- | :--- |
| 工作区 / 会话 / 会话文件夹 | 只读身份信息 |
| 当前构件包 | 该会话已绑定的 `.rcif` |
| 源文件目录 | `inputs/`，相对会话文件夹 |
| 源文件 | 输入框可从 `inputs/` 下拉选择，或点旁边的“选择本地文件”；类型由后缀自动识别 |
| 构件名称 | 中文/显示名，写入会话元数据 |
| 族 ID | 英文转换 ID（3–32 位，字母开头） |
| 版本 | `x.y.z` |
| 输出路径 | 相对会话文件夹，默认 `out/<family>_<revision>.rcif` |

按钮：

- **保存会话信息**：`PUT /api/session-meta`；
- **让智能助手转换**：`POST /api/command`，把会话文件夹、源文件、族 ID、输出写给当前助手会话；
- **直接转换**：`POST /api/ingest`，工作台后端直接调用转换器；**仅支持 STEP / IFC**；
- **PDF 禁止直接转换**：PDF 按钮置灰，`/api/ingest` 返回 403；PDF 只能走“让智能助手转换”生成规则推断草稿；
- **选择本地文件**：原始字节上传到 `POST /api/upload`，写入 `<会话文件夹>/inputs/` 并更新会话元数据；
  外部文件选中后会先复制到 `inputs/`，再作为源文件路径使用。

### 2.4 PDF 通用草稿（未登记图号）

PDF 不再“只认登记表”：

| 情况 | 处理 |
| :--- | :--- |
| 已登记图号（4332A 桥台、2322A 箱梁等） | 走专用模板族 + 已校核 `table_truth.json` |
| 未登记但有文字层 | `pdf_generic.py` 提取文字、类型关键词和标注尺寸，生成通用体量草稿 |
| 未登记扫描件（无文字层） | 生成低置信度占位体量，`textLayer=false`、`missingParameters` 明确列出缺失尺寸 |

通用草稿会生成真实的：

- `generators/generator.py`（可读、可批注、可修改的 CadQuery 脚本）；
- `geometry/default.step`；
- `display/preview.glb`；
- `component.json`，`qa.extractionMode=generic_heuristic_draft`，并带 `confidence`、
  `missingParameters`、`warnings`、`dimensionEvidence`。

它不是精确 2D→3D 识别：文字层/尺寸缺失时会用同类默认体量。工程师通过 3D 截图、
脚本批注和助手多轮对话核对，修改生成器或参数后重建。

### 2.5 通用草稿一键重建

通用草稿提供三条重建入口，底层都调用 `generic_rebuild.py`：

```text
当前 .rcif
→ 解包
→ 读取/覆盖 generator.py
→ 执行 build_parts(params)
→ 重新导出 geometry/default.step + display/preview.glb
→ 更新 component.json / bounds / volume / recipe / checksums
→ 写新版本 .rcif
→ 绑定当前会话，工作台自动刷新
```

| 入口 | 操作 |
| :--- | :--- |
| 工作台参数页 | 修改 `Length/Width/Height/Diameter/WallThickness` → “重建草稿” |
| 工作台脚本页 | “编辑源码并重建” → 修改 `generators/generator.py` → “保存源码并重建” |
| 助手 | 调用 `rcif_rebuild_generic`，传 `paramsJson` 或完整 `generatorSource` |

版本默认自动 `patch +1`，旧包保留；重建记录写入 `qa.rebuilds` 和 `changelog`。
重建只校验通用草稿语义（B-Rep 有效、STEP/GLB、checksums、体积/包围盒自洽），
不套用桥台/箱梁专用门禁。

### 2.6 多段代码批注与批注历史

代码批注按“代码审查”式流程组织：

```text
选中 L10-L25（可拖动/Shift+点击，任意长度）
→ 代码页悬浮“＋ 批注” → 弹窗写批注 → 保存到右侧批注侧边栏
→ 再选 L40-L42 → 同样加入
→ 右侧点批注项定位代码，可编辑/删除
→ 底部“发送给助手”一次发送：批注 2 + 截图 1 + 文字
```

- “构建脚本”页右侧为批注侧边栏，不再使用底部批注输入框；
- 支持任意行范围，不再固定 3 行；
- 支持多个非连续代码段，一次最多 10 段；
- 点击侧边栏批注可定位并高亮对应代码，批注可编辑、删除；
- 底部“向智能助手发送”实时显示当前批注数量、3D 截图数量；
- 批注、截图、文字共同作为当前会话内容，一次 `/api/feedback` 发送；
- 历史记录按会话持久化到 `<workspace>/.rcif/annotations/<sessionId>.json`；
- 历史面板支持查看批注详情、载入为当前批注、删除。

API：

```text
GET    /rcif-studio/api/annotation-history?sessionId=
DELETE /rcif-studio/api/annotation-history?sessionId=&id=
```

历史记录包含 `id/at/annotations[].lines/comment/delivered/packagePath`。

---

## 3. 会话元数据

文件：`<workspace>/.rcif/sessions/<sessionId>.json`

```json
{
  "sessionId": "session-f0cb7630-a116-4967-a147-a58786c5b091",
  "workspaceId": "c0c11349-6155-4127-bf22-a1243e1249e0",
  "workspacePath": "D:\\QtOCCTApp\\铁路构件智能助手\\桥台工作区",
  "title": "构件会话 b091",
  "inputFile": "inputs\\上传测试.ifc",
  "sourceKind": "ifc",
  "componentName": "上传测试构件",
  "familyId": "UploadTest",
  "revision": "1.0.0",
  "outputPath": "out\\UploadTest_1.0.0.rcif",
  "packagePath": "D:\\QtOCCTApp\\铁路构件智能助手\\桥台工作区\\out\\UploadTest_1.0.0.rcif",
  "updatedAt": "2026-10-26T06:28:07.494Z"
}
```

- 每次 `rcif_ingest_*` / `rcif_compile_package` / 工作台直接转换后自动写回；
- 选中会话时 `GET /api/session-meta?sessionId=` 恢复源文件、构件名称、族 ID、版本、输出路径、构件包；
- DSH 重启后内存 `sessionPackages` 丢失，`/api/state` 会从 `packagePath` sidecar **自动恢复包绑定**。

---

## 4. 工作台与助手切换同步

### 4.1 工作台 → 助手

工作台切换会话时：

1. 更新 URL `?sessionId=...`；
2. `BroadcastChannel('rcif-session-bridge')` 发送 `{type:'rcif:open-session', sessionId}`；
3. 同时写入 `localStorage['rcif:open-session']` 作为冷启动兜底；
4. 客户端插件调用 DSH 真实导航 API：

```js
ctx.uiWorkspace.openSession(sessionId)
```

DSH 的 `uiWorkspace` 会把主视图、会话选择持久化键 `dsh.sessions.current` 一起切过去。实测：

```text
工作台点击 session-2faf…376d
→ dsh.sessions.current = {"sessionId":"session-2faf…376d"}
```

### 4.2 助手 → 工作台

- 智能助手侧创建/切换会话后，工作台 10 秒轮询 `/api/tree` 自动发现；
- 用户在工作台选中该会话即可加载其构件包与输入信息；
- 工作台启动时优先恢复 `rcif:last-session`，否则选“已有构件包的会话”。

### 4.3 客户端插件

`dsh-rcif/dist/client.js`（`dsh.client.platform=web`）只做一件事：

- 监听 BroadcastChannel 与 `storage`；
- 调用 `ctx.uiWorkspace.openSession(sessionId)`；
- 不修改 DSH 其他 UI。

---

## 5. 后端接口

| 方法 | 路径 | 说明 |
| :--- | :--- | :--- |
| GET | `/rcif-studio/api/tree` | 固定根目录下工作区 + 会话树（含元数据/包标记） |
| POST | `/rcif-studio/api/workspaces` | 在根目录创建子目录并注册 DSH 工作区 |
| POST | `/rcif-studio/api/sessions` | 在指定工作区创建 DSH 会话并写 sidecar |
| GET/PUT | `/rcif-studio/api/session-meta` | 读取/保存源文件、构件名、族 ID、输出路径 |
| GET | `/rcif-studio/api/session-files` | 列出会话文件夹 `inputs/` 下可转换的源文件 |
| POST | `/rcif-studio/api/upload` | 上传 `.pdf/.step/.stp/.ifc` 到 `<workspace>/inputs/` |
| GET | `/rcif-studio/api/state?sessionId=` | 当前会话包、工作区、元数据、能力 |
| GET/DELETE | `/rcif-studio/api/annotation-history` | 代码批注历史：列表 / 删除 |
| POST | `/rcif-studio/api/generic/rebuild` | 重建通用 PDF 草稿（参数/生成器源码/版本） |
| POST | `/rcif-studio/api/ingest` | 转换 STEP/IFC；**PDF 返回 403**，只能走助手 |
| POST | `/rcif-studio/api/command` | 转换/修改指令送达当前 DSH 会话 |
| POST | `/rcif-studio/api/feedback` | 自然语言/批注/截图送达当前会话 |

---

## 6. 验证记录（2026-10-26）

| 验证 | 结果 |
| :--- | :--- |
| 宿主契约 | 19/19 PASS（含工作区/会话树、sidecar、上传、会话绑定、PDF 禁止直接转换） |
| Python 契约 | 12/12 OK（含 FACETED_BREP STEP 回退、未登记 PDF 通用草稿、参数/源码重建） |
| 工作区创建 | `桥台工作区`、`UI新建工作区` 注册成功；中文名正常 |
| 会话创建 | DSH 返回 `session-…`，自动写 sidecar，标题“构件会话 xxxx” |
| 工作台建工作区 | 点击按钮 + 输入名称 → 工作区 + 会话自动创建并选中 |
| 源文件上传 | `上传测试.ifc` 写入 `inputs/`，元数据 `inputFile` 自动更新 |
| 源文件选择 | `inputs/` 下拉列出 2 个文件；外部 `piles.ifc` 经本地按钮复制为 `inputs/piles.ifc`；0 JS 异常 |
| 会话恢复 | 切换会话 → 源文件、构件名 `上传测试构件`、族 ID `UploadTest`、输出路径恢复 |
| 包恢复 | DSH 重启后 `/state` 仍返回 `UploadTest_1.0.0.rcif` |
| 助手切换 | BroadcastChannel → `dsh.sessions.current` 切换到同一 sessionId |
| 直接转换 | 工作台“直接转换”生成 `out/UiConvert_1.0.0.rcif`，0 JS 异常 |
| PDF 直接转换限制 | 选中 `桥通（2022）4332A-Ⅰ...pdf` 后“直接转换”置灰；`/api/ingest` 返 403 `PDF_DIRECT_CONVERSION_DISABLED`；“让智能助手转换”保持可用 |
| PDF 通用草稿 | 未登记文字层 PDF（桥墩）→ `genericType=pier`、置信度 0.89、3 个形状；未登记扫描件 `8388A`（134 页）→ `confidence=0.25`、`textLayer=false`、缺失尺寸明确列出；均含 STEP/GLB/generator/checksums |
| 通用参数重建 | v0.1.0 → v0.1.1，Height 8000→9000、Width 3000→3500；体积 22.296→26.195 m³，bounds Z 9440→10520 mm，工作台参数页一键完成 |
| 通用源码重建 | 修改 generator 顶帽厚度后 v0.1.2，体积 26.195→24.655 m³，`sourceChanged=true` |
| 多段代码批注 | 右侧批注栏：L10-L25 + L40-L42；选中代码悬浮“＋批注”，弹窗保存；点击批注定位代码；一次发送 2 段 |
| 批注/截图计数 | 底部显示“批注 2 · 截图 1”；发送后归零并生成 1 条历史记录 |
| 代码语法着色 | 关键字/字符串/注释/数字/函数/内置名分色；382 行脚本 0 JS 异常 |
| 3D | 真实 GLB，1 个部位，HUD 正常 |
| FACETED_BREP STEP | `Abutment.step` 原先报 `Bnd_Box is void`；修复后 148 个三角面重建为 3 个实体，体积 213.496 m³，bounds 12.6m × 5.65m × 7.756m，`native-brep` / `exactBrep=true` |
| 调试后端 | 3088 `/tree`、`/session-meta`、`/session-files`、`/upload` 可用（不能创建 DSH 会话） |

![当前会话](rcif_assistant_workspace_2026-10-26.png)

---

## 7. 已知限制

1. **调试后端不能创建会话**：`3088` 无 DSH `sessionController`，`POST /api/sessions` 返回 501；请使用 DSH `3090` 工作台。
2. **助手切换依赖客户端插件**：插件已随 Host 包安装；若用其他 DSH 前端/旧缓存，需刷新页面加载 `@rcif/dsh-studio-plugin/client.js`。
3. **旧会话迁移**：只有根目录下已注册工作区中的会话会出现在左侧；`D:\QtOCCTApp` 旧工作区不会自动迁移。
4. **删除工作区**：当前只提供创建/选择，不提供删除入口，避免误删会话数据。
5. **PDF 禁止直接转换**：只能由智能助手生成规则推断草稿，且必须人工复核型号/尺寸；直接转换按钮置灰，API 返回 403。
6. **PDF 仍是规则推断草稿**，不是 OCR。
7. **IFC 仍是 mesh-only**。
