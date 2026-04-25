#include "BrNode_ActiveObject.h"

// OCCT RTTI
IMPLEMENT_STANDARD_RTTIEXT(BrNode_ActiveObject, ActData_BaseNode)

Handle(BrNode_ActiveObject) BrNode_ActiveObject::Instance()
{
    return new BrNode_ActiveObject();
}

void BrNode_ActiveObject::InitNode()
{
    // Initialize parameters
    
    this->InitParameter(PID_GlobalID, "GlobalID");
    
    this->InitParameter(PID_Name, "Name");
    
    this->InitParameter(PID_ObjectType, "ObjectType");
    
    this->InitParameter(PID_BrEntityRef, "BrEntityRef");
    
    this->InitParameter(PID_Geometry, "Geometry");
    
    this->InitParameter(PID_GeometryRef, "GeometryRef");
    
    this->InitParameter(PID_Transformation, "Transformation");
    
    this->InitParameter(PID_MaterialType, "MaterialType");
    
    this->InitParameter(PID_Color, "Color");
    
    this->InitParameter(PID_Transparency, "Transparency");
    
    this->InitParameter(PID_Visibility, "Visibility");
    
}

TCollection_ExtendedString BrNode_ActiveObject::GetName()
{
    return "BrNode_ActiveObject";
}

void BrNode_ActiveObject::SetName(const TCollection_ExtendedString& /*theName*/)
{
}


// --- GlobalID ---

void BrNode_ActiveObject::SetGlobalID(const TCollection_AsciiString& value)
{
    ActData_ParameterFactory::AsAsciiString(this->Parameter(PID_GlobalID))->SetValue(value);
}

TCollection_AsciiString BrNode_ActiveObject::GetGlobalID() const
{
    return ActData_ParameterFactory::AsAsciiString(this->Parameter(PID_GlobalID))->GetValue();
}

// --- Name ---

void BrNode_ActiveObject::SetName(const TCollection_AsciiString& value)
{
    ActData_ParameterFactory::AsAsciiString(this->Parameter(PID_Name))->SetValue(value);
}

TCollection_AsciiString BrNode_ActiveObject::GetName() const
{
    return ActData_ParameterFactory::AsAsciiString(this->Parameter(PID_Name))->GetValue();
}

// --- ObjectType ---

void BrNode_ActiveObject::SetObjectType(const TCollection_AsciiString& value)
{
    ActData_ParameterFactory::AsAsciiString(this->Parameter(PID_ObjectType))->SetValue(value);
}

TCollection_AsciiString BrNode_ActiveObject::GetObjectType() const
{
    return ActData_ParameterFactory::AsAsciiString(this->Parameter(PID_ObjectType))->GetValue();
}

// --- BrEntityRef ---

void BrNode_ActiveObject::SetBrEntityRef(const TCollection_AsciiString& value)
{
    ActData_ParameterFactory::AsAsciiString(this->Parameter(PID_BrEntityRef))->SetValue(value);
}

TCollection_AsciiString BrNode_ActiveObject::GetBrEntityRef() const
{
    return ActData_ParameterFactory::AsAsciiString(this->Parameter(PID_BrEntityRef))->GetValue();
}

// --- Geometry ---

void BrNode_ActiveObject::SetGeometry(const TopoDS_Shape& value)
{
    ActData_ParameterFactory::AsShape(this->Parameter(PID_Geometry))->SetShape(value);
}

TopoDS_Shape BrNode_ActiveObject::GetGeometry() const
{
    return ActData_ParameterFactory::AsShape(this->Parameter(PID_Geometry))->GetShape();
}

// --- GeometryRef ---

void BrNode_ActiveObject::SetGeometryRef(const Handle(ActAPI_IDataCursor)& value)
{
    ActData_ParameterFactory::AsReference(this->Parameter(PID_GeometryRef))->SetTarget(value);
}

Handle(ActAPI_IDataCursor) BrNode_ActiveObject::GetGeometryRef() const
{
    return ActData_ParameterFactory::AsReference(this->Parameter(PID_GeometryRef))->GetTarget();
}

// --- Transformation ---

void BrNode_ActiveObject::SetTransformation(const Handle(TColStd_HArray1OfReal)& value)
{
    ActData_ParameterFactory::AsRealArray(this->Parameter(PID_Transformation))->SetArray(value);
}

Handle(TColStd_HArray1OfReal) BrNode_ActiveObject::GetTransformation() const
{
    return ActData_ParameterFactory::AsRealArray(this->Parameter(PID_Transformation))->GetArray();
}

// --- MaterialType ---

void BrNode_ActiveObject::SetMaterialType(const int& value)
{
    ActData_ParameterFactory::AsInt(this->Parameter(PID_MaterialType))->SetValue(value);
}

int BrNode_ActiveObject::GetMaterialType() const
{
    return ActData_ParameterFactory::AsInt(this->Parameter(PID_MaterialType))->GetValue();
}

// --- Color ---

void BrNode_ActiveObject::SetColor(const int& value)
{
    ActData_ParameterFactory::AsInt(this->Parameter(PID_Color))->SetValue(value);
}

int BrNode_ActiveObject::GetColor() const
{
    return ActData_ParameterFactory::AsInt(this->Parameter(PID_Color))->GetValue();
}

// --- Transparency ---

void BrNode_ActiveObject::SetTransparency(const double& value)
{
    ActData_ParameterFactory::AsReal(this->Parameter(PID_Transparency))->SetValue(value);
}

double BrNode_ActiveObject::GetTransparency() const
{
    return ActData_ParameterFactory::AsReal(this->Parameter(PID_Transparency))->GetValue();
}

// --- Visibility ---

void BrNode_ActiveObject::SetVisibility(const bool& value)
{
    ActData_ParameterFactory::AsBool(this->Parameter(PID_Visibility))->SetValue(value);
}

bool BrNode_ActiveObject::GetVisibility() const
{
    return ActData_ParameterFactory::AsBool(this->Parameter(PID_Visibility))->GetValue();
}


// --- Children management ---


void BrNode_ActiveObject::AddPropertySets(const Handle(BrNode_PropertySet)& node)
{
    this->AddChildNode(node);
}

NCollection_Sequence<Handle(BrNode_PropertySet)> BrNode_ActiveObject::GetPropertySetsList() const
{
    NCollection_Sequence<Handle(BrNode_PropertySet)> res;
    Handle(ActAPI_IChildIterator) it = this->GetChildIterator();
    if (!it.IsNull())
    {
        for (; it->More(); it->Next())
        {
            Handle(BrNode_PropertySet) child = Handle(BrNode_PropertySet)::DownCast(it->Value());
            if (!child.IsNull())
                res.Append(child);
        }
    }
    return res;
}



void BrNode_ActiveObject::AddSubObjects(const Handle(BrNode_ActiveObject)& node)
{
    this->AddChildNode(node);
}

NCollection_Sequence<Handle(BrNode_ActiveObject)> BrNode_ActiveObject::GetSubObjectsList() const
{
    NCollection_Sequence<Handle(BrNode_ActiveObject)> res;
    Handle(ActAPI_IChildIterator) it = this->GetChildIterator();
    if (!it.IsNull())
    {
        for (; it->More(); it->Next())
        {
            Handle(BrNode_ActiveObject) child = Handle(BrNode_ActiveObject)::DownCast(it->Value());
            if (!child.IsNull())
                res.Append(child);
        }
    }
    return res;
}

