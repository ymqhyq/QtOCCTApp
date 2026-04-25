#include "DataNode_GeometricDefinition.h"



// OCCT RTTI
IMPLEMENT_STANDARD_RTTI_EXT(DataNode_GeometricDefinition, ActData_BaseNode)

Handle(DataNode_GeometricDefinition) DataNode_GeometricDefinition::Instance()
{
    return new DataNode_GeometricDefinition();
}

void DataNode_GeometricDefinition::Init()
{
    // Initialize parent
    ActData_BaseNode::Init();

    // Initialize parameters
    
    this->InitParameter(PID_MasterGeometry, "原型几何");
    
    this->InitParameter(PID_Creator, "创建者");
    
    this->InitParameter(PID_Version, "版本号");
    
}


// --- 原型几何 ---

void DataNode_GeometricDefinition::SetMasterGeometry(const TopoDS_Shape& value)
{
    ActData_ParameterFactory::AsShape(this->Parameter(PID_MasterGeometry))->SetShape(value);
}

TopoDS_Shape DataNode_GeometricDefinition::GetMasterGeometry() const
{
    return ActData_ParameterFactory::AsShape(this->Parameter(PID_MasterGeometry))->GetShape();
}

// --- 创建者 ---

void DataNode_GeometricDefinition::SetCreator(const TCollection_AsciiString& value)
{
    ActData_ParameterFactory::AsString(this->Parameter(PID_Creator))->SetValue(value);
}

TCollection_AsciiString DataNode_GeometricDefinition::GetCreator() const
{
    return ActData_ParameterFactory::AsString(this->Parameter(PID_Creator))->GetValue();
}

// --- 版本号 ---

void DataNode_GeometricDefinition::SetVersion(const int& value)
{
    ActData_ParameterFactory::AsInt(this->Parameter(PID_Version))->SetValue(value);
}

int DataNode_GeometricDefinition::GetVersion() const
{
    return ActData_ParameterFactory::AsInt(this->Parameter(PID_Version))->GetValue();
}


// --- Children management ---

