#include "BrNode_adGeometricDef.h"
#include "ActData_ParameterFactory.h"
#include <TDF_Tool.hxx>
#include <TDF_Label.hxx>
#include <ActData_Utils.h>
#include <iostream>

// Register this node type with Active Data's NodeFactory
REGISTER_NODE_TYPE(BrNode_adGeometricDef)

BrNode_adGeometricDef::BrNode_adGeometricDef() : BrNode_adRoot()
{
    
    REGISTER_PARAMETER(Shape, PID_Shape);
    
}

Handle(BrNode_adGeometricDef) BrNode_adGeometricDef::Instance()
{
    return new BrNode_adGeometricDef();
}

void BrNode_adGeometricDef::InitNode()
{
    
    BrNode_adRoot::InitNode();
    
    
    // Initialize parameters with default names and flags
    
    this->InitParameter(PID_Shape, "Shape");
    
}



// --- Shape ---

void BrNode_adGeometricDef::SetShape(const TopoDS_Shape& value)
{
    std::cout << "    [Setter] Entering SetShape for BrNode_adGeometricDef" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_Shape);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_Shape is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsShape(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_Shape cannot be cast to Shape!" << std::endl;
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

TopoDS_Shape BrNode_adGeometricDef::GetShape() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_Shape);
    if (p.IsNull()) return TopoDS_Shape();
    
    auto typedP = ActData_ParameterFactory::AsShape(p);
    if (typedP.IsNull()) return TopoDS_Shape();
    
    
    return typedP->GetShape();
    
}



// Mandatory overrides for ActData_BaseNode
TCollection_ExtendedString BrNode_adGeometricDef::GetName()
{
    
    return BrNode_adRoot::GetName();
    
}

void BrNode_adGeometricDef::SetName(const TCollection_ExtendedString& theName)
{
    
    BrNode_adRoot::SetName(theName);
    
}

// --- Children management ---
