#include "DataNode_ActiveObject.h"


#include "DataNode_PropertySet.h"

#include "DataNode_ActiveObject.h"


// OCCT RTTI
IMPLEMENT_STANDARD_RTTI_EXT(DataNode_ActiveObject, ActData_BaseNode)

Handle(DataNode_ActiveObject) DataNode_ActiveObject::Instance()
{
    return new DataNode_ActiveObject();
}

void DataNode_ActiveObject::Init()
{
    // Initialize parent
    ActData_BaseNode::Init();

    // Initialize parameters
    
    this->InitParameter(PID_GlobalID, "唯一标识");
    
    this->InitParameter(PID_Name, "显示名称");
    
    this->InitParameter(PID_ObjectType, "对象类型");
    
    this->InitParameter(PID_BrEntityRef, "外部业务实体引用(GUID)");
    
    this->InitParameter(PID_Geometry, "实例独有几何(可选)");
    
    this->InitParameter(PID_GeometryRef, "复用原型引用");
    
    this->InitParameter(PID_Transformation, "实例化变换矩阵");
    
    this->InitParameter(PID_MaterialType, "显示材质");
    
    this->InitParameter(PID_Color, "颜色(RGB)");
    
    this->InitParameter(PID_Transparency, "透明度");
    
    this->InitParameter(PID_Visibility, "可见性");
    
}


// --- 唯一标识 ---

void DataNode_ActiveObject::SetGlobalID(const TCollection_AsciiString& value)
{
    ActData_ParameterFactory::AsString(this->Parameter(PID_GlobalID))->SetValue(value);
}

TCollection_AsciiString DataNode_ActiveObject::GetGlobalID() const
{
    return ActData_ParameterFactory::AsString(this->Parameter(PID_GlobalID))->GetValue();
}

// --- 显示名称 ---

void DataNode_ActiveObject::SetName(const TCollection_AsciiString& value)
{
    ActData_ParameterFactory::AsString(this->Parameter(PID_Name))->SetValue(value);
}

TCollection_AsciiString DataNode_ActiveObject::GetName() const
{
    return ActData_ParameterFactory::AsString(this->Parameter(PID_Name))->GetValue();
}

// --- 对象类型 ---

void DataNode_ActiveObject::SetObjectType(const TCollection_AsciiString& value)
{
    ActData_ParameterFactory::AsString(this->Parameter(PID_ObjectType))->SetValue(value);
}

TCollection_AsciiString DataNode_ActiveObject::GetObjectType() const
{
    return ActData_ParameterFactory::AsString(this->Parameter(PID_ObjectType))->GetValue();
}

// --- 外部业务实体引用(GUID) ---

void DataNode_ActiveObject::SetBrEntityRef(const TCollection_AsciiString& value)
{
    ActData_ParameterFactory::AsString(this->Parameter(PID_BrEntityRef))->SetValue(value);
}

TCollection_AsciiString DataNode_ActiveObject::GetBrEntityRef() const
{
    return ActData_ParameterFactory::AsString(this->Parameter(PID_BrEntityRef))->GetValue();
}

// --- 实例独有几何(可选) ---

void DataNode_ActiveObject::SetGeometry(const TopoDS_Shape& value)
{
    ActData_ParameterFactory::AsShape(this->Parameter(PID_Geometry))->SetShape(value);
}

TopoDS_Shape DataNode_ActiveObject::GetGeometry() const
{
    return ActData_ParameterFactory::AsShape(this->Parameter(PID_Geometry))->GetShape();
}

// --- 复用原型引用 ---

void DataNode_ActiveObject::SetGeometryRef(const Handle(ActData_BaseNode)& value)
{
    ActData_ParameterFactory::AsReference(this->Parameter(PID_GeometryRef))->SetTargetNode(value);
}

Handle(ActData_BaseNode) DataNode_ActiveObject::GetGeometryRef() const
{
    return ActData_ParameterFactory::AsReference(this->Parameter(PID_GeometryRef))->GetTargetNode();
}

// --- 实例化变换矩阵 ---

void DataNode_ActiveObject::SetTransformation(const Handle(TColStd_HArray1OfReal)& value)
{
    ActData_ParameterFactory::AsRealArray(this->Parameter(PID_Transformation))->SetArray(value);
}

Handle(TColStd_HArray1OfReal) DataNode_ActiveObject::GetTransformation() const
{
    return ActData_ParameterFactory::AsRealArray(this->Parameter(PID_Transformation))->GetArray();
}

// --- 显示材质 ---

void DataNode_ActiveObject::SetMaterialType(const int& value)
{
    ActData_ParameterFactory::AsInt(this->Parameter(PID_MaterialType))->SetValue(value);
}

int DataNode_ActiveObject::GetMaterialType() const
{
    return ActData_ParameterFactory::AsInt(this->Parameter(PID_MaterialType))->GetValue();
}

// --- 颜色(RGB) ---

void DataNode_ActiveObject::SetColor(const int& value)
{
    ActData_ParameterFactory::AsInt(this->Parameter(PID_Color))->SetValue(value);
}

int DataNode_ActiveObject::GetColor() const
{
    return ActData_ParameterFactory::AsInt(this->Parameter(PID_Color))->GetValue();
}

// --- 透明度 ---

void DataNode_ActiveObject::SetTransparency(const double& value)
{
    ActData_ParameterFactory::AsReal(this->Parameter(PID_Transparency))->SetValue(value);
}

double DataNode_ActiveObject::GetTransparency() const
{
    return ActData_ParameterFactory::AsReal(this->Parameter(PID_Transparency))->GetValue();
}

// --- 可见性 ---

void DataNode_ActiveObject::SetVisibility(const bool& value)
{
    ActData_ParameterFactory::AsBool(this->Parameter(PID_Visibility))->SetValue(value);
}

bool DataNode_ActiveObject::GetVisibility() const
{
    return ActData_ParameterFactory::AsBool(this->Parameter(PID_Visibility))->GetValue();
}


// --- Children management ---



void DataNode_ActiveObject::AddPropertySets(const Handle(DataNode_PropertySet)& node)
{
    this->AddChildNode(node);
}

NCollection_Sequence<Handle(DataNode_PropertySet)> DataNode_ActiveObject::GetPropertySetsList() const
{
    NCollection_Sequence<Handle(DataNode_PropertySet)> result;
    // Implementation: Filter children by type
    for (int i = 1; i <= this->GetNbChildren(); ++i) {
        Handle(DataNode_PropertySet) c = Handle(DataNode_PropertySet)::DownCast(this->GetChild(i));
        if (!c.IsNull()) result.Append(c);
    }
    return result;
}



void DataNode_ActiveObject::AddSubObjects(const Handle(DataNode_ActiveObject)& node)
{
    this->AddChildNode(node);
}

NCollection_Sequence<Handle(DataNode_ActiveObject)> DataNode_ActiveObject::GetSubObjectsList() const
{
    NCollection_Sequence<Handle(DataNode_ActiveObject)> result;
    // Implementation: Filter children by type
    for (int i = 1; i <= this->GetNbChildren(); ++i) {
        Handle(DataNode_ActiveObject) c = Handle(DataNode_ActiveObject)::DownCast(this->GetChild(i));
        if (!c.IsNull()) result.Append(c);
    }
    return result;
}

