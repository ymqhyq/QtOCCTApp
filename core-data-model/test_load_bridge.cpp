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
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <math.h>

#include <ActData_BasePartition.h>

using json = nlohmann::json;

static std::string ToStr(const TCollection_ExtendedString& es) {
    TCollection_AsciiString as(es);
    return std::string(as.ToCString());
}

void PrintGeometryStats(const Handle(ActData_BaseModel)& model) {
    Handle(ActAPI_IPartition) geoPart = model->Partition(DataModel::PID_GeometryDefinitions);
    Handle(ActAPI_IPartition) topoPart = model->Partition(DataModel::PID_Topology);
    if (geoPart.IsNull() || topoPart.IsNull()) return;

    std::cout << std::endl;
    std::cout << "========== Geometry Reuse Stats ==========" << std::endl;
    
    struct GeoStat {
        Handle(BrNode_adGeometricDef) node;
        int count = 0;
    };
    std::map<std::string, GeoStat> stats;
    
    Handle(ActData_BasePartition) geoPartBase = Handle(ActData_BasePartition)::DownCast(geoPart);
    if (!geoPartBase.IsNull()) {
        for (ActData_BasePartition::Iterator it(geoPartBase); it.More(); it.Next()) {
            Handle(BrNode_adGeometricDef) gd = Handle(BrNode_adGeometricDef)::DownCast(it.Value());
            if (!gd.IsNull()) {
                stats[ToStr(gd->GetParamGeoID())] = {gd, 0};
            }
        }
    }

    Handle(ActData_BasePartition) topoPartBase = Handle(ActData_BasePartition)::DownCast(topoPart);
    if (!topoPartBase.IsNull()) {
        for (ActData_BasePartition::Iterator it(topoPartBase); it.More(); it.Next()) {
            Handle(BrNode_adGeometry) geom = Handle(BrNode_adGeometry)::DownCast(it.Value());
            if (!geom.IsNull()) {
                Handle(ActAPI_IDataCursor) ref = geom->GetGeometryRef();
                Handle(BrNode_adGeometricDef) gd = Handle(BrNode_adGeometricDef)::DownCast(ref);
                if (!gd.IsNull()) {
                    std::string id = ToStr(gd->GetParamGeoID());
                    if (stats.count(id)) {
                        stats[id].count++;
                    }
                }
            }
        }
    }
    
    int totalDefs = 0;
    for (auto const& [id, stat] : stats) {
        totalDefs++;
        std::string name = ToStr(stat.node->GetName());
        std::cout << "  - [GeoDef] " << name << " (ID: " << id.substr(0,8) << "...)"
                  << "  Count: " << stat.count << std::endl;
    }
    
    std::cout << "Total GeoDefs: " << totalDefs << std::endl;
    std::cout << "===========================================" << std::endl;
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
                    if (pIt.value().is_number()) propVal = std::to_string(pIt.value().get<double>());
                    else propVal = pIt.value().get<std::string>();

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
    Handle(BrNode_adGeometricDef) geoDef = geoService.BuildGeometry(node);

    if (!geoDef.IsNull()) {
        Handle(DataModel) dataModel = Handle(DataModel)::DownCast(model);
        Handle(BrNode_adGeometry) geom = dataModel->AddadGeometry();
        geom->SetName(node->GetName());
        geom->SetGeometryRef(geoDef);
        node->SetGeometry(geom);
    }
    
    NCollection_Sequence<Handle(BrNode_adObject)> children = node->GetSubObjectsList();
    for (int i = 1; i <= children.Length(); ++i) {
        BuildAllGeometry(model, children.Value(i), geoService);
    }
}

int main(int argc, char** argv) {
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
    json data = json::parse(f);

    model->OpenCommand();
    try {
        Handle(BrNode_adObject) rootBridge = ProcessJsonObject(model, data["bridge"]);
        Handle(BrNode_adModelRoot) modelRoot = Handle(BrNode_adModelRoot)::DownCast(model->GetRootNode());
        if (!modelRoot.IsNull() && !rootBridge.IsNull()) {
            modelRoot->AddSubObjects(rootBridge);
        }
        
        GeometryService geoService(model, "http://127.0.0.1:3500/v1.0/invoke/modeling-service/method");
        BuildAllGeometry(model, rootBridge, geoService);
        
        model->CommitCommand();

        if (!rootBridge.IsNull()) {
            std::cout << std::endl << "========== Bridge Assembly Tree ==========" << std::endl;
            PrintAssemblyTree(rootBridge);
            std::cout << "===========================================" << std::endl;
            PrintGeometryStats(model);
            
            TCollection_AsciiString savePath = "D:/QtOCCTApp/bridge_test_save.asi";
            if (model->SaveAs(savePath)) {
                std::cout << "Model saved to: " << savePath.ToCString() << std::endl;
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
