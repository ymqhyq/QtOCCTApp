#ifndef BrNode_PropertySet_HeaderFile
#define BrNode_PropertySet_HeaderFile

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
    
  





  
#include "BrNode_Property.h"
  


/**
 * @brief MDA Generated Node for BrNode_PropertySet
 */
class BrNode_PropertySet : public ActData_BaseNode
{
public:
    // OCCT RTTI
    DEFINE_STANDARD_RTTIEXT(BrNode_PropertySet, ActData_BaseNode)

public:
    // Parameter IDs (Tags)
    enum ParamId
    {
        PID_FirstAttr = ActData_BaseNode::UserParam_Last,
        
        PID_SetName,
        
        PID_Last
    };

public:
    static Handle(BrNode_PropertySet) Instance();

    // Custom initialization
    void InitNode();

    // Mandatory overrides for ActData_BaseNode
    virtual TCollection_ExtendedString GetName() override;
    virtual void SetName(const TCollection_ExtendedString& theName) override;

public:
    // Getters / Setters (MDA Generated)
    
    void SetSetName(const TCollection_AsciiString& value);
    TCollection_AsciiString GetSetName() const;
    

public:
    // Children management (MDA Generated)
    
    
    void AddProperties(const Handle(BrNode_Property)& node);
    NCollection_Sequence<Handle(BrNode_Property)> GetPropertiesList() const;
    
    

protected:
    BrNode_PropertySet() : ActData_BaseNode() {}

};

#endif