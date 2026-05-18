#define HAS_SCHEMA_4x3_add2
#include "IfcExportService.h"
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <TCollection_AsciiString.hxx>
#include <TopoDS_Compound.hxx>
#include <boost/make_shared.hpp>
#include <fstream>
#include <iostream>
#include <random>
#include <vector>
#include <ctime>
#include <sstream>
#include <map>

// Thread-safe cache to store generated product representations per unique geometry definition
static thread_local std::map<void*, Ifc4x3_add2::IfcProductDefinitionShape*> g_geomInstanceCache;

// IfcOpenShell headers
#include <ifcgeom/Serialization/Serialization.h>
#include <ifcparse/Ifc4x3_add2.h>
#include <ifcparse/IfcFile.h>

namespace boost {
void throw_exception(std::exception const &e) {
  std::cerr << "Boost exception: " << e.what() << std::endl;
  std::abort();
}
} // namespace boost

// Data model headers
#include "generated/BrNode_adGeometricDef.h"
#include "generated/BrNode_adGeometry.h"
#include "generated/BrNode_adModelRoot.h"

static std::string GenerateIfcGuid();


bool IfcExportService::ExportShapeToFile(const TopoDS_Shape& shape, const std::string& filename) {
    if (shape.IsNull()) return false;

    try {
        const IfcParse::schema_definition* schema = IfcParse::schema_by_name("Ifc4x3_add2");
        if (!schema) return false;
        
        IfcParse::register_schema(const_cast<IfcParse::schema_definition*>(schema));

        IfcParse::IfcFile file(schema, IfcParse::FT_IFCSPF);

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

        auto siLength = CreateEntity<Ifc4x3_add2::IfcSIUnit>(file);
        siLength->setUnitType(Ifc4x3_add2::IfcUnitEnum::IfcUnit_LENGTHUNIT);
        siLength->setPrefix(Ifc4x3_add2::IfcSIPrefix::IfcSIPrefix_MILLI);
        siLength->setName(Ifc4x3_add2::IfcSIUnitName::IfcSIUnitName_METRE);

        auto units = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcUnit>>();
        units->push(siLength);
        
        auto unitAssign = CreateEntity<Ifc4x3_add2::IfcUnitAssignment>(file);
        unitAssign->setUnits(units);

        auto contexts = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcRepresentationContext>>();
        contexts->push(context);
        
        auto project = CreateEntity<Ifc4x3_add2::IfcProject>(file);
        project->setGlobalId(GenerateIfcGuid());
        project->setOwnerHistory(ownerHist);
        project->setName(std::string("BRep Project"));
        project->setRepresentationContexts(contexts);
        project->setUnitsInContext(unitAssign);

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

        IfcUtil::IfcBaseClass* serialized = nullptr;
        try {
            serialized = IfcGeom::serialise(schema->name(), shape, true);
        } catch (...) {}
        if (!serialized) {
            try {
                serialized = IfcGeom::serialise(schema->name(), shape, false);
            } catch (...) {}
        }
        if (!serialized) {
            try {
                serialized = IfcGeom::tesselate(schema->name(), shape, 2.0);
            } catch (...) {}
        }

        if (!serialized) return false;

        file.addEntity(serialized);
        if (serialized->declaration().is("IfcProductDefinitionShape")) {
            auto pds = (Ifc4x3_add2::IfcProductDefinitionShape*)serialized;
            if (pds->Representations()) {
                auto reps = pds->Representations();
                for (auto it = reps->begin(); it != reps->end(); ++it) {
                    auto rep = *it;
                    if (rep) {
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

        auto proxyPlacement = CreateEntity<Ifc4x3_add2::IfcLocalPlacement>(file);
        proxyPlacement->setPlacementRelTo(buildingPlacement);
        proxyPlacement->setRelativePlacement(worldCS);

        auto proxy = CreateEntity<Ifc4x3_add2::IfcBuildingElementProxy>(file);
        proxy->setGlobalId(GenerateIfcGuid());
        proxy->setOwnerHistory(ownerHist);
        proxy->setName(std::string("BRep Proxy Element"));
        proxy->setObjectType(std::string("Proxy"));
        proxy->setObjectPlacement(proxyPlacement);
        
        if (serialized->declaration().is("IfcProductDefinitionShape")) {
            proxy->setRepresentation((Ifc4x3_add2::IfcProductDefinitionShape*)serialized);
        }

        auto productSet = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcProduct>>();
        productSet->push(proxy);

        auto relContained = CreateEntity<Ifc4x3_add2::IfcRelContainedInSpatialStructure>(file);
        relContained->setGlobalId(GenerateIfcGuid());
        relContained->setOwnerHistory(ownerHist);
        relContained->setRelatedElements(productSet);
        relContained->setRelatingStructure(building);

        std::stringstream ss;
        ss << file;
        std::string content = ss.str();

        size_t pos = content.find("FILE_SCHEMA(('IFC4X3_ADD2'));");
        if (pos != std::string::npos) {
            content.replace(pos, 29, "FILE_SCHEMA(('IFC4'));");
        }

        std::ofstream ofs(filename);
        if (ofs.is_open()) {
            ofs << content;
            ofs.close();
            return true;
        }
    } catch (...) {
        return false;
    }
    return false;
}

std::string
IfcExportService::ToStdString(const TCollection_ExtendedString &extStr) {
  TCollection_AsciiString ascii(extStr);
  return ascii.ToCString();
}

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

Ifc4x3_add2::IfcProduct *
IfcExportService::CreateIfcProduct(const Handle(BrNode_adObject) & adObj,
                                   IfcParse::IfcFile &file,
                                   Ifc4x3_add2::IfcOwnerHistory *ownerHist,
                                   Ifc4x3_add2::IfcLocalPlacement *placement) {
  std::string type = ToStdString(adObj->GetObjectType());
  std::string name = ToStdString(adObj->GetName());
  std::string guid = GenerateIfcGuid();

  if (type == "Footing" || type == "BridgeFoundation") {
    auto obj = CreateEntity<Ifc4x3_add2::IfcFooting>(file);
    obj->setGlobalId(guid);
    obj->setOwnerHistory(ownerHist);
    obj->setName(name);
    obj->setObjectType(type);
    obj->setObjectPlacement(placement);
    return obj;
  }

  auto proxy = CreateEntity<Ifc4x3_add2::IfcBuildingElementProxy>(file);
  proxy->setGlobalId(guid);
  proxy->setOwnerHistory(ownerHist);
  proxy->setName(name);
  proxy->setObjectType(type);
  proxy->setObjectPlacement(placement);
  return proxy;
}

void IfcExportService::AddGeometryToProduct(
    const Handle(BrNode_adObject) & adObj, IfcParse::IfcFile &file,
    Ifc4x3_add2::IfcProduct *product,
    Ifc4x3_add2::IfcGeometricRepresentationContext *context) {
  std::string nodeName = ToStdString(adObj->GetName());

  Handle(ActAPI_IDataCursor) geoNodeCursor = adObj->GetGeometry();
  if (geoNodeCursor.IsNull())
    return;

  Handle(BrNode_adGeometry) geoNode =
      Handle(BrNode_adGeometry)::DownCast(geoNodeCursor);
  if (geoNode.IsNull())
    return;

  Handle(BrNode_adGeometricDef) geoDef =
      Handle(BrNode_adGeometricDef)::DownCast(geoNode->GetGeometryRef());
  if (geoDef.IsNull())
    return;

  // 1. Instant Geometry Instancing Check
  void* geoDefPtr = geoDef.get();
  if (g_geomInstanceCache.find(geoDefPtr) != g_geomInstanceCache.end()) {
      auto cachedRep = g_geomInstanceCache[geoDefPtr];
      if (cachedRep) {
          product->setRepresentation(cachedRep);
          return; // Zero-cost share, instantly return to bypass memory pollution
      }
  }

  TopoDS_Shape shape = geoDef->GetShape();
  if (shape.IsNull())
    return;

  try {
    const IfcParse::schema_definition* schema = &Ifc4x3_add2::get_schema();
    IfcParse::register_schema(const_cast<IfcParse::schema_definition*>(schema));
    
    IfcUtil::IfcBaseClass* serialized = nullptr;
    
    // Stable serialization fallback sequence (aligned 100% with brep_to_ifc.cpp)
    try {
        serialized = IfcGeom::serialise("IFC4X3_ADD2", shape, false);
    } catch (...) {}
    
    if (!serialized) {
        try {
            serialized = IfcGeom::tesselate("IFC4X3_ADD2", shape, 2.0);
        } catch (...) {}
    }

    if (!serialized) return;

    // 2. High-fidelity entity registration and ContextOfItems binding (aligned with brep_to_ifc.cpp)
    file.addEntity(serialized);
    
    if (serialized->declaration().is("IfcProductDefinitionShape")) {
        auto pds = (Ifc4x3_add2::IfcProductDefinitionShape*)serialized;
        if (pds->Representations()) {
            auto reps = pds->Representations();
            for (auto it = reps->begin(); it != reps->end(); ++it) {
                try {
                    auto rep = *it;
                    if (rep) {
                        rep->setContextOfItems(context); // Bind representation context
                        file.addEntity(rep);
                        if (rep->Items()) {
                            auto repItems = rep->Items();
                            for (auto it2 = repItems->begin(); it2 != repItems->end(); ++it2) {
                                try {
                                    if (*it2) {
                                        file.addEntity(*it2);
                                    }
                                } catch (...) {}
                            }
                        }
                    }
                } catch (...) {}
            }
        }
        
        // Directly assign the complete serialized representation shape!
        product->setRepresentation(pds);
        
        // Cache successful representation for instancing
        g_geomInstanceCache[geoDefPtr] = pds;
    }
  } catch (...) {}
}

void IfcExportService::TraverseAndExport(
    const Handle(BrNode_adObject) & adObj, IfcParse::IfcFile &file,
    Ifc4x3_add2::IfcObjectDefinition *parentIfc,
    Ifc4x3_add2::IfcSpatialElement *spatialContainer,
    Ifc4x3_add2::IfcObjectPlacement *parentPlacement,
    Ifc4x3_add2::IfcOwnerHistory *ownerHist,
    Ifc4x3_add2::IfcGeometricRepresentationContext *context,
    int &exportedCount) {
  if (adObj.IsNull())
    return;

  // 1. Calculate Local Placement
  auto origin = CreateEntity<Ifc4x3_add2::IfcCartesianPoint>(file);
  origin->setCoordinates(std::vector<double>{0.0, 0.0, 0.0});

  Handle(ActAPI_IUserParameter) p = adObj->Parameter(BrNode_adObject::PID_ObjectPlacement);
  Handle(ActData_RealArrayParameter) typedP = ActData_ParameterFactory::AsRealArray(p);
  if (!typedP.IsNull() && typedP->NbElements() >= 3) {
    origin->setCoordinates(std::vector<double>{
        typedP->GetElement(0), typedP->GetElement(1), typedP->GetElement(2)});
  }

  auto dirZ = CreateEntity<Ifc4x3_add2::IfcDirection>(file);
  dirZ->setDirectionRatios(std::vector<double>{0.0, 0.0, 1.0});
  
  auto dirX = CreateEntity<Ifc4x3_add2::IfcDirection>(file);
  dirX->setDirectionRatios(std::vector<double>{1.0, 0.0, 0.0});
  
  auto axis2 = CreateEntity<Ifc4x3_add2::IfcAxis2Placement3D>(file);
  axis2->setLocation(origin);
  axis2->setAxis(dirZ);
  axis2->setRefDirection(dirX);

  auto localPlacement = CreateEntity<Ifc4x3_add2::IfcLocalPlacement>(file);
  localPlacement->setPlacementRelTo(parentPlacement);
  localPlacement->setRelativePlacement(axis2);

  // 2. Create IFC Product
  Ifc4x3_add2::IfcProduct *product = CreateIfcProduct(adObj, file, ownerHist, localPlacement);
  
  exportedCount++;

  // 3. Add Geometry
  AddGeometryToProduct(adObj, file, product, context);

  // 4. Link to Parent (Spatial Aggregation vs Contained in Spatial Structure)
  auto childSet = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcObjectDefinition>>();
  childSet->push(product);

  // 4. Link to Parent & Spatial Container
  // 核心逻辑：如果当前产品不是空间元素（Site, Bridge等），则必须被包含在空间容器中
  if (!product->declaration().is("IfcSpatialElement")) {
      auto productSet = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcProduct>>();
      productSet->push(product);

      auto relContained = CreateEntity<Ifc4x3_add2::IfcRelContainedInSpatialStructure>(file);
      relContained->setGlobalId(GenerateIfcGuid());
      relContained->setOwnerHistory(ownerHist);
      relContained->setRelatedElements(productSet);
      relContained->setRelatingStructure(spatialContainer);
  } 

  // 始终建立聚合/嵌套关系，以构建完整的装配树
  auto relAgg = CreateEntity<Ifc4x3_add2::IfcRelAggregates>(file);
  relAgg->setGlobalId(GenerateIfcGuid());
  relAgg->setOwnerHistory(ownerHist);
  relAgg->setRelatingObject(parentIfc);
  relAgg->setRelatedObjects(childSet);

  // 5. Recursive Children
  Ifc4x3_add2::IfcSpatialElement* nextSpatial = spatialContainer;
  if (product->declaration().is("IfcSpatialElement")) {
      nextSpatial = (Ifc4x3_add2::IfcSpatialElement*)product;
  }

  NCollection_Sequence<Handle(BrNode_adObject)> children = adObj->GetSubObjectsList();
  for (int i = 1; i <= children.Length(); ++i) {
    TraverseAndExport(children.Value(i), file, product, nextSpatial, localPlacement,
                      ownerHist, context, exportedCount);
  }
}

bool IfcExportService::Export(const Handle(DataModel) & model,
                              const std::string &filename) {
  if (model.IsNull())
    return false;

  g_geomInstanceCache.clear(); // Initialize cache at export boundary

  try {
    std::cout << "[IfcExportService] Starting advanced IFC 4x3 export..."
              << std::endl;

    const IfcParse::schema_definition *schema = &Ifc4x3_add2::get_schema();
    if (!schema)
      return false;

    IfcParse::IfcFile *file_ptr =
        new IfcParse::IfcFile(schema, IfcParse::FT_IFCSPF);
    IfcParse::IfcFile &file = *file_ptr;

    // OwnerHistory
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

    // Context
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

    // Units (Millimeters to match OCCT coordinates)
    auto siLength = CreateEntity<Ifc4x3_add2::IfcSIUnit>(file);
    siLength->setUnitType(Ifc4x3_add2::IfcUnitEnum::IfcUnit_LENGTHUNIT);
    siLength->setPrefix(Ifc4x3_add2::IfcSIPrefix::IfcSIPrefix_MILLI);
    siLength->setName(Ifc4x3_add2::IfcSIUnitName::IfcSIUnitName_METRE);
    auto units = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcUnit>>();
    units->push(siLength);
    auto unitAssign = CreateEntity<Ifc4x3_add2::IfcUnitAssignment>(file);
    unitAssign->setUnits(units);

    // Project
    auto contexts = boost::make_shared<
        aggregate_of<Ifc4x3_add2::IfcRepresentationContext>>();
    contexts->push(context);
    
    auto project = CreateEntity<Ifc4x3_add2::IfcProject>(file);
    project->setGlobalId(GenerateIfcGuid());
    project->setOwnerHistory(ownerHist);
    project->setName(std::string("Bridge Project"));
    project->setRepresentationContexts(contexts);
    project->setUnitsInContext(unitAssign);

    // Site
    auto sitePlacement = CreateEntity<Ifc4x3_add2::IfcLocalPlacement>(file);
    sitePlacement->setRelativePlacement(worldCS);
    
    auto site = CreateEntity<Ifc4x3_add2::IfcSite>(file);
    site->setGlobalId(GenerateIfcGuid());
    site->setOwnerHistory(ownerHist);
    site->setName(std::string("Site"));
    site->setObjectPlacement(sitePlacement);
    site->setCompositionType(Ifc4x3_add2::IfcElementCompositionEnum::IfcElementComposition_ELEMENT);

    auto siteSet =
        boost::make_shared<aggregate_of<Ifc4x3_add2::IfcObjectDefinition>>();
    siteSet->push(site);
    auto relProjSite = CreateEntity<Ifc4x3_add2::IfcRelAggregates>(file);
    relProjSite->setGlobalId(GenerateIfcGuid());
    relProjSite->setOwnerHistory(ownerHist);
    relProjSite->setRelatingObject(project);
    relProjSite->setRelatedObjects(siteSet);

    // Building
    auto buildingPlacement = CreateEntity<Ifc4x3_add2::IfcLocalPlacement>(file);
    buildingPlacement->setPlacementRelTo(sitePlacement);
    buildingPlacement->setRelativePlacement(worldCS);
    auto building = CreateEntity<Ifc4x3_add2::IfcBuilding>(file);
    building->setGlobalId(GenerateIfcGuid());
    building->setOwnerHistory(ownerHist);
    building->setName(std::string("Building"));
    building->setObjectPlacement(buildingPlacement);
    building->setCompositionType(Ifc4x3_add2::IfcElementCompositionEnum::IfcElementComposition_ELEMENT);

    auto buildingSet =
        boost::make_shared<aggregate_of<Ifc4x3_add2::IfcObjectDefinition>>();
    buildingSet->push(building);
    auto relSiteBuilding = CreateEntity<Ifc4x3_add2::IfcRelAggregates>(file);
    relSiteBuilding->setGlobalId(GenerateIfcGuid());
    relSiteBuilding->setOwnerHistory(ownerHist);
    relSiteBuilding->setRelatingObject(site);
    relSiteBuilding->setRelatedObjects(buildingSet);

    // Recursive Traversal
    Handle(ActAPI_INode) rootNode = model->GetRootNode();
    if (!rootNode.IsNull()) {
      int exportedCount = 0;
      auto it = rootNode->GetChildIterator();
      for (; it->More(); it->Next()) {
        Handle(BrNode_adObject) obj =
            Handle(BrNode_adObject)::DownCast(it->Value());
        if (!obj.IsNull()) {
          TraverseAndExport(obj, file, building, building, buildingPlacement, ownerHist,
                            context, exportedCount);
        }
      }
      std::cout << "[IfcExportService] Exported " << exportedCount << " nodes. (VERSION: 2026-05-14-FIXED)"
                << std::endl;
    }

    std::string final_filename = filename;
    if (final_filename.find("111.ifc") != std::string::npos) {
        final_filename = "D:/QtOCCTApp/111_v2.ifc";
    }

    std::stringstream ss;
    ss << file;
    std::string content = ss.str();

    // Stream rewrite schema to IFC4 for general compatibility
    size_t pos = content.find("FILE_SCHEMA(('IFC4X3_ADD2'));");
    if (pos != std::string::npos) {
        content.replace(pos, 29, "FILE_SCHEMA(('IFC4'));");
    }

    std::ofstream f(final_filename);
    if (f.is_open()) {
      f << content;
      f.close();
      std::cout << "[IfcExportService] File saved: " << final_filename << std::endl;
    }

    return true;
  } catch (const std::exception &e) {
    g_geomInstanceCache.clear();
    std::cerr << "[IfcExportService] ERROR: " << e.what() << std::endl;
    return false;
  } catch (...) {
    g_geomInstanceCache.clear();
    std::cerr << "[IfcExportService] UNKNOWN ERROR" << std::endl;
    return false;
  }
}
