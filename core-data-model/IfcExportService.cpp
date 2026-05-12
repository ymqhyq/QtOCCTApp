#define HAS_SCHEMA_4x3_add2
#include "IfcExportService.h"
#include <TCollection_AsciiString.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <BRepTools.hxx>
#include <iostream>
#include <fstream>
#include <boost/make_shared.hpp>
#include <random>
#include <vector>

// IfcOpenShell headers
#include <ifcparse/IfcFile.h>
#include <ifcparse/Ifc4x3_add2.h>
#include <ifcgeom/Serialization/Serialization.h>

// Data model headers
#include "generated/BrNode_adGeometricDef.h"
#include "generated/BrNode_adModelRoot.h"
#include "generated/BrNode_adGeometry.h"

// ABI Safe Protection
static std::vector<boost::shared_ptr<void>> g_leak_protection;

std::string IfcExportService::ToStdString(const TCollection_ExtendedString& extStr) {
    TCollection_AsciiString ascii(extStr);
    return ascii.ToCString();
}

static std::string GenerateIfcGuid() {
    static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_$";
    static std::mt19937 gen(std::random_device{}());
    static std::uniform_int_distribution<> dis(0, 63);
    std::string res;
    for (int i = 0; i < 22; ++i) res += charset[dis(gen)];
    return res;
}

Ifc4x3_add2::IfcProduct* IfcExportService::CreateIfcProduct(const Handle(BrNode_adObject)& adObj,
                                                          IfcParse::IfcFile& file,
                                                          Ifc4x3_add2::IfcOwnerHistory* ownerHist,
                                                          Ifc4x3_add2::IfcLocalPlacement* placement) {
    std::string type = ToStdString(adObj->GetObjectType());
    std::string name = ToStdString(adObj->GetName());
    std::string guid = GenerateIfcGuid();

    if (type == "Footing" || type == "BridgeFoundation") {
        return new Ifc4x3_add2::IfcFooting(guid, ownerHist, name, boost::none, type, placement, nullptr, boost::none, boost::none);
    }
    
    return new Ifc4x3_add2::IfcBuildingElementProxy(guid, ownerHist, name, boost::none, type, placement, nullptr, boost::none, boost::none);
}

void IfcExportService::AddGeometryToProduct(const Handle(BrNode_adObject)& adObj,
                                           IfcParse::IfcFile& file,
                                           Ifc4x3_add2::IfcProduct* product,
                                           Ifc4x3_add2::IfcGeometricRepresentationContext* context) {
    std::string nodeName = ToStdString(adObj->GetName());
    
    Handle(ActAPI_IDataCursor) geoNodeCursor = adObj->GetGeometry();
    if (geoNodeCursor.IsNull()) return;

    Handle(BrNode_adGeometry) geoNode = Handle(BrNode_adGeometry)::DownCast(geoNodeCursor);
    if (geoNode.IsNull()) return;

    Handle(BrNode_adGeometricDef) geoDef = Handle(BrNode_adGeometricDef)::DownCast(geoNode->GetGeometryRef());
    if (geoDef.IsNull()) return;

    TopoDS_Shape shape = geoDef->GetShape();
    if (shape.IsNull()) return;

    try {
        IfcUtil::IfcBaseClass* serialised = IfcGeom::serialise("IFC4X3_ADD2", shape, true);
        if (!serialised) return;

        std::string entityName = serialised->declaration().name();
        
        if (entityName == "IfcProductDefinitionShape") {
            // Case 1: Serialise returned the whole representation structure
            auto prodRep = static_cast<Ifc4x3_add2::IfcProductDefinitionShape*>(serialised);
            file.addEntity(prodRep);
            
            // Fix contexts in the generated representations
            auto representations = prodRep->Representations();
            if (representations) {
                for (auto it = representations->begin(); it != representations->end(); ++it) {
                    auto rep = static_cast<Ifc4x3_add2::IfcRepresentation*>(*it);
                    if (rep) {
                        file.addEntity(rep);
                        rep->setContextOfItems(context);
                        
                        // Also ensure items are added
                        auto items = rep->Items();
                        if (items) {
                            for (auto it2 = items->begin(); it2 != items->end(); ++it2) {
                                file.addEntity(*it2);
                            }
                        }
                    }
                }
            }
            product->setRepresentation(prodRep);
        } else {
            // Case 2: Serialise returned a single representation item (e.g. IfcAdvancedBrep)
            auto repItem = static_cast<Ifc4x3_add2::IfcRepresentationItem*>(serialised);
            file.addEntity(repItem);

            auto items = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcRepresentationItem>>();
            items->push(repItem);
            g_leak_protection.push_back(items);

            auto rep = new Ifc4x3_add2::IfcShapeRepresentation(
                context, boost::optional<std::string>("Body"), boost::optional<std::string>("AdvancedBrep"), items);
            file.addEntity(rep);

            auto representations = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcRepresentation>>();
            representations->push(rep);
            g_leak_protection.push_back(representations);

            auto productRep = new Ifc4x3_add2::IfcProductDefinitionShape(boost::none, boost::none, representations);
            file.addEntity(productRep);

            product->setRepresentation(productRep);
        }
        
        std::cout << "[IfcExportService] +++ ADDED GEOMETRY for " << nodeName << " (" << entityName << ")" << std::endl;
    } catch (...) {
        std::cerr << "[IfcExportService] !!! Error adding geometry for " << nodeName << std::endl;
    }
}

void IfcExportService::TraverseAndExport(const Handle(BrNode_adObject)& adObj,
                                       IfcParse::IfcFile& file,
                                       Ifc4x3_add2::IfcObjectDefinition* parentIfc,
                                       Ifc4x3_add2::IfcObjectPlacement* parentPlacement,
                                       Ifc4x3_add2::IfcOwnerHistory* ownerHist,
                                       Ifc4x3_add2::IfcGeometricRepresentationContext* context,
                                       int& exportedCount) {
    if (adObj.IsNull()) return;

    // 1. Calculate Local Placement
    auto origin = new Ifc4x3_add2::IfcCartesianPoint(std::vector<double>{0.0, 0.0, 0.0});
    file.addEntity(origin);
    
    Handle(ActAPI_IUserParameter) p = adObj->Parameter(BrNode_adObject::PID_ObjectPlacement);
    Handle(ActData_RealArrayParameter) typedP = ActData_ParameterFactory::AsRealArray(p);
    if (!typedP.IsNull() && typedP->NbElements() >= 3) {
        origin->setCoordinates(std::vector<double>{typedP->GetElement(0), typedP->GetElement(1), typedP->GetElement(2)});
    }

    auto dirZ = new Ifc4x3_add2::IfcDirection(std::vector<double>{0.0, 0.0, 1.0});
    file.addEntity(dirZ);
    auto dirX = new Ifc4x3_add2::IfcDirection(std::vector<double>{1.0, 0.0, 0.0});
    file.addEntity(dirX);
    auto axis2 = new Ifc4x3_add2::IfcAxis2Placement3D(origin, dirZ, dirX);
    file.addEntity(axis2);

    auto localPlacement = new Ifc4x3_add2::IfcLocalPlacement(parentPlacement, axis2);
    file.addEntity(localPlacement);

    // 2. Create IFC Product
    Ifc4x3_add2::IfcProduct* product = CreateIfcProduct(adObj, file, ownerHist, localPlacement);
    file.addEntity(product);
    exportedCount++;

    // 3. Add Geometry
    AddGeometryToProduct(adObj, file, product, context);

    // 4. Link to Parent (Spatial Aggregation)
    auto childSet = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcObjectDefinition>>();
    childSet->push(product);
    g_leak_protection.push_back(childSet);

    auto rel = new Ifc4x3_add2::IfcRelAggregates(
        GenerateIfcGuid(), ownerHist, boost::none, boost::none, parentIfc, childSet);
    file.addEntity(rel);

    // 5. Recursive Children
    NCollection_Sequence<Handle(BrNode_adObject)> children = adObj->GetSubObjectsList();
    for (int i = 1; i <= children.Length(); ++i) {
        TraverseAndExport(children.Value(i), file, product, localPlacement, ownerHist, context, exportedCount);
    }
}

bool IfcExportService::Export(const Handle(DataModel)& model, const std::string& filename) {
    if (model.IsNull()) return false;

    try {
        std::cout << "[IfcExportService] Starting advanced IFC 4x3 export..." << std::endl;

        const IfcParse::schema_definition* schema = IfcParse::schema_by_name("IFC4X3_ADD2");
        if (!schema) return false;

        IfcParse::IfcFile* file_ptr = new IfcParse::IfcFile(schema, IfcParse::FT_IFCSPF);
        IfcParse::IfcFile& file = *file_ptr;

        // OwnerHistory
        auto person = new Ifc4x3_add2::IfcPerson(boost::none, boost::none, std::string("User"), boost::none, boost::none, boost::none, boost::none, boost::none);
        file.addEntity(person);
        auto org = new Ifc4x3_add2::IfcOrganization(boost::none, std::string("QtOCCTApp"), boost::none, boost::none, boost::none);
        file.addEntity(org);
        auto personOrg = new Ifc4x3_add2::IfcPersonAndOrganization(person, org, boost::none);
        file.addEntity(personOrg);
        auto app = new Ifc4x3_add2::IfcApplication(org, std::string("1.0"), std::string("QtOCCTApp"), std::string("QtOCCTApp"));
        file.addEntity(app);
        auto ownerHist = new Ifc4x3_add2::IfcOwnerHistory(personOrg, app, boost::none, 
            boost::optional<Ifc4x3_add2::IfcChangeActionEnum::Value>(Ifc4x3_add2::IfcChangeActionEnum::IfcChangeAction_NOCHANGE),
            boost::none, nullptr, nullptr, (int)time(NULL));
        file.addEntity(ownerHist);

        // Context
        auto origin = new Ifc4x3_add2::IfcCartesianPoint(std::vector<double>{0.0, 0.0, 0.0});
        file.addEntity(origin);
        auto dirZ = new Ifc4x3_add2::IfcDirection(std::vector<double>{0.0, 0.0, 1.0});
        file.addEntity(dirZ);
        auto dirX = new Ifc4x3_add2::IfcDirection(std::vector<double>{1.0, 0.0, 0.0});
        file.addEntity(dirX);
        auto worldCS = new Ifc4x3_add2::IfcAxis2Placement3D(origin, dirZ, dirX);
        file.addEntity(worldCS);

        auto context = new Ifc4x3_add2::IfcGeometricRepresentationContext(boost::optional<std::string>("Model"), boost::optional<std::string>("Model"), 3, boost::optional<double>(1e-5), worldCS, nullptr);
        file.addEntity(context);

        // Units
        auto siLength = new Ifc4x3_add2::IfcSIUnit(Ifc4x3_add2::IfcUnitEnum::IfcUnit_LENGTHUNIT, boost::optional<Ifc4x3_add2::IfcSIPrefix::Value>(Ifc4x3_add2::IfcSIPrefix::IfcSIPrefix_MILLI), Ifc4x3_add2::IfcSIUnitName::IfcSIUnitName_METRE);
        file.addEntity(siLength);
        auto units = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcUnit>>();
        units->push(siLength);
        g_leak_protection.push_back(units);
        auto unitAssign = new Ifc4x3_add2::IfcUnitAssignment(units);
        file.addEntity(unitAssign);

        // Project
        auto contexts = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcRepresentationContext>>();
        contexts->push(context);
        g_leak_protection.push_back(contexts);
        auto project = new Ifc4x3_add2::IfcProject(GenerateIfcGuid(), ownerHist, boost::optional<std::string>("Bridge Project"), boost::none, boost::none, boost::none, boost::none, contexts, unitAssign);
        file.addEntity(project);

        // Site
        auto sitePlacement = new Ifc4x3_add2::IfcLocalPlacement(nullptr, worldCS);
        file.addEntity(sitePlacement);
        auto site = new Ifc4x3_add2::IfcSite(GenerateIfcGuid(), ownerHist, boost::optional<std::string>("Site"), boost::none, boost::none, sitePlacement, nullptr, boost::none, boost::optional<Ifc4x3_add2::IfcElementCompositionEnum::Value>(Ifc4x3_add2::IfcElementCompositionEnum::IfcElementComposition_ELEMENT), boost::none, boost::none, boost::none, boost::none, nullptr);
        file.addEntity(site);

        auto siteSet = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcObjectDefinition>>();
        siteSet->push(site);
        g_leak_protection.push_back(siteSet);
        file.addEntity(new Ifc4x3_add2::IfcRelAggregates(GenerateIfcGuid(), ownerHist, boost::none, boost::none, project, siteSet));

        // Bridge
        auto bridgePlacement = new Ifc4x3_add2::IfcLocalPlacement(sitePlacement, worldCS);
        file.addEntity(bridgePlacement);
        auto bridge = new Ifc4x3_add2::IfcBridge(GenerateIfcGuid(), ownerHist, boost::optional<std::string>("Bridge"), boost::none, boost::none, bridgePlacement, nullptr, boost::none, boost::none, boost::optional<Ifc4x3_add2::IfcBridgeTypeEnum::Value>(Ifc4x3_add2::IfcBridgeTypeEnum::IfcBridgeType_GIRDER));
        file.addEntity(bridge);

        auto bridgeSet = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcObjectDefinition>>();
        bridgeSet->push(bridge);
        g_leak_protection.push_back(bridgeSet);
        file.addEntity(new Ifc4x3_add2::IfcRelAggregates(GenerateIfcGuid(), ownerHist, boost::none, boost::none, site, bridgeSet));

        // Recursive Traversal
        Handle(ActAPI_INode) rootNode = model->GetRootNode();
        if (!rootNode.IsNull()) {
            int exportedCount = 0;
            auto it = rootNode->GetChildIterator();
            for (; it->More(); it->Next()) {
                Handle(BrNode_adObject) obj = Handle(BrNode_adObject)::DownCast(it->Value());
                if (!obj.IsNull()) {
                    TraverseAndExport(obj, file, bridge, bridgePlacement, ownerHist, context, exportedCount);
                }
            }
            std::cout << "[IfcExportService] Exported " << exportedCount << " nodes." << std::endl;
        }

        std::ofstream f(filename);
        if (f.is_open()) {
            f << file;
            f.close();
            std::cout << "[IfcExportService] File saved: " << filename << std::endl;
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[IfcExportService] ERROR: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "[IfcExportService] UNKNOWN ERROR" << std::endl;
        return false;
    }
}
