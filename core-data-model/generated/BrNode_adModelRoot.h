#ifndef BR_EXPORT
#ifdef BR_EXPORT_EXPORTS
  #define BR_EXPORT __declspec(dllexport)
#else
  #define BR_EXPORT __declspec(dllimport)
#endif
#endif

#ifndef BrNode_adModelRoot_HeaderFile
#define BrNode_adModelRoot_HeaderFile

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




#include "BrNode_adRoot.h"



  
#include "BrNode_adObject.h"
  


/**
 * @brief MDA Generated Node for BrNode_adModelRoot
 */
class BR_EXPORT BrNode_adModelRoot : public BrNode_adRoot
{
public:
    // OCCT RTTI
    DEFINE_STANDARD_RTTI_INLINE(BrNode_adModelRoot, BrNode_adRoot)

    // Active Data Node Factory registration
    static Handle(ActAPI_INode) AllocInstance() { return BrNode_adModelRoot::Instance(); }
    DEFINE_NODE_FACTORY(BrNode_adModelRoot, BrNode_adModelRoot::AllocInstance)

public:
    // Parameter IDs (Tags)
    enum ParamId
    {
        
        PID_FirstAttr = BrNode_adRoot::PID_Last,
        
        
        
        
        PID_Last
    };

public:
    static Handle(BrNode_adModelRoot) Instance();

    // Custom initialization
    virtual void InitNode();

public:
    // Mandatory overrides for ActData_BaseNode
    virtual TCollection_ExtendedString GetName() override;
    virtual void SetName(const TCollection_ExtendedString& theName) override;

public:
    // Get/Set methods for attributes
    

public:
    // Children management (MDA Generated)
    
    
    void AddSubObjects(const Handle(BrNode_adObject)& node);
    NCollection_Sequence<Handle(BrNode_adObject)> GetSubObjectsList() const;
    
    

protected:
    BrNode_adModelRoot();

};

#endif