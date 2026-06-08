# 实施计划：使用多链接文档实现 3D 模型与 2D 图纸统一管理

本计划旨在现有的 `asiActiveData` / `XCAF` 架构之上，实现一种高可扩展、支持延迟加载和多人并发编辑的多文档管理机制。

### 核心架构决策
1. **物理文件解耦**：采用**“业务层软链接主控文件”**机制，将 3D 模型文件（`models/bridge_3d.cbf`）与 2D 图纸文件（`drawings/drawing_2d.cbf`）进行物理隔离。
2. **2D 表达独立存储**：所有的二维表达数据（如路基示坡线、引线标注）保存在图纸文档中。
3. **特征引用与延迟同步 (Lazy Sync)**：2D 表达节点通过 GUID 和相对路径软链接关联 3D 实体，并在图纸文件打开时执行特征线几何比对，在检测到 3D 发生变化时在前端本地静默重算。
4. **混合表达与图层管理**：
   * 2D 表达节点作为 ActiveData 业务骨架；几何边存储于本地属性缓存；
   * 在导出或渲染时，由生成器调用 XCAF 接口拆解并注册进子文档的 XCAF 图层中（分别绑定路肩、坡脚虚线、齿线图层与颜色）。

---

## User Review Required

> [!IMPORTANT]
> **设计决议与物理部署规范**：
> 1. **物理目录结构约束**：为保证软链接不失效，所有子文档必须保存在主控文件所在目录的子目录中。推荐结构：
>    ```
>    [Project_Dir]
>      ├── master.cbf         (主控工程索引)
>      ├── models/
>      │     └── subgrade_3d.cbf (3D 物理模型文档)
>      └── drawings/
>            └── plan_view.cbf   (2D 图纸与表达文档)
>    ```
> 2. **XCAF 图层映射规范**：
>    示坡线在写入 XCAF 时会被拆解为三个子零件：路肩线注册至 `Layer_RoadShoulder`（设为蓝色实线）；坡脚线注册至 `Layer_SlopeToe_Dashed`（设为红色虚线）；齿线注册至 `Layer_SlopeTeeth`（设为灰色细线）。
> 3. **延时同步机制 (Lazy Sync)**：
>    图纸在加载时会自动检测对应 3D 模型的特征线修改情况。如果检测到变更，会在前端本地重算示坡线。

---

## Proposed Changes

### 1. 核心数据模型层 (core-data-model)

---

#### [MODIFY] [core-data-model.yaml](file:///d:/QtOCCTApp/core-data-model/schema/core-data-model.yaml)
在 Schema 中增加主控软链接节点和 2D 业务节点定义：
* **新增 `adSubDocRef` 节点**：用于主控文件对子工程的路径引用。
* **新增 `adDrawing2D` 节点**：图纸管理节点，修改为支持挂载 `Representations`（示坡线等）以及新增的 `LeaderAnnotations`（引线标注）。
* **新增 `adLeaderAnnotation` 节点**：引线标注节点，父级为 `adDrawing2D` 的 `children`。
* **新增 `adRepresentation2D` 基类节点**：通用的二维表达基类，持有生成的拓扑几何。
* **新增 `adSlopeIndication` 业务呈现节点**：继承自 `adRepresentation2D`。包含齿线等间距、长短线比例、特征输入线，以及跨文档引用的对象 ID 和路径属性。

```yaml
# 挂载在主控文件根下的子文档引用
adSubDocRef:
  name: "子文档引用"
  base: adRoot
  attributes:
    DocPath: { type: "String", default: "", label: "子文档相对路径 (e.g. './drawings/plan_view.cbf')" }
    DocType: { type: "String", default: "3DModel", label: "子文档类型(3DModel / 2DDrawing)" }
    IsLoaded: { type: "Bool", default: false, label: "内存加载状态" }
  default_partition: "Topology"

# 二维图纸节点
adDrawing2D:
  name: "二维图纸"
  base: adRoot
  attributes:
    DrawingType: { type: "String", default: "PlanView", label: "图纸类型(平面/剖面/横断)" }
    Scale: { type: "Real", default: 1.0, label: "比例尺" }
  children:
    Representations: { type: "adRepresentation2D", cardinality: "0..*" }
    LeaderAnnotations: { type: "adLeaderAnnotation", cardinality: "0..*" } # [新] 挂载引线标注
  default_partition: "Topology"

# 二维表达基类
adRepresentation2D:
  name: "二维表达基类"
  base: adRoot
  attributes:
    RepresentationType: { type: "String", default: "PlanView", label: "表达视图类型(PlanView/SectionView)" }
    GeneratedShape:      { type: "Shape", label: "缓存在本图纸中的二维几何" }
  default_partition: "Topology"

# 引线标注节点 (继承自二维表达基类)
adLeaderAnnotation:
  name: "引线标注"
  base: adRepresentation2D
  attributes:
    NoteText: { type: "String", default: "", label: "标注文本" }
    ArrowPoint: { type: "RealArray", label: "引线箭头指向的3D点" }
    TextPoint: { type: "RealArray", label: "文本放置的3D点" }
  default_partition: "Topology"

# 路基示坡线呈现对象 (继承自二维表达基类)
adSlopeIndication:
  name: "路基示坡线表达"
  base: adRepresentation2D
  attributes:
    # 跨文档关联关系
    TargetModelDoc: { type: "String", default: "", label: "目标3D模型文档相对路径" }
    TargetObjectID: { type: "GUID", default: "", label: "目标3D路基构件ID" }
    
    # 输入特征几何暂存 (用于断线重算)
    ShoulderLine:   { type: "Shape", label: "路肩特征线" }
    ToeLine:        { type: "Shape", label: "坡脚特征线" }
    
    # 示坡齿线控制参数
    Spacing:        { type: "Real", default: 2000.0, label: "等间距距离" }
    LongLineRatio:  { type: "Real", default: 0.6, label: "长齿线长度占比" }
    ShortLineRatio: { type: "Real", default: 0.3, label: "短齿线长度占比" }
```

---

#### [NEW] [ProjectManager.h](file:///d:/QtOCCTApp/core-data-model/ProjectManager.h) & [ProjectManager.cpp](file:///d:/QtOCCTApp/core-data-model/ProjectManager.cpp)
定义并实现多文档生命周期管理器类，提供：
* 主控工程加载与统一保存。
* 子文档按需懒加载。
* **延迟同步机制 (Lazy Sync)**：在加载图纸子文档时，自动读取关联的 3D 模型特征线几何。比对特征线几何的哈希值，若不一致，调用生成器重算 2D 几何并同步更新本地缓存。

---

#### [NEW] [RwBuilder.h](file:///d:/QtOCCTApp/core-data-model/RwBuilder.h) & [RwBuilder.cpp](file:///d:/QtOCCTApp/core-data-model/RwBuilder.cpp)
新增几何/XDE 构建基类，为以后的铁路专业构件图形表达和标注预留通用接口。
* 定义虚函数 `Build()` 用于执行几何拓扑计算。
* 定义虚函数 `SaveToXDE(Handle(TDocStd_Document)& doc, ...)` 负责将生成的拓扑几何分解注册进对应图纸文档的 XCAF 树。

---

#### [NEW] [RwSlope2DGeometryBuilder.h](file:///d:/QtOCCTApp/core-data-model/RwSlope2DGeometryBuilder.h) & [RwSlope2DGeometryBuilder.cpp](file:///d:/QtOCCTApp/core-data-model/RwSlope2DGeometryBuilder.cpp)
继承自 `RwBuilder` 基类，实现路基示坡线的专用构建器。
* **`Build()`**：在路肩特征线（`TopoDS_Wire`）上通过弧长等间距参数化离散，计算出离散点；在各点上做垂直于路肩线切线的射线；射线与坡脚特征线进行求交并提取交点；根据长短线比例和奇偶项分别裁剪出长线与短线，最后打包输出齿线几何 Compound。
* **`SaveToXDE(...)`**：实现将生成的示坡线拆解保存到 XDE。分别把路肩线、坡脚线、齿线注册至 `ShapeTool` 的独立 Label，使用 `LayerTool` 绑定各自的图层（如 `Layer_SlopeToe_Dashed`），并通过 `ColorTool` 设定其颜色外观。

---

#### [MODIFY] [CMakeLists.txt](file:///d:/QtOCCTApp/core-data-model/CMakeLists.txt)
添加新类源码以及对应的 gtest 单元测试到编译目标中：
```cmake
# 添加源文件
set(CORE_SOURCES
    GeometryService.cpp
    IfcExportService.cpp
    ProjectManager.cpp # 新增
    RwBuilder.cpp # 新增
    RwSlope2DGeometryBuilder.cpp # 新增
    # ...
)

# 添加 gtest 测试模块 (配置 vs2026-x64-v142 编译)
add_executable(test_master_project 
    tests/test_master_project.cpp
)
target_link_libraries(test_master_project PRIVATE 
    core-data-model 
    GTest::gtest_main
)
```

---

#### [NEW] [test_master_project.cpp](file:///d:/QtOCCTApp/core-data-model/tests/test_master_project.cpp)
编写 gtest 单元测试代码：
* **多文档读写与同步测试**：新建主控文档与子文档，模拟 3D 模型特征线数据。
* **RwSlope2DGeometryBuilder 生成与 XDE 存储验证**：测试 `Build()` 算法结果，调用 `SaveToXDE` 保存，并反向读取 XCAF 验证图层（`Layer_SlopeToe_Dashed` 等）与曲线颜色属性是否正确挂载。

---

## Verification Plan

### Automated Tests
1. **MDA 生成验证**：
   运行 `mda_generator.py`，检查 `generated/` 目录下是否正确生成 `BrNode_adSubDocRef.h`、`BrNode_adDrawing2D.h`、`BrNode_adRepresentation2D.h`、`BrNode_adLeaderAnnotation.h`、`BrNode_adSlopeIndication.h`。
2. **GTest 单元测试验证**：
   在 MSVC (v142) 环境下编译并运行测试程序：
   ```powershell
   # 运行 gtest 单元测试
   .\build_v142\bin\Release\test_master_project.exe
   ```
   断言 `ProjectManager` 懒加载、`RwSlope2DGeometryBuilder` 几何裁剪以及 XDE 物理图层存储全部测试通过。

### Manual Verification
1. **多文档隔离性与协作测试**：
   只读加载 3D 模型文件，测试单独编辑并保存 2D 图纸文件 `plan_view.cbf`。验证其能正常保存，且 3D 文件未被加锁占用。
2. **图层与样式验证**：
   在 Qt 界面视口中加载图纸文档，控制 `Layer_SlopeToe_Dashed` 图层的显示/隐藏。验证坡脚线在视口中以虚线形式正确隐藏或渲染，且长短齿线以细灰色线型正确渲染在最上层。
