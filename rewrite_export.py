import os

file_path = r"D:\QtOCCTApp\core-data-model\IfcExportService.cpp"
with open(file_path, "r", encoding="utf-8") as f:
    content = f.read()

# Remove the previously added static GenerateIfcGuid declaration
content = content.replace("static std::string GenerateIfcGuid();\n\n", "")

# Add CreateEntity at the top
create_entity_code = """
// Safe Entity Creation Helper to bypass the pre-compiled header bug
// and properly route through IfcOpenShell's storage allocator
template <typename T>
static T* CreateEntity(IfcParse::IfcFile& file) {
    auto* inst = file.create<T>();
    if (inst) {
        inst->file_ = nullptr; // Reset the bypass flag
        file.addEntity(inst);  // Call full registration logic
    }
    return inst;
}
"""

if "CreateEntity" not in content:
    content = content.replace("#include \"generated/BrNode_adModelRoot.h\"\n", 
                              "#include \"generated/BrNode_adModelRoot.h\"\n" + create_entity_code + "\nstatic std::string GenerateIfcGuid();\n")

# Now rewrite ExportShapeToFile entirely since we already have the perfect version in brep_to_ifc.cpp
new_export_shape = """bool IfcExportService::ExportShapeToFile(const TopoDS_Shape& shape, const std::string& filename) {
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
}"""

import re
# Regex to replace ExportShapeToFile
content = re.sub(r'bool IfcExportService::ExportShapeToFile.*?return false;\n}', new_export_shape, content, flags=re.DOTALL)

# Now for CreateIfcProduct
new_create_product = """Ifc4x3_add2::IfcProduct *
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
}"""
content = re.sub(r'Ifc4x3_add2::IfcProduct \*\nIfcExportService::CreateIfcProduct.*?\n}', new_create_product, content, flags=re.DOTALL)

# For AddGeometryToProduct
# Replace new IfcShapeRepresentation -> CreateEntity
content = content.replace("""auto shapeRep = new Ifc4x3_add2::IfcShapeRepresentation(
              context, boost::optional<std::string>("Body"),
              boost::optional<std::string>("Brep"), items);
          file.addEntity(shapeRep);""", """auto shapeRep = CreateEntity<Ifc4x3_add2::IfcShapeRepresentation>(file);
          shapeRep->setContextOfItems(context);
          shapeRep->setRepresentationIdentifier(std::string("Body"));
          shapeRep->setRepresentationType(std::string("Brep"));
          shapeRep->setItems(items);""")

content = content.replace("""auto shapeRep = new Ifc4x3_add2::IfcShapeRepresentation(
              context, boost::optional<std::string>("Body"),
              boost::optional<std::string>("Tessellation"), items);
          file.addEntity(shapeRep);""", """auto shapeRep = CreateEntity<Ifc4x3_add2::IfcShapeRepresentation>(file);
          shapeRep->setContextOfItems(context);
          shapeRep->setRepresentationIdentifier(std::string("Body"));
          shapeRep->setRepresentationType(std::string("Tessellation"));
          shapeRep->setItems(items);""")

content = content.replace("""auto pds = new Ifc4x3_add2::IfcProductDefinitionShape(
          boost::none, boost::none, reps);
      file.addEntity(pds);""", """auto pds = CreateEntity<Ifc4x3_add2::IfcProductDefinitionShape>(file);
      pds->setRepresentations(reps);""")


# For TraverseAndExport
content = content.replace("""auto productPlacement = new Ifc4x3_add2::IfcLocalPlacement(
        parentPlacement, localPlacement);
    file.addEntity(productPlacement);""", """auto productPlacement = CreateEntity<Ifc4x3_add2::IfcLocalPlacement>(file);
    productPlacement->setPlacementRelTo(parentPlacement);
    productPlacement->setRelativePlacement(localPlacement);""")

content = content.replace("""auto relContained = new Ifc4x3_add2::IfcRelContainedInSpatialStructure(
        GenerateIfcGuid(), ownerHist, boost::none, boost::none, elements,
        spatialContainer);
    file.addEntity(relContained);""", """auto relContained = CreateEntity<Ifc4x3_add2::IfcRelContainedInSpatialStructure>(file);
    relContained->setGlobalId(GenerateIfcGuid());
    relContained->setOwnerHistory(ownerHist);
    relContained->setRelatedElements(elements);
    relContained->setRelatingStructure(spatialContainer);""")


# For Export
content = content.replace("""auto person = new Ifc4x3_add2::IfcPerson(
        boost::none, boost::none, std::string("User"), boost::none, boost::none,
        boost::none, boost::none, boost::none);
    file.addEntity(person);""", """auto person = CreateEntity<Ifc4x3_add2::IfcPerson>(file);
    person->setGivenName(std::string("User"));""")

content = content.replace("""auto org = new Ifc4x3_add2::IfcOrganization(
        boost::none, std::string("QtOCCTApp"), boost::none, boost::none,
        boost::none);
    file.addEntity(org);""", """auto org = CreateEntity<Ifc4x3_add2::IfcOrganization>(file);
    org->setName(std::string("QtOCCTApp"));""")

content = content.replace("""auto personOrg =
        new Ifc4x3_add2::IfcPersonAndOrganization(person, org, boost::none);
    file.addEntity(personOrg);""", """auto personOrg = CreateEntity<Ifc4x3_add2::IfcPersonAndOrganization>(file);
    personOrg->setThePerson(person);
    personOrg->setTheOrganization(org);""")

content = content.replace("""auto app = new Ifc4x3_add2::IfcApplication(org, std::string("1.0"),
                                             std::string("QtOCCTApp"),
                                             std::string("QtOCCTApp"));
    file.addEntity(app);""", """auto app = CreateEntity<Ifc4x3_add2::IfcApplication>(file);
    app->setApplicationDeveloper(org);
    app->setVersion(std::string("1.0"));
    app->setApplicationFullName(std::string("QtOCCTApp"));
    app->setApplicationIdentifier(std::string("QtOCCTApp"));""")

content = content.replace("""auto ownerHist = new Ifc4x3_add2::IfcOwnerHistory(
        personOrg, app, boost::none,
        boost::optional<Ifc4x3_add2::IfcChangeActionEnum::Value>(
            Ifc4x3_add2::IfcChangeActionEnum::IfcChangeAction_NOCHANGE),
        boost::none, nullptr, nullptr, (int)time(NULL));
    file.addEntity(ownerHist);""", """auto ownerHist = CreateEntity<Ifc4x3_add2::IfcOwnerHistory>(file);
    ownerHist->setOwningUser(personOrg);
    ownerHist->setOwningApplication(app);
    ownerHist->setChangeAction(Ifc4x3_add2::IfcChangeActionEnum::IfcChangeAction_NOCHANGE);
    ownerHist->setCreationDate((int)time(NULL));""")

content = content.replace("""auto origin =
        new Ifc4x3_add2::IfcCartesianPoint(std::vector<double>{0.0, 0.0, 0.0});
    file.addEntity(origin);
    auto dirZ =
        new Ifc4x3_add2::IfcDirection(std::vector<double>{0.0, 0.0, 1.0});
    file.addEntity(dirZ);
    auto dirX =
        new Ifc4x3_add2::IfcDirection(std::vector<double>{1.0, 0.0, 0.0});
    file.addEntity(dirX);
    auto worldCS = new Ifc4x3_add2::IfcAxis2Placement3D(origin, dirZ, dirX);
    file.addEntity(worldCS);""", """auto origin = CreateEntity<Ifc4x3_add2::IfcCartesianPoint>(file);
    origin->setCoordinates(std::vector<double>{0.0, 0.0, 0.0});
    
    auto dirZ = CreateEntity<Ifc4x3_add2::IfcDirection>(file);
    dirZ->setDirectionRatios(std::vector<double>{0.0, 0.0, 1.0});
    
    auto dirX = CreateEntity<Ifc4x3_add2::IfcDirection>(file);
    dirX->setDirectionRatios(std::vector<double>{1.0, 0.0, 0.0});
    
    auto worldCS = CreateEntity<Ifc4x3_add2::IfcAxis2Placement3D>(file);
    worldCS->setLocation(origin);
    worldCS->setAxis(dirZ);
    worldCS->setRefDirection(dirX);""")

content = content.replace("""auto context = new Ifc4x3_add2::IfcGeometricRepresentationContext(
        boost::optional<std::string>("Model"),
        boost::optional<std::string>("Model"), 3,
        boost::optional<double>(1e-5), worldCS, nullptr);
    file.addEntity(context);""", """auto context = CreateEntity<Ifc4x3_add2::IfcGeometricRepresentationContext>(file);
    context->setContextType(std::string("Model"));
    context->setContextIdentifier(std::string("Model"));
    context->setCoordinateSpaceDimension(3);
    context->setPrecision(1e-5);
    context->setWorldCoordinateSystem(worldCS);""")

content = content.replace("""auto siLength = new Ifc4x3_add2::IfcSIUnit(
        Ifc4x3_add2::IfcUnitEnum::IfcUnit_LENGTHUNIT,
        boost::optional<Ifc4x3_add2::IfcSIPrefix::Value>(
            Ifc4x3_add2::IfcSIPrefix::IfcSIPrefix_MILLI),
        Ifc4x3_add2::IfcSIUnitName::IfcSIUnitName_METRE);
    file.addEntity(siLength);""", """auto siLength = CreateEntity<Ifc4x3_add2::IfcSIUnit>(file);
    siLength->setUnitType(Ifc4x3_add2::IfcUnitEnum::IfcUnit_LENGTHUNIT);
    siLength->setPrefix(Ifc4x3_add2::IfcSIPrefix::IfcSIPrefix_MILLI);
    siLength->setName(Ifc4x3_add2::IfcSIUnitName::IfcSIUnitName_METRE);""")

content = content.replace("""auto unitAssign = new Ifc4x3_add2::IfcUnitAssignment(units);
    file.addEntity(unitAssign);""", """auto unitAssign = CreateEntity<Ifc4x3_add2::IfcUnitAssignment>(file);
    unitAssign->setUnits(units);""")

content = content.replace("""auto project = new Ifc4x3_add2::IfcProject(
        GenerateIfcGuid(), ownerHist,
        boost::optional<std::string>("BRep Project"), boost::none,
        boost::none, boost::none, boost::none, contexts, unitAssign);
    file.addEntity(project);""", """auto project = CreateEntity<Ifc4x3_add2::IfcProject>(file);
    project->setGlobalId(GenerateIfcGuid());
    project->setOwnerHistory(ownerHist);
    project->setName(std::string("BRep Project"));
    project->setRepresentationContexts(contexts);
    project->setUnitsInContext(unitAssign);""")

content = content.replace("""auto sitePlacement = new Ifc4x3_add2::IfcLocalPlacement(nullptr, worldCS);
    file.addEntity(sitePlacement);""", """auto sitePlacement = CreateEntity<Ifc4x3_add2::IfcLocalPlacement>(file);
    sitePlacement->setRelativePlacement(worldCS);""")

content = content.replace("""auto site = new Ifc4x3_add2::IfcSite(
        GenerateIfcGuid(), ownerHist, boost::optional<std::string>("Site"),
        boost::none, boost::none, sitePlacement, nullptr, boost::none,
        boost::optional<Ifc4x3_add2::IfcElementCompositionEnum::Value>(
            Ifc4x3_add2::IfcElementCompositionEnum::IfcElementComposition_ELEMENT),
        boost::none, boost::none, boost::none, boost::none, nullptr);
    file.addEntity(site);""", """auto site = CreateEntity<Ifc4x3_add2::IfcSite>(file);
    site->setGlobalId(GenerateIfcGuid());
    site->setOwnerHistory(ownerHist);
    site->setName(std::string("Site"));
    site->setObjectPlacement(sitePlacement);
    site->setCompositionType(Ifc4x3_add2::IfcElementCompositionEnum::IfcElementComposition_ELEMENT);""")

content = content.replace("""file.addEntity(new Ifc4x3_add2::IfcRelAggregates(
        GenerateIfcGuid(), ownerHist, boost::none, boost::none, project,
        siteSet));""", """auto relProjSite = CreateEntity<Ifc4x3_add2::IfcRelAggregates>(file);
    relProjSite->setGlobalId(GenerateIfcGuid());
    relProjSite->setOwnerHistory(ownerHist);
    relProjSite->setRelatingObject(project);
    relProjSite->setRelatedObjects(siteSet);""")

content = content.replace("""auto buildingPlacement =
        new Ifc4x3_add2::IfcLocalPlacement(sitePlacement, worldCS);
    file.addEntity(buildingPlacement);""", """auto buildingPlacement = CreateEntity<Ifc4x3_add2::IfcLocalPlacement>(file);
    buildingPlacement->setPlacementRelTo(sitePlacement);
    buildingPlacement->setRelativePlacement(worldCS);""")

content = content.replace("""auto building = new Ifc4x3_add2::IfcBuilding(
        GenerateIfcGuid(), ownerHist, boost::optional<std::string>("Building"),
        boost::none, boost::none, buildingPlacement, nullptr, boost::none,
        boost::optional<Ifc4x3_add2::IfcElementCompositionEnum::Value>(
            Ifc4x3_add2::IfcElementCompositionEnum::IfcElementComposition_ELEMENT),
        boost::none, boost::none, nullptr);
    file.addEntity(building);""", """auto building = CreateEntity<Ifc4x3_add2::IfcBuilding>(file);
    building->setGlobalId(GenerateIfcGuid());
    building->setOwnerHistory(ownerHist);
    building->setName(std::string("Building"));
    building->setObjectPlacement(buildingPlacement);
    building->setCompositionType(Ifc4x3_add2::IfcElementCompositionEnum::IfcElementComposition_ELEMENT);""")

content = content.replace("""file.addEntity(new Ifc4x3_add2::IfcRelAggregates(
        GenerateIfcGuid(), ownerHist, boost::none, boost::none, site,
        buildingSet));""", """auto relSiteBuilding = CreateEntity<Ifc4x3_add2::IfcRelAggregates>(file);
    relSiteBuilding->setGlobalId(GenerateIfcGuid());
    relSiteBuilding->setOwnerHistory(ownerHist);
    relSiteBuilding->setRelatingObject(site);
    relSiteBuilding->setRelatedObjects(buildingSet);""")

# Add check to remove any duplicate addEntity inside TraverseAndExport for the proxy
content = content.replace("file.addEntity(product);", "")

with open(file_path, "w", encoding="utf-8") as f:
    f.write(content)
print("IfcExportService refactored successfully.")
