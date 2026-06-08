#include "BrNode_adSubDocRef.h"
#include "ActData_ParameterFactory.h"
#include <TDF_Tool.hxx>
#include <TDF_Label.hxx>
#include <ActData_Utils.h>
#include <iostream>

// Register this node type with Active Data's NodeFactory
REGISTER_NODE_TYPE(BrNode_adSubDocRef)

BrNode_adSubDocRef::BrNode_adSubDocRef() : BrNode_adRoot()
{
    
    REGISTER_PARAMETER(Name, PID_DocPath);
    
    REGISTER_PARAMETER(Name, PID_DocType);
    
    REGISTER_PARAMETER(Bool, PID_IsLoaded);
    
}

Handle(BrNode_adSubDocRef) BrNode_adSubDocRef::Instance()
{
    return new BrNode_adSubDocRef();
}

void BrNode_adSubDocRef::InitNode()
{
    
    BrNode_adRoot::InitNode();
    
    
    // Initialize parameters with default names and flags
    
    this->InitParameter(PID_DocPath, "DocPath");
    
    this->InitParameter(PID_DocType, "DocType");
    
    this->InitParameter(PID_IsLoaded, "IsLoaded");
    
}



// --- DocPath ---

void BrNode_adSubDocRef::SetDocPath(const TCollection_ExtendedString& value)
{
    std::cout << "    [Setter] Entering SetDocPath for BrNode_adSubDocRef" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_DocPath);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_DocPath is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_DocPath cannot be cast to Name!" << std::endl;
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

TCollection_ExtendedString BrNode_adSubDocRef::GetDocPath() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_DocPath);
    if (p.IsNull()) return TCollection_ExtendedString();
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) return TCollection_ExtendedString();
    
    
    return typedP->GetValue();
    
}



// --- DocType ---

void BrNode_adSubDocRef::SetDocType(const TCollection_ExtendedString& value)
{
    std::cout << "    [Setter] Entering SetDocType for BrNode_adSubDocRef" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_DocType);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_DocType is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_DocType cannot be cast to Name!" << std::endl;
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

TCollection_ExtendedString BrNode_adSubDocRef::GetDocType() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_DocType);
    if (p.IsNull()) return TCollection_ExtendedString();
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) return TCollection_ExtendedString();
    
    
    return typedP->GetValue();
    
}



// --- IsLoaded ---

void BrNode_adSubDocRef::SetIsLoaded(const bool& value)
{
    std::cout << "    [Setter] Entering SetIsLoaded for BrNode_adSubDocRef" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_IsLoaded);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_IsLoaded is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsBool(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_IsLoaded cannot be cast to Bool!" << std::endl;
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

bool BrNode_adSubDocRef::GetIsLoaded() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_IsLoaded);
    if (p.IsNull()) return bool();
    
    auto typedP = ActData_ParameterFactory::AsBool(p);
    if (typedP.IsNull()) return bool();
    
    
    return typedP->GetValue();
    
}



// Mandatory overrides for ActData_BaseNode
TCollection_ExtendedString BrNode_adSubDocRef::GetName()
{
    
    return BrNode_adRoot::GetName();
    
}

void BrNode_adSubDocRef::SetName(const TCollection_ExtendedString& theName)
{
    
    BrNode_adRoot::SetName(theName);
    
}

// --- Children management ---
