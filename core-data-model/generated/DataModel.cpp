#include "DataModel.h"
#include <ActData_BaseNode.h>
#include <ActData_BasePartition.h>


  

  
#include "BrNode_GeometricDefinition.h"
  

  
#include "BrNode_Property.h"
  

  
#include "BrNode_PropertySet.h"
  

  
#include "BrNode_ActiveObject.h"
  

  

  

  


IMPLEMENT_STANDARD_RTTIEXT(DataModel, ActData_BaseModel)

DataModel::DataModel() : ActData_BaseModel() {}

void DataModel::InitPartitions() {
    Handle(ActData_BaseNode) rootNode = Handle(ActData_BaseNode)::DownCast(this->GetRootNode());
    if ( rootNode.IsNull() ) return;
    
    TDF_Label rootLab = rootNode->RootLabel();
    
    rootLab.FindChild(1, Standard_True);
    
    rootLab.FindChild(2, Standard_True);
    
    rootLab.FindChild(3, Standard_True);
    
    rootLab.FindChild(4, Standard_True);
    
    rootLab.FindChild(5, Standard_True);
    
}

TDF_Label DataModel::GetPartitionLabel(const PartitionID pid) const {
    Handle(ActData_BaseNode) rootNode = Handle(ActData_BaseNode)::DownCast(this->GetRootNode());
    if ( rootNode.IsNull() ) return TDF_Label();
    return rootNode->RootLabel().FindChild((Standard_Integer) pid, Standard_False);
}





Handle(BrNode_GeometricDefinition) DataModel::AddGeometricDefinition() {
    
    Handle(ActData_BasePartition) part = Handle(ActData_BasePartition)::DownCast(this->Partition((Standard_Integer) PID_GeometryDefinitions));
    
    if ( part.IsNull() ) return nullptr;

    ActAPI_DataObjectId nodeId = part->AddNode(BrNode_GeometricDefinition::Instance());
    Handle(BrNode_GeometricDefinition) resNode = Handle(BrNode_GeometricDefinition)::DownCast(this->FindNode(nodeId));
    if (!resNode.IsNull()) {
        resNode->InitNode();
    }
    return resNode;
}



Handle(BrNode_Property) DataModel::AddProperty() {
    
    Handle(ActData_BasePartition) part = Handle(ActData_BasePartition)::DownCast(this->Partition((Standard_Integer) PID_Topology));
    
    if ( part.IsNull() ) return nullptr;

    ActAPI_DataObjectId nodeId = part->AddNode(BrNode_Property::Instance());
    Handle(BrNode_Property) resNode = Handle(BrNode_Property)::DownCast(this->FindNode(nodeId));
    if (!resNode.IsNull()) {
        resNode->InitNode();
    }
    return resNode;
}



Handle(BrNode_PropertySet) DataModel::AddPropertySet() {
    
    Handle(ActData_BasePartition) part = Handle(ActData_BasePartition)::DownCast(this->Partition((Standard_Integer) PID_Topology));
    
    if ( part.IsNull() ) return nullptr;

    ActAPI_DataObjectId nodeId = part->AddNode(BrNode_PropertySet::Instance());
    Handle(BrNode_PropertySet) resNode = Handle(BrNode_PropertySet)::DownCast(this->FindNode(nodeId));
    if (!resNode.IsNull()) {
        resNode->InitNode();
    }
    return resNode;
}



Handle(BrNode_ActiveObject) DataModel::AddActiveObject() {
    
    Handle(ActData_BasePartition) part = Handle(ActData_BasePartition)::DownCast(this->Partition((Standard_Integer) PID_Topology));
    
    if ( part.IsNull() ) return nullptr;

    ActAPI_DataObjectId nodeId = part->AddNode(BrNode_ActiveObject::Instance());
    Handle(BrNode_ActiveObject) resNode = Handle(BrNode_ActiveObject)::DownCast(this->FindNode(nodeId));
    if (!resNode.IsNull()) {
        resNode->InitNode();
    }
    return resNode;
}







