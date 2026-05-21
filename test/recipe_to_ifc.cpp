#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <random>
#include <ctime>
#include <sstream>

// IfcOpenShell
#define HAS_SCHEMA_4x3_add2
#include <ifcgeom/Serialization/Serialization.h>
#include <ifcparse/Ifc4x3_add2.h>
#include <ifcparse/IfcFile.h>

#include <boost/make_shared.hpp>
#include <nlohmann/json.hpp>

// OCCT
#include <TopoDS_Shape.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>

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

// Helper to create IfcAxis2Placement3D
static Ifc4x3_add2::IfcAxis2Placement3D* CreateAxis2Placement3D(
    IfcParse::IfcFile& file,
    const std::vector<double>& loc,
    const std::vector<double>& axisRatios,
    const std::vector<double>& refDirRatios) 
{
    auto* location = CreateEntity<Ifc4x3_add2::IfcCartesianPoint>(file);
    location->setCoordinates(loc);

    auto* placement = CreateEntity<Ifc4x3_add2::IfcAxis2Placement3D>(file);
    placement->setLocation(location);

    if (axisRatios.size() == 3) {
        auto* axis = CreateEntity<Ifc4x3_add2::IfcDirection>(file);
        axis->setDirectionRatios(axisRatios);
        placement->setAxis(axis);
    }

    if (refDirRatios.size() == 3) {
        auto* refDir = CreateEntity<Ifc4x3_add2::IfcDirection>(file);
        refDir->setDirectionRatios(refDirRatios);
        placement->setRefDirection(refDir);
    }

    return placement;
}

#include <cstdlib>

// 辅助函数：解析路径的目录和文件名不含后缀
static void ParsePath(const std::string& path, std::string& dir, std::string& baseName) {
    size_t lastSlash = path.find_last_of("\\/");
    if (lastSlash == std::string::npos) {
        dir = ".";
        baseName = path;
    } else {
        dir = path.substr(0, lastSlash);
        baseName = path.substr(lastSlash + 1);
    }
    
    size_t lastDot = baseName.find_last_of(".");
    if (lastDot != std::string::npos) {
        baseName = baseName.substr(0, lastDot);
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: recipe_to_ifc <modeling_script.py> [modeling_script_args...]" << std::endl;
        return 1;
    }

    std::string scriptPath = argv[1];

    // 解析目录和基本名字
    std::string scriptDir = "";
    std::string scriptBase = "";
    ParsePath(scriptPath, scriptDir, scriptBase);

    // 组合命名：脚本名称与后续输入参数的组合
    std::string combinedName = scriptBase;
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        std::string cleanArg = "";
        for (char c : arg) {
            // 清洗非法文件名字符
            if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
                cleanArg += '_';
            } else {
                cleanArg += c;
            }
        }
        if (!cleanArg.empty()) {
            combinedName += "_" + cleanArg;
        }
    }

    std::string jsonPath = scriptDir + "/" + combinedName + ".json";
    std::string ifcPath = scriptDir + "/" + combinedName + ".ifc";

    std::cout << "=== Auto Modeling & IFC Generation Suite ===" << std::endl;
    std::cout << "[INFO] Script: " << scriptPath << std::endl;
    std::cout << "[INFO] Target JSON: " << jsonPath << std::endl;
    std::cout << "[INFO] Target IFC: " << ifcPath << std::endl;

    // 组装 python 命令行，原样传递其余输入参数
    std::string cmd = "python \"" + scriptPath + "\"";
    for (int i = 2; i < argc; ++i) {
        cmd += " \"" + std::string(argv[i]) + "\"";
    }

    std::cout << "[INFO] Executing modeling script: " << cmd << std::endl;
    int retCode = std::system(cmd.c_str());
    if (retCode != 0) {
        std::cerr << "[ERROR] Modeling script failed with exit code: " << retCode << std::endl;
        return retCode;
    }
    std::cout << "[OK] Modeling script execution completed." << std::endl;

    // 处理 JSON 文件的重命名/复制流转逻辑 (不修改建模脚本本身，只在 C++ 端对它产生的文件做流转)
    std::string defaultJson1 = scriptDir + "/pile_cap_assembly.json";
    std::string defaultJson2 = scriptDir + "/" + scriptBase + ".json";
    std::string sourceJson = "";

    // 检查默认生成的文件是否存在
    {
        std::ifstream f1(defaultJson1);
        if (f1.good()) {
            sourceJson = defaultJson1;
        }
    }
    if (sourceJson.empty()) {
        std::ifstream f2(defaultJson2);
        if (f2.good()) {
            sourceJson = defaultJson2;
        }
    }

    if (sourceJson.empty()) {
        std::cerr << "[ERROR] Could not find generated JSON from modeling script. Checked:" << std::endl;
        std::cerr << "  - " << defaultJson1 << std::endl;
        std::cerr << "  - " << defaultJson2 << std::endl;
        return 1;
    }

    if (sourceJson != jsonPath) {
        std::cout << "[INFO] Aligning generated JSON to: " << jsonPath << std::endl;
        std::remove(jsonPath.c_str()); // 移除旧的目标文件
        if (std::rename(sourceJson.c_str(), jsonPath.c_str()) != 0) {
            // 如果重命名失败，采用复制文件的方法
            std::ifstream src(sourceJson, std::ios::binary);
            std::ofstream dst(jsonPath, std::ios::binary);
            if (src.good() && dst.good()) {
                dst << src.rdbuf();
                src.close();
                dst.close();
                std::remove(sourceJson.c_str());
                std::cout << "[OK] JSON aligned via copy and delete." << std::endl;
            } else {
                std::cerr << "[ERROR] Failed to copy/rename source JSON to target path." << std::endl;
                return 1;
            }
        } else {
            std::cout << "[OK] JSON aligned via rename." << std::endl;
        }
    }

    // 设置为原流程所使用的 inputPath 与 outputPath
    std::string inputPath = jsonPath;
    std::string outputPath = ifcPath;

    std::cout << "=== Recipe JSON to IFC Parametric Converter ===" << std::endl;
    std::cout << "[INFO] Loading Recipe JSON: " << inputPath << std::endl;

    // 1. Load and parse JSON
    std::ifstream ifs(inputPath);
    if (!ifs.is_open()) {
        std::cerr << "[ERROR] Failed to open input Recipe JSON file: " << inputPath << std::endl;
        return 1;
    }
    nlohmann::json j;
    try {
        ifs >> j;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] JSON parse exception: " << e.what() << std::endl;
        return 1;
    }
    std::cout << "[OK] JSON parsed successfully." << std::endl;

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
        auto* person = CreateEntity<Ifc4x3_add2::IfcPerson>(file);
        person->setGivenName(std::string("Antigravity"));

        auto* org = CreateEntity<Ifc4x3_add2::IfcOrganization>(file);
        org->setName(std::string("QtOCCTApp"));

        auto* personOrg = CreateEntity<Ifc4x3_add2::IfcPersonAndOrganization>(file);
        personOrg->setThePerson(person);
        personOrg->setTheOrganization(org);

        auto* app = CreateEntity<Ifc4x3_add2::IfcApplication>(file);
        app->setApplicationDeveloper(org);
        app->setVersion(std::string("1.2"));
        app->setApplicationFullName(std::string("QtOCCTApp Recipe-IFC Parser"));
        app->setApplicationIdentifier(std::string("QtOCCTApp_Recipe_Parser"));

        auto* ownerHist = CreateEntity<Ifc4x3_add2::IfcOwnerHistory>(file);
        ownerHist->setOwningUser(personOrg);
        ownerHist->setOwningApplication(app);
        ownerHist->setChangeAction(Ifc4x3_add2::IfcChangeActionEnum::IfcChangeAction_NOCHANGE);
        ownerHist->setCreationDate((int)time(NULL));

        std::cout << "[INFO] Constructing Geometrical Context..." << std::endl;
        auto* origin = CreateEntity<Ifc4x3_add2::IfcCartesianPoint>(file);
        origin->setCoordinates(std::vector<double>{0.0, 0.0, 0.0});

        auto* dirZ = CreateEntity<Ifc4x3_add2::IfcDirection>(file);
        dirZ->setDirectionRatios(std::vector<double>{0.0, 0.0, 1.0});

        auto* dirX = CreateEntity<Ifc4x3_add2::IfcDirection>(file);
        dirX->setDirectionRatios(std::vector<double>{1.0, 0.0, 0.0});

        auto* worldCS = CreateEntity<Ifc4x3_add2::IfcAxis2Placement3D>(file);
        worldCS->setLocation(origin);
        worldCS->setAxis(dirZ);
        worldCS->setRefDirection(dirX);

        auto* context = CreateEntity<Ifc4x3_add2::IfcGeometricRepresentationContext>(file);
        context->setContextType(std::string("Model"));
        context->setContextIdentifier(std::string("Model"));
        context->setCoordinateSpaceDimension(3);
        context->setPrecision(1e-5);
        context->setWorldCoordinateSystem(worldCS);

        std::cout << "[INFO] Constructing Units..." << std::endl;
        auto* siLength = CreateEntity<Ifc4x3_add2::IfcSIUnit>(file);
        siLength->setUnitType(Ifc4x3_add2::IfcUnitEnum::IfcUnit_LENGTHUNIT);
        siLength->setPrefix(Ifc4x3_add2::IfcSIPrefix::IfcSIPrefix_MILLI);
        siLength->setName(Ifc4x3_add2::IfcSIUnitName::IfcSIUnitName_METRE);

        auto units = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcUnit>>();
        units->push(siLength);
        
        auto* unitAssign = CreateEntity<Ifc4x3_add2::IfcUnitAssignment>(file);
        unitAssign->setUnits(units);

        std::cout << "[INFO] Constructing Project..." << std::endl;
        auto contexts = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcRepresentationContext>>();
        contexts->push(context);
        
        auto* project = CreateEntity<Ifc4x3_add2::IfcProject>(file);
        project->setGlobalId(GenerateIfcGuid());
        project->setOwnerHistory(ownerHist);
        project->setName(std::string("Recipe Parameterized Project"));
        project->setRepresentationContexts(contexts);
        project->setUnitsInContext(unitAssign);

        std::cout << "[INFO] Constructing Site..." << std::endl;
        auto* sitePlacement = CreateEntity<Ifc4x3_add2::IfcLocalPlacement>(file);
        sitePlacement->setRelativePlacement(worldCS);

        auto* site = CreateEntity<Ifc4x3_add2::IfcSite>(file);
        site->setGlobalId(GenerateIfcGuid());
        site->setOwnerHistory(ownerHist);
        site->setName(std::string("BridgeSite"));
        site->setObjectPlacement(sitePlacement);
        site->setCompositionType(Ifc4x3_add2::IfcElementCompositionEnum::IfcElementComposition_ELEMENT);

        auto siteSet = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcObjectDefinition>>();
        siteSet->push(site);
        
        auto* relProjSite = CreateEntity<Ifc4x3_add2::IfcRelAggregates>(file);
        relProjSite->setGlobalId(GenerateIfcGuid());
        relProjSite->setOwnerHistory(ownerHist);
        relProjSite->setRelatingObject(project);
        relProjSite->setRelatedObjects(siteSet);

        std::cout << "[INFO] Constructing Building..." << std::endl;
        auto* buildingPlacement = CreateEntity<Ifc4x3_add2::IfcLocalPlacement>(file);
        buildingPlacement->setPlacementRelTo(sitePlacement);
        buildingPlacement->setRelativePlacement(worldCS);

        auto* building = CreateEntity<Ifc4x3_add2::IfcBuilding>(file);
        building->setGlobalId(GenerateIfcGuid());
        building->setOwnerHistory(ownerHist);
        building->setName(std::string("BridgeStructure"));
        building->setObjectPlacement(buildingPlacement);
        building->setCompositionType(Ifc4x3_add2::IfcElementCompositionEnum::IfcElementComposition_ELEMENT);

        auto buildingSet = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcObjectDefinition>>();
        buildingSet->push(building);
        
        auto* relSiteBuilding = CreateEntity<Ifc4x3_add2::IfcRelAggregates>(file);
        relSiteBuilding->setGlobalId(GenerateIfcGuid());
        relSiteBuilding->setOwnerHistory(ownerHist);
        relSiteBuilding->setRelatingObject(site);
        relSiteBuilding->setRelatedObjects(buildingSet);

        std::cout << "[OK] Spatial structures constructed successfully." << std::endl;

        // 3.5 Collect referenced Profile IDs from features
        std::set<std::string> usedProfiles;
        if (j.contains("features")) {
            for (const auto& f : j["features"]) {
                if (f.contains("SweptArea") && f["SweptArea"].is_string()) {
                    usedProfiles.insert(f["SweptArea"].get<std::string>());
                }
                if (f.contains("CrossSections") && f["CrossSections"].is_array()) {
                    for (const auto& cs : f["CrossSections"]) {
                        if (cs.is_string()) {
                            usedProfiles.insert(cs.get<std::string>());
                        }
                    }
                }
            }
        }
        std::cout << "[INFO] Collected " << usedProfiles.size() << " referenced Profile IDs." << std::endl;

        // 4. Parse Declarations -> Profiles
        std::cout << "[INFO] Parsing Profiles..." << std::endl;
        std::map<std::string, Ifc4x3_add2::IfcProfileDef*> profileMap;

        if (j.contains("declarations") && j["declarations"].contains("profiles")) {
            for (const auto& p : j["declarations"]["profiles"]) {
                std::string id = p["id"];
                std::string type = p["type"];
                
                // Only instantiate profiles that are actually used in features
                if (usedProfiles.find(id) == usedProfiles.end()) {
                    std::cout << "[INFO] Skipping unused profile: " << id << std::endl;
                    continue;
                }
                
                if (type == "IfcRectangleProfileDef") {
                    auto* rect = CreateEntity<Ifc4x3_add2::IfcRectangleProfileDef>(file);
                    rect->setProfileType(Ifc4x3_add2::IfcProfileTypeEnum::IfcProfileType_AREA);
                    rect->setProfileName(id);
                    
                    auto* origin2d = CreateEntity<Ifc4x3_add2::IfcCartesianPoint>(file);
                    origin2d->setCoordinates(std::vector<double>{0.0, 0.0});
                    
                    auto* placement2d = CreateEntity<Ifc4x3_add2::IfcAxis2Placement2D>(file);
                    placement2d->setLocation(origin2d);
                    rect->setPosition(placement2d);
                    
                    rect->setXDim(p["XDim"].get<double>());
                    rect->setYDim(p["YDim"].get<double>());
                    
                    profileMap[id] = rect;
                    std::cout << "[OK] Profile " << id << " (IfcRectangleProfileDef) created." << std::endl;
                }
                else if (type == "IfcCircleProfileDef") {
                    auto* circ = CreateEntity<Ifc4x3_add2::IfcCircleProfileDef>(file);
                    circ->setProfileType(Ifc4x3_add2::IfcProfileTypeEnum::IfcProfileType_AREA);
                    circ->setProfileName(id);
                    
                    auto* origin2d = CreateEntity<Ifc4x3_add2::IfcCartesianPoint>(file);
                    origin2d->setCoordinates(std::vector<double>{0.0, 0.0});
                    
                    auto* placement2d = CreateEntity<Ifc4x3_add2::IfcAxis2Placement2D>(file);
                    placement2d->setLocation(origin2d);
                    circ->setPosition(placement2d);
                    
                    circ->setRadius(p["Radius"].get<double>());
                    
                    profileMap[id] = circ;
                    std::cout << "[OK] Profile " << id << " (IfcCircleProfileDef) created." << std::endl;
                }
                else if (type == "IfcArbitraryClosedProfileDef") {
                    auto outerCurveJson = p["OuterCurve"];
                    std::string curveType = outerCurveJson["type"];
                    if (curveType == "IfcPolyline") {
                        auto pointsJson = outerCurveJson["Points"];
                        size_t nPoints = pointsJson.size();
                        if (nPoints < 3) {
                            std::cout << "[WARNING] Skipping profile " << id << " because it has only " << nPoints << " point(s) and cannot form a closed loop (which triggers viewer crash)." << std::endl;
                            continue;
                        }
                        
                        std::vector<std::pair<double, double>> pts;
                        for (const auto& pt : pointsJson) {
                            pts.push_back({pt[0].get<double>(), pt[1].get<double>()});
                        }
                        
                        // Check if closed (Euclidean distance tolerance 1e-4)
                        double dx = pts[0].first - pts[pts.size() - 1].first;
                        double dy = pts[0].second - pts[pts.size() - 1].second;
                        if ((dx * dx + dy * dy) > 1e-8) {
                            std::cout << "[INFO] Profile " << id << " is not closed. Automatically appending start point to close the loop." << std::endl;
                            pts.push_back(pts[0]);
                        }
                        
                        auto* arb = CreateEntity<Ifc4x3_add2::IfcArbitraryClosedProfileDef>(file);
                        arb->setProfileType(Ifc4x3_add2::IfcProfileTypeEnum::IfcProfileType_AREA);
                        arb->setProfileName(id);
                        
                        auto* poly = CreateEntity<Ifc4x3_add2::IfcPolyline>(file);
                        auto pointsAgg = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcCartesianPoint>>();
                        
                        for (const auto& pt : pts) {
                            auto* ptEntity = CreateEntity<Ifc4x3_add2::IfcCartesianPoint>(file);
                            ptEntity->setCoordinates(std::vector<double>{pt.first, pt.second});
                            pointsAgg->push(ptEntity);
                        }
                        
                        poly->setPoints(pointsAgg);
                        arb->setOuterCurve(poly);
                        
                        profileMap[id] = arb;
                        std::cout << "[OK] Profile " << id << " (IfcArbitraryClosedProfileDef) created (points count: " << pts.size() << ")." << std::endl;
                    }
                }
            }
        }

        // 5. Parse Features -> Geometric Representation Items
        std::cout << "[INFO] Parsing Features..." << std::endl;
        std::map<std::string, Ifc4x3_add2::IfcRepresentationItem*> featureMap;
        std::map<std::string, Ifc4x3_add2::IfcRepresentationMap*> repMapCache;

        if (j.contains("features")) {
            for (const auto& f : j["features"]) {
                std::string id = f["id"];
                std::string type = f["type"];
                
                if (type == "IfcExtrudedAreaSolid") {
                    auto* solid = CreateEntity<Ifc4x3_add2::IfcExtrudedAreaSolid>(file);
                    
                    std::string sweptAreaId = f["SweptArea"];
                    if (profileMap.find(sweptAreaId) != profileMap.end()) {
                        solid->setSweptArea(profileMap[sweptAreaId]);
                    }
                    
                    auto posJson = f["Position"];
                    auto* pos = CreateAxis2Placement3D(file, 
                        posJson["Location"].get<std::vector<double>>(),
                        posJson["Axis"].get<std::vector<double>>(),
                        posJson["RefDirection"].get<std::vector<double>>());
                    solid->setPosition(pos);
                    
                    auto* dir = CreateEntity<Ifc4x3_add2::IfcDirection>(file);
                    dir->setDirectionRatios(f["ExtrudedDirection"].get<std::vector<double>>());
                    solid->setExtrudedDirection(dir);
                    
                    solid->setDepth(f["Depth"].get<double>());
                    
                    featureMap[id] = solid;
                    std::cout << "[OK] Feature " << id << " (IfcExtrudedAreaSolid) created." << std::endl;
                }
                else if (type == "IfcMappedItem") {
                    auto* mappedItem = CreateEntity<Ifc4x3_add2::IfcMappedItem>(file);
                    
                    std::string sourceGeomId = f["SourceGeometry"];
                    Ifc4x3_add2::IfcRepresentationMap* repMap = nullptr;
                    
                    if (repMapCache.find(sourceGeomId) != repMapCache.end()) {
                        repMap = repMapCache[sourceGeomId];
                    } else {
                        // Create shape representation for basis geometry
                        auto* shapeRep = CreateEntity<Ifc4x3_add2::IfcShapeRepresentation>(file);
                        shapeRep->setContextOfItems(context);
                        shapeRep->setRepresentationIdentifier(std::string("Body"));
                        shapeRep->setRepresentationType(std::string("SweptSolid"));
                        
                        auto repItems = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcRepresentationItem>>();
                        if (featureMap.find(sourceGeomId) != featureMap.end()) {
                            repItems->push(featureMap[sourceGeomId]);
                        }
                        shapeRep->setItems(repItems);
                        
                        // Origin Axis
                        auto* originPoint = CreateEntity<Ifc4x3_add2::IfcCartesianPoint>(file);
                        originPoint->setCoordinates(std::vector<double>{0.0, 0.0, 0.0});
                        
                        auto* originAxis = CreateEntity<Ifc4x3_add2::IfcAxis2Placement3D>(file);
                        originAxis->setLocation(originPoint);
                        auto* originZ = CreateEntity<Ifc4x3_add2::IfcDirection>(file);
                        originZ->setDirectionRatios(std::vector<double>{0.0, 0.0, 1.0});
                        auto* originX = CreateEntity<Ifc4x3_add2::IfcDirection>(file);
                        originX->setDirectionRatios(std::vector<double>{1.0, 0.0, 0.0});
                        originAxis->setAxis(originZ);
                        originAxis->setRefDirection(originX);
                        
                        // Representation Map
                        repMap = CreateEntity<Ifc4x3_add2::IfcRepresentationMap>(file);
                        repMap->setMappingOrigin(originAxis);
                        repMap->setMappedRepresentation(shapeRep);
                        
                        repMapCache[sourceGeomId] = repMap;
                    }
                    
                    // Transformation operator
                    auto* trans = CreateEntity<Ifc4x3_add2::IfcCartesianTransformationOperator3D>(file);
                    auto* targetPt = CreateEntity<Ifc4x3_add2::IfcCartesianPoint>(file);
                    targetPt->setCoordinates(f["TargetLocation"].get<std::vector<double>>());
                    trans->setLocalOrigin(targetPt);
                    trans->setScale(1.0);
                    
                    auto* axisX = CreateEntity<Ifc4x3_add2::IfcDirection>(file);
                    axisX->setDirectionRatios(std::vector<double>{1.0, 0.0, 0.0});
                    auto* axisY = CreateEntity<Ifc4x3_add2::IfcDirection>(file);
                    axisY->setDirectionRatios(std::vector<double>{0.0, 1.0, 0.0});
                    auto* axisZ = CreateEntity<Ifc4x3_add2::IfcDirection>(file);
                    axisZ->setDirectionRatios(std::vector<double>{0.0, 0.0, 1.0});
                    trans->setAxis1(axisX);
                    trans->setAxis2(axisY);
                    trans->setAxis3(axisZ);
                    
                    mappedItem->setMappingSource(repMap);
                    mappedItem->setMappingTarget(trans);
                    
                    featureMap[id] = mappedItem;
                    std::cout << "[OK] Feature " << id << " (IfcMappedItem) mapped from " << sourceGeomId << "." << std::endl;
                }
                else if (type == "IfcSectionedSpine") {
                    std::cout << "[INFO] IfcSectionedSpine feature " << id << " detected. Attempting high-fidelity Tessellation..." << std::endl;
                    
                    bool tessellated = false;
                    Ifc4x3_add2::IfcRepresentationItem* geomItem = nullptr;
                    
                    // 1. Try to load high-fidelity BRep geometry from local files
                    TopoDS_Shape pierShape;
                    BRep_Builder builder;
                    std::string expectedBrep = "export_" + scriptBase + ".brep";
                    std::vector<std::string> pathsToTry = {
                        "d:\\QtOCCTApp\\" + expectedBrep,
                        expectedBrep
                    };
                    
                    size_t lastSlash = inputPath.find_last_of("\\/");
                    if (lastSlash != std::string::npos) {
                        std::string baseDir = inputPath.substr(0, lastSlash);
                        pathsToTry.push_back(baseDir + "\\..\\" + expectedBrep);
                        pathsToTry.push_back(baseDir + "\\" + expectedBrep);
                    }
                    
                    bool brepLoaded = false;
                    for (const auto& path : pathsToTry) {
                        try {
                            if (BRepTools::Read(pierShape, path.c_str(), builder)) {
                                std::cout << "[OK] Successfully loaded BRep from " << path << std::endl;
                                brepLoaded = true;
                                break;
                            }
                        } catch (...) {}
                    }
                    
                    if (brepLoaded) {
                        // 2. Serialize BRep to IFC geometry
                        IfcUtil::IfcBaseClass* serialized = nullptr;
                        try {
                            serialized = IfcGeom::serialise("Ifc4x3_add2", pierShape, false);
                        } catch (...) {}
                        
                        if (!serialized) {
                            std::cout << "[INFO] Planar BRep serialization returned null for spine. Trying tesselation..." << std::endl;
                            try {
                                serialized = IfcGeom::tesselate("Ifc4x3_add2", pierShape, 2.0);
                            } catch (...) {}
                        }
                        
                        if (serialized) {
                            std::cout << "[OK] Geometry serialized successfully for spine. Type: " << serialized->declaration().name() << std::endl;
                            
                            // 3. Register geometry entities to file
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
                                                        if (!geomItem) {
                                                            geomItem = *it2;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    
                    if (geomItem) {
                        featureMap[id] = geomItem;
                        tessellated = true;
                        std::cout << "[SUCCESS] IfcSectionedSpine feature " << id << " successfully tessellated to " << geomItem->declaration().name() << std::endl;
                    }
                    
                    // 4. Fallback to default IfcSectionedSpine if BRep not found or serialization failed
                    if (!tessellated) {
                        std::cout << "[WARNING] Tessellation bypass failed or BRep not found. Falling back to default IfcSectionedSpine." << std::endl;
                        
                        auto* spine = CreateEntity<Ifc4x3_add2::IfcSectionedSpine>(file);
                        
                        // 1. Build SpineCurve (wrap IfcPolyline in IfcCompositeCurve for IFC4x3 compliance)
                        auto spineJson = f["SpineCurve"];
                        if (spineJson["type"] == "IfcPolyline") {
                            auto* spinePoly = CreateEntity<Ifc4x3_add2::IfcPolyline>(file);
                            auto spinePointsAgg = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcCartesianPoint>>();
                            
                            for (const auto& pt : spineJson["Points"]) {
                                auto* ptEntity = CreateEntity<Ifc4x3_add2::IfcCartesianPoint>(file);
                                ptEntity->setCoordinates(pt.get<std::vector<double>>());
                                spinePointsAgg->push(ptEntity);
                            }
                            spinePoly->setPoints(spinePointsAgg);
                            
                            auto* segment = CreateEntity<Ifc4x3_add2::IfcCompositeCurveSegment>(file);
                            segment->setTransition(Ifc4x3_add2::IfcTransitionCode::IfcTransitionCode_CONTINUOUS);
                            segment->setSameSense(true);
                            segment->setParentCurve(spinePoly);
                            
                            auto segmentsAgg = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcSegment>>();
                            segmentsAgg->push(segment);
                            
                            auto* compositeCurve = CreateEntity<Ifc4x3_add2::IfcCompositeCurve>(file);
                            compositeCurve->setSegments(segmentsAgg);
                            compositeCurve->setSelfIntersect(boost::logic::tribool(false));
                            
                            spine->setSpineCurve(compositeCurve);
                        }
                        
                        // 2. Build CrossSections aggregate
                        auto crossSectionsAgg = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcProfileDef>>();
                        for (const auto& csId : f["CrossSections"]) {
                            std::string csName = csId.get<std::string>();
                            if (profileMap.find(csName) != profileMap.end()) {
                                crossSectionsAgg->push(profileMap[csName]);
                            }
                        }
                        spine->setCrossSections(crossSectionsAgg);
                        
                        // 3. Build CrossSectionPositions aggregate
                        auto positionsAgg = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcAxis2Placement3D>>();
                        for (const auto& posJson : f["Positions"]) {
                            auto* pos = CreateAxis2Placement3D(file,
                                posJson["Location"].get<std::vector<double>>(),
                                posJson["Axis"].get<std::vector<double>>(),
                                posJson["RefDirection"].get<std::vector<double>>());
                            positionsAgg->push(pos);
                        }
                        spine->setCrossSectionPositions(positionsAgg);
                        
                        featureMap[id] = spine;
                        std::cout << "[OK] Feature " << id << " (IfcSectionedSpine) created with full fidelity." << std::endl;
                    }
                }
                else if (type == "IfcBooleanResult") {
                    // Full-fidelity IfcBooleanResult implementation
                    auto* boolResult = CreateEntity<Ifc4x3_add2::IfcBooleanResult>(file);
                    
                    // Set operator
                    std::string op = f.value("operator", "DIFFERENCE");
                    if (op == "DIFFERENCE") {
                        boolResult->setOperator(Ifc4x3_add2::IfcBooleanOperator::IfcBooleanOperator_DIFFERENCE);
                    } else if (op == "UNION") {
                        boolResult->setOperator(Ifc4x3_add2::IfcBooleanOperator::IfcBooleanOperator_UNION);
                    } else if (op == "INTERSECTION") {
                        boolResult->setOperator(Ifc4x3_add2::IfcBooleanOperator::IfcBooleanOperator_INTERSECTION);
                    }
                    
                    // Set first operand (dynamic_cast to IfcBooleanOperand)
                    std::string firstId = f["first_operand"].get<std::string>();
                    Ifc4x3_add2::IfcBooleanOperand* firstOp = nullptr;
                    if (featureMap.find(firstId) != featureMap.end()) {
                        firstOp = dynamic_cast<Ifc4x3_add2::IfcBooleanOperand*>(featureMap[firstId]);
                        if (firstOp) boolResult->setFirstOperand(firstOp);
                    }
                    
                    // Set second operand (dynamic_cast to IfcBooleanOperand)
                    std::string secondId = f["second_operand"].get<std::string>();
                    Ifc4x3_add2::IfcBooleanOperand* secondOp = nullptr;
                    if (featureMap.find(secondId) != featureMap.end()) {
                        secondOp = dynamic_cast<Ifc4x3_add2::IfcBooleanOperand*>(featureMap[secondId]);
                        if (secondOp) boolResult->setSecondOperand(secondOp);
                    }
                    
                    // Smart fallback when dynamic_cast fails (e.g. IfcSectionedSpine is not an IfcBooleanOperand in IFC4)
                    if (firstOp && secondOp) {
                        featureMap[id] = boolResult;
                        std::cout << "[OK] Feature " << id << " (IfcBooleanResult, op=" << op << ") created with full fidelity." << std::endl;
                    } else {
                        if (featureMap.find(firstId) != featureMap.end()) {
                            // Smart fallback: Bypass boolean cut and map the first operand (the spine) directly as this feature
                            featureMap[id] = featureMap[firstId];
                            std::cout << "[WARNING] Feature " << id << " (IfcBooleanResult) failed to cast operands to IfcBooleanOperand (e.g. Spine is not a solid). "
                                      << "Smart fallback active: Bypassing boolean cut and using first operand (" << firstId << ") directly." << std::endl;
                        } else {
                            featureMap[id] = boolResult;
                            std::cout << "[WARNING] Feature " << id << " (IfcBooleanResult) has invalid first operand." << std::endl;
                        }
                    }
                }
            }
        }

        // 6. Parse Products -> BIM Semantic Elements
        std::cout << "[INFO] Parsing Products..." << std::endl;
        auto productSet = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcProduct>>();

        // Gather all sub-features (boolean operands and mapped source geometries)
        // that must not be directly included in any product's representations list.
        std::set<std::string> subFeatures;
        if (j.contains("features")) {
            for (const auto& f : j["features"]) {
                std::string type = f.value("type", "");
                if (type == "IfcBooleanResult") {
                    if (f.contains("first_operand") && f["first_operand"].is_string()) {
                        subFeatures.insert(f["first_operand"].get<std::string>());
                    }
                    if (f.contains("second_operand") && f["second_operand"].is_string()) {
                        subFeatures.insert(f["second_operand"].get<std::string>());
                    }
                }
                else if (type == "IfcMappedItem") {
                    if (f.contains("SourceGeometry") && f["SourceGeometry"].is_string()) {
                        subFeatures.insert(f["SourceGeometry"].get<std::string>());
                    }
                }
            }
        }

        if (j.contains("products")) {
            for (const auto& prod : j["products"]) {
                std::string prodId = prod["id"];
                std::string ifcType = prod["ifc_type"];
                std::string name = prod["name"];
                
                // Special high-fidelity split logic for the automatically recorded proxy product
                if (ifcType == "IfcBuildingElementProxy" && name == "Auto_Recorded_Product") {
                    std::cout << "[INFO] Processing Auto_Recorded_Product. Splitting into high-fidelity sub-products..." << std::endl;
                    
                    std::vector<std::string> columnReps;
                    std::vector<std::string> baseReps;
                    std::vector<std::string> pedestalReps;
                    
                    if (prod.contains("representations")) {
                        for (const auto& repId : prod["representations"]) {
                            std::string rId = repId.get<std::string>();
                            if (subFeatures.count(rId) > 0) {
                                std::cout << "[INFO] Filtering out sub-feature: " << rId << " from auto split." << std::endl;
                                continue;
                            }
                            
                            if (rId.find("Boolean") != std::string::npos || rId.find("Loft") != std::string::npos) {
                                columnReps.push_back(rId);
                            } else if (rId.find("Extrude") != std::string::npos) {
                                baseReps.push_back(rId);
                            } else if (rId.find("Mapped") != std::string::npos) {
                                pedestalReps.push_back(rId);
                            } else {
                                // Default fallback to column for other features
                                columnReps.push_back(rId);
                            }
                        }
                    }
                    
                    // 1. Create clean IfcColumn for '变截面墩身'
                    if (!columnReps.empty()) {
                        auto* column = CreateEntity<Ifc4x3_add2::IfcColumn>(file);
                        column->setPredefinedType(Ifc4x3_add2::IfcColumnTypeEnum::IfcColumnType_COLUMN);
                        column->setGlobalId(GenerateIfcGuid());
                        column->setOwnerHistory(ownerHist);
                        column->setName(std::string("变截面墩身"));
                        
                        auto* placement = CreateEntity<Ifc4x3_add2::IfcLocalPlacement>(file);
                        placement->setPlacementRelTo(buildingPlacement);
                        placement->setRelativePlacement(worldCS);
                        column->setObjectPlacement(placement);
                        
                        auto representationsAgg = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcRepresentation>>();
                        for (const auto& rId : columnReps) {
                            if (featureMap.find(rId) != featureMap.end()) {
                                auto* featEntity = featureMap[rId];
                                std::string repType = "SweptSolid";
                                if (featEntity->declaration().is("IfcMappedItem")) {
                                    repType = "MappedRepresentation";
                                } else if (featEntity->declaration().is("IfcBooleanResult")) {
                                    repType = "CSG";
                                } else if (featEntity->declaration().is("IfcSectionedSpine")) {
                                    repType = "AdvancedSweptSolid";
                                } else if (featEntity->declaration().is("IfcShellBasedSurfaceModel") ||
                                           featEntity->declaration().is("IfcFaceBasedSurfaceModel")) {
                                    repType = "SurfaceModel";
                                } else if (featEntity->declaration().is("IfcFacetedBrep")) {
                                    repType = "Brep";
                                }
                                
                                auto* shapeRep = CreateEntity<Ifc4x3_add2::IfcShapeRepresentation>(file);
                                shapeRep->setContextOfItems(context);
                                shapeRep->setRepresentationIdentifier(std::string("Body"));
                                shapeRep->setRepresentationType(repType);
                                
                                auto repItems = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcRepresentationItem>>();
                                repItems->push(featEntity);
                                shapeRep->setItems(repItems);
                                representationsAgg->push(shapeRep);
                                std::cout << "[OK] Built shape representation for top-level feature " << rId << " with type " << repType << "." << std::endl;
                            }
                        }
                        if (representationsAgg->size() > 0) {
                            auto* pds = CreateEntity<Ifc4x3_add2::IfcProductDefinitionShape>(file);
                            pds->setRepresentations(representationsAgg);
                            column->setRepresentation(pds);
                        }
                        productSet->push(column);
                        std::cout << "[SUCCESS] Split and created product '变截面墩身' (IfcColumn)" << std::endl;
                    }
                    
                    // 2. Create IfcBuildingElementProxy for '桥墩底座垫层'
                    if (!baseReps.empty()) {
                        auto* baseProxy = CreateEntity<Ifc4x3_add2::IfcBuildingElementProxy>(file);
                        baseProxy->setGlobalId(GenerateIfcGuid());
                        baseProxy->setOwnerHistory(ownerHist);
                        baseProxy->setName(std::string("桥墩底座垫层"));
                        
                        auto* placement = CreateEntity<Ifc4x3_add2::IfcLocalPlacement>(file);
                        placement->setPlacementRelTo(buildingPlacement);
                        placement->setRelativePlacement(worldCS);
                        baseProxy->setObjectPlacement(placement);
                        
                        auto representationsAgg = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcRepresentation>>();
                        for (const auto& rId : baseReps) {
                            if (featureMap.find(rId) != featureMap.end()) {
                                auto* featEntity = featureMap[rId];
                                std::string repType = "SweptSolid";
                                
                                auto* shapeRep = CreateEntity<Ifc4x3_add2::IfcShapeRepresentation>(file);
                                shapeRep->setContextOfItems(context);
                                shapeRep->setRepresentationIdentifier(std::string("Body"));
                                shapeRep->setRepresentationType(repType);
                                
                                auto repItems = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcRepresentationItem>>();
                                repItems->push(featEntity);
                                shapeRep->setItems(repItems);
                                representationsAgg->push(shapeRep);
                                std::cout << "[OK] Built shape representation for top-level feature " << rId << " with type " << repType << "." << std::endl;
                            }
                        }
                        if (representationsAgg->size() > 0) {
                            auto* pds = CreateEntity<Ifc4x3_add2::IfcProductDefinitionShape>(file);
                            pds->setRepresentations(representationsAgg);
                            baseProxy->setRepresentation(pds);
                        }
                        productSet->push(baseProxy);
                        std::cout << "[SUCCESS] Split and created product '桥墩底座垫层' (IfcBuildingElementProxy)" << std::endl;
                    }
                    
                    // 3. Create IfcBuildingElementProxy for '支座垫石'
                    if (!pedestalReps.empty()) {
                        auto* pedProxy = CreateEntity<Ifc4x3_add2::IfcBuildingElementProxy>(file);
                        pedProxy->setGlobalId(GenerateIfcGuid());
                        pedProxy->setOwnerHistory(ownerHist);
                        pedProxy->setName(std::string("支座垫石"));
                        
                        auto* placement = CreateEntity<Ifc4x3_add2::IfcLocalPlacement>(file);
                        placement->setPlacementRelTo(buildingPlacement);
                        placement->setRelativePlacement(worldCS);
                        pedProxy->setObjectPlacement(placement);
                        
                        auto representationsAgg = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcRepresentation>>();
                        for (const auto& rId : pedestalReps) {
                            if (featureMap.find(rId) != featureMap.end()) {
                                auto* featEntity = featureMap[rId];
                                std::string repType = "MappedRepresentation";
                                
                                auto* shapeRep = CreateEntity<Ifc4x3_add2::IfcShapeRepresentation>(file);
                                shapeRep->setContextOfItems(context);
                                shapeRep->setRepresentationIdentifier(std::string("Body"));
                                shapeRep->setRepresentationType(repType);
                                
                                auto repItems = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcRepresentationItem>>();
                                repItems->push(featEntity);
                                shapeRep->setItems(repItems);
                                representationsAgg->push(shapeRep);
                                std::cout << "[OK] Built shape representation for top-level feature " << rId << " with type " << repType << "." << std::endl;
                            }
                        }
                        if (representationsAgg->size() > 0) {
                            auto* pds = CreateEntity<Ifc4x3_add2::IfcProductDefinitionShape>(file);
                            pds->setRepresentations(representationsAgg);
                            pedProxy->setRepresentation(pds);
                        }
                        productSet->push(pedProxy);
                        std::cout << "[SUCCESS] Split and created product '支座垫石' (IfcBuildingElementProxy)" << std::endl;
                    }
                    
                    continue; // Skip standard parsing for this Auto_Recorded_Product!
                }
                
                Ifc4x3_add2::IfcProduct* ifcProd = nullptr;
                
                if (ifcType == "IfcFooting") {
                    auto* footing = CreateEntity<Ifc4x3_add2::IfcFooting>(file);
                    if (prod.contains("predefined_type")) {
                        std::string preType = prod["predefined_type"];
                        if (preType == "PILE_CAP") {
                            footing->setPredefinedType(Ifc4x3_add2::IfcFootingTypeEnum::IfcFootingType_PILE_CAP);
                        }
                    }
                    ifcProd = footing;
                } else if (ifcType == "IfcPile") {
                    auto* pile = CreateEntity<Ifc4x3_add2::IfcPile>(file);
                    if (prod.contains("predefined_type")) {
                        std::string preType = prod["predefined_type"];
                        if (preType == "BORED") {
                            pile->setPredefinedType(Ifc4x3_add2::IfcPileTypeEnum::IfcPileType_BORED);
                        }
                    }
                    ifcProd = pile;
                } else if (ifcType == "IfcColumn") {
                    auto* column = CreateEntity<Ifc4x3_add2::IfcColumn>(file);
                    column->setPredefinedType(Ifc4x3_add2::IfcColumnTypeEnum::IfcColumnType_COLUMN);
                    ifcProd = column;
                } else {
                    auto* proxy = CreateEntity<Ifc4x3_add2::IfcBuildingElementProxy>(file);
                    ifcProd = proxy;
                }
                
                if (!ifcProd) continue;
                
                ifcProd->setGlobalId(GenerateIfcGuid());
                ifcProd->setOwnerHistory(ownerHist);
                ifcProd->setName(name);
                
                // Object Placement relative to building spatial container
                auto* prodPlacement = CreateEntity<Ifc4x3_add2::IfcLocalPlacement>(file);
                prodPlacement->setPlacementRelTo(buildingPlacement);
                prodPlacement->setRelativePlacement(worldCS);
                ifcProd->setObjectPlacement(prodPlacement);
                
                // Geometry Representation
                if (prod.contains("representations") && prod["representations"].size() > 0) {
                    auto representationsAgg = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcRepresentation>>();
                    
                    for (const auto& repId : prod["representations"]) {
                        std::string rId = repId.get<std::string>();
                        if (subFeatures.count(rId) > 0) {
                            std::cout << "[INFO] Filtering out sub-feature: " << rId << " from representation." << std::endl;
                            continue;
                        }
                        
                        if (featureMap.find(rId) != featureMap.end()) {
                            auto* featEntity = featureMap[rId];
                            
                            // Determine correct representation type for this single item
                            std::string repType = "SweptSolid";
                            if (featEntity->declaration().is("IfcMappedItem")) {
                                repType = "MappedRepresentation";
                            } else if (featEntity->declaration().is("IfcBooleanResult")) {
                                repType = "CSG";
                            } else if (featEntity->declaration().is("IfcSectionedSpine")) {
                                repType = "AdvancedSweptSolid";
                            } else if (featEntity->declaration().is("IfcShellBasedSurfaceModel") ||
                                       featEntity->declaration().is("IfcFaceBasedSurfaceModel")) {
                                repType = "SurfaceModel";
                            } else if (featEntity->declaration().is("IfcFacetedBrep")) {
                                repType = "Brep";
                            }
                            
                            auto* shapeRep = CreateEntity<Ifc4x3_add2::IfcShapeRepresentation>(file);
                            shapeRep->setContextOfItems(context);
                            shapeRep->setRepresentationIdentifier(std::string("Body"));
                            shapeRep->setRepresentationType(repType);
                            
                            auto repItems = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcRepresentationItem>>();
                            repItems->push(featEntity);
                            shapeRep->setItems(repItems);
                            
                            representationsAgg->push(shapeRep);
                            std::cout << "[OK] Built shape representation for top-level feature " << rId << " with type " << repType << "." << std::endl;
                        }
                    }
                    
                    if (representationsAgg->size() > 0) {
                        auto* pds = CreateEntity<Ifc4x3_add2::IfcProductDefinitionShape>(file);
                        pds->setRepresentations(representationsAgg);
                        ifcProd->setRepresentation(pds);
                    }
                }
                
                productSet->push(ifcProd);
                std::cout << "[OK] Product " << name << " (" << ifcType << ") built and bound to geometry." << std::endl;
            }
        }

        // 7. Relate products to Building container
        if (productSet->size() > 0) {
            auto* relContained = CreateEntity<Ifc4x3_add2::IfcRelContainedInSpatialStructure>(file);
            relContained->setGlobalId(GenerateIfcGuid());
            relContained->setOwnerHistory(ownerHist);
            relContained->setRelatedElements(productSet);
            relContained->setRelatingStructure(building);
            std::cout << "[OK] Linked all products to Building structure." << std::endl;
        }

        // 8. Write to output file with forced compatibility translation
        std::cout << "[INFO] Serializing IFC database to SPF..." << std::endl;
        std::stringstream ss;
        ss << file;
        std::string content = ss.str();

        // Force header schema to IFC4 for maximum viewer compatibility
        size_t pos = content.find("FILE_SCHEMA(('IFC4X3_ADD2'));");
        if (pos != std::string::npos) {
            content.replace(pos, 29, "FILE_SCHEMA(('IFC4'));");
            std::cout << "[INFO] Schema tag header converted to 'IFC4' for high compatibility." << std::endl;
        }

        std::ofstream ofs(outputPath);
        if (ofs.is_open()) {
            ofs << content;
            ofs.close();
            std::cout << "[SUCCESS] Parameterized IFC file successfully generated: " << outputPath << std::endl;
        } else {
            std::cerr << "[ERROR] Failed to open output file for writing: " << outputPath << std::endl;
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
