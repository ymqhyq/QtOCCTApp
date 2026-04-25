#ifndef BrNode_ActiveObject_HeaderFile
#define BrNode_ActiveObject_HeaderFile

// OCCT & Active Data includes
#include <Standard_Boolean.hxx>
#include <Standard_Handle.hxx>
#include <Standard_Type.hxx>
#include <ActData_BaseNode.h>
#include <ActData_ParameterFactory.h>
#include <TopoDS_Shape.hxx>
#include <TColStd_HArray1OfReal.hxx>
#include <NCollection_Sequence.hxx>

// Parameter specific includes


  
#include <ActData_AsciiStringParameter.h>
    
  

  

  

  

  
#include <ActData_ShapeParameter.h>
    
  

  
#include <ActData_ReferenceParameter.h>
    
  

  
#include <ActData_RealArrayParameter.h>
    
  

  
#include <ActData_IntParameter.h>
    
  

  

  
#include <ActData_RealParameter.h>
    
  

  
#include <ActData_BoolParameter.h>
    
  





  
#include "BrNode_PropertySet.h"
  

  


/**
 * @brief MDA Generated Node for BrNode_ActiveObject
 */
class BrNode_ActiveObject : public ActData_BaseNode
{
public:
    // OCCT RTTI
    DEFINE_STANDARD_RTTIEXT(BrNode_ActiveObject, ActData_BaseNode)

public:
    // Parameter IDs (Tags)
    enum ParamId
    {
        PID_FirstAttr = ActData_BaseNode::UserParam_Last,
        
        PID_GlobalID,
        
        PID_Name,
        
        PID_ObjectType,
        
        PID_BrEntityRef,
        
        PID_Geometry,
        
        PID_GeometryRef,
        
        PID_Transformation,
        
        PID_MaterialType,
        
        PID_Color,
        
        PID_Transparency,
        
        PID_Visibility,
        
        PID_Last
    };

public:
    static Handle(BrNode_ActiveObject) Instance();

    // Custom initialization
    void InitNode();

    // Mandatory overrides for ActData_BaseNode
    virtual TCollection_ExtendedString GetName() override;
    virtual void SetName(const TCollection_ExtendedString& theName) override;

public:
    // Getters / Setters (MDA Generated)
    
    void SetGlobalID(const TCollection_AsciiString& value);
    TCollection_AsciiString GetGlobalID() const;
    
    void SetName(const TCollection_AsciiString& value);
    TCollection_AsciiString GetName() const;
    
    void SetObjectType(const TCollection_AsciiString& value);
    TCollection_AsciiString GetObjectType() const;
    
    void SetBrEntityRef(const TCollection_AsciiString& value);
    TCollection_AsciiString GetBrEntityRef() const;
    
    void SetGeometry(const TopoDS_Shape& value);
    TopoDS_Shape GetGeometry() const;
    
    void SetGeometryRef(const Handle(ActAPI_IDataCursor)& value);
    Handle(ActAPI_IDataCursor) GetGeometryRef() const;
    
    void SetTransformation(const Handle(TColStd_HArray1OfReal)& value);
    Handle(TColStd_HArray1OfReal) GetTransformation() const;
    
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
    
    
    void AddPropertySets(const Handle(BrNode_PropertySet)& node);
    NCollection_Sequence<Handle(BrNode_PropertySet)> GetPropertySetsList() const;
    
    
    
    void AddSubObjects(const Handle(BrNode_ActiveObject)& node);
    NCollection_Sequence<Handle(BrNode_ActiveObject)> GetSubObjectsList() const;
    
    

protected:
    BrNode_ActiveObject() : ActData_BaseNode() {}

};

#endif