# 以 3090 为唯一后端：能力迁移与同步约定（2026-10-31）

## 决策

- **3090（DSH 宿主 + `@rcif/dsh-studio-plugin`）是唯一正式后端**；
- 3088（`rcif-tools/workbench_server.py`）降级为**离线调试后端**，仅用于没有 DSH 时的转换/预览；
- `启动助手.bat` 只启动 3090；`启动工作台.bat` 保留但标注“旧后端”。

## 3088 独有能力迁移到 3090

| 3088 能力 | 迁移结果 |
| :--- | :--- |
| `GET /rcif-studio/api/package/grep`（读取 `source/grep.json`） | ✅ 已加入插件，行为与 Python 一致（含 `sessionId`/`path` 解析、404 语义） |
| `POST /rcif-studio/api/assistant/launch` | ✅ 已加入插件：返回 `running:true, launched:false`（宿主即助手，无需再拉起） |
| `GET /` 重定向到工作台 | ⚠️ 部分迁移：DSH 拥有根路径，插件新增 `GET /rcif-studio` 与 `/rcif-studio/` → 302 `/rcif-studio/workbench/` |
| 无 DSH 独立运行 | ❌ 不可迁移（这是 3088 仅存的定位），保留为离线模式 |

`/command`、`/feedback` 在 3088 上始终 `delivered:false`，不属于可迁移能力；3090 已通过 `sessionController.prompt` 真正投递。

## 路由对照（迁移后）

```text
3088 独有：GET /（根重定向，DSH 不可占用）
3090 独有：/annotation-history、/health、/package/edit、/package/patch
共享路径：其余 27 条
```

用 `python scripts/check_rcif_api_parity.py` 可随时复查。

## 同步约定

1. **UI 与插件唯一源**：`dsh-rcif/`，不要直接改安装目录；
2. 打包安装统一走：
   ```powershell
   .\scripts\sync_rcif_plugin.ps1          # 打包 + 备份 + 安装 + 校验
   .\scripts\sync_rcif_plugin.ps1 -Check   # 只校验
   ```
3. 后端行为以插件契约测试为准：`node dsh-rcif/test/plugin-contract.test.mjs`（当前 21/21 PASS）；
4. 新增 3088 专属功能需明确说明“仅离线”，否则应只实现于 3090；
5. 3090 的宿主代码改动必须重启 DSH 生效，静态工作台文件刷新即可。

## 生效

插件已重新打包并安装到 profile；重启 3090 后新路由生效：

```text
GET  /rcif-studio
GET  /rcif-studio/
GET  /rcif-studio/api/package/grep
POST /rcif-studio/api/assistant/launch
```
