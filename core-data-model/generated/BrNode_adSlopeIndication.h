#ifndef BR_EXPORT
#ifdef BR_EXPORT_EXPORTS
  #define BR_EXPORT __declspec(dllexport)
#else
  #define BR_EXPORT __declspec(dllimport)
#endif
#endif

#ifndef BrNode_adSlopeIndication_HeaderFile
#define BrNode_adSlopeIndication_HeaderFile

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
    
  

  

  
#include <ActData_ShapeParameter.h>
    
  

  

  
#include <ActData_RealParameter.h>
    
  

  

  



#include "BrNode_adRepresentation2D.h"




/**
 * @brief MDA Generated Node for BrNode_adSlopeIndication
 */
class BR_EXPORT BrNode_adSlopeIndication : public BrNode_adRepresentation2D
{
public:
    // OCCT RTTI
    DEFINE_STANDARD_RTTI_INLINE(BrNode_adSlopeIndication, BrNode_adRepresentation2D)

    // Active Data Node Factory registration
    static Handle(ActAPI_INode) AllocInstance() { return BrNode_adSlopeIndication::Instance(); }
    DEFINE_NODE_FACTORY(BrNode_adSlopeIndication, BrNode_adSlopeIndication::AllocInstance)

public:
    // Parameter IDs (Tags)
    enum ParamId
    {
        
        PID_FirstAttr = BrNode_adRepresentation2D::PID_Last,
        
        
        
        PID_TargetModelDoc,
        
        PID_TargetObjectID,
        
        PID_ShoulderLine,
        
        PID_ToeLine,
        
        PID_Spacing,
        
        PID_LongLineRatio,
        
        PID_ShortLineRatio,
        
        
        PID_Last
    };

public:
    static Handle(BrNode_adSlopeIndication) Instance();

    // Custom initialization
    virtual void InitNode();

public:
    // Mandatory overrides for ActData_BaseNode
    virtual TCollection_ExtendedString GetName() override;
    virtual void SetName(const TCollection_ExtendedString& theName) override;

public:
    // Get/Set methods for attributes
    
    
    void SetTargetModelDoc(const TCollection_ExtendedString& value);
    TCollection_ExtendedString GetTargetModelDoc() const;
    
    
    
    void SetTargetObjectID(const TCollection_ExtendedString& value);
    TCollection_ExtendedString GetTargetObjectID() const;
    
    
    
    void SetShoulderLine(const TopoDS_Shape& value);
    TopoDS_Shape GetShoulderLine() const;
    
    
    
    void SetToeLine(const TopoDS_Shape& value);
    TopoDS_Shape GetToeLine() const;
    
    
    
    void SetSpacing(const double& value);
    double GetSpacing() const;
    
    
    
    void SetLongLineRatio(const double& value);
    double GetLongLineRatio() const;
    
    
    
    void SetShortLineRatio(const double& value);
    double GetShortLineRatio() const;
    
    

public:
    // Children management (MDA Generated)
    

protected:
    BrNode_adSlopeIndication();

};

#endif