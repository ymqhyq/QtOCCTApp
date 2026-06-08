#include "DataFactory.h"
#include "DataModel.h"
#include <ActData_BasePartition.h>
#include <TDF_Tool.hxx>

Handle(BrNode_adObject) DataFactory::CreateObject(const Handle(ActData_BaseModel)& model, 
                                                        const TCollection_AsciiString& typeName)
{
    std::cout << "    DataFactory::CreateObject: " << typeName.ToCString() << std::endl;
    Handle(DataModel) dm = Handle(DataModel)::DownCast(model);
    Handle(BrNode_adObject) obj = dm->AddadObject();
    if (obj.IsNull()) {
        std::cerr << "    FAILED dm->AddadObject()!" << std::endl;
        return nullptr;
    }

    obj->SetObjectType(typeName);
    obj->SetName(typeName);
    std::cout << "    Object base created." << std::endl;

    
    if (typeName == "SubgradeSlope") {
        std::cout << "    Initializing PropertySets for type: SubgradeSlope" << std::endl;
        
        {
            Handle(BrNode_adPropertySet) pset = CreatePset(model, "Pset_SlopeGeometry");
            if (obj->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: obj RootLabel is NULL!" << std::endl;
            if (pset.IsNull()) std::cerr << "    [DataFactory] ERROR: pset is NULL!" << std::endl;
            else if (pset->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: pset RootLabel is NULL!" << std::endl;
            
            try {
                obj->AddPropertySets(pset);
            } catch (Standard_Failure& e) {
                std::cerr << "    [DataFactory] OCCT EXCEPTION in AddPropertySets: " << e.GetMessageString() << std::endl;
            } catch (...) {
                std::cerr << "    [DataFactory] UNKNOWN EXCEPTION in AddPropertySets!" << std::endl;
            }
        }
        
        {
            Handle(BrNode_adPropertySet) pset = CreatePset(model, "Pset_MaterialConcrete");
            if (obj->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: obj RootLabel is NULL!" << std::endl;
            if (pset.IsNull()) std::cerr << "    [DataFactory] ERROR: pset is NULL!" << std::endl;
            else if (pset->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: pset RootLabel is NULL!" << std::endl;
            
            try {
                obj->AddPropertySets(pset);
            } catch (Standard_Failure& e) {
                std::cerr << "    [DataFactory] OCCT EXCEPTION in AddPropertySets: " << e.GetMessageString() << std::endl;
            } catch (...) {
                std::cerr << "    [DataFactory] UNKNOWN EXCEPTION in AddPropertySets!" << std::endl;
            }
        }
        
    }
    
    if (typeName == "Bridge") {
        std::cout << "    Initializing PropertySets for type: Bridge" << std::endl;
        
        {
            Handle(BrNode_adPropertySet) pset = CreatePset(model, "Pset_Bridge");
            if (obj->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: obj RootLabel is NULL!" << std::endl;
            if (pset.IsNull()) std::cerr << "    [DataFactory] ERROR: pset is NULL!" << std::endl;
            else if (pset->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: pset RootLabel is NULL!" << std::endl;
            
            try {
                obj->AddPropertySets(pset);
            } catch (Standard_Failure& e) {
                std::cerr << "    [DataFactory] OCCT EXCEPTION in AddPropertySets: " << e.GetMessageString() << std::endl;
            } catch (...) {
                std::cerr << "    [DataFactory] UNKNOWN EXCEPTION in AddPropertySets!" << std::endl;
            }
        }
        
    }
    
    if (typeName == "BridgePier") {
        std::cout << "    Initializing PropertySets for type: BridgePier" << std::endl;
        
        {
            Handle(BrNode_adPropertySet) pset = CreatePset(model, "Pset_Pier");
            if (obj->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: obj RootLabel is NULL!" << std::endl;
            if (pset.IsNull()) std::cerr << "    [DataFactory] ERROR: pset is NULL!" << std::endl;
            else if (pset->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: pset RootLabel is NULL!" << std::endl;
            
            try {
                obj->AddPropertySets(pset);
            } catch (Standard_Failure& e) {
                std::cerr << "    [DataFactory] OCCT EXCEPTION in AddPropertySets: " << e.GetMessageString() << std::endl;
            } catch (...) {
                std::cerr << "    [DataFactory] UNKNOWN EXCEPTION in AddPropertySets!" << std::endl;
            }
        }
        
        {
            Handle(BrNode_adPropertySet) pset = CreatePset(model, "Pset_MaterialConcrete");
            if (obj->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: obj RootLabel is NULL!" << std::endl;
            if (pset.IsNull()) std::cerr << "    [DataFactory] ERROR: pset is NULL!" << std::endl;
            else if (pset->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: pset RootLabel is NULL!" << std::endl;
            
            try {
                obj->AddPropertySets(pset);
            } catch (Standard_Failure& e) {
                std::cerr << "    [DataFactory] OCCT EXCEPTION in AddPropertySets: " << e.GetMessageString() << std::endl;
            } catch (...) {
                std::cerr << "    [DataFactory] UNKNOWN EXCEPTION in AddPropertySets!" << std::endl;
            }
        }
        
    }
    
    if (typeName == "PierBody") {
        std::cout << "    Initializing PropertySets for type: PierBody" << std::endl;
        
        {
            Handle(BrNode_adPropertySet) pset = CreatePset(model, "Pset_PierBody");
            if (obj->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: obj RootLabel is NULL!" << std::endl;
            if (pset.IsNull()) std::cerr << "    [DataFactory] ERROR: pset is NULL!" << std::endl;
            else if (pset->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: pset RootLabel is NULL!" << std::endl;
            
            try {
                obj->AddPropertySets(pset);
            } catch (Standard_Failure& e) {
                std::cerr << "    [DataFactory] OCCT EXCEPTION in AddPropertySets: " << e.GetMessageString() << std::endl;
            } catch (...) {
                std::cerr << "    [DataFactory] UNKNOWN EXCEPTION in AddPropertySets!" << std::endl;
            }
        }
        
        {
            Handle(BrNode_adPropertySet) pset = CreatePset(model, "Pset_PierBodyGeometry");
            if (obj->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: obj RootLabel is NULL!" << std::endl;
            if (pset.IsNull()) std::cerr << "    [DataFactory] ERROR: pset is NULL!" << std::endl;
            else if (pset->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: pset RootLabel is NULL!" << std::endl;
            
            try {
                obj->AddPropertySets(pset);
            } catch (Standard_Failure& e) {
                std::cerr << "    [DataFactory] OCCT EXCEPTION in AddPropertySets: " << e.GetMessageString() << std::endl;
            } catch (...) {
                std::cerr << "    [DataFactory] UNKNOWN EXCEPTION in AddPropertySets!" << std::endl;
            }
        }
        
        {
            Handle(BrNode_adPropertySet) pset = CreatePset(model, "Pset_MaterialConcrete");
            if (obj->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: obj RootLabel is NULL!" << std::endl;
            if (pset.IsNull()) std::cerr << "    [DataFactory] ERROR: pset is NULL!" << std::endl;
            else if (pset->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: pset RootLabel is NULL!" << std::endl;
            
            try {
                obj->AddPropertySets(pset);
            } catch (Standard_Failure& e) {
                std::cerr << "    [DataFactory] OCCT EXCEPTION in AddPropertySets: " << e.GetMessageString() << std::endl;
            } catch (...) {
                std::cerr << "    [DataFactory] UNKNOWN EXCEPTION in AddPropertySets!" << std::endl;
            }
        }
        
    }
    
    if (typeName == "PierTray") {
        std::cout << "    Initializing PropertySets for type: PierTray" << std::endl;
        
        {
            Handle(BrNode_adPropertySet) pset = CreatePset(model, "Pset_PierTrayGeometry");
            if (obj->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: obj RootLabel is NULL!" << std::endl;
            if (pset.IsNull()) std::cerr << "    [DataFactory] ERROR: pset is NULL!" << std::endl;
            else if (pset->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: pset RootLabel is NULL!" << std::endl;
            
            try {
                obj->AddPropertySets(pset);
            } catch (Standard_Failure& e) {
                std::cerr << "    [DataFactory] OCCT EXCEPTION in AddPropertySets: " << e.GetMessageString() << std::endl;
            } catch (...) {
                std::cerr << "    [DataFactory] UNKNOWN EXCEPTION in AddPropertySets!" << std::endl;
            }
        }
        
        {
            Handle(BrNode_adPropertySet) pset = CreatePset(model, "Pset_MaterialConcrete");
            if (obj->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: obj RootLabel is NULL!" << std::endl;
            if (pset.IsNull()) std::cerr << "    [DataFactory] ERROR: pset is NULL!" << std::endl;
            else if (pset->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: pset RootLabel is NULL!" << std::endl;
            
            try {
                obj->AddPropertySets(pset);
            } catch (Standard_Failure& e) {
                std::cerr << "    [DataFactory] OCCT EXCEPTION in AddPropertySets: " << e.GetMessageString() << std::endl;
            } catch (...) {
                std::cerr << "    [DataFactory] UNKNOWN EXCEPTION in AddPropertySets!" << std::endl;
            }
        }
        
    }
    
    if (typeName == "Abutment") {
        std::cout << "    Initializing PropertySets for type: Abutment" << std::endl;
        
        {
            Handle(BrNode_adPropertySet) pset = CreatePset(model, "Pset_Abutment");
            if (obj->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: obj RootLabel is NULL!" << std::endl;
            if (pset.IsNull()) std::cerr << "    [DataFactory] ERROR: pset is NULL!" << std::endl;
            else if (pset->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: pset RootLabel is NULL!" << std::endl;
            
            try {
                obj->AddPropertySets(pset);
            } catch (Standard_Failure& e) {
                std::cerr << "    [DataFactory] OCCT EXCEPTION in AddPropertySets: " << e.GetMessageString() << std::endl;
            } catch (...) {
                std::cerr << "    [DataFactory] UNKNOWN EXCEPTION in AddPropertySets!" << std::endl;
            }
        }
        
        {
            Handle(BrNode_adPropertySet) pset = CreatePset(model, "Pset_MaterialConcrete");
            if (obj->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: obj RootLabel is NULL!" << std::endl;
            if (pset.IsNull()) std::cerr << "    [DataFactory] ERROR: pset is NULL!" << std::endl;
            else if (pset->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: pset RootLabel is NULL!" << std::endl;
            
            try {
                obj->AddPropertySets(pset);
            } catch (Standard_Failure& e) {
                std::cerr << "    [DataFactory] OCCT EXCEPTION in AddPropertySets: " << e.GetMessageString() << std::endl;
            } catch (...) {
                std::cerr << "    [DataFactory] UNKNOWN EXCEPTION in AddPropertySets!" << std::endl;
            }
        }
        
    }
    
    if (typeName == "Girder") {
        std::cout << "    Initializing PropertySets for type: Girder" << std::endl;
        
        {
            Handle(BrNode_adPropertySet) pset = CreatePset(model, "Pset_Girder");
            if (obj->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: obj RootLabel is NULL!" << std::endl;
            if (pset.IsNull()) std::cerr << "    [DataFactory] ERROR: pset is NULL!" << std::endl;
            else if (pset->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: pset RootLabel is NULL!" << std::endl;
            
            try {
                obj->AddPropertySets(pset);
            } catch (Standard_Failure& e) {
                std::cerr << "    [DataFactory] OCCT EXCEPTION in AddPropertySets: " << e.GetMessageString() << std::endl;
            } catch (...) {
                std::cerr << "    [DataFactory] UNKNOWN EXCEPTION in AddPropertySets!" << std::endl;
            }
        }
        
        {
            Handle(BrNode_adPropertySet) pset = CreatePset(model, "Pset_GirderGeometry");
            if (obj->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: obj RootLabel is NULL!" << std::endl;
            if (pset.IsNull()) std::cerr << "    [DataFactory] ERROR: pset is NULL!" << std::endl;
            else if (pset->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: pset RootLabel is NULL!" << std::endl;
            
            try {
                obj->AddPropertySets(pset);
            } catch (Standard_Failure& e) {
                std::cerr << "    [DataFactory] OCCT EXCEPTION in AddPropertySets: " << e.GetMessageString() << std::endl;
            } catch (...) {
                std::cerr << "    [DataFactory] UNKNOWN EXCEPTION in AddPropertySets!" << std::endl;
            }
        }
        
        {
            Handle(BrNode_adPropertySet) pset = CreatePset(model, "Pset_MaterialConcrete");
            if (obj->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: obj RootLabel is NULL!" << std::endl;
            if (pset.IsNull()) std::cerr << "    [DataFactory] ERROR: pset is NULL!" << std::endl;
            else if (pset->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: pset RootLabel is NULL!" << std::endl;
            
            try {
                obj->AddPropertySets(pset);
            } catch (Standard_Failure& e) {
                std::cerr << "    [DataFactory] OCCT EXCEPTION in AddPropertySets: " << e.GetMessageString() << std::endl;
            } catch (...) {
                std::cerr << "    [DataFactory] UNKNOWN EXCEPTION in AddPropertySets!" << std::endl;
            }
        }
        
    }
    
    if (typeName == "BridgeFoundation") {
        std::cout << "    Initializing PropertySets for type: BridgeFoundation" << std::endl;
        
        {
            Handle(BrNode_adPropertySet) pset = CreatePset(model, "Pset_Foundation");
            if (obj->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: obj RootLabel is NULL!" << std::endl;
            if (pset.IsNull()) std::cerr << "    [DataFactory] ERROR: pset is NULL!" << std::endl;
            else if (pset->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: pset RootLabel is NULL!" << std::endl;
            
            try {
                obj->AddPropertySets(pset);
            } catch (Standard_Failure& e) {
                std::cerr << "    [DataFactory] OCCT EXCEPTION in AddPropertySets: " << e.GetMessageString() << std::endl;
            } catch (...) {
                std::cerr << "    [DataFactory] UNKNOWN EXCEPTION in AddPropertySets!" << std::endl;
            }
        }
        
        {
            Handle(BrNode_adPropertySet) pset = CreatePset(model, "Pset_MaterialConcrete");
            if (obj->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: obj RootLabel is NULL!" << std::endl;
            if (pset.IsNull()) std::cerr << "    [DataFactory] ERROR: pset is NULL!" << std::endl;
            else if (pset->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: pset RootLabel is NULL!" << std::endl;
            
            try {
                obj->AddPropertySets(pset);
            } catch (Standard_Failure& e) {
                std::cerr << "    [DataFactory] OCCT EXCEPTION in AddPropertySets: " << e.GetMessageString() << std::endl;
            } catch (...) {
                std::cerr << "    [DataFactory] UNKNOWN EXCEPTION in AddPropertySets!" << std::endl;
            }
        }
        
    }
    
    if (typeName == "PileCap") {
        std::cout << "    Initializing PropertySets for type: PileCap" << std::endl;
        
        {
            Handle(BrNode_adPropertySet) pset = CreatePset(model, "Pset_PileCapGeometry");
            if (obj->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: obj RootLabel is NULL!" << std::endl;
            if (pset.IsNull()) std::cerr << "    [DataFactory] ERROR: pset is NULL!" << std::endl;
            else if (pset->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: pset RootLabel is NULL!" << std::endl;
            
            try {
                obj->AddPropertySets(pset);
            } catch (Standard_Failure& e) {
                std::cerr << "    [DataFactory] OCCT EXCEPTION in AddPropertySets: " << e.GetMessageString() << std::endl;
            } catch (...) {
                std::cerr << "    [DataFactory] UNKNOWN EXCEPTION in AddPropertySets!" << std::endl;
            }
        }
        
        {
            Handle(BrNode_adPropertySet) pset = CreatePset(model, "Pset_MaterialConcrete");
            if (obj->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: obj RootLabel is NULL!" << std::endl;
            if (pset.IsNull()) std::cerr << "    [DataFactory] ERROR: pset is NULL!" << std::endl;
            else if (pset->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: pset RootLabel is NULL!" << std::endl;
            
            try {
                obj->AddPropertySets(pset);
            } catch (Standard_Failure& e) {
                std::cerr << "    [DataFactory] OCCT EXCEPTION in AddPropertySets: " << e.GetMessageString() << std::endl;
            } catch (...) {
                std::cerr << "    [DataFactory] UNKNOWN EXCEPTION in AddPropertySets!" << std::endl;
            }
        }
        
    }
    
    if (typeName == "Pile") {
        std::cout << "    Initializing PropertySets for type: Pile" << std::endl;
        
        {
            Handle(BrNode_adPropertySet) pset = CreatePset(model, "Pset_SinglePileGeometry");
            if (obj->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: obj RootLabel is NULL!" << std::endl;
            if (pset.IsNull()) std::cerr << "    [DataFactory] ERROR: pset is NULL!" << std::endl;
            else if (pset->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: pset RootLabel is NULL!" << std::endl;
            
            try {
                obj->AddPropertySets(pset);
            } catch (Standard_Failure& e) {
                std::cerr << "    [DataFactory] OCCT EXCEPTION in AddPropertySets: " << e.GetMessageString() << std::endl;
            } catch (...) {
                std::cerr << "    [DataFactory] UNKNOWN EXCEPTION in AddPropertySets!" << std::endl;
            }
        }
        
        {
            Handle(BrNode_adPropertySet) pset = CreatePset(model, "Pset_MaterialConcrete");
            if (obj->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: obj RootLabel is NULL!" << std::endl;
            if (pset.IsNull()) std::cerr << "    [DataFactory] ERROR: pset is NULL!" << std::endl;
            else if (pset->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: pset RootLabel is NULL!" << std::endl;
            
            try {
                obj->AddPropertySets(pset);
            } catch (Standard_Failure& e) {
                std::cerr << "    [DataFactory] OCCT EXCEPTION in AddPropertySets: " << e.GetMessageString() << std::endl;
            } catch (...) {
                std::cerr << "    [DataFactory] UNKNOWN EXCEPTION in AddPropertySets!" << std::endl;
            }
        }
        
    }
    
    if (typeName == "BedStone") {
        std::cout << "    Initializing PropertySets for type: BedStone" << std::endl;
        
        {
            Handle(BrNode_adPropertySet) pset = CreatePset(model, "Pset_BedStoneGeometry");
            if (obj->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: obj RootLabel is NULL!" << std::endl;
            if (pset.IsNull()) std::cerr << "    [DataFactory] ERROR: pset is NULL!" << std::endl;
            else if (pset->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: pset RootLabel is NULL!" << std::endl;
            
            try {
                obj->AddPropertySets(pset);
            } catch (Standard_Failure& e) {
                std::cerr << "    [DataFactory] OCCT EXCEPTION in AddPropertySets: " << e.GetMessageString() << std::endl;
            } catch (...) {
                std::cerr << "    [DataFactory] UNKNOWN EXCEPTION in AddPropertySets!" << std::endl;
            }
        }
        
        {
            Handle(BrNode_adPropertySet) pset = CreatePset(model, "Pset_MaterialConcrete");
            if (obj->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: obj RootLabel is NULL!" << std::endl;
            if (pset.IsNull()) std::cerr << "    [DataFactory] ERROR: pset is NULL!" << std::endl;
            else if (pset->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: pset RootLabel is NULL!" << std::endl;
            
            try {
                obj->AddPropertySets(pset);
            } catch (Standard_Failure& e) {
                std::cerr << "    [DataFactory] OCCT EXCEPTION in AddPropertySets: " << e.GetMessageString() << std::endl;
            } catch (...) {
                std::cerr << "    [DataFactory] UNKNOWN EXCEPTION in AddPropertySets!" << std::endl;
            }
        }
        
    }
    
    if (typeName == "Bearing") {
        std::cout << "    Initializing PropertySets for type: Bearing" << std::endl;
        
        {
            Handle(BrNode_adPropertySet) pset = CreatePset(model, "Pset_Bearing");
            if (obj->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: obj RootLabel is NULL!" << std::endl;
            if (pset.IsNull()) std::cerr << "    [DataFactory] ERROR: pset is NULL!" << std::endl;
            else if (pset->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: pset RootLabel is NULL!" << std::endl;
            
            try {
                obj->AddPropertySets(pset);
            } catch (Standard_Failure& e) {
                std::cerr << "    [DataFactory] OCCT EXCEPTION in AddPropertySets: " << e.GetMessageString() << std::endl;
            } catch (...) {
                std::cerr << "    [DataFactory] UNKNOWN EXCEPTION in AddPropertySets!" << std::endl;
            }
        }
        
        {
            Handle(BrNode_adPropertySet) pset = CreatePset(model, "Pset_BearingGeometry");
            if (obj->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: obj RootLabel is NULL!" << std::endl;
            if (pset.IsNull()) std::cerr << "    [DataFactory] ERROR: pset is NULL!" << std::endl;
            else if (pset->RootLabel().IsNull()) std::cerr << "    [DataFactory] ERROR: pset RootLabel is NULL!" << std::endl;
            
            try {
                obj->AddPropertySets(pset);
            } catch (Standard_Failure& e) {
                std::cerr << "    [DataFactory] OCCT EXCEPTION in AddPropertySets: " << e.GetMessageString() << std::endl;
            } catch (...) {
                std::cerr << "    [DataFactory] UNKNOWN EXCEPTION in AddPropertySets!" << std::endl;
            }
        }
        
    }
    

    return obj;
}

Handle(BrNode_adPropertySet) DataFactory::CreatePset(const Handle(ActData_BaseModel)& model, 
                                                     const TCollection_AsciiString& psetName)
{
    std::cout << "      DataFactory::CreatePset: " << psetName.ToCString() << std::endl;
    Handle(DataModel) dm = Handle(DataModel)::DownCast(model);
    Handle(BrNode_adPropertySet) pset = dm->AddadPropertySet();
    
    std::cout << "      [CreatePset] Node created. Checking Label..." << std::endl;
    if (pset->RootLabel().IsNull()) {
        std::cerr << "      [CreatePset] ERROR: pset Label is NULL!" << std::endl;
    } else {
        TCollection_AsciiString entry;
        TDF_Tool::Entry(pset->RootLabel(), entry);
        std::cout << "      [CreatePset] pset Label: " << entry.ToCString() << std::endl;
    }
    
    if (pset.IsNull()) return nullptr;

    pset->SetName(psetName);

    
    if (psetName == "Pset_SlopeGeometry") {
        
        {
            Handle(BrNode_adProperty) prop = dm->AddadProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("Length");
                prop->SetPropertyValue("20000.0");
                prop->SetValueType("Real");
                pset->AddProperties(prop);
            }
        }
        
        {
            Handle(BrNode_adProperty) prop = dm->AddadProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("Height");
                prop->SetPropertyValue("8000.0");
                prop->SetValueType("Real");
                pset->AddProperties(prop);
            }
        }
        
        {
            Handle(BrNode_adProperty) prop = dm->AddadProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("SlopeRatio");
                prop->SetPropertyValue("1.5");
                prop->SetValueType("Real");
                pset->AddProperties(prop);
            }
        }
        
    }
    
    if (psetName == "Pset_MaterialConcrete") {
        
        {
            Handle(BrNode_adProperty) prop = dm->AddadProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("StrengthGrade");
                prop->SetPropertyValue("C30");
                prop->SetValueType("enum");
                pset->AddProperties(prop);
            }
        }
        
    }
    
    if (psetName == "Pset_PierBodyGeometry") {
        
        {
            Handle(BrNode_adProperty) prop = dm->AddadProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("ModelNumber");
                prop->SetPropertyValue("");
                prop->SetValueType("String");
                pset->AddProperties(prop);
            }
        }
        
        {
            Handle(BrNode_adProperty) prop = dm->AddadProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("Height");
                prop->SetPropertyValue("12000.0");
                prop->SetValueType("Real");
                pset->AddProperties(prop);
            }
        }
        
    }
    
    if (psetName == "Pset_PierTrayGeometry") {
        
        {
            Handle(BrNode_adProperty) prop = dm->AddadProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("ModelNumber");
                prop->SetPropertyValue("");
                prop->SetValueType("String");
                pset->AddProperties(prop);
            }
        }
        
        {
            Handle(BrNode_adProperty) prop = dm->AddadProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("Length");
                prop->SetPropertyValue("200.0");
                prop->SetValueType("Real");
                pset->AddProperties(prop);
            }
        }
        
        {
            Handle(BrNode_adProperty) prop = dm->AddadProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("Width");
                prop->SetPropertyValue("200.0");
                prop->SetValueType("Real");
                pset->AddProperties(prop);
            }
        }
        
        {
            Handle(BrNode_adProperty) prop = dm->AddadProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("Height");
                prop->SetPropertyValue("2750.0");
                prop->SetValueType("Real");
                pset->AddProperties(prop);
            }
        }
        
    }
    
    if (psetName == "Pset_GirderGeometry") {
        
        {
            Handle(BrNode_adProperty) prop = dm->AddadProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("ModelNumber");
                prop->SetPropertyValue("");
                prop->SetValueType("String");
                pset->AddProperties(prop);
            }
        }
        
        {
            Handle(BrNode_adProperty) prop = dm->AddadProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("Length");
                prop->SetPropertyValue("31500.0");
                prop->SetValueType("Real");
                pset->AddProperties(prop);
            }
        }
        
        {
            Handle(BrNode_adProperty) prop = dm->AddadProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("Width");
                prop->SetPropertyValue("12600.0");
                prop->SetValueType("Real");
                pset->AddProperties(prop);
            }
        }
        
        {
            Handle(BrNode_adProperty) prop = dm->AddadProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("Height");
                prop->SetPropertyValue("3000.0");
                prop->SetValueType("Real");
                pset->AddProperties(prop);
            }
        }
        
    }
    
    if (psetName == "Pset_PileCapGeometry") {
        
        {
            Handle(BrNode_adProperty) prop = dm->AddadProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("ModelNumber");
                prop->SetPropertyValue("");
                prop->SetValueType("String");
                pset->AddProperties(prop);
            }
        }
        
        {
            Handle(BrNode_adProperty) prop = dm->AddadProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("Layers");
                prop->SetPropertyValue("2");
                prop->SetValueType("Int");
                pset->AddProperties(prop);
            }
        }
        
        {
            Handle(BrNode_adProperty) prop = dm->AddadProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("TotalH");
                prop->SetPropertyValue("0.0");
                prop->SetValueType("Real");
                pset->AddProperties(prop);
            }
        }
        
    }
    
    if (psetName == "Pset_PileGeometry") {
        
        {
            Handle(BrNode_adProperty) prop = dm->AddadProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("ModelNumber");
                prop->SetPropertyValue("");
                prop->SetValueType("String");
                pset->AddProperties(prop);
            }
        }
        
        {
            Handle(BrNode_adProperty) prop = dm->AddadProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("Diameter");
                prop->SetPropertyValue("1000.0");
                prop->SetValueType("Real");
                pset->AddProperties(prop);
            }
        }
        
        {
            Handle(BrNode_adProperty) prop = dm->AddadProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("Length");
                prop->SetPropertyValue("15000.0");
                prop->SetValueType("Real");
                pset->AddProperties(prop);
            }
        }
        
        {
            Handle(BrNode_adProperty) prop = dm->AddadProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("Layout");
                prop->SetPropertyValue("");
                prop->SetValueType("String");
                pset->AddProperties(prop);
            }
        }
        
        {
            Handle(BrNode_adProperty) prop = dm->AddadProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("HSpacing");
                prop->SetPropertyValue("2500.0");
                prop->SetValueType("Real");
                pset->AddProperties(prop);
            }
        }
        
        {
            Handle(BrNode_adProperty) prop = dm->AddadProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("VSpacing");
                prop->SetPropertyValue("3000.0");
                prop->SetValueType("Real");
                pset->AddProperties(prop);
            }
        }
        
    }
    
    if (psetName == "Pset_BedStoneGeometry") {
        
        {
            Handle(BrNode_adProperty) prop = dm->AddadProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("ModelNumber");
                prop->SetPropertyValue("");
                prop->SetValueType("String");
                pset->AddProperties(prop);
            }
        }
        
        {
            Handle(BrNode_adProperty) prop = dm->AddadProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("Length");
                prop->SetPropertyValue("1200.0");
                prop->SetValueType("Real");
                pset->AddProperties(prop);
            }
        }
        
        {
            Handle(BrNode_adProperty) prop = dm->AddadProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("Width");
                prop->SetPropertyValue("1200.0");
                prop->SetValueType("Real");
                pset->AddProperties(prop);
            }
        }
        
        {
            Handle(BrNode_adProperty) prop = dm->AddadProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("Height");
                prop->SetPropertyValue("300.0");
                prop->SetValueType("Real");
                pset->AddProperties(prop);
            }
        }
        
    }
    
    if (psetName == "Pset_BearingGeometry") {
        
        {
            Handle(BrNode_adProperty) prop = dm->AddadProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("ModelNumber");
                prop->SetPropertyValue("");
                prop->SetValueType("String");
                pset->AddProperties(prop);
            }
        }
        
        {
            Handle(BrNode_adProperty) prop = dm->AddadProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("Length");
                prop->SetPropertyValue("200.0");
                prop->SetValueType("Real");
                pset->AddProperties(prop);
            }
        }
        
        {
            Handle(BrNode_adProperty) prop = dm->AddadProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("Width");
                prop->SetPropertyValue("200.0");
                prop->SetValueType("Real");
                pset->AddProperties(prop);
            }
        }
        
        {
            Handle(BrNode_adProperty) prop = dm->AddadProperty();
            if (!prop.IsNull()) {
                prop->SetPropertyName("Height");
                prop->SetPropertyValue("200.0");
                prop->SetValueType("Real");
                pset->AddProperties(prop);
            }
        }
        
    }
    

    return pset;
}