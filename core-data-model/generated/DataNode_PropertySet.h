#ifndef DataNode_PropertySet_HeaderFile
#define DataNode_PropertySet_HeaderFile

// Active Data includes (No Qt!)
#include <ActData_BaseNode.h>
#include <ActData_ParameterFactory.h>
#include <ActData_RealParameter.h>
#include <ActData_IntParameter.h>
#include <ActData_StringParameter.h>
#include <ActData_ShapeParameter.h>
#include <ActData_RealArrayParameter.h>
#include <TopoDS_Shape.h>
#include <TColStd_HArray1OfReal.h>
#include <NCollection_Sequence.hxx>


class DataNode_GeometricDefinition;

class DataNode_Property;

class DataNode_PropertySet;

class DataNode_ActiveObject;

class DataNode_Partitions;



#include "ActData_BaseNode.h"


/**
 * @brief MDA Generated Node for 属性集
 * This class is part of the portable data model for Docker-based microservices.
 */
class DataNode_PropertySet : public ActData_BaseNode
{
public:
    // OCCT RTTI
    DEFINE_STANDARD_RTTI_EXT(DataNode_PropertySet, ActData_BaseNode)

public:
    // Parameter IDs (Tags)
    enum ParamId
    {
        PID_SetName = ActData_BaseNode::PID_Last,
        PID_Last
    };

public:
    static Handle(DataNode_PropertySet) Instance();

    // Initialization
    virtual void Init();

public:
    // Getters / Setters (MDA Generated)
    
    // 属性集名称
    void SetSetName(const TCollection_AsciiString& value);
    TCollection_AsciiString GetSetName() const;
    

public:
    // Children management (MDA Generated)
    
    // --- Properties (0..*) ---
    
    void AddProperties(const Handle(DataNode_Property)& node);
    NCollection_Sequence<Handle(DataNode_Property)> GetPropertiesList() const;
    
    

protected:
    DataNode_PropertySet() : ActData_BaseNode() {}

};

#endif