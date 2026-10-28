# 工作台与智能助手审核整改记录

- 被审报告：`docs/rcif_workbench_assistant_code_review_2026-10-28.md`
- 整改日期：2026-10-28
- 结论：报告中的 1 项 BLOCKER、4 项 MAJOR 已修复；6 项 MINOR 已处理 5 项，工程拆分保留为后续优化。

## 1. 审核结论复核

报告中的问题均已复核成立：

| 编号 | 结论 | 说明 |
| :--- | :--- | :--- |
| AUD-01 | 成立 | `generic_rebuild.py` 确实直接 `exec_module` 外部生成器源码，未过 GATE-01。 |
| AUD-02 | 成立 | `pdf_generic.py` / `generic_rebuild.py` 先写 `qa.packageSha256` 再二次打包，形成自指哈希失配。 |
| AUD-03 | 成立 | 包围盒从 `[0,0]` 起算会拉偏；cacheKey 也未走 `rcifc.compute_cache_key`。 |
| AUD-04 | 成立 | `rcif_open_package` 只写内存映射，未更新会话 sidecar。 |
| AUD-05 | 成立 | `renderParts` / `selectPart` 只按 `shape.key` 找 mesh，未兼容 `shape.node`。 |
| AUD-06 | 成立 | `api()` 没有 403 `CAP_TOKEN_INVALID` 自动续期。 |
| AUD-07 | 成立 | 非激活 3D 页仍逐帧渲染；`selectPart` 直接改共享材质。 |
| AUD-08 | 成立 | 型号下拉切换后缺少“3D 仍是默认预烘焙”提示。 |
| AUD-09 | 成立 | 截图用 `window.open + document.write`，CSP/WebView 下不可靠。 |
| AUD-10 | 成立 | 仍残留 `3080` 文案。 |
| AUD-11 | 成立 | 存在测试/备份残留与超大单文件。 |

## 2. 修复内容

### AUD-01：通用草稿重建接入 GATE-01

`generic_rebuild.py` 现在：

1. 执行前调用 `verify_sandbox.check_gate1_ast()`，不通过直接拒绝；
2. 生成器模块注入受限 `__builtins__`（无 `open/eval/exec/getattr`）；
3. `__import__` 仅允许 CadQuery/math/typing 等白名单；
4. 运行时启用 PEP 578 审计钩子，禁止写文件、子进程、网络、系统调用；
5. 临时替换 CadQuery 导出入口，防止生成器自行写 STEP/STL；
6. 执行结束后恢复导出接口与字节码设置。

### AUD-02：消除二次打包

- `pdf_generic.py` 只调用一次 `rcifc.build_rcif_package`；
- `generic_rebuild.py` 只调用一次；
- 包内 `qa.packageSha256` 字段移除；外层 SHA 由打包函数返回；
- 新增测试校验 `metadata.packageSha256 == sha256(package file)`。

### AUD-03：包围盒与 cacheKey

- bounds 使用 `inf/-inf` 初始化，循环后 `round(..., 3)`；
- `defaultCacheKey` / `presets[].cacheKey` 统一调用 `rcifc.compute_cache_key(digest, params)`。

### AUD-04：`rcif_open_package` 持久化

工具 `run` 在 `setCurrentPackage` 后调用 `recordSessionArtifacts`，写入：

```text
<workspace>/.rcif/sessions/<sessionId>.json
```

新增宿主契约测试验证 `session-meta.packagePath` 已更新。

### AUD-05：key/node 映射与材质隔离

- 新增 `meshForShape()`，同时匹配 `shape.key`、`shape.node`；
- `selectPart` 归一化为 `shape.key`，同时记录实际 mesh key；
- GLB 加载时逐个 Mesh 克隆材质，避免共享材质高亮污染；
- 材质数组使用 `materialsOf()` 统一遍历；
- 部位表显隐、3D 部位面板、表格定位均走统一映射。

### AUD-06：CapToken 自动续期

`api()` 在 403 `CAP_TOKEN_INVALID` 时：

1. `connectSession()` 重新握手；
2. 更新 `state.capToken`；
3. 透明重试一次原请求。

上传接口也加了同样的 403 重试。

### AUD-07：渲染与材质

- 3D 非激活或页面隐藏时跳过 `renderer.render()`；
- 材质高亮按实际 mesh key 设置；
- 材质数组逐个处理。

### AUD-08：型号预览提示

切换 `presetSelect` / `paramPresetSelect` 后，底部状态栏提示：

```text
3D 为默认型号 <默认型号> 预览；<当前型号> 需转换/重建
```

切回默认型号后恢复 `真实 GLB · N 个部位`。

### AUD-09：截图查看改为应用内弹窗

- 移除 `window.open()` + `document.write()`；
- 新增应用内 `#screenshotModal`；
- 支持关闭按钮、点击遮罩关闭、Esc 关闭。

### AUD-10：端口文案

`3080` 统一改为 `3090`，涉及：

- `dsh-rcif/workbench/workbench.js`
- `rcif-tools/workbench_server.py`

### AUD-11：清理

- 删除 `dsh-rcif/test/.contract-b7uW8Q` 临时目录；
- 删除空 `dsh-rcif/rcif-tools` 残留；
- `.gitignore` 增加 `dsh-rcif/test/.contract-*/`、`rcif-tools/_backup_*/`、`__pycache__/` 等规则；
- `dist/index.js` / `workbench.js` 的模块拆分仍作为后续工程化事项，不在本次安全修复中强行拆分。

## 3. 验证

```text
宿主契约：19/19 PASS
Python 契约：13/13 OK
rcif-web vitest：39/39 PASS（前次验证）
新增安全测试：恶意 import os 生成器被 GATE-01 拒绝
新增哈希测试：包 SHA 与 metadata.packageSha256 一致
新增持久化测试：rcif_open_package 后 session-meta.packagePath 存在
浏览器：3D 工具栏单行、截图弹窗、型号提示均正常，0 JS 异常
```

## 4. 保留事项

1. 当前 GATE-01 + 受限 builtins + 审计钩子仍在宿主进程内执行生成器；比原先直接 `exec_module` 安全，但没有把“任意 CadQuery 生成器”完全移到独立 worker 进程。若要达到最高隔离等级，下一步应设计跨进程几何序列化协议。
2. JHB 双模型对比、拓扑边拾取、stencil 截面封盖属于构件库专用能力，工作台 GLB 单模型不直接适用。
3. `dist/index.js` 与 `workbench.js` 体积仍较大，后续按路由/视图模块拆分。
