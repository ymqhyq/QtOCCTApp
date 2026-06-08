#ifndef BR_EXPORT
#ifdef BR_EXPORT_EXPORTS
  #define BR_EXPORT __declspec(dllexport)
#else
  #define BR_EXPORT __declspec(dllimport)
#endif
#endif

#ifndef BrNode_adDrawing2D_HeaderFile
#define BrNode_adDrawing2D_HeaderFile

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
    
  

  
#include <ActData_RealParameter.h>
    
  



#include "BrNode_adRoot.h"



  
#include "BrNode_adRepresentation2D.h"
  

  
#include "BrNode_adLeaderAnnotation.h"
  


/**
 * @brief MDA Generated Node for BrNode_adDrawing2D
 */
class BR_EXPORT BrNode_adDrawing2D : public BrNode_adRoot
{
public:
    // OCCT RTTI
    DEFINE_STANDARD_RTTI_INLINE(BrNode_adDrawing2D, BrNode_adRoot)

    // Active Data Node Factory registration
    static Handle(ActAPI_INode) AllocInstance() { return BrNode_adDrawing2D::Instance(); }
    DEFINE_NODE_FACTORY(BrNode_adDrawing2D, BrNode_adDrawing2D::AllocInstance)

public:
    // Parameter IDs (Tags)
    enum ParamId
    {
        
        PID_FirstAttr = BrNode_adRoot::PID_Last,
        
        
        
        PID_DrawingType,
        
        PID_Scale,
        
        
        PID_Last
    };

public:
    static Handle(BrNode_adDrawing2D) Instance();

    // Custom initialization
    virtual void InitNode();

public:
    // Mandatory overrides for ActData_BaseNode
    virtual TCollection_ExtendedString GetName() override;
    virtual void SetName(const TCollection_ExtendedString& theName) override;

public:
    // Get/Set methods for attributes
    
    
    void SetDrawingType(const TCollection_ExtendedString& value);
    TCollection_ExtendedString GetDrawingType() const;
    
    
    
    void SetScale(const double& value);
    double GetScale() const;
    
    

public:
    // Children management (MDA Generated)
    
    
    void AddRepresentations(const Handle(BrNode_adRepresentation2D)& node);
    NCollection_Sequence<Handle(BrNode_adRepresentation2D)> GetRepresentationsList() const;
    
    
    
    void AddLeaderAnnotations(const Handle(BrNode_adLeaderAnnotation)& node);
    NCollection_Sequence<Handle(BrNode_adLeaderAnnotation)> GetLeaderAnnotationsList() const;
    
    

protected:
    BrNode_adDrawing2D();

};

#endif