# QtOCCTApp 深度技术架构指南

本文档总结了关于基于 Qt、OCCT、Active Data 和 CadQuery 构建参数化 CAD 系统的深度技术讨论内容。

---

## 1. 核心架构设计

### 1.1 技术栈选择
*   **Qt 6**: 负责 UI、信号槽及跨平台基础。
*   **OCCT (B-Rep 内核)**: 处理精确几何运算（布尔、倒角、投影）。
*   **Active Data (基于 OCAF)**: 核心数据模型。利用 Label-Attribute 机制管理对象生命周期、Undo/Redo 及持久化。
*   **CadQuery (Python)**: 作为 DSL（领域特定语言），通过嵌入式 Python 解释器驱动几何生成。

---

## 2. 数据管理 (Active Data 深入)

### 2.1 节点与参数模式 (Node-Parameter Pattern)
*   **构件实例化**: 使用 `AssemblyNode` 与 `InstanceNode`。`InstanceNode` 通过 `ReferenceParameter` 指向 `DefinitionNode`，实现“一次定义，多次引用”。
*   **变换管理**: 实例节点携带 `TransformationParameter` (存储 `gp_Trsf`)，在渲染时应用到 AIS 对象上。

### 2.2 属性集 (Property Sets) 实现
模仿 IFC 机制，实现动态属性：
*   **模式**: `ProductNode` -> `ReferenceListParameter` -> `[PropertySetNode1, PropertySetNode2, ...]`。
*   **动态扩展**: `PropertySetNode` 提供 `AddProperty(name, type, value)` 接口，在运行时利用 OCAF 动态创建子 Label 和 Attribute，无需修改 C++ 代码。

### 2.3 性能优化
*   **BinOCAF**: 利用二进制格式和内存映射实现秒级大文件加载。
*   **Label 限制**: 建议单文档 Label 总数控制在 50 万以内。大规模数据（如 100 万个点）应存储在单个 `RealArrayParameter` 中，而非 100 万个子节点。

---

## 3. MDA (模型驱动) 与自动化

### 3.1 模式定义 (YAML DSL)
推荐使用 YAML 定义实体，支持**派生**、**聚合**和**引用**。
```yaml
Wall:
  parent: BuildingElement
  attributes:
    Thickness: Real
  children:
    Openings: { type: Opening, cardinality: 0..* }
```

### 3.2 代码生成流
*   使用 **Python + Jinja2** 模版。
*   输入：YAML Schema。
*   输出：`ActData_BaseNode` 的 C++ 子类。
*   **优势**: 自动处理 Active Data 的 Boilerplate 代码（注册 DTO、参数 ID 管理）。

---

## 4. 几何语义标记 (Semantic Tagging)

### 4.1 实现方案：子节点引用
1.  **定义节点**: `SemanticNode` (包含 `ShapeParameter` 和 `StringParameter`)。
2.  **挂载**: 作为 `PartNode` 的子节点。
3.  **存储**: 将用户选中的 `TopoDS_Edge` 存入 `ShapeParameter`。由于 OCCT 底层共享 `TShape`，内存占用极低。

### 4.2 反向查询逻辑
用户点击 3D 边时，通过以下逻辑找语义：
```cpp
// 遍历语义子节点
if (selectedEdge.IsSame(sNode->GetShape())) {
    return sNode->GetTag();
}
```
*高级方案*: 自定义 `AIS_EntityOwner`，在渲染层直接绑定 `SemanticNode` 引用，实现 O(1) 查找。

---

## 5. 参数化脚本引擎 (CadQuery)

### 5.1 嵌入式集成
在 C++ Driver 中嵌入 Python 解释器：
1.  **注入**: 将 Active Data 参数注入 Python 全局变量（如 `L = 100`）。
2.  **执行**: `PyRun_SimpleString(script_from_node)`。
3.  **桥接**: 通过 **OCP** (OCCT Python Bindings) 获取结果 Shape 的指针或通过内存 Brep 序列化传回 C++。

### 5.2 宏录制 (Macro Recording)
*   **命令模式**: 每个 GUI 操作（如 `DrawBox`）对应一个 `Command` 对象。
*   **ToScript()**: 命令对象生成对应的 CadQuery 语句。
*   **变量替换**: 如果用户输入的值匹配参数表中的变量名，自动将字面量替换为变量名（如 `box(10, 20, 30)` -> `box(W, D, H)`）。

---

## 6. Tree Function (依赖图)
*   **触发机制**: 利用 `ActData_TreeFunctionParameter`。
*   **传播**: 修改参数 A -> 标记 Function 节点为 Dirty -> 触发 Driver 执行 -> 调用脚本生成几何 -> 更新 Shape 属性。

---
*此文档包含了对话中探讨的所有关键 C++ 逻辑与架构设计思路。*
