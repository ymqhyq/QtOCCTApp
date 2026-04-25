#ifndef DataModel_HeaderFile
#define DataModel_HeaderFile

#include <Standard_Boolean.hxx>
#include <Standard_Handle.hxx>
#include <ActData_BaseModel.h>
#include <TDF_Label.hxx>


  

  
#include "BrNode_GeometricDefinition.h"
  

  
#include "BrNode_Property.h"
  

  
#include "BrNode_PropertySet.h"
  

  
#include "BrNode_ActiveObject.h"
  

  

  

  


/**
 * @brief 自动生成的业务模型管理类
 */
class DataModel : public ActData_BaseModel
{
public:
    DEFINE_STANDARD_RTTIEXT(DataModel, ActData_BaseModel)

public:
    enum PartitionID {
        
        PID_GeometryDefinitions = 1,
        
        PID_Topology = 2,
        
        PID_Resources = 3,
        
        PID_Auxiliary = 4,
        
        PID_Results = 5,
        
    };

    DataModel();

    void InitPartitions();
    TDF_Label GetPartitionLabel(const PartitionID pid) const;

    
    
    
    
    Handle(BrNode_GeometricDefinition) AddGeometricDefinition();
    
    
    
    Handle(BrNode_Property) AddProperty();
    
    
    
    Handle(BrNode_PropertySet) AddPropertySet();
    
    
    
    Handle(BrNode_ActiveObject) AddActiveObject();
    
    
    
    
    
    
    
    
};

#endif