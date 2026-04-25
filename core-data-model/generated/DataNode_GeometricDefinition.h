#ifndef DataNode_GeometricDefinition_HeaderFile
#define DataNode_GeometricDefinition_HeaderFile

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
 * @brief MDA Generated Node for 几何原型
 * This class is part of the portable data model for Docker-based microservices.
 */
class DataNode_GeometricDefinition : public ActData_BaseNode
{
public:
    // OCCT RTTI
    DEFINE_STANDARD_RTTI_EXT(DataNode_GeometricDefinition, ActData_BaseNode)

public:
    // Parameter IDs (Tags)
    enum ParamId
    {
        PID_MasterGeometry = ActData_BaseNode::PID_Last,PID_Creator,PID_Version,
        PID_Last
    };

public:
    static Handle(DataNode_GeometricDefinition) Instance();

    // Initialization
    virtual void Init();

public:
    // Getters / Setters (MDA Generated)
    
    // 原型几何
    void SetMasterGeometry(const TopoDS_Shape& value);
    TopoDS_Shape GetMasterGeometry() const;
    
    // 创建者
    void SetCreator(const TCollection_AsciiString& value);
    TCollection_AsciiString GetCreator() const;
    
    // 版本号
    void SetVersion(const int& value);
    int GetVersion() const;
    

public:
    // Children management (MDA Generated)
    

protected:
    DataNode_GeometricDefinition() : ActData_BaseNode() {}

};

#endif