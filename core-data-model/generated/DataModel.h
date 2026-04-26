#ifndef BR_EXPORT
#ifdef BR_EXPORT_EXPORTS
  #define BR_EXPORT __declspec(dllexport)
#else
  #define BR_EXPORT __declspec(dllimport)
#endif
#endif

#ifndef DataModel_HeaderFile
#define DataModel_HeaderFile

#include <Standard_Boolean.hxx>
#include <Standard_Handle.hxx>
#include <ActData_BaseModel.h>
#include <ActAPI_IPartition.h>
#include <ActAPI_INode.h>
#include <ActData_CAFConverter.h>
#include <TDF_Label.hxx>


  

  

  
#include "BrNode_adRoot.h"
  

  
#include "BrNode_adModelRoot.h"
  

  
#include "BrNode_adGeometricDef.h"
  

  
#include "BrNode_adGeometry.h"
  

  
#include "BrNode_adProperty.h"
  

  
#include "BrNode_adPropertySet.h"
  

  
#include "BrNode_adObject.h"
  

  

  


/**
 * @brief 自动生成的业务模型管理类
 */
class BR_EXPORT DataModel : public ActData_BaseModel
{
public:
    // OCCT RTTI
    DEFINE_STANDARD_RTTI_INLINE(DataModel, ActData_BaseModel)

public:
    enum PartitionID {
        
        PID_GeometryDefinitions = 1,
        
        PID_Topology = 2,
        
        PID_Resources = 3,
        
        PID_Auxiliary = 4,
        
        PID_Results = 5,
        
    };

    DataModel();

    TDF_Label GetPartitionLabel(const PartitionID pid) const;

    
    
    
    
    
    
    Handle(BrNode_adRoot) AddadRoot();
    
    
    
    Handle(BrNode_adModelRoot) AddadModelRoot();
    
    
    
    Handle(BrNode_adGeometricDef) AddadGeometricDef();
    
    
    
    Handle(BrNode_adGeometry) AddadGeometry();
    
    
    
    Handle(BrNode_adProperty) AddadProperty();
    
    
    
    Handle(BrNode_adPropertySet) AddadPropertySet();
    
    
    
    Handle(BrNode_adObject) AddadObject();
    
    
    
    
    
    

protected:
    virtual void initPartitions() override;
    virtual void initFunctionDrivers() override {}
    virtual Handle(ActAPI_IPartition) getVariablePartition(const VariableType&) const override { return Handle(ActAPI_IPartition)(); }
    virtual Handle(ActAPI_INode) getRootNode() const override { return m_root; }
    virtual void invariantCopyRefs(ActAPI_FuncGUIDStream&, ActAPI_ParameterLocatorStream&) const override {}
    virtual Standard_Integer actualVersionApp() override { return 1; }
    virtual Handle(ActData_CAFConverter) converterApp() override { return Handle(ActData_CAFConverter)(); }
    virtual Handle(ActAPI_IModel) Clone() const override { return Handle(ActAPI_IModel)(); }

private:
    Handle(BrNode_adModelRoot) m_root;
};

#endif