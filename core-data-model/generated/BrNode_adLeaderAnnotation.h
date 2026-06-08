#ifndef BR_EXPORT
#ifdef BR_EXPORT_EXPORTS
  #define BR_EXPORT __declspec(dllexport)
#else
  #define BR_EXPORT __declspec(dllimport)
#endif
#endif

#ifndef BrNode_adLeaderAnnotation_HeaderFile
#define BrNode_adLeaderAnnotation_HeaderFile

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
    
  

  
#include <ActData_RealArrayParameter.h>
    
  

  



#include "BrNode_adRepresentation2D.h"




/**
 * @brief MDA Generated Node for BrNode_adLeaderAnnotation
 */
class BR_EXPORT BrNode_adLeaderAnnotation : public BrNode_adRepresentation2D
{
public:
    // OCCT RTTI
    DEFINE_STANDARD_RTTI_INLINE(BrNode_adLeaderAnnotation, BrNode_adRepresentation2D)

    // Active Data Node Factory registration
    static Handle(ActAPI_INode) AllocInstance() { return BrNode_adLeaderAnnotation::Instance(); }
    DEFINE_NODE_FACTORY(BrNode_adLeaderAnnotation, BrNode_adLeaderAnnotation::AllocInstance)

public:
    // Parameter IDs (Tags)
    enum ParamId
    {
        
        PID_FirstAttr = BrNode_adRepresentation2D::PID_Last,
        
        
        
        PID_NoteText,
        
        PID_ArrowPoint,
        
        PID_TextPoint,
        
        
        PID_Last
    };

public:
    static Handle(BrNode_adLeaderAnnotation) Instance();

    // Custom initialization
    virtual void InitNode();

public:
    // Mandatory overrides for ActData_BaseNode
    virtual TCollection_ExtendedString GetName() override;
    virtual void SetName(const TCollection_ExtendedString& theName) override;

public:
    // Get/Set methods for attributes
    
    
    void SetNoteText(const TCollection_ExtendedString& value);
    TCollection_ExtendedString GetNoteText() const;
    
    
    
    void SetArrowPoint(const Handle(TColStd_HArray1OfReal)& value);
    Handle(TColStd_HArray1OfReal) GetArrowPoint() const;
    
    
    
    void SetTextPoint(const Handle(TColStd_HArray1OfReal)& value);
    Handle(TColStd_HArray1OfReal) GetTextPoint() const;
    
    

public:
    // Children management (MDA Generated)
    

protected:
    BrNode_adLeaderAnnotation();

};

#endif