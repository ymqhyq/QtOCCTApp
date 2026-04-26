#include "BrNode_adRoot.h"
#include "ActData_ParameterFactory.h"
#include <TDF_Tool.hxx>
#include <TDF_Label.hxx>
#include <ActData_Utils.h>
#include <iostream>

// Register this node type with Active Data's NodeFactory
REGISTER_NODE_TYPE(BrNode_adRoot)

BrNode_adRoot::BrNode_adRoot() : ActData_BaseNode()
{
    
    REGISTER_PARAMETER(Name, PID_GlobalID);
    
    REGISTER_PARAMETER(Name, PID_Name);
    
    REGISTER_PARAMETER(Name, PID_label);
    
}

Handle(BrNode_adRoot) BrNode_adRoot::Instance()
{
    return new BrNode_adRoot();
}

void BrNode_adRoot::InitNode()
{
    
    
    // Initialize parameters with default names and flags
    
    this->InitParameter(PID_GlobalID, "GlobalID");
    
    this->InitParameter(PID_Name, "Name");
    
    this->InitParameter(PID_label, "label");
    
}



// --- GlobalID ---

void BrNode_adRoot::SetGlobalID(const TCollection_ExtendedString& value)
{
    std::cout << "    [Setter] Entering SetGlobalID for BrNode_adRoot" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_GlobalID);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_GlobalID is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_GlobalID cannot be cast to Name!" << std::endl;
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

TCollection_ExtendedString BrNode_adRoot::GetGlobalID() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_GlobalID);
    if (p.IsNull()) return TCollection_ExtendedString();
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) return TCollection_ExtendedString();
    
    
    return typedP->GetValue();
    
}





// --- label ---

void BrNode_adRoot::Setlabel(const TCollection_ExtendedString& value)
{
    std::cout << "    [Setter] Entering Setlabel for BrNode_adRoot" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_label);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_label is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_label cannot be cast to Name!" << std::endl;
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

TCollection_ExtendedString BrNode_adRoot::Getlabel() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_label);
    if (p.IsNull()) return TCollection_ExtendedString();
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) return TCollection_ExtendedString();
    
    
    return typedP->GetValue();
    
}



// Mandatory overrides for ActData_BaseNode
TCollection_ExtendedString BrNode_adRoot::GetName()
{
    
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_Name);
    if (p.IsNull()) return "BrNode_adRoot";
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) return "BrNode_adRoot";
    return typedP->GetValue();
    
}

void BrNode_adRoot::SetName(const TCollection_ExtendedString& theName)
{
    
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_Name);
    if (p.IsNull()) return;
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) return;
    
    typedP->SetValue(theName);
    
}

// --- Children management ---
