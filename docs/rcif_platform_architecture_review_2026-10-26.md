# 《铁路工程参数化构件智能构建平台》源代码与架构评估及优化实施报告

- **评估对象**：`dsh-rcif/`（DSH 宿主插件 + 工作台）、`rcif-tools/`（Python 几何/门禁/打包工具链）、`rcif-web/`（B/S 构件库演示系统）、`test/`
- **评估时间**：2026-10-26
- **评估方式**：源码静态审查 + DSH 运行时证据（`~/.dsh/logs`）+ 对照 `app.asar` 内已发布包契约 + 可执行契约测试
- **总体判断**：业务建模与安全沙箱的**意图**完整、文档极充分，但“DSH 原生集成层”与“跨语言确定性契约”两处**核心实现与契约不一致**，此前多轮审核（Round 5 APPROVED）主要覆盖 Python 工具链，未覆盖真实宿主 API 与前后端契约，因此存在“文档与测试全绿、产品却跑不起来”的系统性偏差。

---

## 1. 执行摘要

| 维度 | 评级 | 结论 |
| :--- | :---: | :--- |
| 领域建模与规范（RCIF v1.4.2） | B+ | 包结构、门禁维度、审批/溯源设计完整；但确定性序列化有三套实现 |
| DSH 宿主集成 | **D** | 插件对 `webServer`/`tools`/`sidebarRightTabs` 使用了不存在的 API，**实际未激活** |
| 工具链（Python） | B | 门禁与沙箱有真材实料；PDF 抽取、E2E、可复现打包存在硬伤 |
| Web 演示系统 | B- | 分层与 UI 完成度高；`api.ts` 网关单体、内存态、若干授权/令牌问题 |
| 可测试性 | C+ | Python/前端有测试，但缺少宿主契约、跨语言向量、API 级测试；E2E 存在 golden 自证 |
| 可维护性 | C | 多个 1000+ 行单体（`api.ts` 1737 行路由、`ViewerEngine` 1614 行、50KB 级 View），逻辑重复 |

**本次已修复的 6 个 P0/关键项**：

1. DSH 宿主插件按真实 API 重写并新增 **11 项契约测试**；
2. 工作台与宿主 API 契约对齐（cap token、响应结构、审批→编译链路、离线 3D）；
3. 统一 **Python/TypeScript DeterministicJSON `%.12g`** 实现，新增 18 条跨语言向量；
4. `.rcif` 打包改为**确定性归档**（排序 + 固定时间戳），并补充可复现测试；
5. 打通 **Ed25519 审批 → 沙箱重跑 → 验签打包 → provenance** 全链路（此前恒为 unsigned）；
6. rcif-web：cacheKey 收敛为单一实现；预览图令牌移出 URL（改 HttpOnly Cookie）；合作方导出与相似检索越权修复；分页参数兜底。

---

## 2. 架构现状与数据流

```text
┌────────────────────────── DeepSeek Harness (Electron, dsh 0.2.1-rc.3) ──────────────────────────┐
│                                                                                               │
│  Host 进程                                  Browser (Web GUI)                                 │
│  ├─ dsh-host-webserver  (route registry)     ├─ dsh-client-ui-sidebar-right (React Tab 域)     │
│  ├─ dsh-client-connection (Host/Origin + Cookie 准入)                                          │
│  ├─ dsh-tools (ctx.tools.register)           └─ iframe: /rcif-studio/workbench/                │
│  └─ @rcif/dsh-studio-plugin (本插件, Host)            │                                       │
│         │  spawn(python)                              │ fetch /rcif-studio/api/*             │
│         ▼                                             ▼                                       │
│  rcif-tools: verify_sandbox.py → solve_preset_worker.py → cadquery/OCCT                      │
│              pack_rcif_cli.py → rcifc.py → .rcif + .provenance.json                           │
└───────────────────────────────────────────────────────────────────────────────────────────────┘
              ▲ 另一条并行、未被 DSH 托管的入口
              └─ rcif-tools/workbench_server.py (FastAPI :3088)  ← 无鉴权、CORS 任意 localhost
```

**结构性问题**：同一份“构件创族工作台”和同一组 `/rcif-studio/api/*` 契约被实现了两遍（DSH 插件 + FastAPI），语义、鉴权、响应结构各不相同；同一个“门禁/打包/确定性序列化”又被 `rcif-tools`、`abutment_family/`、`rcif-web/server` 各复制一套。**三套实现、一个名字**，是当前维护成本与不一致缺陷的根源。

---

## 3. 关键发现（按严重度）

### P0-1 DSH 插件未真正接入，运行时激活失败

证据（`%USERPROFILE%\.dsh\logs\startup-2026-10-25T06-19-19*.log`）：

```text
rcif-studio-plugin  Error: cannot get property "sidebarRightTabs" without inject
    at RcifStudioPlugin.initSidebarTab (D:\QtOCCTApp\dsh-rcif\dist\index.js:83:18)
```

对照 `app.asar` 内已发布包的真实契约（非推测）：

| 代码中的用法 | 真实契约 | 后果 |
| :--- | :--- | :--- |
| `webServer.use(path, handler)` | `webServer.register({ kind:'exact'\|'prefix', path, handler })` | 路由**一条都未注册**（`use` 不存在，静默返回） |
| `sidebarRightTabs.register({ id, title, icon, render })` | 客户端插件 `register({ id, kind, title, ... })` + `slots.register({name:'sidebar.right.pane.tab'}, React 组件)`；且需 `package.json.dsh.client` | 宿主侧无 `document`；即便到浏览器也**不会渲染** |
| `ctx.get('tool')` + `{name, parameters, execute}` | `ctx.tools.register(definition)`，且必须有 `output.schema` 与 `output.render` | 工具**全部未注册**（`get('tool')` 返回 `undefined`） |
| `admission.rejected` | `admit()` 返回 `{ peer }` 或 `{ rejection: 401\|403 }` | 拒绝分支被误判，语义错误 |
| `spawn(..., {timeout})` | Node `spawn` 无 `timeout` 选项（仅 `exec`） | 超时依赖 Python 侧兜底 |

**修改**：`dsh-rcif/dist/index.js` 全量重写为真实 Host 插件（`inject: ['webServer','connection','tools']`、`ctx.effect(() => ctx.webServer.register(...))`、`connection.requestRejection` + owner Peer、`ctx.tools.register` 合规定义、总线式 `child` 看门狗、路径收敛、结构化错误）。新增 `dsh-rcif/test/plugin-contract.test.mjs`（11 项，全部通过）。

**诚实边界**：原生 React 右栏 Tab 需要独立客户端插件（`dsh.client` + `slots` React 组件），本次未伪造一个未经运行时验证的客户端 bundle；现阶段工作台通过 DSH 内置 `ui-sidebar-browser`（profile 已启用）访问 `http://127.0.0.1:3080/rcif-studio/workbench/`。插件已提供 `rcif:init` / `?sessionId=` 握手，后续替换为原生 Tab 时前端无需改动。

### P0-2 工作台与宿主契约互相错位

- 前端调用 `POST /rcif-studio/api/compile-package`，宿主**根本没有该路由** → 打包按钮必然 404；
- `run-sandbox` 前端不带 `X-RCIF-Cap-Token`（宿主守卫要求）→ 401/403；
- 宿主旧实现返回 `{exitCode, output, passed}`，前端却读取 `data.ok && data.report.checks` → 门禁卡永远不刷新；
- `init-token` 把 token 绑定到 `Origin || Host`，而后续 POST 的 `Origin` 带 `http(s)://`、GET 握手没有 `Origin` → **同一客户端自签的 token 自己校验不过**；
- Three.js / OrbitControls 走 CDN，涉密内网离线即空白；且 3D 始终是示例盒体，未加载真实 `preview.glb`（ISSUE-012 长期未闭环）。

**修改**：`workbench.js` 重写为 ES module；统一 `apiFetch/apiPost` 注入 cap token；补齐审批→编译链路；3D 改为 GLTFLoader 加载真实 GLB（失败时明确标注“示例几何”）；Three.js 全量本地 vendor（`workbench/vendor/`，无网络依赖）；门禁初始状态改为“待验证”，消除默认假绿。

### P0-3 跨语言确定性契约失效（cacheKey 三套实现）

规范 §5.1 明确要求 `%.12g`（`docs/rcif_component_format_spec_v1.4.2.md:308`），但：

| 位置 | 实现 | 结果 |
| :--- | :--- | :--- |
| `rcif-tools/rcifc.py:26` | `f"{val:.6f}"` | `1.23456789 → 1.234568`，违反规范 |
| `rcif-web/shared/deterministic.ts` | `toPrecision(12)` | 指数阈值是 `-6`，与 C `%g` 的 `-4` 不同 |
| `rcif-web/server/api.ts` vs `family.ts` | 两份 `cacheKeyFor`，PierBody 使用不同 digest | 同一参数得到 **3 个不同 cacheKey**（详情/评测/下载互不命中） |

**修改**：
- Python 改为 C `%g` 语义 + 拒绝 NaN/Inf + `|v|<1e-12→0`；
- TS 改为显式 `%g` 算法（指数 < -4 或 ≥ 12 用科学计数法）；
- 新增 `rcif-web/shared/fixtures/deterministic_vectors.json`（18 条）与 `approval_canonical.json`；
- `rcif-web/server/cache-key.ts` 成为唯一实现，`api.ts`/`family.ts` 均改为引用。

**验证**：`node rcif-web/scripts/check-determinism.mjs` 全部通过（TS 与 Python 逐字节一致）；`python -m unittest ...` 7 项通过（含 `cryptography` 环境下的 Ed25519 往返）。

### P0-4 审批/签名链是“死代码”，UI 却在宣称已验签

- `pack_rcif_cli.py` 的 `approval_json_path/pubkey` 参数从未被任何调用方传入；
- `workbench_server.py` 明确返回 `approvalAlgorithm: "Ed25519-DEV-MOCK"`、`approvalSignature: "DEV_UNSIGNED_STUB"`；
- 插件旧版 `approve-control-grid` 只签名、不与打包连接；
- 于是所有包 `signatureStatus: "unsigned-dev-build"`，而工作台日志却打印“正在验证 Ed25519 签名审批单”。

**修改**：形成闭环：

```text
POST approve-control-grid  →  Host 计算 table_truth/params SHA-256，用内存 Ed25519 私钥签名
POST compile-package       →  校验签名 + 会话 + 摘要新鲜度 → 写临时审批单 → python CLI 验签
pack_rcif_cli --approval/--pubkey → Python 用同一公钥复核 → provenance.signatureStatus = verified
```

同时把 `pack_rcif_cli` 从裸 `sys.argv` 改为 `argparse`，签名 payload 统一 `ensure_ascii=False`，并新增 `tests` 覆盖规范化字节串。

### P0-5 安全与授权缺陷（rcif-web / 双后端）

| 问题 | 证据 | 处理 |
| :--- | :--- | :--- |
| Bearer token 拼进 `<img>` URL | `client.ts:292-294`、`374-387` | 改为 HttpOnly `SameSite=Strict` 会话 Cookie；前端不再生成 `?token=` |
| 合作方绕过导出审批 | `/components/{id}/export` 无 partner 分支（`api.ts:1579`） | 与 `/download` 对齐返回 202 `request-required` |
| 相似检索泄露私有构件名 | `similarTo` 未做 `accessible` 校验（`api.ts:878`） | 无权限直接 403，不回显 reference |
| 第二套后端完全无鉴权 | `workbench_server.py` FastAPI `init-token` 返回 `cap-{uuid}`，CORS 放行任意 localhost | **架构上应删除**；保留时仅作本机调试，并在文档标注 deprecated |
| 工具可读写任意路径 | 旧插件 `pdfPath/stepPath/outputDir` 直接 `spawn` | 统一 `resolveInsideWorkspace`（realpath + 越界/符号链接校验） |
| 审批参数被 URL/日志泄露 | `?token=` + `Cache-Control` | Cookie + `no-store`；保留 legacy query 仅为安全冒烟脚本兼容 |

### P1 级问题（未全部实施，列入路线图）

1. **`api.ts` 网关单体**：`handleApi` 约 1737 行、39 个分支、18 处内联角色判断；状态为模块级单例（`statePromise`），无法测试隔离。
2. **沙箱/门禁仍有缺口**：`checksums_checked` 不参与裁决；`recipe.digest` 未与源码哈希校验；`solve_preset_worker` 受限 builtins 缺 `__build_class__`（任何 `class/Enum/dataclass` 生成器必失败）；`total_budget_sec` 在并发提交下失效；读路径未白名单（可把任意可读文件内容带进错误文本）；Gate 3 仅按 `BearingPad/Left` 键名特化。
3. **PDF 摄取是常量回填**：`pdf_ingest.py` 的 49 行真值与 bbox 均硬编码，Gate 4 实际在给常量打分 → 必须改为真实抽取或显式 `manual_required`。
4. **E2E 自证**：`test_rcif_platform_e2e.py` 把 golden 的 `component.json/geometry/display` 复制进被测目录再比对自身；只有 E4 真跑生成器。
5. **多份生成器/契约拷贝**：`rcif-web/server/cq/generator.py` 与 `abutment_family/generators/generator.py` 字节相同，`cq_script/generators/generator.py` 又是第三套；`jhb.ts` 与 `decode.ts`、`rcifc.py` 与 `make_component.py` 各一套 DeterministicJSON。
6. **前端视图即应用层**：`LibraryView`/`IngestionView` 约 50KB/958 行；`ViewerEngine` 1614 行；视图内直接解 ZIP/GLB、拼 URL、做权限判断。
7. **测试工具自身失效**：`smoke-api.mjs` 打印 `LEAK` 仍 `exit 0`；README 的 `e2e:security 11/11` 与实际 15 项不符；缺 `api.spec.ts`。

---

## 4. 目标架构（建议）

### 4.1 分层与边界

```text
packages/
├─ rcif-spec/                 # 唯一契约：JSON Schema + 测试向量 + 版本迁移
│  ├─ schema/component.schema.json
│  ├─ vectors/deterministic.json   # TS/Python 共用（已有雏形）
│  └─ codegen/                     # 由 schema 生成 TS 类型 / Python dataclass
├─ rcif-domain/               # 纯领域：参数校验、cacheKey、版本/发布状态机、审批模型
├─ rcif-runtime/              # 用例编排：evaluate / verify-gates / package / approve
│  ├─ ports/ (GeometryEngine, Repository, Signer, Clock, EventBus)
│  └─ usecases/
├─ rcif-adapters/
│  ├─ cadquery/               # solve_preset_worker、BRep/GLB/STEP
│  ├─ packaging/              # 确定性 zip、checksums、provenance
│  └─ persistence/            # SQLite/Postgres 仓储（替代内存单例）
├─ rcif-dsh-plugin/           # 仅 DSH 适配：路由 + 工具 + 客户端 Tab（host/client 两入口）
│  ├─ lib/host.js             # ctx.webServer / ctx.tools
│  └─ lib/client.js           # ctx.sidebarRightTabs + ctx.slots (React)
└─ rcif-web/                  # B/S 演示：feature 目录 + transport/queries + viewers
```

**强制规则**：

- `rcif-web` 与 `rcif-tools` **不得各自实现** cacheKey/DeterministicJSON/门禁；只能调用 `rcif-spec` + `rcif-runtime`。
- 一个构件只有一个 `CacheKeyService.compute(component, params)`；输入必须覆盖 **全部** schema 参数（含枚举/布尔），禁止 `Number(v)||0` 降维。
- 所有对外 API 先定义 **OpenAPI/JSON Schema**，Node 与 Python 共用同一份契约测试向量。
- 删除 `workbench_server.py`；本机调试统一走 DSH 宿主（或将其降级为仅 `127.0.0.1` 的只读 viewer，且不带写/打包能力）。

### 4.2 DSH 集成

```text
Host 插件:  webServer.register (exact/prefix) + tools.register + connection fence
Client 插件: package.json { dsh.client: { platform:'web' } } + lib/client.js
            ctx.sidebarRightTabs.register({id:'rcif-workbench', kind:'rcif-workbench', title})
            ctx.slots.register({name:'sidebar.right.pane.tab', key:'rcif-workbench'}, View)
View:       React 容器内嵌同一 workbench iframe；通过 postMessage 传递 sessionId（一次性）
```

在客户端插件落地前，**不要**在 Host 插件里访问 `document` 或注册 Tab；当前实现已删除该伪实现。

### 4.3 rcif-web

```text
server/http/{router,respond,middleware/auth,errors}.ts
server/domains/{catalog,jobs,uploads,reviews,semantics,families}/{service,repo,dto}.ts
server/core/{container,cache-key,policy}.ts
server/adapters/{fs,cq,artifacts}.ts
```

- `ApiContext{req,res,url,user,traceId}` + 路由表 `{method,pattern,roles,handler}`；
- `Policy.can(user, action, resource)` 取代 18 处内联判断；
- `ComponentRepository` 屏蔽直接改 state，持久化用 SQLite（演示）→ Postgres（生产）；
- 统一 `zod`/JSON Schema 入参校验；错误类型化；分页/排序全端点一致；
- 前端按 feature 拆分，`ViewerEngine` 拆为 `Renderer/Camera/Picker/Section/Measure/Compare`，Vue 只持快照。

### 4.4 迁移顺序（低风险→高收益）

1. **契约收敛**（已完成大半）：DeterministicJSON/cacheKey/审批字节串唯一化 + 契约测试进 CI。
2. **宿主可用性**：插件契约测试 + 真实 DSH 冒烟（启动一次 profile，验证 `/health`、`/init-token`、静态页、`preview.glb`、4 个工具在工具清单可见）。
3. **门禁硬化**：`checksums.json`/`recipe.digest`/zip 安全/JSON Schema 变成硬失败；`__build_class__` 白名单；读取白名单 + 错误脱敏；总预算改为“提交-回收”双约束。
4. **删除重复**：删 `workbench_server.py`；`rcif-web` 生成器只保留一份；`jhb.ts`/`decode.ts` 收敛为 spec 生成代码。
5. **Web 拆分**：`api.ts` 按域搬迁（catalog→jobs→uploads→reviews→semantics→families），每域先补 `api.spec.ts` 再搬。
6. **前端重构**：token 出 URL（已完成）、ZIP/GLB 限额 + Worker 化、ViewerEngine 拆分、视图 feature 化。
7. **持久化与可观测性**：仓储替换内存、任务 TTL/清扫、traceId、真实审计 IP。

---

## 5. 本次修改清单

| 文件 | 修改 |
| :--- | :--- |
| `dsh-rcif/dist/index.js` | 按真实 DSH API 重写 Host 插件：路由注册、owner 准入、cap token、路径收敛、工具注册、看门狗、审批/打包闭环、会话注入 |
| `dsh-rcif/test/plugin-contract.test.mjs` | 新增 11 项契约测试（路由/工具/鉴权/越界/重放/静态/GLB/审批/规范化） |
| `dsh-rcif/workbench/workbench.js` | ES module 重写；token/响应/审批链路修正；真实 GLB 加载；门禁待验证态；诚实错误提示 |
| `dsh-rcif/workbench/index.html` | 移除 CDN；importmap；状态徽标；会话/几何状态展示 |
| `dsh-rcif/workbench/workbench.css` | 新增 pending/warn/disconnected/session 样式 |
| `dsh-rcif/workbench/vendor/**` | 本地 vendor Three.js / OrbitControls / GLTFLoader / BufferGeometryUtils（离线） |
| `dsh-rcif/package.json` | 测试与 prepack 脚本；files 白名单 |
| `rcif-tools/rcifc.py` | `%.12g` + NaN/Inf + epsilon；确定性内存归档 `build_rcif_archive`；固定 ZipInfo；不再改写 stage |
| `rcif-tools/pack_rcif_cli.py` | argparse；`--approval/--pubkey/--session`；签名 payload `ensure_ascii=False`；provenance 路径鲁棒 |
| `rcif-tools/tests/test_deterministic_contract.py` | 7 项：18 条跨语言向量、`%g`、NaN、规范向量、可复现归档、审批规范化、Ed25519 往返 |
| `rcif-web/shared/deterministic.ts` | 实现真正的 `%g`（阈值 -4/12），与 Python 对齐 |
| `rcif-web/shared/deterministic.spec.ts` | 新增阈值/跨语言 fixture 断言 |
| `rcif-web/shared/fixtures/*.json` | 跨语言确定性向量、审批规范化向量 |
| `rcif-web/scripts/check-determinism.mjs` | 无 Vite 即可运行的 TS↔Python 一致性自检（已加入 `npm run check:determinism`） |
| `rcif-web/server/cache-key.ts` | cacheKey 唯一实现（枚举/布尔不降维、非参数化走 DeterministicJSON） |
| `rcif-web/server/api.ts` / `family.ts` | 改用统一 cacheKey；HttpOnly Cookie 会话 + logout；合作方导出 202；相似检索鉴权；分页兜底 |
| `rcif-web/server/cache-key.spec.ts` | 4 项 cacheKey 契约测试 |
| `rcif-web/src/api/client.ts` | 预览/投影 URL 不再携带 Bearer token |
| `rcif-web/scripts/cdp-features.mjs` | 断言改为“URL 不含 token、Cookie 鉴权” |

### 验证结果

```text
node dsh-rcif/test/plugin-contract.test.mjs      11/11 pass
node rcif-web/scripts/check-determinism.mjs      全部通过（18 向量 + 4 规范向量 + 审批规范化）
python -m unittest discover -s rcif-tools/tests   7/7 OK（cq 环境；默认环境跳过 cryptography 用例）
npx vue-tsc --noEmit                              PASS
npm test (vitest)                                 8 files / 39 tests pass
node --check dsh-rcif/dist/index.js               PASS
node --check workbench.js (module)                PASS
verify_sandbox.py abutment_family                 49/49 型号 PASS，0 Blocker / 0 Major，42.31s
```

**跨语言端到端验收（真实签名 + 真实门禁 + 真实打包）**：

```text
Node(Ed25519) 签发审批单 → Python 验签通过（NODE_SIGNATURE_VERIFIED_BY_PYTHON）
→ 49 型号门禁重跑 → 打包 crosslang.rcif
→ signatureStatus = verified，humanConfirmations = 1
→ 连续两次打包 SHA-256 均为 e87f59f3…bd71b（可复现）
```

> `npm test` 在受限沙箱下会因 esbuild 无法 `spawn` 返回 `EPERM`（DSH 执行边界，非代码失败）；本次已在完整访问模式下实跑 39/39 通过。

---

## 6. 仍需决策的事项

1. **是否保留 `workbench_server.py`**：建议删除；若保留必须补 cap token、绑定 `127.0.0.1`、移除写/打包能力并标注 deprecated。
2. **是否实现原生 React Tab**：需要新增客户端插件包并做一次真实 DSH 启动验证；在此之前保持“内置浏览器 Tab + iframe”方案，避免再次出现“文档有、运行无”的假集成。
3. **PDF 真抽取**：若短期无法接入 OCR/表格识别，必须把 `extractionMode` 标为 `manual_required` 并让 Gate 4 阻断，而不是继续用常量回填。
4. **E2E golden 注入**：改为隔离目录现场生成，golden 只读；否则回归测试对生成器改动不敏感。
5. **持久化与多租户**：演示内存态可接受，但“权威构件库”定位要求尽快落地仓储与审计存储。

---

## 7. 结论

本轮评估没有停留在“文档对照文档”。通过读取 DSH 发布包的真实契约、检查运行时启动日志，确认了**插件从未成功接入**这一此前所有审核都未覆盖的事实；通过同一组数值在 TS/Python 两侧实算，确认了**确定性契约已经分裂**。这两项都不是风格问题，而是“平台能不能跑、构件能不能复现”的根因。

本次修改优先修复了“契约正确性 + 集成可用性 + 安全边界”三类根因，并用可执行测试把它们钉住。剩余工作（门禁硬化、单体拆分、真 PDF、原生 Tab、持久化）已在第 4 节给出有顺序、可独立上线的路线图，建议按该顺序推进，而不是继续在现有三层重复实现上叠加功能。

---

## 8. 增量：闭环创作工作流（同日实施）

在第 1–7 节之外，按用户提出的理想流程补充实现了完整的“对话 → 转换 → 工作台 → 反馈 → 重建 → 入库”闭环，详见 [工作流实现说明](rcif_workbench_workflow_2026-10-26.md)：

- **新增转换器**：`rcif-tools/ifc2rcif.py`（IFC→mesh-only，单位纠偏/命名部位/体积）、`rcif-tools/pdf2rcif.py`（PDF→参数化草稿，包内生成器可审阅）。
- **包检查**：Node `dist/lib/{zip,package}.js` 与 Python `package_api.py`，统一 metadata/checksums/GLB/source 契约。
- **七功能区工作台**：元数据 / 3D / 参数 / 脚本 / 部位 / 型号 / 入库；自然语言、行批注、3D 截图统一经 `/api/feedback` 送达 Agent；工作台 2.5s 轮询 `/api/state` 自动加载新包。
- **Agent 工具扩充**：`rcif_ingest_ifc`、`rcif_open_package`，所有 `rcif_ingest_*` 完成后自动推送工作台。
- **入库**：`POST /api/library/ingest` + `rcif-web /api/v1/components/import-rcif`，实测生成 `RV-2026-324`（PdfUi，49 型号，8 shapes，reviewing）。
- **验证**：宿主契约 19/19、Python 12/12、rcif-web 39/39、headless Chrome 八功能区 0 JS 异常、真实 GLB/截图/入库全链路通过；多会话下 `sessionId → package` 隔离已实测（A 有包、B 为空）。

## 9. 增量：固定工作目录 → 工作区 → 会话（同日实施）

详见 [铁路构件智能助手工作区/会话模型](rcif_assistant_workspace_model_2026-10-26.md)。

- **固定根目录**：`D:\QtOCCTApp\铁路构件智能助手`；工作区是根下子目录，用 DSH `workspaceController.create` 注册，会话用 `sessionController.create` 创建。
- **工作台重构**：左侧“工作区/会话”树，顶部 8 个标签（新增“当前会话”），移除底部文件路径输入。
- **会话元数据**：`<workspace>/.rcif/sessions/<sessionId>.json` 保存输入文件、构件名、族 ID、版本、输出路径、包路径；切换会话自动恢复，DSH 重启后从 sidecar 恢复包绑定。
- **输入上传**：`/api/upload` 将 `.pdf/.step/.ifc` 写入 `<workspace>/inputs/`，并自动填会话元数据。
- **助手同步**：新增 `dist/client.js` 客户端插件，监听 BroadcastChannel/localStorage，调用 `uiWorkspace.openSession(sessionId)`；实测 `dsh.sessions.current` 跟随工作台切换。
- **验证**：宿主契约 19/19；工作区/会话创建、中文名、上传、切换恢复、DSH 重启恢复、助手选中同一 session 全部通过，0 JS 异常。

