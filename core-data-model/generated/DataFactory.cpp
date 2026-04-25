#include "DataFactory.h"

Handle(DataNode_ActiveObject) DataFactory::CreateObject(const Handle(ActData_BaseModel)& model, 
                                                        const TCollection_AsciiString& typeName)
{
    Handle(DataNode_ActiveObject) obj = Handle(DataNode_ActiveObject)::DownCast(model->NewNode(DataNode_ActiveObject::Instance()));
    obj->SetObjectType(typeName);
    obj->SetName(typeName); // 默认名称与类型一致

    
    if (typeName == "BridgePier") {
        
        obj->AddPropertySets(CreatePset(model, "Pset_BridgePierGeometry"));
        
        obj->AddPropertySets(CreatePset(model, "Pset_MaterialConcrete"));
        
    }
    
    if (typeName == "SinglePile") {
        
        obj->AddPropertySets(CreatePset(model, "Pset_MaterialConcrete"));
        
    }
    

    return obj;
}

Handle(DataNode_PropertySet) DataFactory::CreatePset(const Handle(ActData_BaseModel)& model, 
                                                     const TCollection_AsciiString& psetName)
{
    Handle(DataNode_PropertySet) pset = Handle(DataNode_PropertySet)::DownCast(model->NewNode(DataNode_PropertySet::Instance()));
    pset->SetSetName(psetName);

    
    if (psetName == "Pset_MaterialConcrete") {
        
        {
            Handle(DataNode_Property) prop = Handle(DataNode_Property)::DownCast(model->NewNode(DataNode_Property::Instance()));
            prop->SetPropertyName("StrengthGrade");
            prop->SetPropertyValue("C30");
            prop->SetValueType("enum");
            pset->AddProperties(prop);
        }
        
    }
    
    if (psetName == "Pset_BridgePierGeometry") {
        
        {
            Handle(DataNode_Property) prop = Handle(DataNode_Property)::DownCast(model->NewNode(DataNode_Property::Instance()));
            prop->SetPropertyName("Height");
            prop->SetPropertyValue("15.0");
            prop->SetValueType("Real");
            pset->AddProperties(prop);
        }
        
        {
            Handle(DataNode_Property) prop = Handle(DataNode_Property)::DownCast(model->NewNode(DataNode_Property::Instance()));
            prop->SetPropertyName("Width");
            prop->SetPropertyValue("2.5");
            prop->SetValueType("Real");
            pset->AddProperties(prop);
        }
        
    }
    

    return pset;
}