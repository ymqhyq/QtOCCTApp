#include "BrNode_adDrawing2D.h"
#include "ActData_ParameterFactory.h"
#include <TDF_Tool.hxx>
#include <TDF_Label.hxx>
#include <ActData_Utils.h>
#include <iostream>

// Register this node type with Active Data's NodeFactory
REGISTER_NODE_TYPE(BrNode_adDrawing2D)

BrNode_adDrawing2D::BrNode_adDrawing2D() : BrNode_adRoot()
{
    
    REGISTER_PARAMETER(Name, PID_DrawingType);
    
    REGISTER_PARAMETER(Real, PID_Scale);
    
}

Handle(BrNode_adDrawing2D) BrNode_adDrawing2D::Instance()
{
    return new BrNode_adDrawing2D();
}

void BrNode_adDrawing2D::InitNode()
{
    
    BrNode_adRoot::InitNode();
    
    
    // Initialize parameters with default names and flags
    
    this->InitParameter(PID_DrawingType, "DrawingType");
    
    this->InitParameter(PID_Scale, "Scale");
    
}



// --- DrawingType ---

void BrNode_adDrawing2D::SetDrawingType(const TCollection_ExtendedString& value)
{
    std::cout << "    [Setter] Entering SetDrawingType for BrNode_adDrawing2D" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_DrawingType);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_DrawingType is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_DrawingType cannot be cast to Name!" << std::endl;
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

TCollection_ExtendedString BrNode_adDrawing2D::GetDrawingType() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_DrawingType);
    if (p.IsNull()) return TCollection_ExtendedString();
    
    auto typedP = ActData_ParameterFactory::AsName(p);
    if (typedP.IsNull()) return TCollection_ExtendedString();
    
    
    return typedP->GetValue();
    
}



// --- Scale ---

void BrNode_adDrawing2D::SetScale(const double& value)
{
    std::cout << "    [Setter] Entering SetScale for BrNode_adDrawing2D" << std::endl;
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_Scale);

    if (p.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_Scale is NULL!" << std::endl;
        return;
    }
    
    auto typedP = ActData_ParameterFactory::AsReal(p);
    if (typedP.IsNull()) {
        std::cerr << "  [Setter] ERROR: Parameter PID_Scale cannot be cast to Real!" << std::endl;
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

double BrNode_adDrawing2D::GetScale() const
{
    Handle(ActAPI_IUserParameter) p = this->Parameter(PID_Scale);
    if (p.IsNull()) return double();
    
    auto typedP = ActData_ParameterFactory::AsReal(p);
    if (typedP.IsNull()) return double();
    
    
    return typedP->GetValue();
    
}



// Mandatory overrides for ActData_BaseNode
TCollection_ExtendedString BrNode_adDrawing2D::GetName()
{
    
    return BrNode_adRoot::GetName();
    
}

void BrNode_adDrawing2D::SetName(const TCollection_ExtendedString& theName)
{
    
    BrNode_adRoot::SetName(theName);
    
}

// --- Children management ---


void BrNode_adDrawing2D::AddRepresentations(const Handle(BrNode_adRepresentation2D)& node)
{
    TCollection_AsciiString entry1, entry2;
    TDF_Tool::Entry(this->RootLabel(), entry1);
    if (!node.IsNull()) TDF_Tool::Entry(node->RootLabel(), entry2);
    std::cout << "    [AddChild] " << entry1.ToCString() << " -> " << entry2.ToCString() << std::endl << std::flush;
    this->AddChildNode(node);
}

NCollection_Sequence<Handle(BrNode_adRepresentation2D)> BrNode_adDrawing2D::GetRepresentationsList() const
{
    NCollection_Sequence<Handle(BrNode_adRepresentation2D)> res;
    Handle(ActAPI_IChildIterator) it = this->GetChildIterator();
    if (!it.IsNull())
    {
        for (; it->More(); it->Next())
        {
            Handle(ActAPI_INode) childBase = it->Value();
            if (childBase.IsNull()) continue;
            
            Handle(BrNode_adRepresentation2D) child = Handle(BrNode_adRepresentation2D)::DownCast(childBase);
            if (!child.IsNull())
                res.Append(child);
        }
    }
    return res;
}



void BrNode_adDrawing2D::AddLeaderAnnotations(const Handle(BrNode_adLeaderAnnotation)& node)
{
    TCollection_AsciiString entry1, entry2;
    TDF_Tool::Entry(this->RootLabel(), entry1);
    if (!node.IsNull()) TDF_Tool::Entry(node->RootLabel(), entry2);
    std::cout << "    [AddChild] " << entry1.ToCString() << " -> " << entry2.ToCString() << std::endl << std::flush;
    this->AddChildNode(node);
}

NCollection_Sequence<Handle(BrNode_adLeaderAnnotation)> BrNode_adDrawing2D::GetLeaderAnnotationsList() const
{
    NCollection_Sequence<Handle(BrNode_adLeaderAnnotation)> res;
    Handle(ActAPI_IChildIterator) it = this->GetChildIterator();
    if (!it.IsNull())
    {
        for (; it->More(); it->Next())
        {
            Handle(ActAPI_INode) childBase = it->Value();
            if (childBase.IsNull()) continue;
            
            Handle(BrNode_adLeaderAnnotation) child = Handle(BrNode_adLeaderAnnotation)::DownCast(childBase);
            if (!child.IsNull())
                res.Append(child);
        }
    }
    return res;
}

