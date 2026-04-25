#include "BrNode_GeometricDefinition.h"

// OCCT RTTI
IMPLEMENT_STANDARD_RTTIEXT(BrNode_GeometricDefinition, ActData_BaseNode)

Handle(BrNode_GeometricDefinition) BrNode_GeometricDefinition::Instance()
{
    return new BrNode_GeometricDefinition();
}

void BrNode_GeometricDefinition::InitNode()
{
    // Initialize parameters
    
    this->InitParameter(PID_MasterGeometry, "MasterGeometry");
    
    this->InitParameter(PID_Creator, "Creator");
    
    this->InitParameter(PID_Version, "Version");
    
}

TCollection_ExtendedString BrNode_GeometricDefinition::GetName()
{
    return "BrNode_GeometricDefinition";
}

void BrNode_GeometricDefinition::SetName(const TCollection_ExtendedString& /*theName*/)
{
}


// --- MasterGeometry ---

void BrNode_GeometricDefinition::SetMasterGeometry(const TopoDS_Shape& value)
{
    ActData_ParameterFactory::AsShape(this->Parameter(PID_MasterGeometry))->SetShape(value);
}

TopoDS_Shape BrNode_GeometricDefinition::GetMasterGeometry() const
{
    return ActData_ParameterFactory::AsShape(this->Parameter(PID_MasterGeometry))->GetShape();
}

// --- Creator ---

void BrNode_GeometricDefinition::SetCreator(const TCollection_AsciiString& value)
{
    ActData_ParameterFactory::AsAsciiString(this->Parameter(PID_Creator))->SetValue(value);
}

TCollection_AsciiString BrNode_GeometricDefinition::GetCreator() const
{
    return ActData_ParameterFactory::AsAsciiString(this->Parameter(PID_Creator))->GetValue();
}

// --- Version ---

void BrNode_GeometricDefinition::SetVersion(const int& value)
{
    ActData_ParameterFactory::AsInt(this->Parameter(PID_Version))->SetValue(value);
}

int BrNode_GeometricDefinition::GetVersion() const
{
    return ActData_ParameterFactory::AsInt(this->Parameter(PID_Version))->GetValue();
}


// --- Children management ---
