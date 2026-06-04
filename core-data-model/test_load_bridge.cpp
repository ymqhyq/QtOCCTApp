#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include <map>
#include <string>

#include "generated/DataModel.h"
#include "generated/DataFactory.h"
#include "generated/BrNode_adObject.h"
#include "generated/BrNode_adPropertySet.h"
#include "generated/BrNode_adProperty.h"
#include "generated/BrNode_adModelRoot.h"
#include "generated/BrNode_adGeometry.h"
#include "generated/BrNode_adGeometricDef.h"

#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <math.h>

#include <ActData_BasePartition.h>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <TDataStd_Name.hxx>
#include <TDataStd_AsciiString.hxx>
#include <TDF_LabelSequence.hxx>

using json = nlohmann::json;

static std::string ToStr(const TCollection_ExtendedString& es) {
    TCollection_AsciiString as(es);
    return std::string(as.ToCString());
}

void PrintGeometryStats(const Handle(ActData_BaseModel)& model) {
    if (model.IsNull()) return;
    std::cout << "[DEBUG] PrintGeometryStats: 1" << std::endl;
    Handle(TDocStd_Document) doc = model->Document();
    if (doc.IsNull()) return;
    std::cout << "[DEBUG] PrintGeometryStats: 2" << std::endl;
    Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
    if (shapeTool.IsNull()) return;
    std::cout << "[DEBUG] PrintGeometryStats: 3" << std::endl;

    TDF_LabelSequence protos;
    shapeTool->GetShapes(protos);
    std::cout << "[DEBUG] PrintGeometryStats: 4, protos.Length = " << protos.Length() << std::endl;

    std::cout << std::endl;
    std::cout << "========== XCAF Geometry Stats ==========" << std::endl;
    std::cout << "Unique Prototype Parts (Shapes): " << protos.Length() << std::endl;
    for (int i = 1; i <= protos.Length(); ++i) {
        std::cout << "[DEBUG] PrintGeometryStats: Loop i = " << i << " (A)" << std::endl;
        TDF_Label protoLabel = protos.Value(i);
        Handle(TDataStd_Name) nameAttr;
        std::string name = "Unnamed";
        if (protoLabel.FindAttribute(TDataStd_Name::GetID(), nameAttr)) {
            name = ToStr(nameAttr->Get());
        }
        std::cout << "[DEBUG] PrintGeometryStats: Loop i = " << i << " (B), name = " << name << std::endl;
        Handle(TDataStd_AsciiString) geoIdAttr;
        std::string md5 = "N/A";
        if (protoLabel.FindAttribute(TDataStd_AsciiString::GetID(), geoIdAttr)) {
            md5 = geoIdAttr->Get().ToCString();
        }
        std::cout << "[DEBUG] PrintGeometryStats: Loop i = " << i << " (C)" << std::endl;
        TDF_LabelSequence users;
        int userCount = XCAFDoc_ShapeTool::GetUsers(protoLabel, users, Standard_True);
        std::cout << "[DEBUG] PrintGeometryStats: Loop i = " << i << " (D), userCount = " << userCount << std::endl;
        std::cout << "  - [Prototype] " << name << " (MD5: " << md5.substr(0,8) << "...)"
                  << "  Instance Count (Users): " << userCount << std::endl;
    }
    std::cout << "=========================================" << std::endl;
}

void PrintAssemblyTree(const Handle(BrNode_adObject)& node, int depth = 0)
{
    if (node.IsNull()) return;

    std::string indent(depth * 2, ' ');
    std::string type = ToStr(node->GetObjectType());
    std::string name = ToStr(node->GetName());
    std::string gid  = ToStr(node->GetGlobalID());

    std::cout << indent << "[" << type << "] " << name;
    if (!gid.empty()) std::cout << "  (ID: " << gid << ")";
    std::cout << std::endl;

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

    NCollection_Sequence<Handle(BrNode_adObject)> children = node->GetSubObjectsList();
    for (int i = 1; i <= children.Length(); ++i) {
        PrintAssemblyTree(children.Value(i), depth + 1);
    }
}

Handle(BrNode_adObject) ProcessJsonObject(const Handle(ActData_BaseModel)& model, const json& objJson) {
    TCollection_AsciiString typeName = objJson.value("ObjectType", "Component").c_str();
    
    Handle(ActAPI_INode) baseNode = DataFactory::CreateObject(model, typeName.ToCString());
    if (baseNode.IsNull()) {
        std::cerr << "  [ProcessJsonObject] ERROR: baseNode is NULL!" << std::endl;
        return nullptr;
    }
    
    Handle(BrNode_adObject) adObj = Handle(BrNode_adObject)::DownCast(baseNode);
    if (adObj.IsNull()) {
        std::cerr << "Failed to create adObject of type: " << typeName.ToCString() << std::endl;
        return nullptr;
    }
    if (objJson.contains("Name")) {
        std::string n = objJson["Name"].get<std::string>();
        adObj->SetName(n.c_str());
    }
    
    if (objJson.contains("GlobalID")) {
        std::string gid = objJson["GlobalID"].get<std::string>();
        adObj->SetGlobalID(TCollection_ExtendedString(gid.c_str()));
    }
    if (objJson.contains("StructuralType")) adObj->SetStructuralType(objJson["StructuralType"].get<std::string>().c_str());

    // --- Handle PropertySets ---
    if (objJson.contains("PropertySets")) {
        const auto& psetsJson = objJson["PropertySets"];
        for (auto it = psetsJson.begin(); it != psetsJson.end(); ++it) {
            TCollection_AsciiString psetName = it.key().c_str();
            const auto& propsJson = it.value();

            Handle(BrNode_adPropertySet) targetPset;
            NCollection_Sequence<Handle(BrNode_adPropertySet)> existingSets = adObj->GetPropertySetsList();
            for (int i = 1; i <= existingSets.Length(); ++i) {
                if (existingSets.Value(i)->GetName() == psetName) {
                    targetPset = existingSets.Value(i);
                    break;
                }
            }

            if (targetPset.IsNull()) {
                targetPset = DataFactory::CreatePset(model, psetName);
                if (!targetPset.IsNull()) {
                    adObj->AddPropertySets(targetPset);
                }
            }

            if (!targetPset.IsNull()) {
                for (auto pIt = propsJson.begin(); pIt != propsJson.end(); ++pIt) {
                    TCollection_AsciiString propKey = pIt.key().c_str();
                    std::string propVal;
                    if (pIt.value().is_number()) {
                        propVal = std::to_string(pIt.value().get<double>());
                    } else if (pIt.value().is_array()) {
                        // Handle array (e.g. BaseColor) as comma separated string
                        std::string s;
                        for (size_t i = 0; i < pIt.value().size(); ++i) {
                            if (i > 0) s += ",";
                            if (pIt.value()[i].is_number()) s += std::to_string(pIt.value()[i].get<double>());
                            else s += pIt.value()[i].get<std::string>();
                        }
                        propVal = s;
                    } else if (pIt.value().is_string()) {
                        propVal = pIt.value().get<std::string>();
                    } else {
                        continue; // Skip other types
                    }

                    bool found = false;
                    NCollection_Sequence<Handle(BrNode_adProperty)> props = targetPset->GetPropertiesList();
                    for (int j = 1; j <= props.Length(); ++j) {
                        if (props.Value(j)->GetPropertyName() == propKey) {
                            props.Value(j)->SetPropertyValue(propVal.c_str());
                            found = true;
                            break;
                        }
                    }

                    if (!found) {
                        Handle(DataModel) dataModel = Handle(DataModel)::DownCast(model);
                        Handle(BrNode_adProperty) newProp = dataModel->AddadProperty();
                        newProp->SetPropertyName(propKey.ToCString());
                        newProp->SetPropertyValue(propVal.c_str());
                        targetPset->AddProperties(newProp);
                    }
                }
            }
        }
    }

    if (objJson.contains("ObjectPlacement")) {
        auto placementJson = objJson["ObjectPlacement"];
        if (placementJson.is_array() && placementJson.size() >= 3) {
            int nParams = (int)placementJson.size();
            if (nParams > 6) nParams = 6;

            auto p = adObj->Parameter(BrNode_adObject::PID_ObjectPlacement);
            auto typedP = ActData_ParameterFactory::AsRealArray(p);
            if (!typedP.IsNull()) {
                Handle(TColStd_HArray1OfReal) tmp = new TColStd_HArray1OfReal(0, nParams - 1);
                typedP->SetArray(tmp);
                for (int i = 0; i < nParams; ++i) {
                    typedP->SetElement(i, placementJson[i].get<double>());
                }
            }
        }
    }

    if (objJson.contains("children")) {
        for (const auto& childJson : objJson["children"]) {
            Handle(BrNode_adObject) childObj = ProcessJsonObject(model, childJson);
            if (!childObj.IsNull()) {
                adObj->AddSubObjects(childObj);
            }
        }
    }

    return adObj;
}

#include "GeometryService.h"
void BuildAllGeometry(const Handle(ActData_BaseModel)& model, const Handle(BrNode_adObject)& node, GeometryService& geoService) {
    if (node.IsNull()) return;
    TDF_Label geoLabel = geoService.BuildGeometry(node);
    
    NCollection_Sequence<Handle(BrNode_adObject)> children = node->GetSubObjectsList();
    for (int i = 1; i <= children.Length(); ++i) {
        BuildAllGeometry(model, children.Value(i), geoService);
    }
}

int main(int argc, char** argv) {
    std::cout << "--- TEST START ---" << std::endl;
    Handle(DataModel) model = new DataModel();
    model->NewEmpty();
    if (!model->Document().IsNull()) {
        model->Document()->SetUndoLimit(10);
    }

    std::ifstream f("bridge_data.json");
    if (!f.is_open()) {
        std::cerr << "Cannot find bridge_data.json" << std::endl;
        return 1;
    }
    json data;
    try {
        f >> data;
    } catch (const json::parse_error& e) {
        std::cerr << "JSON Parse Error: " << e.what() << std::endl;
        return 1;
    }
    model->OpenCommand();
    try {
        // 显式在事务内初始化 XCAF 的各种 Tools 以免由于懒加载在事务外写属性导致崩溃
        XCAFDoc_DocumentTool::ShapeTool(model->Document()->Main());
        XCAFDoc_DocumentTool::ColorTool(model->Document()->Main());
        XCAFDoc_DocumentTool::LayerTool(model->Document()->Main());

        Handle(BrNode_adObject) rootBridge = ProcessJsonObject(model, data["bridge"]);
        Handle(BrNode_adModelRoot) modelRoot = Handle(BrNode_adModelRoot)::DownCast(model->GetRootNode());
        if (!modelRoot.IsNull() && !rootBridge.IsNull()) {
            modelRoot->AddSubObjects(rootBridge);
        }
        
        GeometryService geoService(model, "http://127.0.0.1:3500/v1.0/invoke/modeling-service/method");
        BuildAllGeometry(model, rootBridge, geoService);
        std::cout << "[DEBUG] Committing command..." << std::endl;
        model->CommitCommand();
        std::cout << "[DEBUG] Command committed successfully." << std::endl;

        if (!rootBridge.IsNull()) {
            std::cout << std::endl << "========== Bridge Assembly Tree ==========" << std::endl;
            PrintAssemblyTree(rootBridge);
            std::cout << "===========================================" << std::endl;
            
            std::cout << "[DEBUG] Printing geometry stats..." << std::endl;
            PrintGeometryStats(model);
            std::cout << "[DEBUG] Geometry stats printed successfully." << std::endl;
            
            TCollection_AsciiString savePath = "D:/QtOCCTApp/bridge_test_save.asi";
            std::cout << "[DEBUG] Saving model to: " << savePath.ToCString() << " ..." << std::endl;
            if (model->SaveAs(savePath)) {
                std::cout << "Model saved to: " << savePath.ToCString() << std::endl;
            }
            std::cout << "[DEBUG] Model saved successfully." << std::endl;
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
