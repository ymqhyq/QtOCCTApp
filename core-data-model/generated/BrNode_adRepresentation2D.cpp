#include "BrNode_adRepresentation2D.h"
#include "ActData_ParameterFactory.h"
#include <TDF_Tool.hxx>
#include <TDF_Label.hxx>
#include <ActData_Utils.h>
#include <iostream>

// Register this node type with Active Data's NodeFactory
REGISTER_NODE_TYPE(BrNode_adRepresentation2D)

BrNode_adRepresentation2D::BrNode_adRepresentation2D() : BrNode_adRoot()
{
    
    REGISTER_PARAMETER(Name, PID_RepresentationType);
    
    REGISTER_PARAMETER(Shape, PID_GeneratedShape);
    
}

Handle(BrNode_adRepresentation2D) BrNode_adRepresentation2D::Instance()
{
    return new BrNode_adRepresentation2D();
}

void BrNode_adRepresentation2D::InitNode()
{
    
    BrNode_adRoot::InitNode();
    
    
    // Initialize parameters with default names and flags
    
    this->InitParameter(PID_RepresentationType, "RepresentationType");
    
    this->InitParameter(PID_GeneratedShape, "GeneratedShape");
    
}



// --- RepresentationType ---

void BrNode_adRepresentation2D::SetRepresentationType(const TCollection_ExtendedString& value)
{
    std::cout << "    [Setter] Entering SetRepresentationType for BrNode_adRepresentation2D" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_RepresentationType);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_RepresentationType is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_RepresentationType cannot be cast to Name!" << std::endl;
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

TCollection_ExtendedString BrNode_adRepresentation2D::GetRepresentationType() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_RepresentationType);
    if (p.IsNull()) return TCollection_ExtendedString();
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) return TCollection_ExtendedString();
    
    
    return typedP->GetValue();
    
}



// --- GeneratedShape ---

void BrNode_adRepresentation2D::SetGeneratedShape(const TopoDS_Shape& value)
{
    std::cout << "    [Setter] Entering SetGeneratedShape for BrNode_adRepresentation2D" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_GeneratedShape);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_GeneratedShape is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsShape(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_GeneratedShape cannot be cast to Shape!" << std::endl;
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

TopoDS_Shape BrNode_adRepresentation2D::GetGeneratedShape() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_GeneratedShape);
    if (p.IsNull()) return TopoDS_Shape();
    
    auto typedP = ActData_ParameterFactory::AsShape(p);
    if (typedP.IsNull()) return TopoDS_Shape();
    
    
    return typedP->GetShape();
    
}



// Mandatory overrides for ActData_BaseNode
TCollection_ExtendedString BrNode_adRepresentation2D::GetName()
{
    
    return BrNode_adRoot::GetName();
    
}

void BrNode_adRepresentation2D::SetName(const TCollection_ExtendedString& theName)
{
    
    BrNode_adRoot::SetName(theName);
    
}

// --- Children management ---
