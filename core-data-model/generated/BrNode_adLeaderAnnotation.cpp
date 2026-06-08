#include "BrNode_adLeaderAnnotation.h"
#include "ActData_ParameterFactory.h"
#include <TDF_Tool.hxx>
#include <TDF_Label.hxx>
#include <ActData_Utils.h>
#include <iostream>

// Register this node type with Active Data's NodeFactory
REGISTER_NODE_TYPE(BrNode_adLeaderAnnotation)

BrNode_adLeaderAnnotation::BrNode_adLeaderAnnotation() : BrNode_adRepresentation2D()
{
    
    REGISTER_PARAMETER(Name, PID_NoteText);
    
    REGISTER_PARAMETER(RealArray, PID_ArrowPoint);
    
    REGISTER_PARAMETER(RealArray, PID_TextPoint);
    
}

Handle(BrNode_adLeaderAnnotation) BrNode_adLeaderAnnotation::Instance()
{
    return new BrNode_adLeaderAnnotation();
}

void BrNode_adLeaderAnnotation::InitNode()
{
    
    BrNode_adRepresentation2D::InitNode();
    
    
    // Initialize parameters with default names and flags
    
    this->InitParameter(PID_NoteText, "NoteText");
    
    this->InitParameter(PID_ArrowPoint, "ArrowPoint");
    
    this->InitParameter(PID_TextPoint, "TextPoint");
    
}



// --- NoteText ---

void BrNode_adLeaderAnnotation::SetNoteText(const TCollection_ExtendedString& value)
{
    std::cout << "    [Setter] Entering SetNoteText for BrNode_adLeaderAnnotation" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_NoteText);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_NoteText is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_NoteText cannot be cast to Name!" << std::endl;
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

TCollection_ExtendedString BrNode_adLeaderAnnotation::GetNoteText() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_NoteText);
    if (p.IsNull()) return TCollection_ExtendedString();
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) return TCollection_ExtendedString();
    
    
    return typedP->GetValue();
    
}



// --- ArrowPoint ---

void BrNode_adLeaderAnnotation::SetArrowPoint(const Handle(TColStd_HArray1OfReal)& value)
{
    std::cout << "    [Setter] Entering SetArrowPoint for BrNode_adLeaderAnnotation" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_ArrowPoint);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_ArrowPoint is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsRealArray(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_ArrowPoint cannot be cast to RealArray!" << std::endl;
        return;
    }

    try {
        
        typedP->SetArray(value);
        
        std::cout << "    [Setter] Value set successfully!" << std::endl;
    } catch (Standard_Failure& e) {
        std::cerr << "  [Setter] OCCT EXCEPTION: " << e.GetMessageString() << std::endl;
    } catch (...) {
        std::cerr << "  [Setter] UNKNOWN EXCEPTION!" << std::endl;
    }
}

Handle(TColStd_HArray1OfReal) BrNode_adLeaderAnnotation::GetArrowPoint() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_ArrowPoint);
    if (p.IsNull()) return Handle(TColStd_HArray1OfReal)();
    
    auto typedP = ActData_ParameterFactory::AsRealArray(p);
    if (typedP.IsNull()) return Handle(TColStd_HArray1OfReal)();
    
    
    return typedP->GetArray();
    
}



// --- TextPoint ---

void BrNode_adLeaderAnnotation::SetTextPoint(const Handle(TColStd_HArray1OfReal)& value)
{
    std::cout << "    [Setter] Entering SetTextPoint for BrNode_adLeaderAnnotation" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_TextPoint);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_TextPoint is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsRealArray(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_TextPoint cannot be cast to RealArray!" << std::endl;
        return;
    }

    try {
        
        typedP->SetArray(value);
        
        std::cout << "    [Setter] Value set successfully!" << std::endl;
    } catch (Standard_Failure& e) {
        std::cerr << "  [Setter] OCCT EXCEPTION: " << e.GetMessageString() << std::endl;
    } catch (...) {
        std::cerr << "  [Setter] UNKNOWN EXCEPTION!" << std::endl;
    }
}

Handle(TColStd_HArray1OfReal) BrNode_adLeaderAnnotation::GetTextPoint() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_TextPoint);
    if (p.IsNull()) return Handle(TColStd_HArray1OfReal)();
    
    auto typedP = ActData_ParameterFactory::AsRealArray(p);
    if (typedP.IsNull()) return Handle(TColStd_HArray1OfReal)();
    
    
    return typedP->GetArray();
    
}



// Mandatory overrides for ActData_BaseNode
TCollection_ExtendedString BrNode_adLeaderAnnotation::GetName()
{
    
    return BrNode_adRepresentation2D::GetName();
    
}

void BrNode_adLeaderAnnotation::SetName(const TCollection_ExtendedString& theName)
{
    
    BrNode_adRepresentation2D::SetName(theName);
    
}

// --- Children management ---
