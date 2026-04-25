#include "DataNode_Partitions.h"



// OCCT RTTI
IMPLEMENT_STANDARD_RTTI_EXT(DataNode_Partitions, ActData_BaseNode)

Handle(DataNode_Partitions) DataNode_Partitions::Instance()
{
    return new DataNode_Partitions();
}

void DataNode_Partitions::Init()
{
    // Initialize parent
    ActData_BaseNode::Init();

    // Initialize parameters
    
}



// --- Children management ---

