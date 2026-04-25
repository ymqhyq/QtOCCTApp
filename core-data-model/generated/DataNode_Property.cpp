#include "DataNode_Property.h"



// OCCT RTTI
IMPLEMENT_STANDARD_RTTI_EXT(DataNode_Property, ActData_BaseNode)

Handle(DataNode_Property) DataNode_Property::Instance()
{
    return new DataNode_Property();
}

void DataNode_Property::Init()
{
    // Initialize parent
    ActData_BaseNode::Init();

    // Initialize parameters
    
    this->InitParameter(PID_PropertyName, "属性键");
    
    this->InitParameter(PID_PropertyValue, "属性值");
    
    this->InitParameter(PID_ValueType, "值类型");
    
}


// --- 属性键 ---

void DataNode_Property::SetPropertyName(const TCollection_AsciiString& value)
{
    ActData_ParameterFactory::AsString(this->Parameter(PID_PropertyName))->SetValue(value);
}

TCollection_AsciiString DataNode_Property::GetPropertyName() const
{
    return ActData_ParameterFactory::AsString(this->Parameter(PID_PropertyName))->GetValue();
}

// --- 属性值 ---

void DataNode_Property::SetPropertyValue(const TCollection_AsciiString& value)
{
    ActData_ParameterFactory::AsString(this->Parameter(PID_PropertyValue))->SetValue(value);
}

TCollection_AsciiString DataNode_Property::GetPropertyValue() const
{
    return ActData_ParameterFactory::AsString(this->Parameter(PID_PropertyValue))->GetValue();
}

// --- 值类型 ---

void DataNode_Property::SetValueType(const TCollection_AsciiString& value)
{
    ActData_ParameterFactory::AsString(this->Parameter(PID_ValueType))->SetValue(value);
}

TCollection_AsciiString DataNode_Property::GetValueType() const
{
    return ActData_ParameterFactory::AsString(this->Parameter(PID_ValueType))->GetValue();
}


// --- Children management ---

