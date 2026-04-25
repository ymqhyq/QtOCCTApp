#include "BrNode_PropertySet.h"

// OCCT RTTI
IMPLEMENT_STANDARD_RTTIEXT(BrNode_PropertySet, ActData_BaseNode)

Handle(BrNode_PropertySet) BrNode_PropertySet::Instance()
{
    return new BrNode_PropertySet();
}

void BrNode_PropertySet::InitNode()
{
    // Initialize parameters
    
    this->InitParameter(PID_SetName, "SetName");
    
}

TCollection_ExtendedString BrNode_PropertySet::GetName()
{
    return "BrNode_PropertySet";
}

void BrNode_PropertySet::SetName(const TCollection_ExtendedString& /*theName*/)
{
}


// --- SetName ---

void BrNode_PropertySet::SetSetName(const TCollection_AsciiString& value)
{
    ActData_ParameterFactory::AsAsciiString(this->Parameter(PID_SetName))->SetValue(value);
}

TCollection_AsciiString BrNode_PropertySet::GetSetName() const
{
    return ActData_ParameterFactory::AsAsciiString(this->Parameter(PID_SetName))->GetValue();
}


// --- Children management ---


void BrNode_PropertySet::AddProperties(const Handle(BrNode_Property)& node)
{
    this->AddChildNode(node);
}

NCollection_Sequence<Handle(BrNode_Property)> BrNode_PropertySet::GetPropertiesList() const
{
    NCollection_Sequence<Handle(BrNode_Property)> res;
    Handle(ActAPI_IChildIterator) it = this->GetChildIterator();
    if (!it.IsNull())
    {
        for (; it->More(); it->Next())
        {
            Handle(BrNode_Property) child = Handle(BrNode_Property)::DownCast(it->Value());
            if (!child.IsNull())
                res.Append(child);
        }
    }
    return res;
}

