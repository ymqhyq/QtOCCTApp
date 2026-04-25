#include "DataFactory.h"
#include "DataModel.h"
#include <ActData_BasePartition.h>

Handle(BrNode_ActiveObject) DataFactory::CreateObject(const Handle(ActData_BaseModel)& model, 
                                                        const TCollection_AsciiString& typeName)
{
    Handle(DataModel) dm = Handle(DataModel)::DownCast(model);
    Handle(BrNode_ActiveObject) obj = dm->AddActiveObject();
    if (obj.IsNull()) return nullptr;

    obj->SetObjectType(typeName);
    obj->SetName(typeName);

    
    if (typeName == "BridgePier") {
        
        obj->AddPropertySets(CreatePset(model, "Pset_BridgePierGeometry"));
        
        obj->AddPropertySets(CreatePset(model, "Pset_MaterialConcrete"));
        
    }
    
    if (typeName == "SinglePile") {
        
        obj->AddPropertySets(CreatePset(model, "Pset_MaterialConcrete"));
        
    }
    

    return obj;
}

Handle(BrNode_PropertySet) DataFactory::CreatePset(const Handle(ActData_BaseModel)& model, 
                                                     const TCollection_AsciiString& psetName)
{
    Handle(DataModel) dm = Handle(DataModel)::DownCast(model);
    Handle(BrNode_PropertySet) pset = dm->AddPropertySet();
    if (pset.IsNull()) return nullptr;

    pset->SetSetName(psetName);

    
    if (psetName == "Pset_MaterialConcrete") {
        
        {
            Handle(BrNode_Property) prop = dm->AddProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("StrengthGrade");
                prop->SetPropertyValue("C30");
                prop->SetValueType("enum");
                pset->AddProperties(prop);
            }
        }
        
    }
    
    if (psetName == "Pset_BridgePierGeometry") {
        
        {
            Handle(BrNode_Property) prop = dm->AddProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("Height");
                prop->SetPropertyValue("15.0");
                prop->SetValueType("Real");
                pset->AddProperties(prop);
            }
        }
        
        {
            Handle(BrNode_Property) prop = dm->AddProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("Width");
                prop->SetPropertyValue("2.5");
                prop->SetValueType("Real");
                pset->AddProperties(prop);
            }
        }
        
    }
    

    return pset;
}