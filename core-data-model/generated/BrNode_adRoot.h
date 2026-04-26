#ifndef BR_EXPORT
#ifdef BR_EXPORT_EXPORTS
  #define BR_EXPORT __declspec(dllexport)
#else
  #define BR_EXPORT __declspec(dllimport)
#endif
#endif

#ifndef BrNode_adRoot_HeaderFile
#define BrNode_adRoot_HeaderFile

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
    
  

  

  






/**
 * @brief MDA Generated Node for BrNode_adRoot
 */
class BR_EXPORT BrNode_adRoot : public ActData_BaseNode
{
public:
    // OCCT RTTI
    DEFINE_STANDARD_RTTI_INLINE(BrNode_adRoot, ActData_BaseNode)

    // Active Data Node Factory registration
    static Handle(ActAPI_INode) AllocInstance() { return BrNode_adRoot::Instance(); }
    DEFINE_NODE_FACTORY(BrNode_adRoot, BrNode_adRoot::AllocInstance)

public:
    // Parameter IDs (Tags)
    enum ParamId
    {
        
        PID_FirstAttr = 1000,
        
        
        
        PID_GlobalID,
        
        PID_Name,
        
        PID_label,
        
        
        PID_Last
    };

public:
    static Handle(BrNode_adRoot) Instance();

    // Custom initialization
    virtual void InitNode();

public:
    // Mandatory overrides for ActData_BaseNode
    virtual TCollection_ExtendedString GetName() override;
    virtual void SetName(const TCollection_ExtendedString& theName) override;

public:
    // Get/Set methods for attributes
    
    
    void SetGlobalID(const TCollection_ExtendedString& value);
    TCollection_ExtendedString GetGlobalID() const;
    
    
    
    
    
    void Setlabel(const TCollection_ExtendedString& value);
    TCollection_ExtendedString Getlabel() const;
    
    

public:
    // Children management (MDA Generated)
    

protected:
    BrNode_adRoot();

};

#endif