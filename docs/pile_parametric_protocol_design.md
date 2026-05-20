# 基于 Pile & PileCap 的几何复用与解耦建模协议设计方案

本方案在上一版“语义与几何解耦”的基础上，引入了 BIM 行业标准的**几何实例化复用机制（Geometry Instancing）**。
通过引入 IFC 官方支持的 **`IfcRepresentationMap`（几何表达映射图）** 与 **`IfcMappedItem`（已映射图元）** 概念：
1. 服务端仅需定义**一根单桩的几何特征**（位于坐标原点）。
2. 其余 6 根桩仅在协议中以极简 of “映射图元 (MappedItem)” + “目标坐标平移” 来表示。
3. 泛化 C++ 特征引擎 `IfcGeometryParser` 实现了底层的延迟实例化机制，自动在内存中进行几何块共享（Block Reference），极大压缩了导出的 IFC 文件大小。

---

## 1. 几何复用架构图 (Geometry Instancing Workflow)

```mermaid
graph TD
    subgraph JSON Protocol (几何复用协议)
        A[Single_Pile_Solid<br>定义于坐标原点的单桩拉伸体] 
        B1[Pile_01_Mapped<br>映射到坐标点1]
        B2[Pile_02_Mapped<br>映射到坐标点2]
        
        B1 -.->|引用| A
        B2 -.->|引用| A
    end

    subgraph C++ / Python Parser (泛化特征引擎)
        C[IfcGeometryParser] -->|1. 首次解析| A
        C -->|2. 创建共享块| D[IfcRepresentationMap]
        C -->|3. 解析映射1| E1[IfcMappedItem 1]
        C -->|4. 解析映射2| E2[IfcMappedItem 2]
        
        E1 -->|共享引用| D
        E2 -->|共享引用| D
    end
```

---

## 2. 几何复用版参数化建模协议实例 (CIPR JSON)

相较于前一版为 6 根桩单独定义 6 个 `IfcExtrudedAreaSolid`，本协议：
* 仅声明了一个 `Single_Pile_Solid` 实体特征。
* 新增 6 个 `type = "IfcMappedItem"` 节点，通过 `SourceGeometry` 引用单桩，并通过 `TargetLocation` 指定平移坐标。

```json
{
  "$schema": "https://standards.buildingsmart.org/IFC/RELEASE/IFC4/ADD2/JSON/schema.json",
  "protocol_version": "1.2.0",
  "metadata": {
    "generator": "CadQuery-CIPR-Engine v1.2",
    "model_name": "Pile_And_Cap_Instanced_Assembly"
  },
  "parameters": {
    "pile_diameter": 1000.0,
    "pile_length": 15000.0,
    "pile_layout": "2x3",
    "pile_hSpacing": 2500.0,
    "pile_vSpacing": 3000.0,
    "cap_width": 4000.0,
    "cap_length": 5000.0,
    "cap_thickness": 1500.0
  },
  "declarations": {
    "profiles": [
      {
        "id": "Profile_Pile_Circle",
        "type": "IfcCircleProfileDef",
        "ProfileType": "AREA",
        "Radius": 500.0
      },
      {
        "id": "Profile_Cap_Rectangle",
        "type": "IfcRectangleProfileDef",
        "ProfileType": "AREA",
        "XDim": 4000.0,
        "YDim": 5000.0
      }
    ]
  },
  "features": [
    {
      "id": "Cap_Solid",
      "type": "IfcExtrudedAreaSolid",
      "SweptArea": "Profile_Cap_Rectangle",
      "Position": {
        "type": "IfcAxis2Placement3D",
        "Location": [0.0, 0.0, 0.0],
        "Axis": [0.0, 0.0, 1.0],
        "RefDirection": [1.0, 0.0, 0.0]
      },
      "ExtrudedDirection": [0.0, 0.0, 1.0],
      "Depth": 1500.0
    },
    {
      "id": "Single_Pile_Solid",
      "type": "IfcExtrudedAreaSolid",
      "description": "坐标原点处的单桩拉伸几何（用作复用源）",
      "SweptArea": "Profile_Pile_Circle",
      "Position": {
        "type": "IfcAxis2Placement3D",
        "Location": [0.0, 0.0, 0.0],
        "Axis": [0.0, 0.0, -1.0],
        "RefDirection": [1.0, 0.0, 0.0]
      },
      "ExtrudedDirection": [0.0, 0.0, -1.0],
      "Depth": 15000.0
    },
    {
      "id": "Pile_01_Mapped",
      "type": "IfcMappedItem",
      "SourceGeometry": "Single_Pile_Solid",
      "TargetLocation": [-2500.0, -1500.0, 0.0]
    },
    {
      "id": "Pile_02_Mapped",
      "type": "IfcMappedItem",
      "SourceGeometry": "Single_Pile_Solid",
      "TargetLocation": [-2500.0, 1500.0, 0.0]
    },
    {
      "id": "Pile_03_Mapped",
      "type": "IfcMappedItem",
      "SourceGeometry": "Single_Pile_Solid",
      "TargetLocation": [0.0, -1500.0, 0.0]
    },
    {
      "id": "Pile_04_Mapped",
      "type": "IfcMappedItem",
      "SourceGeometry": "Single_Pile_Solid",
      "TargetLocation": [0.0, 1500.0, 0.0]
    },
    {
      "id": "Pile_05_Mapped",
      "type": "IfcMappedItem",
      "SourceGeometry": "Single_Pile_Solid",
      "TargetLocation": [2500.0, -1500.0, 0.0]
    },
    {
      "id": "Pile_06_Mapped",
      "type": "IfcMappedItem",
      "SourceGeometry": "Single_Pile_Solid",
      "TargetLocation": [2500.0, 1500.0, 0.0]
    }
  ],
  "products": [
    {
      "id": "Product_Cap_01",
      "ifc_type": "IfcFooting",
      "predefined_type": "PILE_CAP",
      "name": "承台_C35",
      "representations": ["Cap_Solid"]
    },
    {
      "id": "Product_Pile_01",
      "ifc_type": "IfcPile",
      "predefined_type": "BORED",
      "name": "基础灌注桩组",
      "representations": [
        "Pile_01_Mapped",
        "Pile_02_Mapped",
        "Pile_03_Mapped",
        "Pile_04_Mapped",
        "Pile_05_Mapped",
        "Pile_06_Mapped"
      ]
    }
  ]
}
```

---

## 3. 泛化 C++ 特征解析引擎中新增 MappedItem 支持 (C++ 方案)

在泛化三维几何特征解析引擎中，我们为 `IfcGeometryParser` 扩展一个 `IfcMappedItem` 识别分支。它使用**全局延迟缓存机制**来动态构建与共享 `IfcRepresentationMap`。

```cpp
#include <nlohmann/json.hpp>
#include <ifcparse/Ifc4.h>
#include <map>
#include <string>
#include <vector>

using json = nlohmann::json;

class IfcGeometryParser {
public:
    // 解析截面定义
    static Ifc4::IfcProfileDef* ParseProfile(Ifc4::IfcModel& model, const json& j) {
        std::string type = j.at("type").get<std::string>();
        auto defaultPlacement = new Ifc4::IfcAxis2Placement2D(
            new Ifc4::IfcCartesianPoint({0.0, 0.0}), boost::none
        );
        model.addEntity(defaultPlacement);

        if (type == "IfcCircleProfileDef") {
            double radius = j.at("Radius").get<double>();
            auto profile = new Ifc4::IfcCircleProfileDef(
                Ifc4::IfcProfileTypeEnum::IfcProfileType_AREA, boost::none, defaultPlacement, radius
            );
            model.addEntity(profile);
            return profile;
        } 
        else if (type == "IfcRectangleProfileDef") {
            double xDim = j.at("XDim").get<double>();
            double yDim = j.at("YDim").get<double>();
            auto profile = new Ifc4::IfcRectangleProfileDef(
                Ifc4::IfcProfileTypeEnum::IfcProfileType_AREA, boost::none, defaultPlacement, xDim, yDim
            );
            model.addEntity(profile);
            return profile;
        }
        return nullptr;
    }

    // 解析几何特征（包含拉伸、布尔、几何映射复用）
    static Ifc4::IfcRepresentationItem* ParseFeature(
        Ifc4::IfcModel& model,
        const json& j,
        const std::map<std::string, Ifc4::IfcProfileDef*>& profiles,
        std::map<std::string, Ifc4::IfcRepresentationItem*>& resolvedFeatures,
        std::map<std::string, Ifc4::IfcRepresentationMap*>& sharedRepMaps) // 传入共享池
    {
        std::string type = j.at("type").get<std::string>();

        if (type == "IfcExtrudedAreaSolid") {
            std::string sweptAreaId = j.at("SweptArea").get<std::string>();
            Ifc4::IfcProfileDef* profile = profiles.at(sweptAreaId);

            auto posJson = j.at("Position");
            auto loc = new Ifc4::IfcCartesianPoint(posJson.at("Location").get<std::vector<double>>());
            auto axis = new Ifc4::IfcDirection(posJson.at("Axis").get<std::vector<double>>());
            auto refDir = new Ifc4::IfcDirection(posJson.at("RefDirection").get<std::vector<double>>());
            auto placement = new Ifc4::IfcAxis2Placement3D(loc, axis, refDir);
            
            model.addEntity(loc);
            model.addEntity(axis);
            model.addEntity(refDir);
            model.addEntity(placement);

            auto extrDir = new Ifc4::IfcDirection(j.at("ExtrudedDirection").get<std::vector<double>>());
            double depth = j.at("Depth").get<double>();
            model.addEntity(extrDir);

            auto solid = new Ifc4::IfcExtrudedAreaSolid(profile, placement, extrDir, depth);
            model.addEntity(solid);
            return solid;
        }
        else if (type == "IfcBooleanResult") {
            std::string opStr = j.at("operator").get<std::string>();
            Ifc4::IfcBooleanOperator::Value op;
            if (opStr == "UNION") op = Ifc4::IfcBooleanOperator::IfcBooleanOperator_UNION;
            else if (opStr == "DIFFERENCE") op = Ifc4::IfcBooleanOperator::IfcBooleanOperator_DIFFERENCE;
            else if (opStr == "INTERSECTION") op = Ifc4::IfcBooleanOperator::IfcBooleanOperator_INTERSECTION;

            std::string operandA_Id = j.at("first_operand").get<std::string>();
            std::string operandB_Id = j.at("second_operand").get<std::string>();

            auto firstOperand = dynamic_cast<Ifc4::IfcBooleanOperand*>(resolvedFeatures.at(operandA_Id));
            auto secondOperand = dynamic_cast<Ifc4::IfcBooleanOperand*>(resolvedFeatures.at(operandB_Id));

            auto booleanResult = new Ifc4::IfcBooleanResult(op, firstOperand, secondOperand);
            model.addEntity(booleanResult);
            return booleanResult;
        }
        // ==========================================
        // 核心亮点：通用的几何实例化复用解析逻辑
        // ==========================================
        else if (type == "IfcMappedItem") {
            std::string sourceGeomId = j.at("SourceGeometry").get<std::string>();
            auto sourceGeomItem = resolvedFeatures.at(sourceGeomId);

            Ifc4::IfcRepresentationMap* repMap = nullptr;

            // 1. 如果源几何没有建立过 RepresentationMap，则就地创建一个共享的 Map
            if (sharedRepMaps.find(sourceGeomId) == sharedRepMaps.end()) {
                auto subRep = new Ifc4::IfcShapeRepresentation(
                    nullptr, "Body", "SweptSolid", { sourceGeomItem }
                );
                model.addEntity(subRep);

                // 映射原点设置为标准的恒等矩阵原点
                auto originPlacement = new Ifc4::IfcAxis2Placement3D(
                    new Ifc4::IfcCartesianPoint({0.0, 0.0, 0.0}),
                    new Ifc4::IfcDirection({0.0, 0.0, 1.0}),
                    new Ifc4::IfcDirection({1.0, 0.0, 0.0})
                );
                model.addEntity(originPlacement->Location());
                model.addEntity(originPlacement->Axis().value());
                model.addEntity(originPlacement->RefDirection().value());
                model.addEntity(originPlacement);

                repMap = new Ifc4::IfcRepresentationMap(originPlacement, subRep);
                model.addEntity(repMap);
                
                // 缓存到共享池，防止重复创建 RepresentationMap 造成垃圾实体
                sharedRepMaps[sourceGeomId] = repMap;
            } else {
                repMap = sharedRepMaps[sourceGeomId];
            }

            // 2. 解析局部的偏移定位坐标，创建三维算子
            auto targetLocCoords = j.at("TargetLocation").get<std::vector<double>>();
            auto targetOrigin = new Ifc4::IfcCartesianPoint(targetLocCoords);
            model.addEntity(targetOrigin);

            auto transformOperator = new Ifc4::IfcCartesianTransformationOperator3D(
                boost::none, // Axis1
                boost::none, // Axis2
                targetOrigin, // LocalOrigin
                boost::none, // Scale
                boost::none  // Axis3
            );
            model.addEntity(transformOperator);

            // 3. 构建 MappedItem 指针，使多根桩共享同一个几何对象
            auto mappedItem = new Ifc4::IfcMappedItem(repMap, transformOperator);
            model.addEntity(mappedItem);
            return mappedItem;
        }

        return nullptr;
    }
};
```

---

## 4. 上层语义装配层 (C++ 方案)

```cpp
class IfcSemanticAssembler {
public:
    static void BuildBimModel(Ifc4::IfcModel& model, const json& recipeJson) {
        // 1. 解析二维截面
        std::map<std::string, Ifc4::IfcProfileDef*> profileMap;
        for (const auto& pJson : recipeJson["declarations"]["profiles"]) {
            std::string id = pJson["id"];
            profileMap[id] = IfcGeometryParser::ParseProfile(model, pJson);
        }

        // 2. 解析几何特征（新增共享池以满足几何多重复用）
        std::map<std::string, Ifc4::IfcRepresentationItem*> resolvedFeatures;
        std::map<std::string, Ifc4::IfcRepresentationMap*> sharedRepMaps;
        
        for (const auto& fJson : recipeJson["features"]) {
            std::string id = fJson["id"];
            resolvedFeatures[id] = IfcGeometryParser::ParseFeature(
                model, fJson, profileMap, resolvedFeatures, sharedRepMaps
            );
        }

        // 3. 解析 BIM 产品并挂载几何表达
        for (const auto& prodJson : recipeJson["products"]) {
            std::string id = prodJson["id"];
            std::string ifcType = prodJson["ifc_type"];
            std::string name = prodJson["name"];

            std::vector<Ifc4::IfcRepresentationItem*> items;
            for (const std::string& featId : prodJson["representations"]) {
                items.push_back(resolvedFeatures.at(featId));
            }

            auto shapeRepresentation = new Ifc4::IfcShapeRepresentation(
                nullptr, "Body", "MappedRepresentation", items
            );
            model.addEntity(shapeRepresentation);

            auto productDefinition = new Ifc4::IfcProductDefinitionShape(
                boost::none, boost::none, { shapeRepresentation }
            );
            model.addEntity(productDefinition);

            Ifc4::IfcProduct* bimProduct = nullptr;
            
            if (ifcType == "IfcPile") {
                bimProduct = new Ifc4::IfcPile(
                    model.getExtent().generateUuid(), nullptr, name, boost::none, boost::none,
                    nullptr, productDefinition, boost::none, Ifc4::IfcPileTypeEnum::IfcPileType_BORED
                );
            } 
            else if (ifcType == "IfcFooting") {
                bimProduct = new Ifc4::IfcFooting(
                    model.getExtent().generateUuid(), nullptr, name, boost::none, boost::none,
                    nullptr, productDefinition, boost::none, Ifc4::IfcFootingTypeEnum::IfcFooting_PILE_CAP
                );
            }

            if (bimProduct) {
                model.addEntity(bimProduct);
            }
        }
    }
};
```

---

## 5. Python (ifcopenshell-py) 特征解析与装配引擎实现

在 Python 服务端或独立 service 中，我们可以使用 `ifcopenshell` 库来解析相同的参数化建模协议（CIPR JSON）。
由于 Python 是动态语言且 `ifcopenshell` 对高层实体创建进行了封装，其实现代码显得更为紧凑。

```python
import json
import ifcopenshell
import ifcopenshell.api

class IfcPythonGeometryParser:
    """通用 Python 几何特征解析器（构件无关）"""
    
    @staticmethod
    def parse_profile(model: ifcopenshell.file, p_json: dict):
        p_type = p_json["type"]
        
        # 默认二维坐标系
        default_placement = model.create_entity(
            "IfcAxis2Placement2D",
            Location=model.create_entity("IfcCartesianPoint", Coordinates=(0.0, 0.0))
        )
        
        if p_type == "IfcCircleProfileDef":
            return model.create_entity(
                "IfcCircleProfileDef",
                ProfileType="AREA",
                Position=default_placement,
                Radius=p_json["Radius"]
            )
        elif p_type == "IfcRectangleProfileDef":
            return model.create_entity(
                "IfcRectangleProfileDef",
                ProfileType="AREA",
                Position=default_placement,
                XDim=p_json["XDim"],
                YDim=p_json["YDim"]
            )
        return None

    @staticmethod
    def parse_feature(
        model: ifcopenshell.file, 
        f_json: dict, 
        profiles: dict, 
        resolved_features: dict, 
        shared_rep_maps: dict
    ):
        f_type = f_json["type"]

        if f_type == "IfcExtrudedAreaSolid":
            profile = profiles[f_json["SweptArea"]]
            pos = f_json["Position"]
            
            # 创建三维坐标定位
            loc = model.create_entity("IfcCartesianPoint", Coordinates=tuple(pos["Location"]))
            axis = model.create_entity("IfcDirection", DirectionRatios=tuple(pos["Axis"]))
            ref_dir = model.create_entity("IfcDirection", DirectionRatios=tuple(pos["RefDirection"]))
            placement = model.create_entity("IfcAxis2Placement3D", Location=loc, Axis=axis, RefDirection=ref_dir)
            
            extr_dir = model.create_entity("IfcDirection", DirectionRatios=tuple(f_json["ExtrudedDirection"]))
            depth = f_json["Depth"]

            return model.create_entity(
                "IfcExtrudedAreaSolid",
                SweptArea=profile,
                Position=placement,
                ExtrudedDirection=extr_dir,
                Depth=depth
            )
            
        elif f_type == "IfcBooleanResult":
            op = f_json["operator"]
            operand_a = resolved_features[f_json["first_operand"]]
            operand_b = resolved_features[f_json["second_operand"]]
            
            return model.create_entity(
                "IfcBooleanResult",
                Operator=op,
                FirstOperand=operand_a,
                SecondOperand=operand_b
            )
            
        elif f_type == "IfcMappedItem":
            source_geom_id = f_json["SourceGeometry"]
            source_geom = resolved_features[source_geom_id]

            # 延迟并共享创建 RepresentationMap
            if source_geom_id not in shared_rep_maps:
                sub_rep = model.create_entity(
                    "IfcShapeRepresentation",
                    ContextOfItems=None,
                    RepresentationIdentifier="Body",
                    RepresentationType="SweptSolid",
                    Items=[source_geom]
                )
                origin_placement = model.create_entity(
                    "IfcAxis2Placement3D",
                    Location=model.create_entity("IfcCartesianPoint", Coordinates=(0.0, 0.0, 0.0)),
                    Axis=model.create_entity("IfcDirection", DirectionRatios=(0.0, 0.0, 1.0)),
                    RefDirection=model.create_entity("IfcDirection", DirectionRatios=(1.0, 0.0, 0.0))
                )
                rep_map = model.create_entity(
                    "IfcRepresentationMap",
                    MappingOrigin=origin_placement,
                    MappedRepresentation=sub_rep
                )
                shared_rep_maps[source_geom_id] = rep_map
            else:
                rep_map = shared_rep_maps[source_geom_id]

            target_loc = model.create_entity("IfcCartesianPoint", Coordinates=tuple(f_json["TargetLocation"]))
            transform_operator = model.create_entity(
                "IfcCartesianTransformationOperator3D",
                LocalOrigin=target_loc
            )
            
            return model.create_entity(
                "IfcMappedItem",
                MappingSource=rep_map,
                MappingTarget=transform_operator
            )
        return None


class IfcPythonSemanticAssembler:
    """上层 Python 语义装配器"""
    
    @staticmethod
    def build_bim_model(recipe_json_str: str, output_path: str):
        recipe = json.loads(recipe_json_str)
        
        # 初始化 IFC4 模型文件
        model = ifcopenshell.file(schema="IFC4")
        
        # 1. 解析二维截面库
        profiles = {}
        for p_json in recipe["declarations"]["profiles"]:
            profiles[p_json["id"]] = IfcPythonGeometryParser.parse_profile(model, p_json)
            
        # 2. 解析纯三维几何特征（并进行特征复用解算）
        resolved_features = {}
        shared_rep_maps = {}
        for f_json in recipe["features"]:
            f_id = f_json["id"]
            resolved_features[f_id] = IfcPythonGeometryParser.parse_feature(
                model, f_json, profiles, resolved_features, shared_rep_maps
            )
            
        # 3. 动态构建语义 BIM 产品
        for prod_json in recipe["products"]:
            ifc_type = prod_json["ifc_type"]
            name = prod_json["name"]
            
            # 使用高层 API 创建具体的物理构件
            product = ifcopenshell.api.run("root.create_entity", model, ifc_class=ifc_type, name=name)
            
            # 设置具体的预定义类型子类（如承台、灌注桩类型）
            if "predefined_type" in prod_json:
                product.PredefinedType = prod_json["predefined_type"]
                
            # 整合构件的多重几何表达
            items = [resolved_features[feat_id] for feat_id in prod_json["representations"]]
            
            representation = model.create_entity(
                "IfcShapeRepresentation",
                ContextOfItems=None,
                RepresentationIdentifier="Body",
                RepresentationType="MappedRepresentation" if ifc_type == "IfcPile" else "SweptSolid",
                Items=items
            )
            
            product_shape = model.create_entity(
                "IfcProductDefinitionShape",
                Representations=[representation]
            )
            
            # 绑定几何到语义构件上
            product.Representation = product_shape
            
        # 写入物理文件
        model.write(output_path)
```

---

## 6. Python 与 C++ 方案深度对比

这两套解析引擎在实际的工程系统集成中各有得失，针对不同场景有着非常明确的选择倾向：

| 比较维度 | C++ (ifcopenshell-cpp) 方案 | Python (ifcopenshell-py) 方案 | 评鉴与分析 |
| :--- | :---: | :---: | :--- |
| **开发效率与维护**| 较低 | **极高** | Python 的语法免编译，极易进行协议扩展测试；C++ 需要指针管理和较长的重构周期。 |
| **运行性能 (大规模模型)**| **极高** | 较高 (核心仍为C++内核) | C++ 在处理数十万级构件和巨型 JSON 文本解析时，拥有极致的内存消耗和极快速度优势。 |
| **内存与代码安全性**| 中等（需手动管理 OCCT 内存） | **极高** | Python 自动进行垃圾回收和引用计数，极少发生 C++ 中的指针悬空或内存泄漏引发的进程崩溃。 |
| **生态成熟度 (BIM API)**| 较低（仅有底层元编程类） | **极高 (拥有 ifcopenshell.api)**| Python 版的 `ifcopenshell.api` 内置了极其丰富的关系链接、空间分类（Spatial structure）、属性集分配等高层成熟功能。 |
| **客户端打包与部署成本**| **极佳 (单文件 DLL 交付)** | 较差 | **Qt 客户端的最佳拍档**。C++ 解析器直接编进二进制，零运行环境依赖；Python 方案需要客户端带上 Python 解释器环境，包体积庞大且配置易碎。 |

---

## 7. 常用高阶建模特征协议与解析实现（CSG、拉伸、扫掠、多截面放样）

为了使此 CIPR 协议能覆盖更通用的 CAD/BIM 特征场景，以下扩展了 **CSG布尔树、带空腔任意多边形拉伸、路径扫掠（SweptArea）、多截面放样（Sectioned Spine）** 等高阶特征的 JSON 语法规范与 C++/Python 泛化实现。

### 7.1 构造实体几何 (CSG 嵌套布尔树)
常用于多级特征修剪（例如：承台挖孔、桩头倒角与切除等）。
在 JSON 中，通过 `IfcBooleanResult` 递归嵌套定义，操作数本身可以指向另一个 `IfcBooleanResult` 或 `IfcExtrudedAreaSolid`。

#### JSON 协议定义
```json
{
  "id": "Nested_CSG_Result",
  "type": "IfcBooleanResult",
  "operator": "DIFFERENCE",
  "first_operand": "Base_Solid_Extrude",
  "second_operand": "Cut_Hole_01_Extrude"
}
```
> *(注：C++ 与 Python 引擎已在第 3、5 节完全支持此嵌套解析逻辑，它们通过拓扑依赖拓扑排序或在 `resolvedFeatures` 缓存中递归寻址。)*

---

### 7.2 高级拉伸体（带空腔的任意闭合多边形拉伸）
常用于空心桥墩、管桩、箱梁等具有空心截面（Voids）的拉伸特征。
在 IFC 中，截面采用 **`IfcArbitraryProfileDefWithVoids`** 实体表示，它由一个 `OuterCurve`（外部边界多段线）与一组 `InnerCurves`（内部空洞多段线数组）共同组成。

#### JSON 协议定义
```json
{
  "id": "Hollow_Pier_Profile",
  "type": "IfcArbitraryProfileDefWithVoids",
  "OuterCurve": {
    "type": "IfcPolyline",
    "Points": [[-2000.0, -2000.0], [2000.0, -2000.0], [2000.0, 2000.0], [-2000.0, 2000.0], [-2000.0, -2000.0]]
  },
  "InnerCurves": [
    {
      "type": "IfcPolyline",
      "Points": [[-1000.0, -1000.0], [-1000.0, 1000.0], [1000.0, 1000.0], [1000.0, -1000.0], [-1000.0, -1000.0]]
    }
  ]
}
```

#### C++ 泛化解析逻辑
```cpp
// 在 IfcGeometryParser::ParseProfile 中扩充：
if (type == "IfcArbitraryProfileDefWithVoids") {
    // 1. 构建外环 Polyline 曲线
    auto outerPoints = j.at("OuterCurve").at("Points").get<std::vector<std::vector<double>>>();
    std::vector<Ifc4::IfcCartesianPoint*> outerPts;
    for (const auto& pt : outerPoints) {
        auto p = new Ifc4::IfcCartesianPoint({pt[0], pt[1]});
        model.addEntity(p);
        outerPts.push_back(p);
    }
    auto outerCurve = new Ifc4::IfcPolyline(outerPts);
    model.addEntity(outerCurve);

    // 2. 构建内环（空腔）曲线数组
    std::vector<Ifc4::IfcCurve*> innerCurves;
    for (const auto& innerJ : j.at("InnerCurves")) {
        auto innerPoints = innerJ.at("Points").get<std::vector<std::vector<double>>>();
        std::vector<Ifc4::IfcCartesianPoint*> innerPts;
        for (const auto& pt : innerPoints) {
            auto p = new Ifc4::IfcCartesianPoint({pt[0], pt[1]});
            model.addEntity(p);
            innerPts.push_back(p);
        }
        auto innerCurve = new Ifc4::IfcPolyline(innerPts);
        model.addEntity(innerCurve);
        innerCurves.push_back(innerCurve);
    }

    // 3. 构建空腔截面实体
    auto profile = new Ifc4::IfcArbitraryProfileDefWithVoids(
        Ifc4::IfcProfileTypeEnum::IfcProfileType_AREA,
        boost::none,
        outerCurve,
        innerCurves
    );
    model.addEntity(profile);
    return profile;
}
```

#### Python 泛化解析逻辑
```python
# 在 IfcPythonGeometryParser.parse_profile 中扩充：
if p_type == "IfcArbitraryProfileDefWithVoids":
    # 1. 外环 Polyline
    outer_pts = [model.create_entity("IfcCartesianPoint", Coordinates=tuple(pt)) 
                 for pt in p_json["OuterCurve"]["Points"]]
    outer_curve = model.create_entity("IfcPolyline", Points=outer_pts)
    
    # 2. 内环数组 Polyline
    inner_curves = []
    for inner_j in p_json["InnerCurves"]:
        inner_pts = [model.create_entity("IfcCartesianPoint", Coordinates=tuple(pt)) 
                     for pt in inner_j["Points"]]
        inner_curve = model.create_entity("IfcPolyline", Points=inner_pts)
        inner_curves.append(inner_curve)
        
    return model.create_entity(
        "IfcArbitraryProfileDefWithVoids",
        ProfileType="AREA",
        OuterCurve=outer_curve,
        InnerCurves=inner_curves
    )
```

---

### 7.3 扫掠体特征 (Swept Solid - 沿弯曲路径扫掠)
用于生成圆弧管道、曲线梁桥、曲线电缆槽等沿特定的三维路径（Directrix）扫掠特定截面（SweptArea）得到的几何。
在 IFC 中，最稳健的泛化表达是 **`IfcFixedReferenceSweptAreaSolid`**，它支持沿着任意三维折线或曲线扫掠，且能提供一个恒定的法线参考向量（如 Z 轴）来约束截面姿态，防止扭转。

#### JSON 协议定义
```json
{
  "id": "Swept_Curved_Girder",
  "type": "IfcFixedReferenceSweptAreaSolid",
  "SweptArea": "Profile_Cap_Rectangle",
  "Position": {
    "type": "IfcAxis2Placement3D",
    "Location": [0.0, 0.0, 0.0],
    "Axis": [0.0, 0.0, 1.0],
    "RefDirection": [1.0, 0.0, 0.0]
  },
  "Directrix": {
    "type": "IfcPolyline",
    "Points": [
      [0.0, 0.0, 0.0],
      [10000.0, 1500.0, 100.0],
      [20000.0, 4000.0, 300.0],
      [30000.0, 8000.0, 600.0]
    ]
  },
  "FixedReference": [0.0, 0.0, 1.0]
}
```

#### C++ 泛化解析逻辑
```cpp
// 在 IfcGeometryParser::ParseFeature 中扩充：
if (type == "IfcFixedReferenceSweptAreaSolid") {
    // 1. 获取截面
    std::string sweptAreaId = j.at("SweptArea").get<std::string>();
    Ifc4::IfcProfileDef* profile = profiles.at(sweptAreaId);

    // 2. 基准坐标系
    auto posJson = j.at("Position");
    auto loc = new Ifc4::IfcCartesianPoint(posJson.at("Location").get<std::vector<double>>());
    auto axis = new Ifc4::IfcDirection(posJson.at("Axis").get<std::vector<double>>());
    auto refDir = new Ifc4::IfcDirection(posJson.at("RefDirection").get<std::vector<double>>());
    auto placement = new Ifc4::IfcAxis2Placement3D(loc, axis, refDir);
    model.addEntity(loc); model.addEntity(axis); model.addEntity(refDir); model.addEntity(placement);

    // 3. 构建 Directrix 扫掠路径曲线 (三维 Polyline 表达)
    auto pathPoints = j.at("Directrix").at("Points").get<std::vector<std::vector<double>>>();
    std::vector<Ifc4::IfcCartesianPoint*> pathPts;
    for (const auto& pt : pathPoints) {
        auto p = new Ifc4::IfcCartesianPoint(pt);
        model.addEntity(p);
        pathPts.push_back(p);
    }
    auto directrix = new Ifc4::IfcPolyline(pathPts);
    model.addEntity(directrix);

    // 4. 固定参考姿态方向约束
    auto fixedRef = new Ifc4::IfcDirection(j.at("FixedReference").get<std::vector<double>>());
    model.addEntity(fixedRef);

    // 5. 扫掠体实例化
    auto solid = new Ifc4::IfcFixedReferenceSweptAreaSolid(
        profile,
        placement,
        directrix,
        0.0, // StartParam (默认 0)
        1.0, // EndParam (默认 1)
        fixedRef
    );
    model.addEntity(solid);
    return solid;
}
```

#### Python 泛化解析逻辑
```python
# 在 IfcPythonGeometryParser.parse_feature 中扩充：
if f_type == "IfcFixedReferenceSweptAreaSolid":
    profile = profiles[f_json["SweptArea"]]
    pos = f_json["Position"]
    
    loc = model.create_entity("IfcCartesianPoint", Coordinates=tuple(pos["Location"]))
    axis = model.create_entity("IfcDirection", DirectionRatios=tuple(pos["Axis"]))
    ref_dir = model.create_entity("IfcDirection", DirectionRatios=tuple(pos["RefDirection"]))
    placement = model.create_entity("IfcAxis2Placement3D", Location=loc, Axis=axis, RefDirection=ref_dir)
    
    # 扫掠三维路径
    path_pts = [model.create_entity("IfcCartesianPoint", Coordinates=tuple(pt)) 
                for pt in f_json["Directrix"]["Points"]]
    directrix = model.create_entity("IfcPolyline", Points=path_pts)
    
    fixed_ref = model.create_entity("IfcDirection", DirectionRatios=tuple(f_json["FixedReference"]))
    
    return model.create_entity(
        "IfcFixedReferenceSweptAreaSolid",
        SweptArea=profile,
        Position=placement,
        Directrix=directrix,
        StartParam=0.0,
        EndParam=1.0,
        FixedReference=fixed_ref
    )
```

---

### 7.4 多截面放样体 (Sectioned Spine - 变截面放样特征)
常用于公路桥跨的变截面连续梁、渐变异形桥墩等，是高精度 CAD 建模中**多截面放样 (Loft)** 的直接映射。
在 IFC 中，使用 **`IfcSectionedSpine`** 表达。它需要沿着一条脊线（SpineCurve）定义若干个渐变截面（CrossSections）以及它们各自在脊线上的三维姿态定位（Positions），BIM 软件会自动根据截面间的插值生成平滑过渡的变截面实体。

#### JSON 协议定义
```json
{
  "id": "Tapered_Bridge_Pier",
  "type": "IfcSectionedSpine",
  "SpineCurve": {
    "type": "IfcPolyline",
    "Points": [
      [0.0, 0.0, 0.0],
      [0.0, 0.0, 6000.0]
    ]
  },
  "CrossSections": [
    "Profile_Pier_Bottom",
    "Profile_Pier_Top"
  ],
  "Positions": [
    {
      "type": "IfcAxis2Placement3D",
      "Location": [0.0, 0.0, 0.0],
      "Axis": [0.0, 0.0, 1.0],
      "RefDirection": [1.0, 0.0, 0.0]
    },
    {
      "type": "IfcAxis2Placement3D",
      "Location": [0.0, 0.0, 6000.0],
      "Axis": [0.0, 0.0, 1.0],
      "RefDirection": [1.0, 0.0, 0.0]
    }
  ]
}
```

#### C++ 泛化解析逻辑
```cpp
// 在 IfcGeometryParser::ParseFeature 中扩充：
if (type == "IfcSectionedSpine") {
    // 1. 构建 SpineCurve (脊线折线)
    auto spinePoints = j.at("SpineCurve").at("Points").get<std::vector<std::vector<double>>>();
    std::vector<Ifc4::IfcCartesianPoint*> spinePts;
    for (const auto& pt : spinePoints) {
        auto p = new Ifc4::IfcCartesianPoint(pt);
        model.addEntity(p);
        spinePts.push_back(p);
    }
    auto spineCurve = new Ifc4::IfcPolyline(spinePts);
    model.addEntity(spineCurve);

    // 2. 解析截面指针数组 (注意：多截面放样的各截面拓扑结构通常需保持一致，如均为圆形或均为4边形)
    std::vector<Ifc4::IfcProfileDef*> crossSections;
    for (const std::string& csId : j.at("CrossSections").get<std::vector<std::string>>()) {
        crossSections.push_back(profiles.at(csId));
    }

    // 3. 构建每个截面对应的三维定位 Placement 数组
    std::vector<Ifc4::IfcAxis2Placement3D*> positions;
    for (const auto& posJson : j.at("Positions")) {
        auto loc = new Ifc4::IfcCartesianPoint(posJson.at("Location").get<std::vector<double>>());
        auto axis = new Ifc4::IfcDirection(posJson.at("Axis").get<std::vector<double>>());
        auto refDir = new Ifc4::IfcDirection(posJson.at("RefDirection").get<std::vector<double>>());
        auto placement = new Ifc4::IfcAxis2Placement3D(loc, axis, refDir);
        
        model.addEntity(loc); model.addEntity(axis); model.addEntity(refDir); model.addEntity(placement);
        positions.push_back(placement);
    }

    // 4. 创建 SectionedSpine 放样实体
    auto spine = new Ifc4::IfcSectionedSpine(
        spineCurve,
        crossSections,
        positions
    );
    model.addEntity(spine);
    return spine;
}
```

#### Python 泛化解析逻辑
```python
# 在 IfcPythonGeometryParser.parse_feature 中扩充：
if f_type == "IfcSectionedSpine":
    # 1. 脊线
    spine_pts = [model.create_entity("IfcCartesianPoint", Coordinates=tuple(pt)) 
                 for pt in f_json["SpineCurve"]["Points"]]
    spine_curve = model.create_entity("IfcPolyline", Points=spine_pts)
    
    # 2. 截面引用解析
    cross_sections = [profiles[cs_id] for cs_id in f_json["CrossSections"]]
    
    # 3. 各放样截面的定位 Placement
    positions = []
    for pos in f_json["Positions"]:
        loc = model.create_entity("IfcCartesianPoint", Coordinates=tuple(pos["Location"]))
        axis = model.create_entity("IfcDirection", DirectionRatios=tuple(pos["Axis"]))
        ref_dir = model.create_entity("IfcDirection", DirectionRatios=tuple(pos["RefDirection"]))
        placement = model.create_entity("IfcAxis2Placement3D", Location=loc, Axis=axis, RefDirection=ref_dir)
        positions.append(placement)
        
    return model.create_entity(
        "IfcSectionedSpine",
        SpineCurve=spine_curve,
        CrossSections=cross_sections,
        Positions=positions
    )
```

---

## 8. 协议兼容性矩阵与 C++ 动态分发器设计

为了保证 CIPR 协议解析的鲁棒性，以下为该协议确立的**类型兼容性黑白名单矩阵**，并展示如何在 C++ 端使用**分发器模式（Dispatcher Pattern）**维护此支持列表。

### 8.1 截面二维定义兼容性矩阵 (Declarations - Profiles)
| 协议类型名称 (`type`) | 对应标准 IFC 实体 | 二维几何图形物理含义 | 引擎支持状态 |
| :--- | :--- | :--- | :---: |
| `IfcCircleProfileDef` | 圆形截面定义 | 指定半径 `Radius` 的参数化圆形 | **已支持** |
| `IfcRectangleProfileDef` | 矩形截面定义 | 指定长宽 `XDim/YDim` 的参数化矩形 | **已支持** |
| `IfcArbitraryClosedProfileDef`| 任意多边形闭合截面| 使用 `IfcPolyline` 多段线闭合围成的任意多边形 | **已支持** |
| `IfcArbitraryProfileDefWithVoids`| 带空腔任意闭合截面 | 一个外圈多段线 + 多个代表孔洞的内圈多段线数组 | **已支持** |

### 8.2 三维造型特征兼容性矩阵 (Features)
| 协议类型名称 (`type`) | 对应标准 IFC 实体 | 三维 CAD 特征算子物理含义 | 引擎支持状态 |
| :--- | :--- | :--- | :---: |
| `IfcExtrudedAreaSolid` | 拉伸体实体 | 将截面沿着指定三维方向拉伸指定深度 `Depth` | **已支持** |
| `IfcBooleanResult` | 构造实体布尔结果 | 两个几何进行并集、差集、交集 (`UNION/DIFFERENCE/INTERSECTION`) | **已支持** |
| `IfcMappedItem` | 映射复用图元 | **几何实例化复用块**。利用平移操作符多次实例化共享源几何 | **已支持** |
| `IfcFixedReferenceSweptAreaSolid` | 固定姿态路径扫掠体 | 将截面沿三维路径折线扫掠，同时通过参考向量约束截面法线 | **已支持** |
| `IfcSectionedSpine` | 变截面放样体 | 沿着脊线定位若干截面，BIM 引擎在其间平滑放样插值生成变截面体 | **已支持** |
| `IfcSphere` | 球体 (CSG Primitive) | 三维标准球体特征，通过球心 Placement 与 `Radius` 定义 | **已支持** |
| `IfcRightCircularCylinder` | 直圆柱 (CSG Primitive) | 直圆柱几何体，通过 Placement、高度 `Height` 与半径 `Radius` 定义 | **已支持** |
| `IfcBlock` | 长方体/立方体 (CSG) | 轴对齐直角六面体，由基准点定位与 `XLength/YLength/ZLength` 定义 | **已支持** |
| `IfcRightCircularCone` | 直圆锥 (CSG Primitive) | 直圆锥几何体，通过 Placement、高度 `Height` 与底面半径 `Radius` 定义 | **已支持** |
| `IfcRevolvedAreaSolid` | 旋转体实体 | 将截面绕空间某条旋转轴旋转指定弧度角 | *待扩展 (下一阶段)*|

---

### 8.3 C++ 动态分发器注册表实现 (Dispatcher Pattern Whitelist)
在 C++ 解析层，通过构建一个类型分发注册表单例，将解析引擎退化为完全松耦合的插件式架构。这可免去冗长易碎的 `if-else` 条件判断，并实现强类型校验拦截。

```cpp
#include <nlohmann/json.hpp>
#include <ifcparse/Ifc4.h>
#include <map>
#include <string>
#include <functional>
#include <stdexcept>

using json = nlohmann::json;

// 1. 定义泛化几何特征解析函数的标准函数指针签名
using FeatureParserFunc = std::function<Ifc4::IfcRepresentationItem*(
    Ifc4::IfcModel&, 
    const json&, 
    const std::map<std::string, Ifc4::IfcProfileDef*>&, 
    std::map<std::string, Ifc4::IfcRepresentationItem*>&,
    std::map<std::string, Ifc4::IfcRepresentationMap*>&
)>;

class IfcGeometryParserRegistry {
private:
    std::map<std::string, FeatureParserFunc> dispatcher;

    // 2. 私有构造函数，在此统一注册受支持的 Whitelist 类型列表
    IfcGeometryParserRegistry() {
        // 在这里显式维护支持 of Type 解析路由
        dispatcher["IfcExtrudedAreaSolid"] = [](Ifc4::IfcModel& m, const json& j, const auto& p, auto& r, auto& s) {
            return IfcGeometryParser::ParseExtrudedSolid(m, j, p, r); 
        };
        
        dispatcher["IfcBooleanResult"] = [](Ifc4::IfcModel& m, const json& j, const auto& p, auto& r, auto& s) {
            return IfcGeometryParser::ParseBooleanResult(m, j, r); 
        };
        
        dispatcher["IfcMappedItem"] = [](Ifc4::IfcModel& m, const json& j, const auto& p, auto& r, auto& s) {
            return IfcGeometryParser::ParseMappedItem(m, j, r, s); 
        };

        dispatcher["IfcFixedReferenceSweptAreaSolid"] = [](Ifc4::IfcModel& m, const json& j, const auto& p, auto& r, auto& s) {
            return IfcGeometryParser::ParseFixedSweptSolid(m, j, p); 
        };

        dispatcher["IfcSectionedSpine"] = [](Ifc4::IfcModel& m, const json& j, const auto& p, auto& r, auto& s) {
            return IfcGeometryParser::ParseSectionedSpine(m, j, p); 
        };

        // 注册 3D 简单几何基本体
        dispatcher["IfcSphere"] = [](Ifc4::IfcModel& m, const json& j, const auto& p, auto& r, auto& s) {
            return IfcGeometryParser::ParseSphere(m, j); 
        };

        dispatcher["IfcRightCircularCylinder"] = [](Ifc4::IfcModel& m, const json& j, const auto& p, auto& r, auto& s) {
            return IfcGeometryParser::ParseRightCircularCylinder(m, j); 
        };

        dispatcher["IfcBlock"] = [](Ifc4::IfcModel& m, const json& j, const auto& p, auto& r, auto& s) {
            return IfcGeometryParser::ParseBlock(m, j); 
        };

        dispatcher["IfcRightCircularCone"] = [](Ifc4::IfcModel& m, const json& j, const auto& p, auto& r, auto& s) {
            return IfcGeometryParser::ParseRightCircularCone(m, j); 
        };
    }

public:
    // 单例模式，全局访问入口
    static IfcGeometryParserRegistry& Instance() {
        static IfcGeometryParserRegistry instance;
        return instance;
    }

    // 核心判定：某个 type 是否受当前解析引擎支持
    bool IsSupported(const std::string& type) const {
        return dispatcher.find(type) != dispatcher.end();
    }

    // 全局泛化解析分发入口
    Ifc4::IfcRepresentationItem* Parse(
        const std::string& type,
        Ifc4::IfcModel& model,
        const json& j,
        const std::map<std::string, Ifc4::IfcProfileDef*>& profiles,
        std::map<std::string, Ifc4::IfcRepresentationItem*>& resolvedFeatures,
        std::map<std::string, Ifc4::IfcRepresentationMap*>& sharedRepMaps) 
    {
        if (!IsSupported(type)) {
            // 前置拦截：输出极富指导意义的友好错误日志，而不是引起程序底层指针越界崩溃
            throw std::runtime_error("Unsupported geometric feature type in CIPR protocol: " + type + 
                                     ". Please ensure server side exporter configuration matches C++ Parser Matrix.");
        }
        // 从分发器注册表中动态路由对应的解析函数
        return dispatcher.at(type)(model, j, profiles, resolvedFeatures, sharedRepMaps);
    }
};
```

---

## 9. 常用 3D 简单几何基本体支持 (CSG Primitives - 球、圆柱、长方体、圆锥)

除了通过二维草图拉伸或扫掠生成三维实体之外，IFC 的 CSG 体系还直接支持原生三维基本图元（CSG Primitives）。使用这些基本几何体能显著减少建模的几何节点，提升运算速度，且极其利于在 BIM 工具中进行二次点对点参数化尺寸修改。

### 9.1 球体 (IfcSphere)
定义在局部 placement 原点处的标准球体。

#### JSON 协议定义
```json
{
  "id": "Standard_Sphere",
  "type": "IfcSphere",
  "Position": {
    "type": "IfcAxis2Placement3D",
    "Location": [0.0, 0.0, 1000.0],
    "Axis": [0.0, 0.0, 1.0],
    "RefDirection": [1.0, 0.0, 0.0]
  },
  "Radius": 800.0
}
```

#### C++ 泛化解析逻辑
```cpp
// 在 IfcGeometryParser 类中增加：
static Ifc4::IfcSphere* ParseSphere(Ifc4::IfcModel& model, const json& j) {
    auto posJson = j.at("Position");
    auto loc = new Ifc4::IfcCartesianPoint(posJson.at("Location").get<std::vector<double>>());
    auto axis = new Ifc4::IfcDirection(posJson.at("Axis").get<std::vector<double>>());
    auto refDir = new Ifc4::IfcDirection(posJson.at("RefDirection").get<std::vector<double>>());
    auto placement = new Ifc4::IfcAxis2Placement3D(loc, axis, refDir);

    model.addEntity(loc); model.addEntity(axis); model.addEntity(refDir); model.addEntity(placement);

    double radius = j.at("Radius").get<double>();
    auto sphere = new Ifc4::IfcSphere(placement, radius);
    model.addEntity(sphere);
    return sphere;
}
```

#### Python 泛化解析逻辑
```python
# 在 IfcPythonGeometryParser 类中增加：
@staticmethod
def parse_sphere(model: ifcopenshell.file, f_json: dict):
    pos = f_json["Position"]
    placement = model.create_entity(
        "IfcAxis2Placement3D",
        Location=model.create_entity("IfcCartesianPoint", Coordinates=tuple(pos["Location"])),
        Axis=model.create_entity("IfcDirection", DirectionRatios=tuple(pos["Axis"])),
        RefDirection=model.create_entity("IfcDirection", DirectionRatios=tuple(pos["RefDirection"]))
    )
    return model.create_entity(
        "IfcSphere",
        Position=placement,
        Radius=f_json["Radius"]
    )
```

---

### 9.2 直圆柱体 (IfcRightCircularCylinder)
定义在局部 placement 原点，沿 Z 轴正方向拉伸的直圆柱。

#### JSON 协议定义
```json
{
  "id": "Standard_Cylinder",
  "type": "IfcRightCircularCylinder",
  "Position": {
    "type": "IfcAxis2Placement3D",
    "Location": [0.0, 0.0, 0.0],
    "Axis": [0.0, 0.0, 1.0],
    "RefDirection": [1.0, 0.0, 0.0]
  },
  "Height": 3000.0,
  "Radius": 500.0
}
```

#### C++ 泛化解析逻辑
```cpp
static Ifc4::IfcRightCircularCylinder* ParseRightCircularCylinder(Ifc4::IfcModel& model, const json& j) {
    auto posJson = j.at("Position");
    auto loc = new Ifc4::IfcCartesianPoint(posJson.at("Location").get<std::vector<double>>());
    auto axis = new Ifc4::IfcDirection(posJson.at("Axis").get<std::vector<double>>());
    auto refDir = new Ifc4::IfcDirection(posJson.at("RefDirection").get<std::vector<double>>());
    auto placement = new Ifc4::IfcAxis2Placement3D(loc, axis, refDir);

    model.addEntity(loc); model.addEntity(axis); model.addEntity(refDir); model.addEntity(placement);

    double height = j.at("Height").get<double>();
    double radius = j.at("Radius").get<double>();
    
    auto cylinder = new Ifc4::IfcRightCircularCylinder(placement, height, radius);
    model.addEntity(cylinder);
    return cylinder;
}
```

#### Python 泛化解析逻辑
```python
@staticmethod
def parse_cylinder(model: ifcopenshell.file, f_json: dict):
    pos = f_json["Position"]
    placement = model.create_entity(
        "IfcAxis2Placement3D",
        Location=model.create_entity("IfcCartesianPoint", Coordinates=tuple(pos["Location"])),
        Axis=model.create_entity("IfcDirection", DirectionRatios=tuple(pos["Axis"])),
        RefDirection=model.create_entity("IfcDirection", DirectionRatios=tuple(pos["RefDirection"]))
    )
    return model.create_entity(
        "IfcRightCircularCylinder",
        Position=placement,
        Height=f_json["Height"],
        Radius=f_json["Radius"]
    )
```

---

### 9.3 长方体/立方体 (IfcBlock)
一个三维轴对齐（或局部放置姿态对齐）的直角六面体。`Location` 对应其左下角的一个角点坐标，并指定了 X、Y、Z 轴方向的绝对延伸尺寸。

#### JSON 协议定义
```json
{
  "id": "Standard_Block",
  "type": "IfcBlock",
  "Position": {
    "type": "IfcAxis2Placement3D",
    "Location": [-1000.0, -1000.0, 0.0],
    "Axis": [0.0, 0.0, 1.0],
    "RefDirection": [1.0, 0.0, 0.0]
  },
  "XLength": 2000.0,
  "YLength": 2000.0,
  "ZLength": 1500.0
}
```

#### C++ 泛化解析逻辑
```cpp
static Ifc4::IfcBlock* ParseBlock(Ifc4::IfcModel& model, const json& j) {
    auto posJson = j.at("Position");
    auto loc = new Ifc4::IfcCartesianPoint(posJson.at("Location").get<std::vector<double>>());
    auto axis = new Ifc4::IfcDirection(posJson.at("Axis").get<std::vector<double>>());
    auto refDir = new Ifc4::IfcDirection(posJson.at("RefDirection").get<std::vector<double>>());
    auto placement = new Ifc4::IfcAxis2Placement3D(loc, axis, refDir);

    model.addEntity(loc); model.addEntity(axis); model.addEntity(refDir); model.addEntity(placement);

    double xLen = j.at("XLength").get<double>();
    double yLen = j.at("YLength").get<double>();
    double zLen = j.at("ZLength").get<double>();

    auto block = new Ifc4::IfcBlock(placement, xLen, yLen, zLen);
    model.addEntity(block);
    return block;
}
```

#### Python 泛化解析逻辑
```python
@staticmethod
def parse_block(model: ifcopenshell.file, f_json: dict):
    pos = f_json["Position"]
    placement = model.create_entity(
        "IfcAxis2Placement3D",
        Location=model.create_entity("IfcCartesianPoint", Coordinates=tuple(pos["Location"])),
        Axis=model.create_entity("IfcDirection", DirectionRatios=tuple(pos["Axis"])),
        RefDirection=model.create_entity("IfcDirection", DirectionRatios=tuple(pos["RefDirection"]))
    )
    return model.create_entity(
        "IfcBlock",
        Position=placement,
        XLength=f_json["XLength"],
        YLength=f_json["YLength"],
        ZLength=f_json["ZLength"]
    )
```

---

### 9.4 直圆锥体 (IfcRightCircularCone)
定义在局部 placement 原点，沿 Z 轴正向延伸，底面半径为 `Radius`、顶部收缩为一个定点的圆锥体。

#### JSON 协议定义
```json
{
  "id": "Standard_Cone",
  "type": "IfcRightCircularCone",
  "Position": {
    "type": "IfcAxis2Placement3D",
    "Location": [0.0, 0.0, 0.0],
    "Axis": [0.0, 0.0, 1.0],
    "RefDirection": [1.0, 0.0, 0.0]
  },
  "Height": 2500.0,
  "Radius": 600.0
}
```

#### C++ 泛化解析逻辑
```cpp
static Ifc4::IfcRightCircularCone* ParseRightCircularCone(Ifc4::IfcModel& model, const json& j) {
    auto posJson = j.at("Position");
    auto loc = new Ifc4::IfcCartesianPoint(posJson.at("Location").get<std::vector<double>>());
    auto axis = new Ifc4::IfcDirection(posJson.at("Axis").get<std::vector<double>>());
    auto refDir = new Ifc4::IfcDirection(posJson.at("RefDirection").get<std::vector<double>>());
    auto placement = new Ifc4::IfcAxis2Placement3D(loc, axis, refDir);

    model.addEntity(loc); model.addEntity(axis); model.addEntity(refDir); model.addEntity(placement);

    double height = j.at("Height").get<double>();
    double radius = j.at("Radius").get<double>();

    auto cone = new Ifc4::IfcRightCircularCone(placement, height, radius);
    model.addEntity(cone);
    return cone;
}
```

#### Python 泛化解析逻辑
```python
@staticmethod
def parse_cone(model: ifcopenshell.file, f_json: dict):
    pos = f_json["Position"]
    placement = model.create_entity(
        "IfcAxis2Placement3D",
        Location=model.create_entity("IfcCartesianPoint", Coordinates=tuple(pos["Location"])),
        Axis=model.create_entity("IfcDirection", DirectionRatios=tuple(pos["Axis"])),
        RefDirection=model.create_entity("IfcDirection", DirectionRatios=tuple(pos["RefDirection"]))
    )
    return model.create_entity(
        "IfcRightCircularCone",
        Position=placement,
        Height=f_json["Height"],
        Radius=f_json["Radius"]
    )
```

---

### 9.5 特征路由集成更新说明

为了使分发引擎能完全调度上述四类三维简单基本体，在 Python 解析层 `IfcPythonGeometryParser.parse_feature` 中应追加相对应的基本体路由：

```python
# 在 IfcPythonGeometryParser.parse_feature 路由中扩充：
elif f_type == "IfcSphere":
    return IfcPythonGeometryParser.parse_sphere(model, f_json)
elif f_type == "IfcRightCircularCylinder":
    return IfcPythonGeometryParser.parse_cylinder(model, f_json)
elif f_type == "IfcBlock":
    return IfcPythonGeometryParser.parse_block(model, f_json)
elif f_type == "IfcRightCircularCone":
    return IfcPythonGeometryParser.parse_cone(model, f_json)
```

而 C++ 端的 `IfcGeometryParserRegistry` 已经在构造函数中通过 Lambda 表达式直接绑定了 `IfcGeometryParser::ParseSphere` 等静态解析函数，前后台完美打通。

---

## 10. 服务端 CadQuery 参数化无感拦截录制机制 (Server-Side Interceptor)

由于 OCCT 核心及 `TopoDS_Shape` 为历史盲（History-blind）BRep 格式，若要让服务端在执行普通的 CadQuery 建模脚本时**自动、轻量化输出符合本协议的 JSON 步骤**，可以利用 Python 的高动态特性建立**动态代理拦截器 (Monkey-Patching)**。

### 10.1 拦截与解算原理
1. **方法劫持**：在建模脚本开始执行前，动态劫持 `cadquery.Workplane` 的核心建模算子（如 `circle`, `rect`, `extrude`, `box`）。
2. **位置自解算**：当捕捉到拉伸与基本体操作时，直接从当前 Workplane 内部维护的 `Plane` 实例中提取底层的 **OCCT 平面定位矩阵（Origin, xDir, zDir）**，直接转录为 IFC 标准的 `IfcAxis2Placement3D`。
3. **特征链累加**：在内存中将执行步骤累加写入 JSON，建模执行完毕时，自动输出完整的参数化协议数据。

---

### 10.2 拦截器核心模块源码 (`CqParametricTracker`)

```python
import json
import cadquery as cq
from typing import List, Dict, Any

class CqParametricTracker:
    """CadQuery 参数化建模步骤拦截录制器"""
    def __init__(self, model_name: str = "Parametric_Assembly"):
        self.recipe = {
            "protocol_version": "1.2.0",
            "metadata": {
                "generator": "CadQuery-CIPR-AutoTracker v1.2",
                "model_name": model_name
            },
            "parameters": {},
            "declarations": {
                "profiles": []
            },
            "features": [],
            "products": []
        }
        self.profile_counter = 0
        self.feature_counter = 0
        self._original_methods = {}

    def _get_placement_3d(self, plane: cq.Plane) -> Dict[str, Any]:
        """将 CadQuery 当前工作平面（Plane）的三维变换矩阵解算为 IFC placement"""
        origin = plane.origin
        z_dir = plane.zDir  # 局部Z轴（法线）
        x_dir = plane.xDir  # 局部X轴
        
        return {
            "type": "IfcAxis2Placement3D",
            "Location": [float(origin.x), float(origin.y), float(origin.z)],
            "Axis": [float(z_dir.x), float(z_dir.y), float(z_dir.z)],
            "RefDirection": [float(x_dir.x), float(x_dir.y), float(x_dir.z)]
        }

    def start_tracking(self):
        """动态代理注入核心建模方法"""
        methods_to_intercept = ["circle", "rect", "extrude", "box"]
        for name in methods_to_intercept:
            if hasattr(cq.Workplane, name):
                self._original_methods[name] = getattr(cq.Workplane, name)
                setattr(cq.Workplane, name, self._make_proxy(name))

    def stop_tracking(self):
        """恢复 CadQuery 原始环境"""
        for name, orig_method in self._original_methods.items():
            setattr(cq.Workplane, name, orig_method)

    def _make_proxy(self, method_name: str):
        outer_self = self
        orig_method = getattr(cq.Workplane, method_name)

        def proxy_method(self_wp, *args, **kwargs):
            # 先行执行原生的 OCCT CAD 几何生成
            result_wp = orig_method(self_wp, *args, **kwargs)
            try:
                outer_self._intercept_call(method_name, self_wp, result_wp, args, kwargs)
            except Exception as e:
                print(f"[Tracker Warning] Failed to log step {method_name}: {e}")
            return result_wp

        return proxy_method

    def _intercept_call(self, name: str, prev_wp: cq.Workplane, next_wp: cq.Workplane, args: tuple, kwargs: dict):
        """同步拦截并序列化为标准协议格式"""
        if name == "circle":
            radius = args[0] if len(args) > 0 else kwargs.get("radius")
            self.profile_counter += 1
            profile_id = f"Profile_Circle_{self.profile_counter}"
            
            self.recipe["declarations"]["profiles"].append({
                "id": profile_id,
                "type": "IfcCircleProfileDef",
                "ProfileType": "AREA",
                "Radius": float(radius)
            })
            next_wp._current_profile_id = profile_id

        elif name == "rect":
            x_dim = args[0] if len(args) > 0 else kwargs.get("xLen")
            y_dim = args[1] if len(args) > 1 else kwargs.get("yLen")
            self.profile_counter += 1
            profile_id = f"Profile_Rect_{self.profile_counter}"
            
            self.recipe["declarations"]["profiles"].append({
                "id": profile_id,
                "type": "IfcRectangleProfileDef",
                "ProfileType": "AREA",
                "XDim": float(x_dim),
                "YDim": float(y_dim)
            })
            next_wp._current_profile_id = profile_id

        elif name == "extrude":
            depth = args[0] if len(args) > 0 else kwargs.get("until")
            self.feature_counter += 1
            feature_id = f"Feature_Extrude_{self.feature_counter}"
            
            profile_id = getattr(prev_wp, "_current_profile_id", "Unknown_Profile")
            placement = self._get_placement_3d(prev_wp.plane)
            normal = prev_wp.plane.zDir
            
            self.recipe["features"].append({
                "id": feature_id,
                "type": "IfcExtrudedAreaSolid",
                "SweptArea": profile_id,
                "Position": placement,
                "ExtrudedDirection": [float(normal.x), float(normal.y), float(normal.z)],
                "Depth": float(depth)
            })

        elif name == "box":
            x = args[0] if len(args) > 0 else kwargs.get("xLen")
            y = args[1] if len(args) > 1 else kwargs.get("yLen")
            z = args[2] if len(args) > 2 else kwargs.get("zLen")
            
            self.feature_counter += 1
            feature_id = f"Feature_Block_{self.feature_counter}"
            placement = self._get_placement_3d(prev_wp.plane)
            
            self.recipe["features"].append({
                "id": feature_id,
                "type": "IfcBlock",
                "Position": placement,
                "XLength": float(x),
                "YLength": float(y),
                "ZLength": float(z)
            })

    def export_json(self) -> str:
        all_features = [f["id"] for f in self.recipe["features"]]
        self.recipe["products"].append({
            "id": "Product_Group_01",
            "ifc_type": "IfcBuildingElementProxy",
            "name": "Auto_Recorded_Product",
            "representations": all_features
        })
        return json.dumps(self.recipe, indent=2, ensure_ascii=False)
```

---

### 10.3 服务端无感调用示例与验证
以下展示了如何在保留原生 CadQuery 脚本写法的前提下，在服务端自动激活拦截并同步产出轻量化 JSON 的业务流：

```python
# 1. 初始化并激活拦截器
tracker = CqParametricTracker(model_name="Auto_Recorded_Structure")
tracker.start_tracking()

# 2. 正常运行原始建模业务代码（开发人员完全零修改！）
# 绘制长方体底座（承台）
cap = cq.Workplane("XY").rect(4000, 5000).extrude(1500)

# 在底座下方拉伸一个单圆柱桩
pile = cq.Workplane("XY").workplane(offset=-1500).circle(500).extrude(-15000)

# 3. 建模结束，注销拦截器并获取生成的 JSON 协议树
tracker.stop_tracking()
recipe_json_output = tracker.export_json()

# 该 JSON 即可通过 HTTP/gRPC 返回给客户端 C++ 泛化解析器进行零损耗导出。
```

---

### 10.4 脚本风格兼容性分析（链式与分步非链式调用）

本追踪引擎的代理劫持机制，在面对非链式调用（分步使用独立变量接收 `Workplane`）时**完全有效，且表现得同样鲁棒**。

#### 1. 分步非链式调用模拟测试
在 Python 中，以下两种书写风格在字节码和方法调度层面是**完全等价**的：

* **链式风格 (Chained Style)**：
  ```python
  cap = cq.Workplane("XY").rect(4000, 5000).extrude(1500)
  ```
* **分步风格 (Step-by-Step Style)**：
  ```python
  wp1 = cq.Workplane("XY")
  wp2 = wp1.rect(4000, 5000)
  cap = wp2.extrude(1500)
  ```

在**分步风格**中，我们的代理拦截函数：
1. 当 `wp1.rect()` 被调用时，代理将拦截它，计算并产生 `Profile_Rect_1`，然后将其动态挂载到返回的 `wp2` 实例上（即 `wp2._current_profile_id = "Profile_Rect_1"`）。
2. 当 `wp2.extrude()` 被调用时，代理将作为 `self_wp` 接收 `wp2`。代理会读取其上挂载的 `_current_profile_id`，正确解算出拉伸源为 `"Profile_Rect_1"`。
3. 这种基于 Python 对象属性绑定的局部状态维持，完美避开了使用“全局静态变量”造成的并发竞态冲突或覆盖风险。

#### 2. 分支建模与参数复用鲁棒性说明
如果用户编写了“一个草图，多次复用拉伸”的复杂分支脚本：
```python
sketch_wp = cq.Workplane("XY").circle(500)  # 生成 Profile_Circle_1 并绑定到 sketch_wp
solid_long = sketch_wp.extrude(10000)        # 第一次拉伸，正确索引 Profile_Circle_1
solid_short = sketch_wp.extrude(2000)        # 第二次拉伸，依旧正确索引 Profile_Circle_1
```
由于 `sketch_wp` 的内部属性 `_current_profile_id` 并没有因为第一次拉伸被销毁，这使得后续所有的分步拉伸操作，均能精准且安全地在协议中表达为对同一个 Profile ID 的引用，这完美贴合了参数化 BIM 的高精度复用语义。

---

## 11. 双通道“双生”协同传输方案：TopoShape与参数化建模步骤统一交付

在典型的 CAD/BIM 混合渲染与导出流程中，客户端 C++（QtOCCTApp）通常面临双重需求：
1. **即时无缝渲染 (Instant Viewport Rendering)**：需要在 3D 视口中立刻用 `AIS_InteractiveContext` 展示极致精细的网格与曲面。这直接依赖于 OCCT BRep 格式的 **`TopoDS_Shape`**。
2. **轻量语义导出 (BIM Parametric Export)**：需要在保存 IFC 时导出轻量、带有工程语义和可二次编辑特征的 `IfcExtrudedAreaSolid`。这直接依赖于 **`CIPR JSON Recipe`**。

为了实现这两种异构数据的同步高速交付，我们设计了 **`CIPR-PKG (双生协同传输包)`** 方案。

### 11.1 双通道传输架构图
```
                  [ 服务端 Python (CadQuery) ]
                               |
               +---------------+---------------+
               | 同步计算                       |
               v                               v
      [ TopoDS_Shape 几何结果 ]        [ CIPR 参数化特征树 JSON ]
               |                               |
               | 序列化为 BREP 文本流            |
               +---------------+---------------+
                               |
                               v (封装为统一 CIPR-PKG 数据包)
               [ HTTP RESTful Response / gRPC ]
                               |
                               v (网络传输)
                  [ 客户端 C++ (QtOCCTApp) ]
                               |
               +---------------+---------------+
               | 快速解析分流                   |
               v                               v
    [ BRepTools::Read 加载 ]         [ IfcGeometryParserRegistry 解析 ]
               |                               |
               v                               v
    [ AIS_Shape 3D 视口即时渲染 ]     [ IfcExportService 参数化无损导出 ]
```

---

### 11.2 服务端统一拼装包协议 (CIPR-PKG Schema)
我们将参数化 Recipe 与原始 BRep 二进制/文本流封装到同一个 JSON 包中交付，避免多次网络 I/O 带来的事务不同步问题：

```json
{
  "pkg_version": "1.0.0",
  "recipe": {
    "protocol_version": "1.2.0",
    "metadata": { "model_name": "Instanced_Pile_Group" },
    "declarations": { "profiles": [ /* 截面定义 */ ] },
    "features": [ /* 特征步骤 */ ],
    "products": [ /* 语义构件 */ ]
  },
  "geometries": {
    "raw_brep_format": "ASCII",
    "raw_brep_data": "DBRep_Topology\nDBRep_DrawableShape\n...\n# 原生的 OCCT BRep 纯几何拓扑序列化文本"
  }
}
```

---

### 11.3 服务端 Python 序列化实现
在 Python 服务端计算完毕后，安全导出 `TopoDS_Shape` 为内存文本流，并封装进 `CIPR-PKG`：

```python
import tempfile
import json
import os

def export_cipr_package(cq_shape: cq.Shape, tracker: CqParametricTracker) -> str:
    """同步打包参数化特征与 TopoShape 几何"""
    
    # 1. 安全将 TopoDS_Shape 序列化为标准的 OCCT BREP 文本格式
    # 利用临时文件作为中转，确保各平台下 OCP 内存流读写的稳定性
    fd, temp_path = tempfile.mkstemp(suffix=".brep")
    try:
        cq_shape.exportBrep(temp_path)
        with open(temp_path, "r", encoding="utf-8") as f:
            brep_data = f.read()
    finally:
        os.close(fd)
        os.remove(temp_path)

    # 2. 读取已录制的参数化特征 Recipe
    recipe_dict = tracker.recipe
    
    # 3. 统一拼装为交付数据包
    cipr_package = {
        "pkg_version": "1.0.0",
        "recipe": recipe_dict,
        "geometries": {
            "raw_brep_format": "ASCII",
            "raw_brep_data": brep_data
        }
    }
    
    return json.dumps(cipr_package, ensure_ascii=False)
```

---

### 11.4 客户端 C++ 反序列化与分流处理
客户端 C++ 接收到该 `CIPR-PKG` 字符串包后，分两条通道同时分发数据，直接在内存中建立起 3D 渲染与 BIM 参数化导出的协同：

```cpp
#include <nlohmann/json.hpp>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Shape.hxx>
#include <sstream>
#include <string>
#include <iostream>

using json = nlohmann::json;

class CiprPackageReceiver {
public:
    static void ProcessReceivedPackage(const std::string& pkgJsonStr, Ifc4::IfcModel& ifcModel) {
        // 解析统一交付包
        json pkg = json::parse(pkgJsonStr);
        
        // ==========================================================
        // 通道 1：即时 3D 渲染通道（使用 raw_brep_data 构建 TopoDS_Shape）
        // ==========================================================
        std::string brepData = pkg["geometries"]["raw_brep_data"].get<std::string>();
        
        std::stringstream ss(brepData);
        TopoDS_Shape visualShape;
        BRep_Builder builder;
        
        // 核心技术：直接从内存文本流中还原 OCCT 拓扑实体，性能极高，免去磁盘 I/O
        Standard_Boolean readResult = BRepTools::Read(visualShape, ss, builder);
        if (readResult) {
            std::cout << "[Channel 1] TopoDS_Shape deserialized successfully. Ready for AIS rendering." << std::endl;
            // 此时可以直接将其送入 QtOCCT 窗口进行渲染：
            // Handle(AIS_Shape) aisShape = new AIS_Shape(visualShape);
            // myAISContext->Display(aisShape, Standard_True);
        } else {
            std::cerr << "[Channel 1 Error] Failed to read BRep shape from package stream." << std::endl;
        }

        // ==========================================================
        // 通道 2：轻量参数化 IFC 导出通道（使用 recipe 解析生成无损 BIM）
        // ==========================================================
        json recipeJson = pkg["recipe"];
        std::cout << "[Channel 2] Beginning semantic IFC reconstruction." << std::endl;
        
        try {
            // 直接调用在 Section 4 实现的泛化语义装配层，高保真生成 IfcExtrudedAreaSolid 等高阶特征
            IfcSemanticAssembler::BuildBimModel(ifcModel, recipeJson);
            std::cout << "[Channel 2] Semantic IFC reconstructed successfully." << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[Channel 2 Error] Failed to build IFC semantic features: " << e.what() << std::endl;
        }
    }
};
```
