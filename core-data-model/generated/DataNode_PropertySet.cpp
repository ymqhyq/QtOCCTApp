#include "DataNode_PropertySet.h"


#include "DataNode_Property.h"


// OCCT RTTI
IMPLEMENT_STANDARD_RTTI_EXT(DataNode_PropertySet, ActData_BaseNode)

Handle(DataNode_PropertySet) DataNode_PropertySet::Instance()
{
    return new DataNode_PropertySet();
}

void DataNode_PropertySet::Init()
{
    // Initialize parent
    ActData_BaseNode::Init();

    // Initialize parameters
    
    this->InitParameter(PID_SetName, "属性集名称");
    
}


// --- 属性集名称 ---

void DataNode_PropertySet::SetSetName(const TCollection_AsciiString& value)
{
    ActData_ParameterFactory::AsString(this->Parameter(PID_SetName))->SetValue(value);
}

TCollection_AsciiString DataNode_PropertySet::GetSetName() const
{
    return ActData_ParameterFactory::AsString(this->Parameter(PID_SetName))->GetValue();
}


// --- Children management ---



void DataNode_PropertySet::AddProperties(const Handle(DataNode_Property)& node)
{
    this->AddChildNode(node);
}

NCollection_Sequence<Handle(DataNode_Property)> DataNode_PropertySet::GetPropertiesList() const
{
    NCollection_Sequence<Handle(DataNode_Property)> result;
    // Implementation: Filter children by type
    for (int i = 1; i <= this->GetNbChildren(); ++i) {
        Handle(DataNode_Property) c = Handle(DataNode_Property)::DownCast(this->GetChild(i));
        if (!c.IsNull()) result.Append(c);
    }
    return result;
}

