#include "BrNode_adSlopeIndication.h"
#include "ActData_ParameterFactory.h"
#include <TDF_Tool.hxx>
#include <TDF_Label.hxx>
#include <ActData_Utils.h>
#include <iostream>

// Register this node type with Active Data's NodeFactory
REGISTER_NODE_TYPE(BrNode_adSlopeIndication)

BrNode_adSlopeIndication::BrNode_adSlopeIndication() : BrNode_adRepresentation2D()
{
    
    REGISTER_PARAMETER(Name, PID_TargetModelDoc);
    
    REGISTER_PARAMETER(Name, PID_TargetObjectID);
    
    REGISTER_PARAMETER(Shape, PID_ShoulderLine);
    
    REGISTER_PARAMETER(Shape, PID_ToeLine);
    
    REGISTER_PARAMETER(Real, PID_Spacing);
    
    REGISTER_PARAMETER(Real, PID_LongLineRatio);
    
    REGISTER_PARAMETER(Real, PID_ShortLineRatio);
    
}

Handle(BrNode_adSlopeIndication) BrNode_adSlopeIndication::Instance()
{
    return new BrNode_adSlopeIndication();
}

void BrNode_adSlopeIndication::InitNode()
{
    
    BrNode_adRepresentation2D::InitNode();
    
    
    // Initialize parameters with default names and flags
    
    this->InitParameter(PID_TargetModelDoc, "TargetModelDoc");
    
    this->InitParameter(PID_TargetObjectID, "TargetObjectID");
    
    this->InitParameter(PID_ShoulderLine, "ShoulderLine");
    
    this->InitParameter(PID_ToeLine, "ToeLine");
    
    this->InitParameter(PID_Spacing, "Spacing");
    
    this->InitParameter(PID_LongLineRatio, "LongLineRatio");
    
    this->InitParameter(PID_ShortLineRatio, "ShortLineRatio");
    
}



// --- TargetModelDoc ---

void BrNode_adSlopeIndication::SetTargetModelDoc(const TCollection_ExtendedString& value)
{
    std::cout << "    [Setter] Entering SetTargetModelDoc for BrNode_adSlopeIndication" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_TargetModelDoc);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_TargetModelDoc is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_TargetModelDoc cannot be cast to Name!" << std::endl;
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

TCollection_ExtendedString BrNode_adSlopeIndication::GetTargetModelDoc() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_TargetModelDoc);
    if (p.IsNull()) return TCollection_ExtendedString();
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) return TCollection_ExtendedString();
    
    
    return typedP->GetValue();
    
}



// --- TargetObjectID ---

void BrNode_adSlopeIndication::SetTargetObjectID(const TCollection_ExtendedString& value)
{
    std::cout << "    [Setter] Entering SetTargetObjectID for BrNode_adSlopeIndication" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_TargetObjectID);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_TargetObjectID is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_TargetObjectID cannot be cast to Name!" << std::endl;
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

TCollection_ExtendedString BrNode_adSlopeIndication::GetTargetObjectID() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_TargetObjectID);
    if (p.IsNull()) return TCollection_ExtendedString();
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) return TCollection_ExtendedString();
    
    
    return typedP->GetValue();
    
}



// --- ShoulderLine ---

void BrNode_adSlopeIndication::SetShoulderLine(const TopoDS_Shape& value)
{
    std::cout << "    [Setter] Entering SetShoulderLine for BrNode_adSlopeIndication" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_ShoulderLine);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_ShoulderLine is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsShape(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_ShoulderLine cannot be cast to Shape!" << std::endl;
        return;
    }

    try {
        
        typedP->SetShape(value);
        
        std::cout << "    [Setter] Value set successfully!" << std::endl;
    } catch (Standard_Failure& e) {
        std::cerr << "  [Setter] OCCT EXCEPTION: " << e.GetMessageString() << std::endl;
    } catch (...) {
        std::cerr << "  [Setter] UNKNOWN EXCEPTION!" << std::endl;
    }
}

TopoDS_Shape BrNode_adSlopeIndication::GetShoulderLine() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_ShoulderLine);
    if (p.IsNull()) return TopoDS_Shape();
    
    auto typedP = ActData_ParameterFactory::AsShape(p);
    if (typedP.IsNull()) return TopoDS_Shape();
    
    
    return typedP->GetShape();
    
}



// --- ToeLine ---

void BrNode_adSlopeIndication::SetToeLine(const TopoDS_Shape& value)
{
    std::cout << "    [Setter] Entering SetToeLine for BrNode_adSlopeIndication" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_ToeLine);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_ToeLine is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsShape(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_ToeLine cannot be cast to Shape!" << std::endl;
        return;
    }

    try {
        
        typedP->SetShape(value);
        
        std::cout << "    [Setter] Value set successfully!" << std::endl;
    } catch (Standard_Failure& e) {
        std::cerr << "  [Setter] OCCT EXCEPTION: " << e.GetMessageString() << std::endl;
    } catch (...) {
        std::cerr << "  [Setter] UNKNOWN EXCEPTION!" << std::endl;
    }
}

TopoDS_Shape BrNode_adSlopeIndication::GetToeLine() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_ToeLine);
    if (p.IsNull()) return TopoDS_Shape();
    
    auto typedP = ActData_ParameterFactory::AsShape(p);
    if (typedP.IsNull()) return TopoDS_Shape();
    
    
    return typedP->GetShape();
    
}



// --- Spacing ---

void BrNode_adSlopeIndication::SetSpacing(const double& value)
{
    std::cout << "    [Setter] Entering SetSpacing for BrNode_adSlopeIndication" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_Spacing);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_Spacing is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsReal(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_Spacing cannot be cast to Real!" << std::endl;
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

double BrNode_adSlopeIndication::GetSpacing() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_Spacing);
    if (p.IsNull()) return double();
    
    auto typedP = ActData_ParameterFactory::AsReal(p);
    if (typedP.IsNull()) return double();
    
    
    return typedP->GetValue();
    
}



// --- LongLineRatio ---

void BrNode_adSlopeIndication::SetLongLineRatio(const double& value)
{
    std::cout << "    [Setter] Entering SetLongLineRatio for BrNode_adSlopeIndication" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_LongLineRatio);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_LongLineRatio is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsReal(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_LongLineRatio cannot be cast to Real!" << std::endl;
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

double BrNode_adSlopeIndication::GetLongLineRatio() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_LongLineRatio);
    if (p.IsNull()) return double();
    
    auto typedP = ActData_ParameterFactory::AsReal(p);
    if (typedP.IsNull()) return double();
    
    
    return typedP->GetValue();
    
}



// --- ShortLineRatio ---

void BrNode_adSlopeIndication::SetShortLineRatio(const double& value)
{
    std::cout << "    [Setter] Entering SetShortLineRatio for BrNode_adSlopeIndication" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_ShortLineRatio);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_ShortLineRatio is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsReal(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_ShortLineRatio cannot be cast to Real!" << std::endl;
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

double BrNode_adSlopeIndication::GetShortLineRatio() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_ShortLineRatio);
    if (p.IsNull()) return double();
    
    auto typedP = ActData_ParameterFactory::AsReal(p);
    if (typedP.IsNull()) return double();
    
    
    return typedP->GetValue();
    
}



// Mandatory overrides for ActData_BaseNode
TCollection_ExtendedString BrNode_adSlopeIndication::GetName()
{
    
    return BrNode_adRepresentation2D::GetName();
    
}

void BrNode_adSlopeIndication::SetName(const TCollection_ExtendedString& theName)
{
    
    BrNode_adRepresentation2D::SetName(theName);
    
}

// --- Children management ---
