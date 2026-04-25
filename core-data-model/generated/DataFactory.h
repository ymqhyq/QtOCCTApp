#ifndef DataFactory_HeaderFile
#define DataFactory_HeaderFile

#include <ActData_BaseModel.h>
#include <TCollection_AsciiString.hxx>
#include "BrNode_ActiveObject.h"
#include "BrNode_PropertySet.h"
#include "BrNode_Property.h"

/**
 * @brief 自动生成的业务对象工厂类
 */
class DataFactory
{
public:
    /**
     * @brief 根据类型名称创建一个 ActiveObject 及其属性集
     */
    static Handle(BrNode_ActiveObject) CreateObject(const Handle(ActData_BaseModel)& model, 
                                                    const TCollection_AsciiString& typeName);

    /**
     * @brief 创建指定的属性集及其属性单元
     */
    static Handle(BrNode_PropertySet) CreatePset(const Handle(ActData_BaseModel)& model, 
                                                 const TCollection_AsciiString& psetName);
};

#endif