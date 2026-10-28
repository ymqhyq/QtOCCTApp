# @rcif/dsh-studio-plugin

DeepSeek Harness（dsh 0.2.1-rc.3）宿主插件：把 RCIF 构件创族能力（门禁、审批、打包、3D 预览、行级批注）接入 DSH。

> 本包是 **Host 插件**。它不访问 `document`，也不直接注册 React 右栏 Tab；
> 浏览器 UI 是一个同源 iframe 工作台（`workbench/`），通过 DSH 内置的
> `ui-sidebar-browser` 打开。原生 React Tab 需要独立的客户端插件包（见路线图）。
>
> **2026-10 结构升级**：固定工作目录为 `D:\QtOCCTApp\铁路构件智能助手`；
> 工作台左侧为“工作区/会话”树，顶部新增“当前会话”页；工作台切换会话时
> 通过客户端插件 `dist/client.js` 调用 `uiWorkspace.openSession`，让智能助手
> 自动切到同一会话。详见根目录 `docs/rcif_assistant_workspace_model_2026-10-26.md`。

## 0. 闭环工作流

```text
① 在 Agent 对话或工作台“新建/转换”中说：
   “把 xxx.step / xxx.ifc / xxx.pdf 转换为构件”
        │  Agent 调用 rcif_ingest_step/ifc/pdf
        ▼
② 生成 .rcif → 插件 setCurrentPackage → 工作台 2.5s 内自动加载
        │  八功能区：当前会话 / 元数据 / 3D / 参数 / 脚本 / 部位 / 型号 / 入库
        ▼
③ 用户看 3D、点部位、读脚本、选行批注、截图、写自然语言意见
        │  POST /api/feedback → sessionController.prompt
        ▼
④ Agent 收到“当前构件包 + 意见 + 批注 + 截图路径” → 改生成器/参数
        │  rcif_compile_package 或 rcif_ingest_*
        ▼
⑤ 新 .rcif 再次自动推送 → 重复 ②–④，直到入库送审
        │  POST /api/library/ingest
        ▼
⑥ 写入入库清单；构件库 5178 在线时直接创建族 + 审核任务
```

转换语义（如实标注，不夸大）：

| 输入 | 产物 | 能力 |
| :--- | :--- | :--- |
| STEP | `native-brep` | 精确 B-Rep、命名部位、GLB 预览；无参数化变参 |
| IFC | `mesh-only` | IFC 命名部位 + 网格 + 方量；不冒充精确/参数化 |
| PDF | `parametric` 草稿 | 已登记图号用专用模板；未登记图号用 `pdf_generic.py` 通用文字/尺寸/体量草稿（含置信度与缺失参数）；**只能通过智能助手转换，直接转换 API 拒绝** |

> **STEP FACETED_BREP**：部分供应商 STEP 用 `FACETED_BREP/CLOSED_SHELL` 表达，OCP reader 可能读成空 shell 并报 `Bnd_Box is void`。`step2rcif.py` 会回退为三角面重建 → 缝合 → 多实体 compound，仍输出 `native-brep` 包（`Abutment.step` 已实测通过）。


## 1. 安装与配置

插件以 `dsh.bundle.patch` 形式挂载，依赖 DSH 的 `webServer` / `connection` / `tools` 三个服务：

```yaml
# 插件配置（可选）
- id: rcif-studio-plugin
  name: './dist/index.js'
  config:
    workspaceRoot: 'D:/QtOCCTApp'      # 默认：仓库根（含 rcif-tools）；否则 process.cwd()
    toolsDir: 'D:/QtOCCTApp/rcif-tools'
    pythonPath: 'D:/ProgramData/miniforge3/envs/cq/python.exe'
    defaultFamilyDir: 'abutment_family'
    defaultSessionId: 'session-default'
    allowUnsignedCompile: false          # 仅本地调试可设 true；生产必须走审批
```

环境变量（优先级低于显式 config）：

| 变量 | 作用 |
| :--- | :--- |
| `RCIF_WORKSPACE_ROOT` | 工作区根（所有路径必须位于其内） |
| `RCIF_TOOLS_DIR` | `rcif-tools/` 目录（安装成 tgz 后必须显式指定） |
| `RCIF_PYTHON` | CadQuery 环境的 Python 解释器 |

## 2. HTTP 接口

所有路由都先经过 DSH 的 `connection` 信任围栏（Host/Origin + 浏览器登录 Cookie + owner Peer）。
除 `init-token`、`health`、`preview.glb`、静态工作台外，均要求 `X-RCIF-Cap-Token`（15 分钟，绑定 sessionId + 服务端 origin）。

| 方法 | 路径 | 说明 |
| :--- | :--- | :--- |
| GET | `/rcif-studio/workbench/*` | 工作台：左侧工作区/会话树 + 顶部 8 个功能区（当前会话/元数据/3D/参数/脚本/部位/型号/入库） |
| GET | `/rcif-studio/api/health` | 插件/工作区/工具链状态 |
| GET | `/rcif-studio/api/init-token?sessionId=` | 签发 cap token + Ed25519 公钥 |
| GET | `/rcif-studio/api/tree` | 固定根目录下工作区与会话树 |
| POST | `/rcif-studio/api/workspaces` | 在根目录创建工作区并注册 DSH Workspace |
| GET | `/rcif-studio/api/state?sessionId=` | 指定会话的当前构件包、工作区、会话元数据、能力、反馈 |
| GET | `/rcif-studio/api/sessions` | DSH 会话目录（ID/标题/是否已有构件包） |
| POST | `/rcif-studio/api/sessions` | 在指定工作区创建 DSH 会话 |
| GET/PUT | `/rcif-studio/api/session-meta` | 读取/保存源文件、构件名、族 ID、输出路径、包路径 |
| GET | `/rcif-studio/api/session-files` | 列出会话文件夹 `inputs/` 下可转换的源文件 |
| GET/DELETE | `/rcif-studio/api/annotation-history` | 多段代码批注历史：列表 / 删除 |
| POST | `/rcif-studio/api/generic/rebuild` | 通用 PDF 草稿按参数/生成器源码重建，生成新版本 |
| POST | `/rcif-studio/api/upload` | 上传 `.pdf/.step/.stp/.ifc` 到会话工作区 `inputs/` |
| GET | `/rcif-studio/api/assistant` | 智能助手地址与运行状态 |
| POST | `/rcif-studio/api/assistant/launch` | 调试后端下启动 DSH 实例 |
| GET | `/rcif-studio/api/package?path=&sessionId=` | 解析 .rcif：component.json、checksums、文件、预览、脚本 |
| GET | `/rcif-studio/api/package/preview.glb?path=` | 直接从包内读取 preview.glb |
| GET | `/rcif-studio/api/package/source?path=&sessionId=` | 读取包内 generators/generator.py |
| POST | `/rcif-studio/api/ingest` | STEP/IFC → .rcif；PDF 返回 403，PDF 只能走助手工具 |
| POST | `/rcif-studio/api/command` | 把自然语言指令发给当前 Agent 会话 |
| POST | `/rcif-studio/api/feedback` | 自然语言/代码批注/3D 截图回传 Agent（带 sessionId） |
| POST | `/rcif-studio/api/library/ingest` | 写入入库清单并提交 B/S 构件库审核 |
| GET | `/rcif-studio/api/preview.glb?familyDir=` | 直接读取构件族 preview.glb（兼容旧入口） |
| POST | `/rcif-studio/api/run-sandbox` | 现场重跑 6 道门禁，返回规范报告 |
| POST | `/rcif-studio/api/approve-control-grid` | 计算真值摘要并签发 Ed25519 审批单 |
| POST | `/rcif-studio/api/compile-package` | 校验审批单 → 重跑门禁 → 打包 `.rcif` + provenance |
| POST | `/rcif-studio/api/session-inject` | 净化批注、nonce 防重放、注入当前 Agent 会话 |

请求示例：

```bash
# 1. 握手（浏览器会带 DSH 会话 Cookie）
curl -s 'http://127.0.0.1:3090/rcif-studio/api/init-token?sessionId=demo' -c cookies.txt

# 2. 现场重跑门禁
curl -s -X POST 'http://127.0.0.1:3090/rcif-studio/api/run-sandbox' \
  -b cookies.txt -H 'content-type: application/json' \
  -H "x-rcif-cap-token: $TOKEN" \
  -d '{"sessionId":"demo","targetDir":"abutment_family"}'

# 3. 审批 → 4. 打包（必须使用同一 sessionId 的审批单）
curl -s -X POST 'http://127.0.0.1:3090/rcif-studio/api/approve-control-grid' ...
curl -s -X POST 'http://127.0.0.1:3090/rcif-studio/api/compile-package' ...
```

## 3. 工具

| 工具 | 输入 | 说明 |
| :--- | :--- | :--- |
| `rcif_ingest_pdf` | `pdfPath`, `familyId?`, `revision?` | PDF → 参数化草稿（已登记走模板，未登记走通用体量解析），完成后推送工作台 |
| `rcif_rebuild_generic` | `packagePath?`, `paramsJson?`, `generatorSource?`, `revision?` | 通用 PDF 草稿按参数/源码重建，新版本绑定当前会话 |
| `rcif_ingest_step` | `stepPath`, `familyId?`, `revision?` | STEP → native-brep 构件，完成后推送工作台 |
| `rcif_ingest_ifc` | `ifcPath`, `familyId?`, `revision?` | IFC → mesh-only 构件（保留命名部位），完成后推送工作台 |
| `rcif_open_package` | `packagePath` | 把已有 .rcif 推送到工作台 |
| `rcif_verify_sandbox` | `familyDir` | 调用 `verify_sandbox.py --json`，返回 Blocker/Major |
| `rcif_compile_package` | `familyDir`, `outputRcifPath` | 自动签发宿主审批单后打包（provenance = verified） |

工具返回值统一为 `{ ok, summary, details }`，并声明了 DSH 要求的 `output.schema/render`。

## 4. 安全边界

- **路径**：所有入参路径 `resolve` + `realpath` 后必须位于 `workspaceRoot` 内；符号链接越界同样拒绝。
- **能力令牌**：HMAC-SHA256，TTL 15 分钟，绑定 `owner|sessionId|origin|expiresAt`。
- **防重放**：`session-inject` 的 nonce 单次使用，TTL ≤ 5 分钟。
- **审批**：Ed25519（私钥仅驻内存）；Python 侧用同一公钥复核签名与 `table_truth.json`/`params.csv` 摘要；摘要不一致返回 `APPROVAL_STALE`。
- **子进程**：无 shell、固定脚本路径、输出上限 2 MB、超时/Abort 硬杀、随插件 dispose 清理。
- **静态资源**：realpath 边界校验、MIME 白名单、CSP、`nosniff`。

## 5. 测试与打包

```bash
# 契约测试（无需真实 DSH / CadQuery；注入假进程 runner）
node test/plugin-contract.test.mjs

# 真实工具链（需要在 CadQuery 环境）
python rcif-tools/verify_sandbox.py abutment_family

# 打包（prepack 会先跑语法检查与契约测试）
npm pack
```

## 6. 已知限制

1. 尚未提供原生 React 右栏 Tab（需客户端插件 + 真实启动验证）。
2. `pdf_ingest.py` 仍是常量回填，Gate 4 对它不具证据力。
3. 工作台 3D 为真实 `preview.glb`；若族未提供预览文件则回退到明确标注的“示例几何”。
4. 插件不持久化审批/审计记录；provenance 随包输出 `.provenance.json`。
