#ifndef BrNode_Property_HeaderFile
#define BrNode_Property_HeaderFile

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
    
  

  

  






/**
 * @brief MDA Generated Node for BrNode_Property
 */
class BrNode_Property : public ActData_BaseNode
{
public:
    // OCCT RTTI
    DEFINE_STANDARD_RTTIEXT(BrNode_Property, ActData_BaseNode)

public:
    // Parameter IDs (Tags)
    enum ParamId
    {
        PID_FirstAttr = ActData_BaseNode::UserParam_Last,
        
        PID_PropertyName,
        
        PID_PropertyValue,
        
        PID_ValueType,
        
        PID_Last
    };

public:
    static Handle(BrNode_Property) Instance();

    // Custom initialization
    void InitNode();

    // Mandatory overrides for ActData_BaseNode
    virtual TCollection_ExtendedString GetName() override;
    virtual void SetName(const TCollection_ExtendedString& theName) override;

public:
    // Getters / Setters (MDA Generated)
    
    void SetPropertyName(const TCollection_AsciiString& value);
    TCollection_AsciiString GetPropertyName() const;
    
    void SetPropertyValue(const TCollection_AsciiString& value);
    TCollection_AsciiString GetPropertyValue() const;
    
    void SetValueType(const TCollection_AsciiString& value);
    TCollection_AsciiString GetValueType() const;
    

public:
    // Children management (MDA Generated)
    

protected:
    BrNode_Property() : ActData_BaseNode() {}

};

#endif