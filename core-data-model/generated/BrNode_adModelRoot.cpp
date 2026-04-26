#include "BrNode_adModelRoot.h"
#include "ActData_ParameterFactory.h"
#include <TDF_Tool.hxx>
#include <TDF_Label.hxx>
#include <ActData_Utils.h>
#include <iostream>

// Register this node type with Active Data's NodeFactory
REGISTER_NODE_TYPE(BrNode_adModelRoot)

BrNode_adModelRoot::BrNode_adModelRoot() : BrNode_adRoot()
{
    
}

Handle(BrNode_adModelRoot) BrNode_adModelRoot::Instance()
{
    return new BrNode_adModelRoot();
}

void BrNode_adModelRoot::InitNode()
{
    
    BrNode_adRoot::InitNode();
    
    
    // Initialize parameters with default names and flags
    
}



// Mandatory overrides for ActData_BaseNode
TCollection_ExtendedString BrNode_adModelRoot::GetName()
{
    
    return BrNode_adRoot::GetName();
    
}

void BrNode_adModelRoot::SetName(const TCollection_ExtendedString& theName)
{
    
    BrNode_adRoot::SetName(theName);
    
}

// --- Children management ---


void BrNode_adModelRoot::AddSubObjects(const Handle(BrNode_adObject)& node)
{
    TCollection_AsciiString entry1, entry2;
    TDF_Tool::Entry(this->RootLabel(), entry1);
    if (!node.IsNull()) TDF_Tool::Entry(node->RootLabel(), entry2);
    std::cout << "    [AddChild] " << entry1.ToCString() << " -> " << entry2.ToCString() << std::endl << std::flush;
    this->AddChildNode(node);
}

NCollection_Sequence<Handle(BrNode_adObject)> BrNode_adModelRoot::GetSubObjectsList() const
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

