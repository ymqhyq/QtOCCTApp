#include "BrNode_adProperty.h"
#include "ActData_ParameterFactory.h"
#include <TDF_Tool.hxx>
#include <TDF_Label.hxx>
#include <ActData_Utils.h>
#include <iostream>

// Register this node type with Active Data's NodeFactory
REGISTER_NODE_TYPE(BrNode_adProperty)

BrNode_adProperty::BrNode_adProperty() : BrNode_adRoot()
{
    
    REGISTER_PARAMETER(Name, PID_PropertyName);
    
    REGISTER_PARAMETER(Name, PID_PropertyValue);
    
    REGISTER_PARAMETER(Name, PID_ValueType);
    
}

Handle(BrNode_adProperty) BrNode_adProperty::Instance()
{
    return new BrNode_adProperty();
}

void BrNode_adProperty::InitNode()
{
    
    BrNode_adRoot::InitNode();
    
    
    // Initialize parameters with default names and flags
    
    this->InitParameter(PID_PropertyName, "PropertyName");
    
    this->InitParameter(PID_PropertyValue, "PropertyValue");
    
    this->InitParameter(PID_ValueType, "ValueType");
    
}



// --- PropertyName ---

void BrNode_adProperty::SetPropertyName(const TCollection_ExtendedString& value)
{
    std::cout << "    [Setter] Entering SetPropertyName for BrNode_adProperty" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_PropertyName);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_PropertyName is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_PropertyName cannot be cast to Name!" << std::endl;
        return;
    }

    try {
        
        typedP->SetValue(value);
        
        std::cout << "    [Setter] Value set successfully!" << std::endl;
    } catch (Standard_Failure& e) {
        std::cerr << "  [Setter] OCCT EXCEPTION: " << e.GetMessageString() << std::endl;
    } catch (...) {
        std::cerr << "  [Setter] UNKNOWN EXCEPTION!" << std::endl;
    }
}

TCollection_ExtendedString BrNode_adProperty::GetPropertyName() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_PropertyName);
    if (p.IsNull()) return TCollection_ExtendedString();
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) return TCollection_ExtendedString();
    
    
    return typedP->GetValue();
    
}



// --- PropertyValue ---

void BrNode_adProperty::SetPropertyValue(const TCollection_ExtendedString& value)
{
    std::cout << "    [Setter] Entering SetPropertyValue for BrNode_adProperty" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_PropertyValue);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_PropertyValue is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_PropertyValue cannot be cast to Name!" << std::endl;
        return;
    }

    try {
        
        typedP->SetValue(value);
        
        std::cout << "    [Setter] Value set successfully!" << std::endl;
    } catch (Standard_Failure& e) {
        std::cerr << "  [Setter] OCCT EXCEPTION: " << e.GetMessageString() << std::endl;
    } catch (...) {
        std::cerr << "  [Setter] UNKNOWN EXCEPTION!" << std::endl;
    }
}

TCollection_ExtendedString BrNode_adProperty::GetPropertyValue() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_PropertyValue);
    if (p.IsNull()) return TCollection_ExtendedString();
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) return TCollection_ExtendedString();
    
    
    return typedP->GetValue();
    
}



// --- ValueType ---

void BrNode_adProperty::SetValueType(const TCollection_ExtendedString& value)
{
    std::cout << "    [Setter] Entering SetValueType for BrNode_adProperty" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_ValueType);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_ValueType is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_ValueType cannot be cast to Name!" << std::endl;
        return;
    }

    try {
        
        typedP->SetValue(value);
        
        std::cout << "    [Setter] Value set successfully!" << std::endl;
    } catch (Standard_Failure& e) {
        std::cerr << "  [Setter] OCCT EXCEPTION: " << e.GetMessageString() << std::endl;
    } catch (...) {
        std::cerr << "  [Setter] UNKNOWN EXCEPTION!" << std::endl;
    }
}

TCollection_ExtendedString BrNode_adProperty::GetValueType() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_ValueType);
    if (p.IsNull()) return TCollection_ExtendedString();
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) return TCollection_ExtendedString();
    
    
    return typedP->GetValue();
    
}



// Mandatory overrides for ActData_BaseNode
TCollection_ExtendedString BrNode_adProperty::GetName()
{
    
    return BrNode_adRoot::GetName();
    
}

void BrNode_adProperty::SetName(const TCollection_ExtendedString& theName)
{
    
    BrNode_adRoot::SetName(theName);
    
}

// --- Children management ---
