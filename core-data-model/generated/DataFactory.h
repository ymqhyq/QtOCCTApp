#ifndef DataFactory_HeaderFile
#define DataFactory_HeaderFile

#include <ActData_BaseModel.h>
#include <TCollection_AsciiString.hxx>
#include "BrNode_adObject.h"
#include "BrNode_adPropertySet.h"
#include "BrNode_adProperty.h"

/**
 * @brief 自动生成的业务对象工厂类
 */
class DataFactory
{
public:
    /**
     * @brief 根据类型名称创建一个 adObject 及其属性集
     */
    static Handle(BrNode_adObject) CreateObject(const Handle(ActData_BaseModel)& model, 
                                                    const TCollection_AsciiString& typeName);

    /**
     * @brief 创建指定的属性集及其属性单元
     */
    static Handle(BrNode_adPropertySet) CreatePset(const Handle(ActData_BaseModel)& model, 
                                                 const TCollection_AsciiString& psetName);
};

#endif