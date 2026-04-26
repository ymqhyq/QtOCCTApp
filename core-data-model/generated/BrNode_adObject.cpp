#include "BrNode_adObject.h"
#include "ActData_ParameterFactory.h"
#include <TDF_Tool.hxx>
#include <TDF_Label.hxx>
#include <ActData_Utils.h>
#include <iostream>

// Register this node type with Active Data's NodeFactory
REGISTER_NODE_TYPE(BrNode_adObject)

BrNode_adObject::BrNode_adObject() : BrNode_adRoot()
{
    
    REGISTER_PARAMETER(Name, PID_ObjectType);
    
    REGISTER_PARAMETER(Name, PID_FunctionalType);
    
    REGISTER_PARAMETER(Name, PID_FunctionalSubType);
    
    REGISTER_PARAMETER(Name, PID_StructuralType);
    
    REGISTER_PARAMETER(Name, PID_StructuralSubType);
    
    REGISTER_PARAMETER(Name, PID_BrEntityRef);
    
    REGISTER_PARAMETER(Reference, PID_Geometry);
    
}

Handle(BrNode_adObject) BrNode_adObject::Instance()
{
    return new BrNode_adObject();
}

void BrNode_adObject::InitNode()
{
    
    BrNode_adRoot::InitNode();
    
    
    // Initialize parameters with default names and flags
    
    this->InitParameter(PID_ObjectType, "ObjectType");
    
    this->InitParameter(PID_FunctionalType, "FunctionalType");
    
    this->InitParameter(PID_FunctionalSubType, "FunctionalSubType");
    
    this->InitParameter(PID_StructuralType, "StructuralType");
    
    this->InitParameter(PID_StructuralSubType, "StructuralSubType");
    
    this->InitParameter(PID_BrEntityRef, "BrEntityRef");
    
    this->InitParameter(PID_Geometry, "Geometry");
    
}



// --- ObjectType ---

void BrNode_adObject::SetObjectType(const TCollection_ExtendedString& value)
{
    std::cout << "    [Setter] Entering SetObjectType for BrNode_adObject" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_ObjectType);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_ObjectType is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_ObjectType cannot be cast to Name!" << std::endl;
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

TCollection_ExtendedString BrNode_adObject::GetObjectType() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_ObjectType);
    if (p.IsNull()) return TCollection_ExtendedString();
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) return TCollection_ExtendedString();
    
    
    return typedP->GetValue();
    
}



// --- FunctionalType ---

void BrNode_adObject::SetFunctionalType(const TCollection_ExtendedString& value)
{
    std::cout << "    [Setter] Entering SetFunctionalType for BrNode_adObject" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_FunctionalType);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_FunctionalType is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_FunctionalType cannot be cast to Name!" << std::endl;
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

TCollection_ExtendedString BrNode_adObject::GetFunctionalType() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_FunctionalType);
    if (p.IsNull()) return TCollection_ExtendedString();
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) return TCollection_ExtendedString();
    
    
    return typedP->GetValue();
    
}



// --- FunctionalSubType ---

void BrNode_adObject::SetFunctionalSubType(const TCollection_ExtendedString& value)
{
    std::cout << "    [Setter] Entering SetFunctionalSubType for BrNode_adObject" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_FunctionalSubType);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_FunctionalSubType is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_FunctionalSubType cannot be cast to Name!" << std::endl;
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

TCollection_ExtendedString BrNode_adObject::GetFunctionalSubType() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_FunctionalSubType);
    if (p.IsNull()) return TCollection_ExtendedString();
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) return TCollection_ExtendedString();
    
    
    return typedP->GetValue();
    
}



// --- StructuralType ---

void BrNode_adObject::SetStructuralType(const TCollection_ExtendedString& value)
{
    std::cout << "    [Setter] Entering SetStructuralType for BrNode_adObject" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_StructuralType);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_StructuralType is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_StructuralType cannot be cast to Name!" << std::endl;
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

TCollection_ExtendedString BrNode_adObject::GetStructuralType() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_StructuralType);
    if (p.IsNull()) return TCollection_ExtendedString();
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) return TCollection_ExtendedString();
    
    
    return typedP->GetValue();
    
}



// --- StructuralSubType ---

void BrNode_adObject::SetStructuralSubType(const TCollection_ExtendedString& value)
{
    std::cout << "    [Setter] Entering SetStructuralSubType for BrNode_adObject" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_StructuralSubType);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_StructuralSubType is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_StructuralSubType cannot be cast to Name!" << std::endl;
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

TCollection_ExtendedString BrNode_adObject::GetStructuralSubType() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_StructuralSubType);
    if (p.IsNull()) return TCollection_ExtendedString();
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) return TCollection_ExtendedString();
    
    
    return typedP->GetValue();
    
}



// --- BrEntityRef ---

void BrNode_adObject::SetBrEntityRef(const TCollection_ExtendedString& value)
{
    std::cout << "    [Setter] Entering SetBrEntityRef for BrNode_adObject" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_BrEntityRef);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_BrEntityRef is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_BrEntityRef cannot be cast to Name!" << std::endl;
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

TCollection_ExtendedString BrNode_adObject::GetBrEntityRef() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_BrEntityRef);
    if (p.IsNull()) return TCollection_ExtendedString();
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) return TCollection_ExtendedString();
    
    
    return typedP->GetValue();
    
}



// --- Geometry ---

void BrNode_adObject::SetGeometry(const Handle(ActAPI_IDataCursor)& value)
{
    std::cout << "    [Setter] Entering SetGeometry for BrNode_adObject" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_Geometry);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_Geometry is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsReference(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_Geometry cannot be cast to Reference!" << std::endl;
        return;
    }

    try {
        
        if (!value.IsNull()) {
            typedP->SetTarget(value);
        }
        
        std::cout << "    [Setter] Value set successfully!" << std::endl;
    } catch (Standard_Failure& e) {
        std::cerr << "  [Setter] OCCT EXCEPTION: " << e.GetMessageString() << std::endl;
    } catch (...) {
        std::cerr << "  [Setter] UNKNOWN EXCEPTION!" << std::endl;
    }
}

Handle(ActAPI_IDataCursor) BrNode_adObject::GetGeometry() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_Geometry);
    if (p.IsNull()) return Handle(ActAPI_IDataCursor)();
    
    auto typedP = ActData_ParameterFactory::AsReference(p);
    if (typedP.IsNull()) return Handle(ActAPI_IDataCursor)();
    
    
    return typedP->GetTarget();
    
}



// Mandatory overrides for ActData_BaseNode
TCollection_ExtendedString BrNode_adObject::GetName()
{
    
    return BrNode_adRoot::GetName();
    
}

void BrNode_adObject::SetName(const TCollection_ExtendedString& theName)
{
    
    BrNode_adRoot::SetName(theName);
    
}

// --- Children management ---


void BrNode_adObject::AddPropertySets(const Handle(BrNode_adPropertySet)& node)
{
    TCollection_AsciiString entry1, entry2;
    TDF_Tool::Entry(this->RootLabel(), entry1);
    if (!node.IsNull()) TDF_Tool::Entry(node->RootLabel(), entry2);
    std::cout << "    [AddChild] " << entry1.ToCString() << " -> " << entry2.ToCString() << std::endl << std::flush;
    this->AddChildNode(node);
}

NCollection_Sequence<Handle(BrNode_adPropertySet)> BrNode_adObject::GetPropertySetsList() const
{
    NCollection_Sequence<Handle(BrNode_adPropertySet)> res;
    Handle(ActAPI_IChildIterator) it = this->GetChildIterator();
    if (!it.IsNull())
    {
        for (; it->More(); it->Next())
        {
            Handle(ActAPI_INode) childBase = it->Value();
            if (childBase.IsNull()) continue;
            
            Handle(BrNode_adPropertySet) child = Handle(BrNode_adPropertySet)::DownCast(childBase);
            if (!child.IsNull())
                res.Append(child);
        }
    }
    return res;
}



void BrNode_adObject::AddSubObjects(const Handle(BrNode_adObject)& node)
{
    TCollection_AsciiString entry1, entry2;
    TDF_Tool::Entry(this->RootLabel(), entry1);
    if (!node.IsNull()) TDF_Tool::Entry(node->RootLabel(), entry2);
    std::cout << "    [AddChild] " << entry1.ToCString() << " -> " << entry2.ToCString() << std::endl << std::flush;
    this->AddChildNode(node);
}

NCollection_Sequence<Handle(BrNode_adObject)> BrNode_adObject::GetSubObjectsList() const
{
    NCollection_Sequence<Handle(BrNode_adObject)> res;
    Handle(ActAPI_IChildIterator) it = this->GetChildIterator();
    if (!it.IsNull())
    {
        for (; it->More(); it->Next())
        {
            Handle(ActAPI_INode) childBase = it->Value();
            if (childBase.IsNull()) continue;
            
            Handle(BrNode_adObject) child = Handle(BrNode_adObject)::DownCast(childBase);
            if (!child.IsNull())
                res.Append(child);
        }
    }
    return res;
}

