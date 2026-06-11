#include "SceneDataExtractor.h"
#include "generated/BrNode_adGeometricDef.h"
#include "generated/BrNode_adPropertySet.h"
#include "generated/BrNode_adProperty.h"

#include <iostream>
#include <cmath>

static std::string ToStdString(const TCollection_ExtendedString& es) {
    char* str = new char[es.Length() * 3 + 1];
    Standard_Integer len = es.ToUTF8CString(str);
    std::string res(str, len);
    delete[] str;
    return res;
}

void SceneDataExtractor::Extract(const Handle(DataModel)& model, std::vector<VisualShape>& outShapes) {
    if (model.IsNull()) return;

    Handle(BrNode_adModelRoot) rootNode = Handle(BrNode_adModelRoot)::DownCast(model->GetRootNode());
    if (rootNode.IsNull()) return;

    NCollection_Sequence<Handle(BrNode_adObject)> topObjs = rootNode->GetSubObjectsList();
    for (int i = 1; i <= topObjs.Length(); ++i) {
        Handle(BrNode_adObject) obj = topObjs.Value(i);
        if (!obj.IsNull()) {
            ExtractFromObject(obj, outShapes);
        }
    }
}

void SceneDataExtractor::ExtractFromObject(const Handle(BrNode_adObject)& rootObj,
                                           std::vector<VisualShape>& outShapes,
                                           const gp_Trsf& parentTrsf) {
    if (rootObj.IsNull()) return;

    // 1. 获取局部相对坐标变换 localTrsf
    gp_Trsf localTrsf;
    try {
        Handle(ActAPI_IUserParameter) p = rootObj->Parameter(BrNode_adObject::PID_ObjectPlacement);
        Handle(ActData_RealArrayParameter) typedP = ActData_ParameterFactory::AsRealArray(p);

        if (!typedP.IsNull()) {
            int nbe = typedP->NbElements();
            if (nbe >= 3) {
                localTrsf.SetTranslation(gp_Vec(
                    typedP->GetElement(0), typedP->GetElement(1), typedP->GetElement(2)));
                if (nbe >= 6) {
                    double rx = typedP->GetElement(3);
                    double ry = typedP->GetElement(4);
                    double rz = typedP->GetElement(5);
                    gp_Trsf rot;
                    if (std::abs(rz) > 1e-6) {
                        gp_Trsf r;
                        r.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), rz * 3.14159265358979323846 / 180.0);
                        rot.Multiply(r);
                    }
                    if (std::abs(ry) > 1e-6) {
                        gp_Trsf r;
                        r.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(0, 1, 0)), ry * 3.14159265358979323846 / 180.0);
                        rot.Multiply(r);
                    }
                    if (std::abs(rx) > 1e-6) {
                        gp_Trsf r;
                        r.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(1, 0, 0)), rx * 3.14159265358979323846 / 180.0);
                        rot.Multiply(r);
                    }
                    localTrsf.Multiply(rot);
                }
            }
        }
    } catch (...) {
        std::cerr << "[SceneDataExtractor] Exception reading ObjectPlacement for " << ToStdString(rootObj->GetName()) << ". Ignored." << std::endl;
    }

    // 计算全局绝对坐标变换供前端渲染使用
    gp_Trsf currentTrsf = parentTrsf * localTrsf;

    // 2. 提取当前对象的几何 (优先直接读取持久化层)
    TopoDS_Shape currentShape;
    std::cout << "[SceneDataExtractor] Extracting object: " << ToStdString(rootObj->GetName()) << std::endl;
    if (!rootObj->GetGeometry().IsNull()) {
        std::cout << "  -> Has Geometry reference parameter." << std::endl;
        Handle(BrNode_adGeometry) geoNode = Handle(BrNode_adGeometry)::DownCast(rootObj->GetGeometry());
        if (!geoNode.IsNull()) {
            std::cout << "  -> Cast to adGeometry success." << std::endl;
            Handle(ActAPI_IDataCursor) geoRefCursor = geoNode->GetGeometryRef();
            if (!geoRefCursor.IsNull()) {
                std::cout << "  -> Has GeometryRef cursor." << std::endl;
                Handle(BrNode_adGeometricDef) selfGeoDef = Handle(BrNode_adGeometricDef)::DownCast(geoRefCursor);
                if (!selfGeoDef.IsNull()) {
                    currentShape = selfGeoDef->GetShape();
                    if (!currentShape.IsNull()) {
                         std::cout << "  -> Successfully retrieved TopoDS_Shape." << std::endl;
                    } else {
                         std::cout << "  -> Shape is NULL in GeometricDef." << std::endl;
                    }
                } else {
                     std::cout << "  -> Cast to adGeometricDef failed." << std::endl;
                }
            } else {
                std::cout << "  -> GeometryRef cursor is NULL." << std::endl;
            }
        } else {
             std::cout << "  -> Cast to adGeometry failed." << std::endl;
        }
    } else {
        std::cout << "  -> Geometry reference is NULL." << std::endl;
    }

    // 3. 构建元数据并压入出参
    if (!currentShape.IsNull()) {
        VisualShape vs;
        vs.shape = currentShape;
        vs.name = ToStdString(rootObj->GetName());
        vs.transform = currentTrsf;
        
        // 提取属性集
        NCollection_Sequence<Handle(BrNode_adPropertySet)> psets = rootObj->GetPropertySetsList();
        for (int i = 1; i <= psets.Length(); ++i) {
            Handle(BrNode_adPropertySet) ps = psets.Value(i);
            if (ps.IsNull()) continue;
            
            std::string psName = ToStdString(ps->GetName());
            json psJson;
            NCollection_Sequence<Handle(BrNode_adProperty)> props = ps->GetPropertiesList();
            for (int j = 1; j <= props.Length(); ++j) {
                Handle(BrNode_adProperty) p = props.Value(j);
                if (p.IsNull()) continue;
                
                std::string key = ToStdString(p->GetPropertyName());
                std::string val = ToStdString(p->GetPropertyValue());
                // 简单尝试转为数字，以供可能使用的地方
                try {
                    if (val.find('.') != std::string::npos) {
                        psJson[key] = std::stod(val);
                    } else {
                        psJson[key] = std::stoi(val);
                    }
                } catch (...) {
                    psJson[key] = val;
                }
            }
            vs.metadata[psName] = psJson;
        }
        vs.metadata["_adNodeId"] = ToStdString(rootObj->GetGlobalID());
        outShapes.push_back(vs);
    }

    // 4. 递归处理所有子构件 (如 Bridge 包含 Girders)
    NCollection_Sequence<Handle(BrNode_adObject)> children = rootObj->GetSubObjectsList();
    for (int i = 1; i <= children.Length(); ++i) {
        ExtractFromObject(children.Value(i), outShapes, currentTrsf);
    }
}
