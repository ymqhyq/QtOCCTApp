#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

// 包含生成的 MDA 类
#include "generated/DataModel.h"
#include "generated/DataFactory.h"
#include "generated/BrNode_adObject.h"
#include "generated/BrNode_adPropertySet.h"
#include "generated/BrNode_adProperty.h"
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <math.h>

// Active Data 基础包含
// #include <ActData_BinBinaryWriter.h> // 移除不正确的包含

using json = nlohmann::json;

// 辅助：将 TCollection_ExtendedString 安全转为控制台可输出的 std::string
static std::string ToStr(const TCollection_ExtendedString& es) {
    TCollection_AsciiString as(es);
    return std::string(as.ToCString());
}

/**
 * @brief 递归打印装配树（含属性集和属性值）
 * @param node  当前节点
 * @param depth 缩进层级
 */
void PrintAssemblyTree(const Handle(BrNode_adObject)& node, int depth = 0)
{
    if (node.IsNull()) return;

    std::string indent(depth * 2, ' ');
    std::string type = ToStr(node->GetObjectType());
    std::string name = ToStr(node->GetName());
    std::string gid  = ToStr(node->GetGlobalID());

    // 节点行
    std::cout << indent << "[" << type << "] " << name;
    if (!gid.empty()) std::cout << "  (ID: " << gid << ")";
    std::cout << std::endl;

    // 属性集
    NCollection_Sequence<Handle(BrNode_adPropertySet)> psets = node->GetPropertySetsList();
    for (int i = 1; i <= psets.Length(); ++i) {
        Handle(BrNode_adPropertySet) ps = psets.Value(i);
        if (ps.IsNull()) continue;
        std::cout << indent << "  {" << ToStr(ps->GetName()) << "}" << std::endl;

        NCollection_Sequence<Handle(BrNode_adProperty)> props = ps->GetPropertiesList();
        for (int j = 1; j <= props.Length(); ++j) {
            Handle(BrNode_adProperty) p = props.Value(j);
            if (p.IsNull()) continue;
            std::cout << indent << "    - " << ToStr(p->GetPropertyName())
                      << " = " << ToStr(p->GetPropertyValue())
                      << "  [" << ToStr(p->GetValueType()) << "]" << std::endl;
        }
    }

    // 递归子对象
    NCollection_Sequence<Handle(BrNode_adObject)> children = node->GetSubObjectsList();
    for (int i = 1; i <= children.Length(); ++i) {
        PrintAssemblyTree(children.Value(i), depth + 1);
    }
}

/**
 * @brief 递归解析 JSON 并构建 adObject 树
 */
Handle(BrNode_adObject) ProcessJsonObject(const Handle(DataModel)& model, const json& objJson) {
    std::cout << "Entering ProcessJsonObject..." << std::endl;
    TCollection_AsciiString typeName = objJson.value("ObjectType", "Component").c_str();
    
    std::cout << "  [ProcessJsonObject] Calling DataFactory::CreateObject..." << std::endl;
    Handle(ActAPI_INode) baseNode = DataFactory::CreateObject(model, typeName.ToCString());
    std::cout << "  [ProcessJsonObject] CreateObject returned. Checking if null..." << std::endl;
    if (baseNode.IsNull()) {
        std::cerr << "  [ProcessJsonObject] ERROR: baseNode is NULL!" << std::endl;
        return nullptr;
    }
    
    std::cout << "  [ProcessJsonObject] Performing DownCast to BrNode_adObject..." << std::endl;
    Handle(BrNode_adObject) adObj = Handle(BrNode_adObject)::DownCast(baseNode);
    std::cout << "  [ProcessJsonObject] DownCast completed." << std::endl;
    
    if (adObj.IsNull()) {
        std::cerr << "Failed to create adObject of type: " << typeName.ToCString() << std::endl;
        return nullptr;
    }
    if (objJson.contains("Name")) {
        std::cout << "  [ProcessJsonObject] About to set Name..." << std::endl;
        std::string n = objJson["Name"].get<std::string>();
        std::cout << "  [ProcessJsonObject] Name value: " << n << std::endl;
        adObj->SetName(n.c_str());
        std::cout << "  [ProcessJsonObject] SetName completed." << std::endl;
    }
    
    if (objJson.contains("GlobalID")) {
        std::cout << "  [ProcessJsonObject] About to set GlobalID..." << std::endl;
        std::string gid = objJson["GlobalID"].get<std::string>();
        std::cout << "  [ProcessJsonObject] GlobalID value: " << gid << std::endl;
        adObj->SetGlobalID(TCollection_ExtendedString(gid.c_str()));
        std::cout << "  [ProcessJsonObject] SetGlobalID completed." << std::endl;
    }
    if (objJson.contains("StructuralType")) adObj->SetStructuralType(objJson["StructuralType"].get<std::string>().c_str());

    // 3. 更新属性集中的值 (Key-Value 模式)
    std::cout << "  [ProcessJsonObject] Checking for PropertySets..." << std::endl;
    if (objJson.contains("PropertySets")) {
        std::cout << "  [ProcessJsonObject] Processing PropertySets..." << std::endl;
        const auto& psetsJson = objJson["PropertySets"];
        for (auto it = psetsJson.begin(); it != psetsJson.end(); ++it) {
            TCollection_AsciiString psetName = it.key().c_str();
            std::cout << "    [ProcessJsonObject] Pset: " << psetName.ToCString() << std::endl;
            const auto& propsJson = it.value();

            // 在对象中查找现有的属性集（由工厂预设）
            Handle(BrNode_adPropertySet) targetPset;
            std::cout << "    [ProcessJsonObject] Getting existing psets list..." << std::endl;
            NCollection_Sequence<Handle(BrNode_adPropertySet)> existingSets = adObj->GetPropertySetsList();
            std::cout << "    [ProcessJsonObject] Found " << existingSets.Length() << " existing sets." << std::endl;
            
            for (int i = 1; i <= existingSets.Length(); ++i) {
                if (existingSets.Value(i)->GetName() == psetName) {
                    targetPset = existingSets.Value(i);
                    break;
                }
            }

            // 如果不存在，则创建一个新的
            if (targetPset.IsNull()) {
                std::cout << "    [ProcessJsonObject] targetPset is NULL, creating new one..." << std::endl;
                targetPset = DataFactory::CreatePset(model, psetName);
                if (!targetPset.IsNull()) {
                    std::cout << "    [ProcessJsonObject] Adding new pset to object..." << std::endl;
                    adObj->AddPropertySets(targetPset);
                } else {
                    std::cerr << "    [ProcessJsonObject] ERROR: Failed to create pset " << psetName.ToCString() << std::endl;
                }
            }

            // 更新具体的属性键值
            if (!targetPset.IsNull()) {
                std::cout << "    [ProcessJsonObject] Updating property values..." << std::endl;
                for (auto pIt = propsJson.begin(); pIt != propsJson.end(); ++pIt) {
                    TCollection_AsciiString propKey = pIt.key().c_str();
                    std::string propVal;
                    if (pIt.value().is_number()) propVal = std::to_string(pIt.value().get<double>());
                    else propVal = pIt.value().get<std::string>();

                    std::cout << "      [Prop] " << propKey.ToCString() << " = " << propVal << std::endl;

                    // 在属性集中查找并更新属性
                    bool found = false;
                    NCollection_Sequence<Handle(BrNode_adProperty)> props = targetPset->GetPropertiesList();
                    for (int j = 1; j <= props.Length(); ++j) {
                        if (props.Value(j)->GetPropertyName() == propKey) {
                            props.Value(j)->SetPropertyValue(propVal.c_str());
                            found = true;
                            break;
                        }
                    }

                    // 如果没找到，则创建新属性并添加
                    if (!found) {
                        std::cout << "      [ProcessJsonObject] Creating new property: " << propKey.ToCString() << std::endl;
                        Handle(BrNode_adProperty) newProp = model->AddadProperty();
                        newProp->SetPropertyName(propKey.ToCString());
                        newProp->SetPropertyValue(propVal.c_str());
                        targetPset->AddProperties(newProp);
                    }
                }
            }
        }
    }

    // 3.5 设置 ObjectPlacement
    if (objJson.contains("ObjectPlacement")) {
        auto placementJson = objJson["ObjectPlacement"];
        if (placementJson.is_array() && placementJson.size() >= 3) {
            int nParams = (int)placementJson.size();
            if (nParams > 6) nParams = 6;

            auto p = adObj->Parameter(BrNode_adObject::PID_ObjectPlacement);
            auto typedP = ActData_ParameterFactory::AsRealArray(p);
            if (!typedP.IsNull()) {
                // 动态分配长度 (0 到 nParams-1)
                Handle(TColStd_HArray1OfReal) tmp = new TColStd_HArray1OfReal(0, nParams - 1);
                typedP->SetArray(tmp);
                
                for (int i = 0; i < nParams; ++i) {
                    typedP->SetElement(i, placementJson[i].get<double>());
                }
                std::cout << "  [ProcessJsonObject] Set ObjectPlacement (" << nParams << " params)" << std::endl;
            }
        }
    }

    // 4. 处理子对象
    if (objJson.contains("children")) {
        std::cout << "  [ProcessJsonObject] Processing " << objJson["children"].size() << " children..." << std::endl;
        for (const auto& childJson : objJson["children"]) {
            Handle(BrNode_adObject) childObj = ProcessJsonObject(model, childJson);
            if (!childObj.IsNull()) {
                std::cout << "  [ProcessJsonObject] Adding child to " << TCollection_AsciiString(adObj->GetName()).ToCString() << "..." << std::endl;
                adObj->AddSubObjects(childObj);
            }
        }
    }

    return adObj;
}

// 递归遍历并构建几何
#include "GeometryService.h"
void BuildAllGeometry(const Handle(DataModel)& model, const Handle(BrNode_adObject)& node, GeometryService& geoService) {
    if (node.IsNull()) return;
    Handle(BrNode_adGeometricDef) geoDef = geoService.BuildGeometry(node);

    if (!geoDef.IsNull()) {
        // 创建 adGeometry 并关联
        Handle(BrNode_adGeometry) geom = model->AddadGeometry();
        geom->SetName(node->GetName());
        geom->SetGeometryRef(geoDef);
        node->SetGeometry(geom);
    }
    
    // 递归子节点
    NCollection_Sequence<Handle(BrNode_adObject)> children = node->GetSubObjectsList();
    for (int i = 1; i <= children.Length(); ++i) {
        BuildAllGeometry(model, children.Value(i), geoService);
    }
}

int main(int argc, char** argv) {
    // 1. 创建模型环境
    Handle(DataModel) model = new DataModel();
    model->NewEmpty();
    
    // VERY IMPORTANT: Enable OCAF transaction engine
    if (!model->Document().IsNull()) {
        model->Document()->SetUndoLimit(10);
    }

    // 2. 读取 JSON 文件
    std::ifstream f("bridge_data.json");
    if (!f.is_open()) {
        std::cerr << "Cannot find bridge_data.json" << std::endl;
        return 1;
    }
    json data = json::parse(f);

    // 3. 从根对象开始构建
    std::cout << "Starting to build assembly tree..." << std::endl;
    
    // OCAF Requires a transaction (command) to be open before adding attributes!
    model->OpenCommand();
    
    try {
        std::cout << "--- RTTI DEBUG ---" << std::endl;
        Handle(BrNode_adObject) testObj = BrNode_adObject::Instance();
        std::cout << "testObj created." << std::endl;
        std::cout << "testObj dynamic type: " << testObj->DynamicType()->Name() << std::endl;
        Handle(ActData_BaseNode) testBase = Handle(ActData_BaseNode)::DownCast(testObj);
        std::cout << "testBase is null? " << testBase.IsNull() << std::endl;
        std::cout << "------------------" << std::endl;
        
        Handle(BrNode_adObject) rootBridge = ProcessJsonObject(model, data["bridge"]);

        Handle(BrNode_adModelRoot) modelRoot = Handle(BrNode_adModelRoot)::DownCast(model->GetRootNode());
        if (!modelRoot.IsNull() && !rootBridge.IsNull()) {
            std::cout << "Linking root assembly..." << std::endl;
            modelRoot->AddSubObjects(rootBridge);
        }
        
        // 构建几何
        std::cout << "Building geometry via microservice..." << std::endl;
        GeometryService geoService(model, "http://127.0.0.1:3500/v1.0/invoke/modeling-service/method");
        BuildAllGeometry(model, rootBridge, geoService);
        
        // Commit the entire assembly transaction
        std::cout << "Committing assembly..." << std::endl;
        model->CommitCommand();

        if (!rootBridge.IsNull()) {
            // 4. 打印完整装配树
            std::cout << std::endl;
            std::cout << "========== 桥梁装配树 ==========" << std::endl;
            PrintAssemblyTree(rootBridge);
            std::cout << "================================" << std::endl;
            std::cout << std::endl;

            // 5. 保存模型为 .asi 文件 (Active Data 二进制格式)
            TCollection_AsciiString savePath = "D:/QtOCCTApp/bridge_test_save.asi";
            std::cout << "Saving model to: " << savePath.ToCString() << "..." << std::endl;
            if (model->SaveAs(savePath)) {
                std::cout << "Model saved successfully." << std::endl;
            } else {
                std::cerr << "Failed to save model." << std::endl;
            }
        }
    } catch (Standard_Failure& e) {
        std::cerr << "!!! FATAL OCCT EXCEPTION: " << e.GetMessageString() << std::endl;
        model->AbortCommand();
    } catch (std::exception& e) {
        std::cerr << "!!! FATAL STD EXCEPTION: " << e.what() << std::endl;
        model->AbortCommand();
    } catch (...) {
        std::cerr << "!!! FATAL UNKNOWN EXCEPTION!" << std::endl;
        model->AbortCommand();
    }

    return 0;
}
