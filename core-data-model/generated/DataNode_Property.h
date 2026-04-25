#ifndef DataNode_Property_HeaderFile
#define DataNode_Property_HeaderFile

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
 * @brief MDA Generated Node for 属性单元
 * This class is part of the portable data model for Docker-based microservices.
 */
class DataNode_Property : public ActData_BaseNode
{
public:
    // OCCT RTTI
    DEFINE_STANDARD_RTTI_EXT(DataNode_Property, ActData_BaseNode)

public:
    // Parameter IDs (Tags)
    enum ParamId
    {
        PID_PropertyName = ActData_BaseNode::PID_Last,PID_PropertyValue,PID_ValueType,
        PID_Last
    };

public:
    static Handle(DataNode_Property) Instance();

    // Initialization
    virtual void Init();

public:
    // Getters / Setters (MDA Generated)
    
    // 属性键
    void SetPropertyName(const TCollection_AsciiString& value);
    TCollection_AsciiString GetPropertyName() const;
    
    // 属性值
    void SetPropertyValue(const TCollection_AsciiString& value);
    TCollection_AsciiString GetPropertyValue() const;
    
    // 值类型
    void SetValueType(const TCollection_AsciiString& value);
    TCollection_AsciiString GetValueType() const;
    

public:
    // Children management (MDA Generated)
    

protected:
    DataNode_Property() : ActData_BaseNode() {}

};

#endif