#include "BrNode_adPropertySet.h"
#include "ActData_ParameterFactory.h"
#include <TDF_Tool.hxx>
#include <TDF_Label.hxx>
#include <ActData_Utils.h>
#include <iostream>

// Register this node type with Active Data's NodeFactory
REGISTER_NODE_TYPE(BrNode_adPropertySet)

BrNode_adPropertySet::BrNode_adPropertySet() : BrNode_adRoot()
{
    
}

Handle(BrNode_adPropertySet) BrNode_adPropertySet::Instance()
{
    return new BrNode_adPropertySet();
}

void BrNode_adPropertySet::InitNode()
{
    
    BrNode_adRoot::InitNode();
    
    
    // Initialize parameters with default names and flags
    
}



// Mandatory overrides for ActData_BaseNode
TCollection_ExtendedString BrNode_adPropertySet::GetName()
{
    
    return BrNode_adRoot::GetName();
    
}

void BrNode_adPropertySet::SetName(const TCollection_ExtendedString& theName)
{
    
    BrNode_adRoot::SetName(theName);
    
}

// --- Children management ---


void BrNode_adPropertySet::AddProperties(const Handle(BrNode_adProperty)& node)
{
    TCollection_AsciiString entry1, entry2;
    TDF_Tool::Entry(this->RootLabel(), entry1);
    if (!node.IsNull()) TDF_Tool::Entry(node->RootLabel(), entry2);
    std::cout << "    [AddChild] " << entry1.ToCString() << " -> " << entry2.ToCString() << std::endl << std::flush;
    this->AddChildNode(node);
}

NCollection_Sequence<Handle(BrNode_adProperty)> BrNode_adPropertySet::GetPropertiesList() const
{
    NCollection_Sequence<Handle(BrNode_adProperty)> res;
    Handle(ActAPI_IChildIterator) it = this->GetChildIterator();
    if (!it.IsNull())
    {
        for (; it->More(); it->Next())
        {
            Handle(ActAPI_INode) childBase = it->Value();
            if (childBase.IsNull()) continue;
            
            Handle(BrNode_adProperty) child = Handle(BrNode_adProperty)::DownCast(childBase);
            if (!child.IsNull())
                res.Append(child);
        }
    }
    return res;
}

