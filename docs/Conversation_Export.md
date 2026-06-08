# QtOCCTApp 技术对话导出记录 (2026-04-24)

## 1. 核心技术架构 (Core Stack)
*   **GUI 框架**: Qt 6.x (C++)
*   **几何内核**: Open CASCADE Technology (OCCT)
*   **数据管理**: Active Data (基于 OCAF)
*   **建模脚本**: CadQuery / PythonOCC (通过嵌入式 Python)
*   **架构范式**: MDA (模型驱动架构)

## 2. 数据管理 (Active Data & OCAF)
*   **Active Data 优势**: 相比 XCAF，Active Data 提供更灵活的节点结构，适合原生应用开发；继承 OCAF 的事务处理、Undo/Redo 和 BinOCAF 的高性能 IO。
*   **装配机制**: 通过 `AssemblyNode` 和 `InstanceNode` 配合 `ReferenceParameter` 实现实例化和变换。
*   **属性集 (Property Sets)**: 模仿 IFC 模式，使用 `ReferenceListParameter` 实现 Product 节点与多个 Pset 节点的动态关联。
*   **数据分区 (Partitioning)**: 推荐 3+1 模式（拓扑、资源、辅助、结果），确保大规模 Label 下的性能。
*   **语义信息**: 通过在 `PartNode` 下挂载 `SemanticNode` (包含子形状引用和 Tag 字符串) 来实现，支持通过 `IsSame()` 进行交互后的反向查询。

## 3. MDA 与 Schema 定义
*   **定义方式**: 推荐使用 **YAML** 定义模型 Schema。
*   **代码生成**: 使用 Python + Jinja2 将 YAML 定义编译为 C++ 的 Active Data 节点类，保证性能与类型安全。
*   **混合模式**: 核心稳定结构用静态 C++，易变/自定义属性用动态 `PropertySetNode`。

## 4. 参数化构件与脚本引擎
*   **构件库**: 构件定义存储脚本“配方”，实例化时根据参数动态计算几何。
*   **脚本引擎**: 推荐集成 **CadQuery**，其 Fluent API 和选择器机制极大地降低了参数化建模的难度。
*   **代码即数据**: 将 Python 脚本作为 `StringParameter` 存储在 `ComponentDefNode` 中，实现构件逻辑的免编译更新。
*   **宏录制**: 通过命令模式 (Command Pattern) 记录 GUI 操作并转化为参数化脚本。

## 5. 交互与可视化
*   **渲染**: 使用 OCCT 的 AIS 模块。
*   **语义选中**: 讨论了通过遍历匹配 (`IsSame`) 或自定义 `AIS_EntityOwner` 来实现从 3D 边/面快速索引到数据节点的方法。

## 6. AI 集成 (Gemini)
*   **Google AI Studio**: 用于获取 API Key、调试 Prompt 和测试 Few-shot 学习。
*   **API 集成**: 将 Gemini API 嵌入应用，实现“自然语言转建模脚本”的高级功能。
*   **数据隐私**: 免费层级数据可能用于训练，企业级需求推荐使用付费版或 Vertex AI。

---
*文档由 Antigravity 自动生成，记录了项目开发过程中的关键架构决策与技术细节。*
