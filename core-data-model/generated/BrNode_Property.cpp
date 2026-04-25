#include "BrNode_Property.h"

// OCCT RTTI
IMPLEMENT_STANDARD_RTTIEXT(BrNode_Property, ActData_BaseNode)

Handle(BrNode_Property) BrNode_Property::Instance()
{
    return new BrNode_Property();
}

void BrNode_Property::InitNode()
{
    // Initialize parameters
    
    this->InitParameter(PID_PropertyName, "PropertyName");
    
    this->InitParameter(PID_PropertyValue, "PropertyValue");
    
    this->InitParameter(PID_ValueType, "ValueType");
    
}

TCollection_ExtendedString BrNode_Property::GetName()
{
    return "BrNode_Property";
}

void BrNode_Property::SetName(const TCollection_ExtendedString& /*theName*/)
{
}


// --- PropertyName ---

void BrNode_Property::SetPropertyName(const TCollection_AsciiString& value)
{
    ActData_ParameterFactory::AsAsciiString(this->Parameter(PID_PropertyName))->SetValue(value);
}

TCollection_AsciiString BrNode_Property::GetPropertyName() const
{
    return ActData_ParameterFactory::AsAsciiString(this->Parameter(PID_PropertyName))->GetValue();
}

// --- PropertyValue ---

void BrNode_Property::SetPropertyValue(const TCollection_AsciiString& value)
{
    ActData_ParameterFactory::AsAsciiString(this->Parameter(PID_PropertyValue))->SetValue(value);
}

TCollection_AsciiString BrNode_Property::GetPropertyValue() const
{
    return ActData_ParameterFactory::AsAsciiString(this->Parameter(PID_PropertyValue))->GetValue();
}

// --- ValueType ---

void BrNode_Property::SetValueType(const TCollection_AsciiString& value)
{
    ActData_ParameterFactory::AsAsciiString(this->Parameter(PID_ValueType))->SetValue(value);
}

TCollection_AsciiString BrNode_Property::GetValueType() const
{
    return ActData_ParameterFactory::AsAsciiString(this->Parameter(PID_ValueType))->GetValue();
}


// --- Children management ---
