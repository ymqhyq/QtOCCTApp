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
    
    REGISTER_PARAMETER(Name, PID_ParamGeoID);
    
    REGISTER_PARAMETER(Name, PID_GeoParameter);
    
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
    
    this->InitParameter(PID_ParamGeoID, "ParamGeoID");
    
    this->InitParameter(PID_GeoParameter, "GeoParameter");
    
    this->InitParameter(PID_Shape, "Shape");
    
}



// --- ParamGeoID ---

void BrNode_adGeometricDef::SetParamGeoID(const TCollection_ExtendedString& value)
{
    std::cout << "    [Setter] Entering SetParamGeoID for BrNode_adGeometricDef" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_ParamGeoID);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_ParamGeoID is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_ParamGeoID cannot be cast to Name!" << std::endl;
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

TCollection_ExtendedString BrNode_adGeometricDef::GetParamGeoID() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_ParamGeoID);
    if (p.IsNull()) return TCollection_ExtendedString();
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) return TCollection_ExtendedString();
    
    
    return typedP->GetValue();
    
}



// --- GeoParameter ---

void BrNode_adGeometricDef::SetGeoParameter(const TCollection_ExtendedString& value)
{
    std::cout << "    [Setter] Entering SetGeoParameter for BrNode_adGeometricDef" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_GeoParameter);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_GeoParameter is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_GeoParameter cannot be cast to Name!" << std::endl;
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

TCollection_ExtendedString BrNode_adGeometricDef::GetGeoParameter() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_GeoParameter);
    if (p.IsNull()) return TCollection_ExtendedString();
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) return TCollection_ExtendedString();
    
    
    return typedP->GetValue();
    
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
