#include "BrNode_adGeometry.h"
#include "ActData_ParameterFactory.h"
#include <TDF_Tool.hxx>
#include <TDF_Label.hxx>
#include <ActData_Utils.h>
#include <iostream>

// Register this node type with Active Data's NodeFactory
REGISTER_NODE_TYPE(BrNode_adGeometry)

BrNode_adGeometry::BrNode_adGeometry() : BrNode_adRoot()
{
    
    REGISTER_PARAMETER(Reference, PID_GeometryRef);
    
    REGISTER_PARAMETER(RealArray, PID_Transformation);
    
    REGISTER_PARAMETER(Int, PID_MaterialType);
    
    REGISTER_PARAMETER(Int, PID_Color);
    
    REGISTER_PARAMETER(Real, PID_Transparency);
    
    REGISTER_PARAMETER(Bool, PID_Visibility);
    
}

Handle(BrNode_adGeometry) BrNode_adGeometry::Instance()
{
    return new BrNode_adGeometry();
}

void BrNode_adGeometry::InitNode()
{
    
    BrNode_adRoot::InitNode();
    
    
    // Initialize parameters with default names and flags
    
    this->InitParameter(PID_GeometryRef, "GeometryRef");
    
    this->InitParameter(PID_Transformation, "Transformation");
    
    this->InitParameter(PID_MaterialType, "MaterialType");
    
    this->InitParameter(PID_Color, "Color");
    
    this->InitParameter(PID_Transparency, "Transparency");
    
    this->InitParameter(PID_Visibility, "Visibility");
    
}



// --- GeometryRef ---

void BrNode_adGeometry::SetGeometryRef(const Handle(ActAPI_IDataCursor)& value)
{
    std::cout << "    [Setter] Entering SetGeometryRef for BrNode_adGeometry" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_GeometryRef);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_GeometryRef is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsReference(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_GeometryRef cannot be cast to Reference!" << std::endl;
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

Handle(ActAPI_IDataCursor) BrNode_adGeometry::GetGeometryRef() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_GeometryRef);
    if (p.IsNull()) return Handle(ActAPI_IDataCursor)();
    
    auto typedP = ActData_ParameterFactory::AsReference(p);
    if (typedP.IsNull()) return Handle(ActAPI_IDataCursor)();
    
    
    return typedP->GetTarget();
    
}



// --- Transformation ---

void BrNode_adGeometry::SetTransformation(const Handle(TColStd_HArray1OfReal)& value)
{
    std::cout << "    [Setter] Entering SetTransformation for BrNode_adGeometry" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_Transformation);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_Transformation is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsRealArray(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_Transformation cannot be cast to RealArray!" << std::endl;
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

Handle(TColStd_HArray1OfReal) BrNode_adGeometry::GetTransformation() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_Transformation);
    if (p.IsNull()) return Handle(TColStd_HArray1OfReal)();
    
    auto typedP = ActData_ParameterFactory::AsRealArray(p);
    if (typedP.IsNull()) return Handle(TColStd_HArray1OfReal)();
    
    
    return typedP->GetArray();
    
}



// --- MaterialType ---

void BrNode_adGeometry::SetMaterialType(const int& value)
{
    std::cout << "    [Setter] Entering SetMaterialType for BrNode_adGeometry" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_MaterialType);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_MaterialType is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsInt(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_MaterialType cannot be cast to Int!" << std::endl;
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

int BrNode_adGeometry::GetMaterialType() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_MaterialType);
    if (p.IsNull()) return int();
    
    auto typedP = ActData_ParameterFactory::AsInt(p);
    if (typedP.IsNull()) return int();
    
    
    return typedP->GetValue();
    
}



// --- Color ---

void BrNode_adGeometry::SetColor(const int& value)
{
    std::cout << "    [Setter] Entering SetColor for BrNode_adGeometry" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_Color);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_Color is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsInt(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_Color cannot be cast to Int!" << std::endl;
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

int BrNode_adGeometry::GetColor() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_Color);
    if (p.IsNull()) return int();
    
    auto typedP = ActData_ParameterFactory::AsInt(p);
    if (typedP.IsNull()) return int();
    
    
    return typedP->GetValue();
    
}



// --- Transparency ---

void BrNode_adGeometry::SetTransparency(const double& value)
{
    std::cout << "    [Setter] Entering SetTransparency for BrNode_adGeometry" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_Transparency);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_Transparency is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsReal(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_Transparency cannot be cast to Real!" << std::endl;
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

double BrNode_adGeometry::GetTransparency() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_Transparency);
    if (p.IsNull()) return double();
    
    auto typedP = ActData_ParameterFactory::AsReal(p);
    if (typedP.IsNull()) return double();
    
    
    return typedP->GetValue();
    
}



// --- Visibility ---

void BrNode_adGeometry::SetVisibility(const bool& value)
{
    std::cout << "    [Setter] Entering SetVisibility for BrNode_adGeometry" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_Visibility);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_Visibility is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsBool(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_Visibility cannot be cast to Bool!" << std::endl;
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

bool BrNode_adGeometry::GetVisibility() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_Visibility);
    if (p.IsNull()) return bool();
    
    auto typedP = ActData_ParameterFactory::AsBool(p);
    if (typedP.IsNull()) return bool();
    
    
    return typedP->GetValue();
    
}



// Mandatory overrides for ActData_BaseNode
TCollection_ExtendedString BrNode_adGeometry::GetName()
{
    
    return BrNode_adRoot::GetName();
    
}

void BrNode_adGeometry::SetName(const TCollection_ExtendedString& theName)
{
    
    BrNode_adRoot::SetName(theName);
    
}

// --- Children management ---
