#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <random>
#include <ctime>

// OCCT
#include <TopoDS_Shape.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>

// IfcOpenShell
#define HAS_SCHEMA_4x3_add2
#include <ifcgeom/Serialization/Serialization.h>
#include <ifcparse/Ifc4x3_add2.h>
#include <ifcparse/IfcFile.h>

#include <boost/make_shared.hpp>

// Boost exception handler
namespace boost {
    void throw_exception(std::exception const& e) {
        std::cerr << "[FATAL] Boost exception: " << e.what() << std::endl;
        std::abort();
    }
}

// Generate Standard IFC GUID
static std::string GenerateIfcGuid() {
  static const char charset[] =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_$";
  static std::mt19937 gen(std::random_device{}());
  static std::uniform_int_distribution<> dis(0, 63);
  std::string res;
  for (int i = 0; i < 22; ++i)
    res += charset[dis(gen)];
  return res;
}

// Safe Entity Creation Helper to bypass the pre-compiled header bug
template <typename T>
static T* CreateEntity(IfcParse::IfcFile& file) {
    auto* inst = file.create<T>();
    if (inst) {
        inst->file_ = nullptr; // Reset the bypass flag
        file.addEntity(inst);  // Call full registration logic
    }
    return inst;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: brep_to_ifc <input.brep> <output.ifc>" << std::endl;
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = argv[2];

    std::cout << "=== BRep to IFC Converter (Safe Dynamic Factory Mode) ===" << std::endl;

    // 1. Load BRep
    TopoDS_Shape shape;
    BRep_Builder builder;
    if (!BRepTools::Read(shape, inputPath.c_str(), builder)) {
        std::cerr << "[ERROR] Failed to read BRep." << std::endl;
        return 1;
    }
    std::cout << "[OK] Successfully loaded BRep shape." << std::endl;

    // 2. Setup Schema
    const IfcParse::schema_definition* schema = IfcParse::schema_by_name("Ifc4x3_add2");
    if (schema) {
        IfcParse::register_schema(const_cast<IfcParse::schema_definition*>(schema));
    } else {
        std::cerr << "[ERROR] Schema Ifc4x3_add2 not found." << std::endl;
        return 1;
    }
    std::cout << "[OK] Registered schema: " << schema->name() << std::endl;

    try {
        IfcParse::IfcFile file(schema, IfcParse::FT_IFCSPF);

        // 3. Construct Standard IFC Structure
        std::cout << "[INFO] Constructing OwnerHistory..." << std::endl;
        auto person = CreateEntity<Ifc4x3_add2::IfcPerson>(file);
        person->setGivenName(std::string("User"));

        auto org = CreateEntity<Ifc4x3_add2::IfcOrganization>(file);
        org->setName(std::string("QtOCCTApp"));

        auto personOrg = CreateEntity<Ifc4x3_add2::IfcPersonAndOrganization>(file);
        personOrg->setThePerson(person);
        personOrg->setTheOrganization(org);

        auto app = CreateEntity<Ifc4x3_add2::IfcApplication>(file);
        app->setApplicationDeveloper(org);
        app->setVersion(std::string("1.0"));
        app->setApplicationFullName(std::string("QtOCCTApp"));
        app->setApplicationIdentifier(std::string("QtOCCTApp"));

        auto ownerHist = CreateEntity<Ifc4x3_add2::IfcOwnerHistory>(file);
        ownerHist->setOwningUser(personOrg);
        ownerHist->setOwningApplication(app);
        ownerHist->setChangeAction(Ifc4x3_add2::IfcChangeActionEnum::IfcChangeAction_NOCHANGE);
        ownerHist->setCreationDate((int)time(NULL));

        std::cout << "[INFO] Constructing Geometrical Context..." << std::endl;
        auto origin = CreateEntity<Ifc4x3_add2::IfcCartesianPoint>(file);
        origin->setCoordinates(std::vector<double>{0.0, 0.0, 0.0});

        auto dirZ = CreateEntity<Ifc4x3_add2::IfcDirection>(file);
        dirZ->setDirectionRatios(std::vector<double>{0.0, 0.0, 1.0});

        auto dirX = CreateEntity<Ifc4x3_add2::IfcDirection>(file);
        dirX->setDirectionRatios(std::vector<double>{1.0, 0.0, 0.0});

        auto worldCS = CreateEntity<Ifc4x3_add2::IfcAxis2Placement3D>(file);
        worldCS->setLocation(origin);
        worldCS->setAxis(dirZ);
        worldCS->setRefDirection(dirX);

        auto context = CreateEntity<Ifc4x3_add2::IfcGeometricRepresentationContext>(file);
        context->setContextType(std::string("Model"));
        context->setContextIdentifier(std::string("Model"));
        context->setCoordinateSpaceDimension(3);
        context->setPrecision(1e-5);
        context->setWorldCoordinateSystem(worldCS);

        std::cout << "[INFO] Constructing Units..." << std::endl;
        auto siLength = CreateEntity<Ifc4x3_add2::IfcSIUnit>(file);
        siLength->setUnitType(Ifc4x3_add2::IfcUnitEnum::IfcUnit_LENGTHUNIT);
        siLength->setPrefix(Ifc4x3_add2::IfcSIPrefix::IfcSIPrefix_MILLI);
        siLength->setName(Ifc4x3_add2::IfcSIUnitName::IfcSIUnitName_METRE);

        auto units = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcUnit>>();
        units->push(siLength);
        
        auto unitAssign = CreateEntity<Ifc4x3_add2::IfcUnitAssignment>(file);
        unitAssign->setUnits(units);

        std::cout << "[INFO] Constructing Project..." << std::endl;
        auto contexts = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcRepresentationContext>>();
        contexts->push(context);
        
        auto project = CreateEntity<Ifc4x3_add2::IfcProject>(file);
        project->setGlobalId(GenerateIfcGuid());
        project->setOwnerHistory(ownerHist);
        project->setName(std::string("BRep Project"));
        project->setRepresentationContexts(contexts);
        project->setUnitsInContext(unitAssign);

        std::cout << "[INFO] Constructing Site..." << std::endl;
        auto sitePlacement = CreateEntity<Ifc4x3_add2::IfcLocalPlacement>(file);
        sitePlacement->setRelativePlacement(worldCS);

        auto site = CreateEntity<Ifc4x3_add2::IfcSite>(file);
        site->setGlobalId(GenerateIfcGuid());
        site->setOwnerHistory(ownerHist);
        site->setName(std::string("Site"));
        site->setObjectPlacement(sitePlacement);
        site->setCompositionType(Ifc4x3_add2::IfcElementCompositionEnum::IfcElementComposition_ELEMENT);

        auto siteSet = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcObjectDefinition>>();
        siteSet->push(site);
        
        auto relProjSite = CreateEntity<Ifc4x3_add2::IfcRelAggregates>(file);
        relProjSite->setGlobalId(GenerateIfcGuid());
        relProjSite->setOwnerHistory(ownerHist);
        relProjSite->setRelatingObject(project);
        relProjSite->setRelatedObjects(siteSet);

        std::cout << "[INFO] Constructing Building..." << std::endl;
        auto buildingPlacement = CreateEntity<Ifc4x3_add2::IfcLocalPlacement>(file);
        buildingPlacement->setPlacementRelTo(sitePlacement);
        buildingPlacement->setRelativePlacement(worldCS);

        auto building = CreateEntity<Ifc4x3_add2::IfcBuilding>(file);
        building->setGlobalId(GenerateIfcGuid());
        building->setOwnerHistory(ownerHist);
        building->setName(std::string("Building"));
        building->setObjectPlacement(buildingPlacement);
        building->setCompositionType(Ifc4x3_add2::IfcElementCompositionEnum::IfcElementComposition_ELEMENT);

        auto buildingSet = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcObjectDefinition>>();
        buildingSet->push(building);
        
        auto relSiteBuilding = CreateEntity<Ifc4x3_add2::IfcRelAggregates>(file);
        relSiteBuilding->setGlobalId(GenerateIfcGuid());
        relSiteBuilding->setOwnerHistory(ownerHist);
        relSiteBuilding->setRelatingObject(site);
        relSiteBuilding->setRelatedObjects(buildingSet);

        // Storey deleted by request to simplify and test direct building containment

        std::cout << "[OK] Spatial structures constructed successfully." << std::endl;

        // 4. Convert geometry (Planar BRep -> Tesselation fallback)
        IfcUtil::IfcBaseClass* serialized = nullptr;
        
        std::cout << "[INFO] Attempting planar BRep serialization..." << std::endl;
        try {
            serialized = IfcGeom::serialise(schema->name(), shape, false);
        } catch (const std::exception& e) {
            std::cout << "[WARNING] Planar BRep serialization exception: " << e.what() << std::endl;
        } catch (...) {
            std::cout << "[WARNING] Planar BRep serialization unknown exception." << std::endl;
        }
        
        if (!serialized) {
            std::cout << "[INFO] Planar BRep serialization returned null. Trying tesselation..." << std::endl;
            try {
                serialized = IfcGeom::tesselate(schema->name(), shape, 2.0);
            } catch (const std::exception& e) {
                std::cout << "[WARNING] Tesselation exception: " << e.what() << std::endl;
            } catch (...) {
                std::cout << "[WARNING] Tesselation unknown exception." << std::endl;
            }
        }

        if (!serialized) {
            std::cerr << "[ERROR] Geometry serialization/tesselation failed." << std::endl;
            return 1;
        }
        std::cout << "[OK] Geometry serialized successfully. Type: " << serialized->declaration().name() << std::endl;

        // 5. Register geometry entities to file
        std::cout << "[INFO] Registering geometry entities to file..." << std::endl;
        file.addEntity(serialized);
        
        if (serialized->declaration().is("IfcProductDefinitionShape")) {
            auto pds = (Ifc4x3_add2::IfcProductDefinitionShape*)serialized;
            if (pds->Representations()) {
                auto reps = pds->Representations();
                for (auto it = reps->begin(); it != reps->end(); ++it) {
                    auto rep = *it;
                    if (rep) {
                        // Bind ContextOfItems to our GeometricRepresentationContext
                        rep->setContextOfItems(context);
                        file.addEntity(rep);
                        if (rep->Items()) {
                            auto repItems = rep->Items();
                            for (auto it2 = repItems->begin(); it2 != repItems->end(); ++it2) {
                                if (*it2) {
                                    file.addEntity(*it2);
                                }
                            }
                        }
                    }
                }
            }
        }
        std::cout << "[OK] All geometry entities registered." << std::endl;

        // 6. Create Proxy Object (IfcBuildingElementProxy)
        std::cout << "[INFO] Creating LocalPlacement for proxy..." << std::endl;
        auto proxyPlacement = CreateEntity<Ifc4x3_add2::IfcLocalPlacement>(file);
        proxyPlacement->setPlacementRelTo(buildingPlacement);
        proxyPlacement->setRelativePlacement(worldCS);

        std::cout << "[INFO] Creating IfcBuildingElementProxy..." << std::endl;
        auto proxy = CreateEntity<Ifc4x3_add2::IfcBuildingElementProxy>(file);
        proxy->setGlobalId(GenerateIfcGuid());
        proxy->setOwnerHistory(ownerHist);
        proxy->setName(std::string("BRep Proxy Element"));
        proxy->setObjectType(std::string("Proxy"));
        proxy->setObjectPlacement(proxyPlacement);
        
        // Directly assign the complete serialized representation shape!
        if (serialized->declaration().is("IfcProductDefinitionShape")) {
            proxy->setRepresentation((Ifc4x3_add2::IfcProductDefinitionShape*)serialized);
        }

        // 7. Contain proxy object in spatial structure (Building)
        std::cout << "[INFO] Constructing relation ContainedInSpatialStructure..." << std::endl;
        auto productSet = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcProduct>>();
        productSet->push(proxy);

        auto relContained = CreateEntity<Ifc4x3_add2::IfcRelContainedInSpatialStructure>(file);
        relContained->setGlobalId(GenerateIfcGuid());
        relContained->setOwnerHistory(ownerHist);
        relContained->setRelatedElements(productSet);
        relContained->setRelatingStructure(building);

        std::cout << "[OK] Proxy object created and assigned geometry successfully." << std::endl;

        // 8. Write to output file
        std::cout << "[INFO] Writing to " << outputPath << "..." << std::endl;
        std::stringstream ss;
        ss << file;
        std::string content = ss.str();

        // Force header schema to IFC4 for maximum viewer compatibility
        size_t pos = content.find("FILE_SCHEMA(('IFC4X3_ADD2'));");
        if (pos != std::string::npos) {
            content.replace(pos, 29, "FILE_SCHEMA(('IFC4'));");
        }

        std::ofstream ofs(outputPath);
        if (ofs.is_open()) {
            ofs << content;
            ofs.close();
            std::cout << "[SUCCESS] File saved successfully!" << std::endl;
        } else {
            std::cerr << "[ERROR] Failed to open output file for writing." << std::endl;
            return 1;
        }

    } catch (const std::exception& e) {
        std::cerr << "[CRASH] Standard Exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "[CRASH] Unknown Exception." << std::endl;
        return 1;
    }

    return 0;
}
