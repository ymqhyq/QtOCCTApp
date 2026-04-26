#include "DataModel.h"
#include <ActData_BaseNode.h>
#include <ActData_BasePartition.h>
#include <ActData_NodeFactory.h>
#include <ActData_ParameterFactory.h>
#include <TDF_Tool.hxx>
#include <TDF_TagSource.hxx>
#include <iostream>





  

  

  
#include "BrNode_adRoot.h"
  

  
#include "BrNode_adModelRoot.h"
  

  
#include "BrNode_adGeometricDef.h"
  

  
#include "BrNode_adGeometry.h"
  

  
#include "BrNode_adProperty.h"
  

  
#include "BrNode_adPropertySet.h"
  

  
#include "BrNode_adObject.h"
  

  

  




#include "BrNode_Partition.h"

DataModel::DataModel() : ActData_BaseModel() {}

void DataModel::initPartitions() {
    this->OpenCommand();
    
    std::cout << "[DataModel] initPartitions started." << std::endl;
    
    // 1. Initialize Root node
    m_root = BrNode_adModelRoot::Instance();
    std::cout << "[DataModel] m_root instance created: " << m_root.get() << std::endl;
    std::cout << "[DataModel] m_root DynamicType: " << m_root->DynamicType()->Name() << std::endl;
    
    std::cout << "[DataModel] Expanding m_root with Document()->Main()..." << std::endl;
    TDF_Label mainLab = this->Document()->Main();
    
    // Use the accessor hack to call protected expandOn
    BrNode_NodeAccessor::Expand(m_root, mainLab);
    
    std::cout << "[DataModel] m_root expanded." << std::endl;
    if (m_root->RootLabel().IsNull()) {
        std::cerr << "[DataModel] ERROR: m_root RootLabel is NULL after settle!" << std::endl;
    } else {
        TCollection_AsciiString entry;
        TDF_Tool::Entry(m_root->RootLabel(), entry);
        std::cout << "[DataModel] m_root RootLabel: " << entry.ToCString() << std::endl;
    }
    
    std::cout << "[DataModel] Initializing m_root..." << std::endl;
    m_root->InitNode();
    m_root->SetName("ModelRoot");

    // 2. Register partitions
    
    {
        std::cout << "[DataModel] Registering partition GeometryDefinitions (ID: 1)..." << std::endl;
        TDF_Label partLab = m_root->RootLabel().FindChild(1, Standard_True);
        Handle(BrNode_Partition) part = BrNode_Partition::Instance();
        part->Settle(partLab);
        this->RegisterPartition(1, part);
    }
    
    {
        std::cout << "[DataModel] Registering partition Topology (ID: 2)..." << std::endl;
        TDF_Label partLab = m_root->RootLabel().FindChild(2, Standard_True);
        Handle(BrNode_Partition) part = BrNode_Partition::Instance();
        part->Settle(partLab);
        this->RegisterPartition(2, part);
    }
    
    {
        std::cout << "[DataModel] Registering partition Resources (ID: 3)..." << std::endl;
        TDF_Label partLab = m_root->RootLabel().FindChild(3, Standard_True);
        Handle(BrNode_Partition) part = BrNode_Partition::Instance();
        part->Settle(partLab);
        this->RegisterPartition(3, part);
    }
    
    {
        std::cout << "[DataModel] Registering partition Auxiliary (ID: 4)..." << std::endl;
        TDF_Label partLab = m_root->RootLabel().FindChild(4, Standard_True);
        Handle(BrNode_Partition) part = BrNode_Partition::Instance();
        part->Settle(partLab);
        this->RegisterPartition(4, part);
    }
    
    {
        std::cout << "[DataModel] Registering partition Results (ID: 5)..." << std::endl;
        TDF_Label partLab = m_root->RootLabel().FindChild(5, Standard_True);
        Handle(BrNode_Partition) part = BrNode_Partition::Instance();
        part->Settle(partLab);
        this->RegisterPartition(5, part);
    }
    
    
    std::cout << "[DataModel] initPartitions finished." << std::endl;
    this->CommitCommand();
}

TDF_Label DataModel::GetPartitionLabel(const PartitionID pid) const {
    Handle(ActAPI_IPartition) part = this->Partition((Standard_Integer) pid);
    if ( part.IsNull() ) return TDF_Label();
    return part->RootLabel();
}







Handle(BrNode_adRoot) DataModel::AddadRoot() {
    // 自动分配分区
    
    
    
    Handle(ActAPI_IPartition) part = this->Partition((Standard_Integer) PID_Topology);
    if ( part.IsNull() ) return nullptr;
    
    Handle(BrNode_adRoot) node = BrNode_adRoot::Instance();
    
    // Use partition to add node (this handles expandOn and Tree Node structure)
    part->AddNode(node);
    
    // Initialize parameters
    node->InitNode();
    
    return node;
}



Handle(BrNode_adModelRoot) DataModel::AddadModelRoot() {
    // 自动分配分区
    
    
    
    Handle(ActAPI_IPartition) part = this->Partition((Standard_Integer) PID_Topology);
    if ( part.IsNull() ) return nullptr;
    
    Handle(BrNode_adModelRoot) node = BrNode_adModelRoot::Instance();
    
    // Use partition to add node (this handles expandOn and Tree Node structure)
    part->AddNode(node);
    
    // Initialize parameters
    node->InitNode();
    
    return node;
}



Handle(BrNode_adGeometricDef) DataModel::AddadGeometricDef() {
    // 自动分配分区
    
    
    
    
    
    Handle(ActAPI_IPartition) part = this->Partition((Standard_Integer) PID_GeometryDefinitions);
    if ( part.IsNull() ) return nullptr;
    
    Handle(BrNode_adGeometricDef) node = BrNode_adGeometricDef::Instance();
    
    // Use partition to add node (this handles expandOn and Tree Node structure)
    part->AddNode(node);
    
    // Initialize parameters
    node->InitNode();
    
    return node;
}



Handle(BrNode_adGeometry) DataModel::AddadGeometry() {
    // 自动分配分区
    
    
    
    
    
    Handle(ActAPI_IPartition) part = this->Partition((Standard_Integer) PID_Topology);
    if ( part.IsNull() ) return nullptr;
    
    Handle(BrNode_adGeometry) node = BrNode_adGeometry::Instance();
    
    // Use partition to add node (this handles expandOn and Tree Node structure)
    part->AddNode(node);
    
    // Initialize parameters
    node->InitNode();
    
    return node;
}



Handle(BrNode_adProperty) DataModel::AddadProperty() {
    // 自动分配分区
    
    
    
    
    
    Handle(ActAPI_IPartition) part = this->Partition((Standard_Integer) PID_Topology);
    if ( part.IsNull() ) return nullptr;
    
    Handle(BrNode_adProperty) node = BrNode_adProperty::Instance();
    
    // Use partition to add node (this handles expandOn and Tree Node structure)
    part->AddNode(node);
    
    // Initialize parameters
    node->InitNode();
    
    return node;
}



Handle(BrNode_adPropertySet) DataModel::AddadPropertySet() {
    // 自动分配分区
    
    
    
    
    
    Handle(ActAPI_IPartition) part = this->Partition((Standard_Integer) PID_Topology);
    if ( part.IsNull() ) return nullptr;
    
    Handle(BrNode_adPropertySet) node = BrNode_adPropertySet::Instance();
    
    // Use partition to add node (this handles expandOn and Tree Node structure)
    part->AddNode(node);
    
    // Initialize parameters
    node->InitNode();
    
    return node;
}



Handle(BrNode_adObject) DataModel::AddadObject() {
    // 自动分配分区
    
    
    
    
    
    Handle(ActAPI_IPartition) part = this->Partition((Standard_Integer) PID_Topology);
    if ( part.IsNull() ) return nullptr;
    
    Handle(BrNode_adObject) node = BrNode_adObject::Instance();
    
    // Use partition to add node (this handles expandOn and Tree Node structure)
    part->AddNode(node);
    
    // Initialize parameters
    node->InitNode();
    
    return node;
}





