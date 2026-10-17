# RDE 工程包格式与多文档设计回顾

> 本文基于对仓库内 `slope.rde`、`bridge.rde` 的实测拆包结果，结合
> `ProjectManager`、`SceneDataExtractor`、模型树/视口联动代码与
> `docs/3d_2d_multi_doc_integration_plan.md` 的设计意图整理。

## 1. RDE 包实测

| 项目 | slope.rde | bridge.rde |
| --- | --- | --- |
| 实际容器格式 | **ZIP**（魔数 `50 4B 03 04`） | **TAR**（ustar，`./` 前缀 + 目录条目） |
| 大小 | 7,575 B | 1,322,496 B |
| 内容 | `master.cbf` 6,476 B<br>`models/subgrade_3d.cbf` 24,411 B<br>`drawings/plan_view.cbf` 11,298 B | 仅 `master.cbf` 1,319,089 B，另有空的 `models/`、`drawings/` 目录 |
| 目录布局 | 扁平（无 `./` 前缀） | 带 `./` 前缀与目录记录 |
| `tar -xf` 解包 | 成功 | 成功 |

补充实测：本机 bsdtar 执行 `tar -cf xxx.zip -C dir .` 产出**仍然是 TAR**
（不按扩展名推断格式），因此应用自身的打包路径产出 TAR，与 `bridge.rde` 一致；
`slope.rde` 的 ZIP 形态来自其它工具（Python `zipfile` 或压缩软件）。

**结论**：`.rde` 的容器格式事实上未定义——同一扩展名下 ZIP/TAR 并存，
读取端"都能用"依赖 bsdtar 的自动识别，换 GNU tar 等实现即失效。

## 2. 设计思路回顾

1. **工程包（RDE）**：交付/迁移单位，约定 `master.cbf + models/ + drawings/`；
   文档要求子文档必须位于主控所在目录的子目录内，以保证相对路径软链接不失效。
2. **文档分层**：主控 = 索引（`adSubDocRef`：DocPath / DocType / IsLoaded）；
   3D 几何与 2D 表达各自独立成文档，物理隔离便于并发与增量传输。
3. **懒加载**：`ProjectManager::GetOrLoadSubDocument()` 按相对路径按需加载，
   `IsLoaded` 记录状态。
4. **懒同步**：打开图纸时比对 3D 特征线，检测到变化在本地静默重算
   （`ProjectManager::Sync2DDrawing()`）。
5. **跨文档身份**：以节点 GUID（`PID_GlobalID`）+ 相对路径构成软链接；
   2D 表达作为 ActiveData 业务节点，几何缓存进本地属性，导出/渲染时
   拆解进 XCAF 图层（`Layer_RoadShoulder` / `SlopeToe_Dashed` / `SlopeTeeth`）。
6. **UI 联动**：模型树、属性面板、视口通过 `_adNodeId` 三向同步。

## 3. 实现与设计的差距

| 问题 | 现象 | 根因 |
| --- | --- | --- |
| GUID 未落库 | 元数据 `_adNodeId`、`updateStretchHandles` 的 nodeId 均为空 | 生产代码从未调用 `SetGlobalID()`（只有测试代码调用过），`PID_GlobalID` 全为空 |
| 包格式未定义 | ZIP/TAR 并存，无版本、无清单、无校验 | `packageToRde` 依赖系统 `tar.exe`；读取靠 bsdtar 自动识别 |
| 主控存在两种形态 | `bridge.rde` 自包含（几何全在主控）；`slope.rde` 纯索引 | 缺少显式布局声明，应用侧只能写兜底逻辑 |
| 分区与父子树混用 | 子文档引用节点与图纸对象在模型树中不可见 | schema 声明这些节点 `default_partition: Topology`，但树/查找按"根节点子级"遍历，且只接受 `adObject` |
| 加载失败静默降级 | 打开失败时日志出现 `creating new document`，最终得到空工程 | `OpenMasterProject` 失败后 `NewDocument` 并返回成功 |
| 打包/解包健壮性 | 解包 10s 硬超时、临时文件固定名、无进度 | 实现细节 |

## 4. 改进建议

### P0（正确性）
1. **GUID 真正落库**：加载工程时为缺失 `GlobalID` 的节点补号并持久化；
   业务侧以 GUID 为首选身份，OCAF 条目 ID 仅作兜底。
2. **统一节点遍历口径**：分区遍历接受任意 `adRoot` 派生节点（不只 `adObject`），
   内部再按 `adObject` 递归，使子文档引用、图纸对象都能进树并被选中。
3. **加载失败不得静默新建空文档**：保留原文档并明确报错。

### P1（包格式规范化）
4. 固定容器格式（建议 ZIP 或 tar.gz），封装为独立模块，去掉对系统 `tar.exe` 的依赖。
5. 包内增加 `manifest.json`：`formatVersion` / `appVersion` / 工程名 / 创建时间 /
   `layout: indexed | self-contained` / 子文档清单（路径 + DocType + SHA-256）/ 单位与坐标系。
6. 打包写入临时文件后原子替换，临时名带 PID/UUID，超时可配置并支持进度回调。

### P2（工程化）
7. RDE 往返测试（打包→解包→逐文件哈希）+ 损坏包/缺子文档/旧格式负向用例。
8. 明确三类入口语义：`.rde`（工程包）/ `master.cbf`（索引工程）/ 普通 `.cbf`（自包含模型）。
9. 打开时校验子文档存在性与版本，给出可操作的错误提示。

## 5. 实施状态

### 已完成
- **P0-1 GUID 落库**：新增 `NodeIdentity`（生成 + 幂等补号）；主控、子文档、普通 CBF
  三条加载路径都会为缺失 `GlobalID` 的节点补号并随保存持久化。
- **P0-2 遍历口径**：模型树的层级与 Topology 分区遍历统一按 `adRoot` 处理，
  子文档引用与图纸对象（`adDrawing2D` 等）均可进树、可被选中；两条路径按条目 ID 去重。
- **P0-3 加载失败**：`OpenMasterProject` 先打开到临时句柄、成功后才替换当前文档；
  会话内已打开的文件直接复用（修复 `PCDM_RS_AlreadyRetrieved`）；
  子文档缺失明确报错，不再静默创建空文档；`loadMasterCbf` 成功后才清空视图。
- **P1-4 容器固定为 ZIP**：新增 `ProjectPackage` 模块，基于 3rpart 自带 zlib 内嵌
  实现 ZIP 读写（无 `tar.exe` 依赖）；旧式 TAR 包读取保留兼容路径。
- **P1-5 manifest.json**：记录 `formatVersion` / `appVersion` / `projectName` /
  `createdAt` / `layout`(`indexed`|`self-contained`) / `master` / 文件清单（大小 + SHA-256）
  / 子文档清单（路径 + DocType）。解包时校验版本、逐文件哈希与 ZIP CRC，
  并拒绝路径穿越（绝对路径、`..`）。
- **P1-6 原子写入**：使用 `QSaveFile` 写临时文件，成功后原子替换目标。
- **P2-7 测试**：新增 `test/test_project_package.cpp`（QtTest）：
  往返哈希比对、旧 ZIP 兼容、截断包拒绝、缺主控报错、路径安全校验；
  已接入 CTest（`ctest -C Release`）。
- **P2-8 入口语义**：日志区分「工程包(.rde) / 主控(master.cbf) / 自包含模型(.cbf)」。

### 本轮后续优化（已落地）
- **P0 正确性**：修复 0 字节文件解包；manifest 的 `master`/`subDocuments`
  路径和重复项校验；打包排除 `manifest.json`、旧 `.rde`、临时/备份/日志文件；
  导出 RDE 前先保存当前工程。
- **P1 大工程**：ZIP 解包改为 64KB 流式解压并逐文件写盘，内存占用不再随文件
  大小线性增长；解包支持进度和取消；打包在压缩时同步计算 SHA-256，避免整包
  二次读取；增加 ZIP32 尺寸/条目限制、总解压大小和磁盘空间检查；ZIP 打包/
  解包在工作线程执行，通过主线程进度对话框反馈。
- **P2 格式**：manifest 增加 `packageId`、`buildId`、`updatedAt`、`units`、
  `sourcePlatform` 等元数据；保留 `projectId`、`coordinateSystem`、
  `schemaVersion`、`comment` 扩展字段；旧式 TAR 包解包前先列出并校验条目路径。
- **暂不实现**：ZIP64、增量包/内容寻址缓存。当前实现会明确拒绝超出 ZIP32
  限制的包。

### 后续补充（本轮新增）
- **旧包升级**：Ribbon「项目」面板新增「升级工程包」，把旧 .rde（无清单的 ZIP 或 TAR）
  解包后重新打包为 v1（`<原名>_v1.rde`，不覆盖原文件），自动按 `models/`、`drawings/`
  约定识别子文档。若已是 v1 会提示无需升级。
- **GUID 格式统一**：「统一 GUID」动作调用 `NodeIdentity::NormalizeGlobalIds()`，
  将非 32 位十六进制的 GUID 批量替换，并同步更新引用它们的
  `adSlopeIndication::TargetObjectID` 与图纸 XCAF 标签上的 `AsciiString`；
  随后带进度保存工程。该操作需用户确认，执行后刷新视图。
- **保存/打包进度条**：`ProjectManager::SaveAll()` 与 `ProjectPackage::createArchive()`
  均支持进度回调（后者按字节、带节流）；导出/升级弹出模态进度条并支持取消，
  取消时不留下半成品文件（QSaveFile 回滚）。
- **ZIP 写入改为单遍流式**（数据描述符模式），进度准确且少一次压缩；
  已用 bsdtar 与 .NET `Expand-Archive` 验证互操作性。
