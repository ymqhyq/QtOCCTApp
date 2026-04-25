#ifndef BrNode_GeometricDefinition_HeaderFile
#define BrNode_GeometricDefinition_HeaderFile

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


  
#include <ActData_ShapeParameter.h>
    
  

  
#include <ActData_AsciiStringParameter.h>
    
  

  
#include <ActData_IntParameter.h>
    
  






/**
 * @brief MDA Generated Node for BrNode_GeometricDefinition
 */
class BrNode_GeometricDefinition : public ActData_BaseNode
{
public:
    // OCCT RTTI
    DEFINE_STANDARD_RTTIEXT(BrNode_GeometricDefinition, ActData_BaseNode)

public:
    // Parameter IDs (Tags)
    enum ParamId
    {
        PID_FirstAttr = ActData_BaseNode::UserParam_Last,
        
        PID_MasterGeometry,
        
        PID_Creator,
        
        PID_Version,
        
        PID_Last
    };

public:
    static Handle(BrNode_GeometricDefinition) Instance();

    // Custom initialization
    void InitNode();

    // Mandatory overrides for ActData_BaseNode
    virtual TCollection_ExtendedString GetName() override;
    virtual void SetName(const TCollection_ExtendedString& theName) override;

public:
    // Getters / Setters (MDA Generated)
    
    void SetMasterGeometry(const TopoDS_Shape& value);
    TopoDS_Shape GetMasterGeometry() const;
    
    void SetCreator(const TCollection_AsciiString& value);
    TCollection_AsciiString GetCreator() const;
    
    void SetVersion(const int& value);
    int GetVersion() const;
    

public:
    // Children management (MDA Generated)
    

protected:
    BrNode_GeometricDefinition() : ActData_BaseNode() {}

};

#endif