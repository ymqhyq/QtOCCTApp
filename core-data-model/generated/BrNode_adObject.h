#ifndef BR_EXPORT
#ifdef BR_EXPORT_EXPORTS
  #define BR_EXPORT __declspec(dllexport)
#else
  #define BR_EXPORT __declspec(dllimport)
#endif
#endif

#ifndef BrNode_adObject_HeaderFile
#define BrNode_adObject_HeaderFile

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


  
#include <ActData_NameParameter.h>
    
  

  

  

  

  

  

  
#include <ActData_ReferenceParameter.h>
    
  



#include "BrNode_adRoot.h"



  
#include "BrNode_adPropertySet.h"
  

  


/**
 * @brief MDA Generated Node for BrNode_adObject
 */
class BR_EXPORT BrNode_adObject : public BrNode_adRoot
{
public:
    // OCCT RTTI
    DEFINE_STANDARD_RTTI_INLINE(BrNode_adObject, BrNode_adRoot)

    // Active Data Node Factory registration
    static Handle(ActAPI_INode) AllocInstance() { return BrNode_adObject::Instance(); }
    DEFINE_NODE_FACTORY(BrNode_adObject, BrNode_adObject::AllocInstance)

public:
    // Parameter IDs (Tags)
    enum ParamId
    {
        
        PID_FirstAttr = BrNode_adRoot::PID_Last,
        
        
        
        PID_ObjectType,
        
        PID_FunctionalType,
        
        PID_FunctionalSubType,
        
        PID_StructuralType,
        
        PID_StructuralSubType,
        
        PID_BrEntityRef,
        
        PID_Geometry,
        
        
        PID_Last
    };

public:
    static Handle(BrNode_adObject) Instance();

    // Custom initialization
    virtual void InitNode();

public:
    // Mandatory overrides for ActData_BaseNode
    virtual TCollection_ExtendedString GetName() override;
    virtual void SetName(const TCollection_ExtendedString& theName) override;

public:
    // Get/Set methods for attributes
    
    
    void SetObjectType(const TCollection_ExtendedString& value);
    TCollection_ExtendedString GetObjectType() const;
    
    
    
    void SetFunctionalType(const TCollection_ExtendedString& value);
    TCollection_ExtendedString GetFunctionalType() const;
    
    
    
    void SetFunctionalSubType(const TCollection_ExtendedString& value);
    TCollection_ExtendedString GetFunctionalSubType() const;
    
    
    
    void SetStructuralType(const TCollection_ExtendedString& value);
    TCollection_ExtendedString GetStructuralType() const;
    
    
    
    void SetStructuralSubType(const TCollection_ExtendedString& value);
    TCollection_ExtendedString GetStructuralSubType() const;
    
    
    
    void SetBrEntityRef(const TCollection_ExtendedString& value);
    TCollection_ExtendedString GetBrEntityRef() const;
    
    
    
    void SetGeometry(const Handle(ActAPI_IDataCursor)& value);
    Handle(ActAPI_IDataCursor) GetGeometry() const;
    
    

public:
    // Children management (MDA Generated)
    
    
    void AddPropertySets(const Handle(BrNode_adPropertySet)& node);
    NCollection_Sequence<Handle(BrNode_adPropertySet)> GetPropertySetsList() const;
    
    
    
    void AddSubObjects(const Handle(BrNode_adObject)& node);
    NCollection_Sequence<Handle(BrNode_adObject)> GetSubObjectsList() const;
    
    

protected:
    BrNode_adObject();

};

#endif