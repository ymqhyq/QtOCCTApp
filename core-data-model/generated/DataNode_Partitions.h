#ifndef DataNode_Partitions_HeaderFile
#define DataNode_Partitions_HeaderFile

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
 * @brief MDA Generated Node for Partitions
 * This class is part of the portable data model for Docker-based microservices.
 */
class DataNode_Partitions : public ActData_BaseNode
{
public:
    // OCCT RTTI
    DEFINE_STANDARD_RTTI_EXT(DataNode_Partitions, ActData_BaseNode)

public:
    // Parameter IDs (Tags)
    enum ParamId
    {
        PID_Dummy = ActData_BaseNode::PID_Last,
        PID_Last
    };

public:
    static Handle(DataNode_Partitions) Instance();

    // Initialization
    virtual void Init();

public:
    // Getters / Setters (MDA Generated)
    

public:
    // Children management (MDA Generated)
    

protected:
    DataNode_Partitions() : ActData_BaseNode() {}

};

#endif