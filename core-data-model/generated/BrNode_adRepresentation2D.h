#ifndef BR_EXPORT
#ifdef BR_EXPORT_EXPORTS
  #define BR_EXPORT __declspec(dllexport)
#else
  #define BR_EXPORT __declspec(dllimport)
#endif
#endif

#ifndef BrNode_adRepresentation2D_HeaderFile
#define BrNode_adRepresentation2D_HeaderFile

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
    
  



#include "BrNode_adRoot.h"




/**
 * @brief MDA Generated Node for BrNode_adRepresentation2D
 */
class BR_EXPORT BrNode_adRepresentation2D : public BrNode_adRoot
{
public:
    // OCCT RTTI
    DEFINE_STANDARD_RTTI_INLINE(BrNode_adRepresentation2D, BrNode_adRoot)

    // Active Data Node Factory registration
    static Handle(ActAPI_INode) AllocInstance() { return BrNode_adRepresentation2D::Instance(); }
    DEFINE_NODE_FACTORY(BrNode_adRepresentation2D, BrNode_adRepresentation2D::AllocInstance)

public:
    // Parameter IDs (Tags)
    enum ParamId
    {
        
        PID_FirstAttr = BrNode_adRoot::PID_Last,
        
        
        
        PID_RepresentationType,
        
        PID_GeneratedShape,
        
        
        PID_Last
    };

public:
    static Handle(BrNode_adRepresentation2D) Instance();

    // Custom initialization
    virtual void InitNode();

public:
    // Mandatory overrides for ActData_BaseNode
    virtual TCollection_ExtendedString GetName() override;
    virtual void SetName(const TCollection_ExtendedString& theName) override;

public:
    // Get/Set methods for attributes
    
    
    void SetRepresentationType(const TCollection_ExtendedString& value);
    TCollection_ExtendedString GetRepresentationType() const;
    
    
    
    void SetGeneratedShape(const TopoDS_Shape& value);
    TopoDS_Shape GetGeneratedShape() const;
    
    

public:
    // Children management (MDA Generated)
    

protected:
    BrNode_adRepresentation2D();

};

#endif