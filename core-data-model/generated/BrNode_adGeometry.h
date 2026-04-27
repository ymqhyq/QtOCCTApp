#ifndef BR_EXPORT
#ifdef BR_EXPORT_EXPORTS
  #define BR_EXPORT __declspec(dllexport)
#else
  #define BR_EXPORT __declspec(dllimport)
#endif
#endif

#ifndef BrNode_adGeometry_HeaderFile
#define BrNode_adGeometry_HeaderFile

// OCCT & Active Data includes
#include <Standard_Boolean.hxx>
#include <Standard_Handle.hxx>
#include <Standard_Type.hxx>
#include <ActData_BaseNode.h>
#include <ActData_ParameterFactory.h>
#include <ActData_NodeFactory.h>
#include <TopoDS_Shape.hxx>
#include <TColStd_HArray1OfReal.hxx>
#include <NCollection_Sequence.hxx>

// Parameter specific includes


  
#include <ActData_ReferenceParameter.h>
    
  

  
#include <ActData_IntParameter.h>
    
  

  

  
#include <ActData_RealParameter.h>
    
  

  
#include <ActData_BoolParameter.h>
    
  



#include "BrNode_adRoot.h"




/**
 * @brief MDA Generated Node for BrNode_adGeometry
 */
class BR_EXPORT BrNode_adGeometry : public BrNode_adRoot
{
public:
    // OCCT RTTI
    DEFINE_STANDARD_RTTI_INLINE(BrNode_adGeometry, BrNode_adRoot)

    // Active Data Node Factory registration
    static Handle(ActAPI_INode) AllocInstance() { return BrNode_adGeometry::Instance(); }
    DEFINE_NODE_FACTORY(BrNode_adGeometry, BrNode_adGeometry::AllocInstance)

public:
    // Parameter IDs (Tags)
    enum ParamId
    {
        
        PID_FirstAttr = BrNode_adRoot::PID_Last,
        
        
        
        PID_GeometryRef,
        
        PID_MaterialType,
        
        PID_Color,
        
        PID_Transparency,
        
        PID_Visibility,
        
        
        PID_Last
    };

public:
    static Handle(BrNode_adGeometry) Instance();

    // Custom initialization
    virtual void InitNode();

public:
    // Mandatory overrides for ActData_BaseNode
    virtual TCollection_ExtendedString GetName() override;
    virtual void SetName(const TCollection_ExtendedString& theName) override;

public:
    // Get/Set methods for attributes
    
    
    void SetGeometryRef(const Handle(ActAPI_IDataCursor)& value);
    Handle(ActAPI_IDataCursor) GetGeometryRef() const;
    
    
    
    void SetMaterialType(const int& value);
    int GetMaterialType() const;
    
    
    
    void SetColor(const int& value);
    int GetColor() const;
    
    
    
    void SetTransparency(const double& value);
    double GetTransparency() const;
    
    
    
    void SetVisibility(const bool& value);
    bool GetVisibility() const;
    
    

public:
    // Children management (MDA Generated)
    

protected:
    BrNode_adGeometry();

};

#endif