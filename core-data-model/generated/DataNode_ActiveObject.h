#ifndef DataNode_ActiveObject_HeaderFile
#define DataNode_ActiveObject_HeaderFile

// Active Data includes (No Qt!)
#include <ActData_BaseNode.h>
#include <ActData_ParameterFactory.h>
#include <ActData_RealParameter.h>
#include <ActData_IntParameter.h>
#include <ActData_StringParameter.h>
#include <ActData_ShapeParameter.h>
#include <ActData_RealArrayParameter.h>
#include <TopoDS_Shape.h>
#include <TColStd_HArray1OfReal.h>
#include <NCollection_Sequence.hxx>


class DataNode_GeometricDefinition;

class DataNode_Property;

class DataNode_PropertySet;

class DataNode_ActiveObject;

class DataNode_Partitions;



#include "ActData_BaseNode.h"


/**
 * @brief MDA Generated Node for 核心对象
 * This class is part of the portable data model for Docker-based microservices.
 */
class DataNode_ActiveObject : public ActData_BaseNode
{
public:
    // OCCT RTTI
    DEFINE_STANDARD_RTTI_EXT(DataNode_ActiveObject, ActData_BaseNode)

public:
    // Parameter IDs (Tags)
    enum ParamId
    {
        PID_GlobalID = ActData_BaseNode::PID_Last,PID_Name,PID_ObjectType,PID_BrEntityRef,PID_Geometry,PID_GeometryRef,PID_Transformation,PID_MaterialType,PID_Color,PID_Transparency,PID_Visibility,
        PID_Last
    };

public:
    static Handle(DataNode_ActiveObject) Instance();

    // Initialization
    virtual void Init();

public:
    // Getters / Setters (MDA Generated)
    
    // 唯一标识
    void SetGlobalID(const TCollection_AsciiString& value);
    TCollection_AsciiString GetGlobalID() const;
    
    // 显示名称
    void SetName(const TCollection_AsciiString& value);
    TCollection_AsciiString GetName() const;
    
    // 对象类型
    void SetObjectType(const TCollection_AsciiString& value);
    TCollection_AsciiString GetObjectType() const;
    
    // 外部业务实体引用(GUID)
    void SetBrEntityRef(const TCollection_AsciiString& value);
    TCollection_AsciiString GetBrEntityRef() const;
    
    // 实例独有几何(可选)
    void SetGeometry(const TopoDS_Shape& value);
    TopoDS_Shape GetGeometry() const;
    
    // 复用原型引用
    void SetGeometryRef(const Handle(ActData_BaseNode)& value);
    Handle(ActData_BaseNode) GetGeometryRef() const;
    
    // 实例化变换矩阵
    void SetTransformation(const Handle(TColStd_HArray1OfReal)& value);
    Handle(TColStd_HArray1OfReal) GetTransformation() const;
    
    // 显示材质
    void SetMaterialType(const int& value);
    int GetMaterialType() const;
    
    // 颜色(RGB)
    void SetColor(const int& value);
    int GetColor() const;
    
    // 透明度
    void SetTransparency(const double& value);
    double GetTransparency() const;
    
    // 可见性
    void SetVisibility(const bool& value);
    bool GetVisibility() const;
    

public:
    // Children management (MDA Generated)
    
    // --- PropertySets (0..*) ---
    
    void AddPropertySets(const Handle(DataNode_PropertySet)& node);
    NCollection_Sequence<Handle(DataNode_PropertySet)> GetPropertySetsList() const;
    
    
    // --- SubObjects (0..*) ---
    
    void AddSubObjects(const Handle(DataNode_ActiveObject)& node);
    NCollection_Sequence<Handle(DataNode_ActiveObject)> GetSubObjectsList() const;
    
    

protected:
    DataNode_ActiveObject() : ActData_BaseNode() {}

};

#endif