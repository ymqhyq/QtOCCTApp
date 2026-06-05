#define HAS_SCHEMA_4x3_add2
#include "IfcExportService.h"
#include <BRepTools.hxx>
#include <codecvt>
#include <locale>
#include <BRep_Builder.hxx>
#include <TCollection_AsciiString.hxx>
#include <TopoDS_Compound.hxx>
#include <boost/make_shared.hpp>
#include <ctime>
#include <fstream>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <iostream>
#include <map>
#include <random>
#include <sstream>
#include <vector>

// XCAF OCAF & TDF Tool Headers
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_LayerTool.hxx>
#include <XCAFDoc_VisMaterialTool.hxx>
#include <XCAFDoc_VisMaterial.hxx>
#include <TDF_ChildIterator.hxx>
#include <TDF_Tool.hxx>
#include <TDF_LabelSequence.hxx>
#include <TColStd_SequenceOfExtendedString.hxx>
#include <TDataStd_Name.hxx>


// Thread-safe cache to store generated product representations per unique
// geometry definition
static thread_local std::map<void *, Ifc4x3_add2::IfcProductDefinitionShape *>
    g_geomInstanceCache;

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

static void ReplaceAll(std::string& str, const std::string& from, const std::string& to);
static std::string GenerateIfcGuid();

bool IfcExportService::ExportShapeToFile(const TopoDS_Shape &shape,
                                         const std::string &filename) {
  if (shape.IsNull())
    return false;

  try {
    const IfcParse::schema_definition *schema =
        IfcParse::schema_by_name("Ifc4x3_add2");
    if (!schema)
      return false;

    IfcParse::register_schema(
        const_cast<IfcParse::schema_definition *>(schema));

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
    ownerHist->setChangeAction(
        Ifc4x3_add2::IfcChangeActionEnum::IfcChangeAction_NOCHANGE);
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

    auto context =
        CreateEntity<Ifc4x3_add2::IfcGeometricRepresentationContext>(file);
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

    auto contexts = boost::make_shared<
        aggregate_of<Ifc4x3_add2::IfcRepresentationContext>>();
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
    site->setCompositionType(
        Ifc4x3_add2::IfcElementCompositionEnum::IfcElementComposition_ELEMENT);

    auto siteSet =
        boost::make_shared<aggregate_of<Ifc4x3_add2::IfcObjectDefinition>>();
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
    building->setCompositionType(
        Ifc4x3_add2::IfcElementCompositionEnum::IfcElementComposition_ELEMENT);

    auto buildingSet =
        boost::make_shared<aggregate_of<Ifc4x3_add2::IfcObjectDefinition>>();
    buildingSet->push(building);

    auto relSiteBuilding = CreateEntity<Ifc4x3_add2::IfcRelAggregates>(file);
    relSiteBuilding->setGlobalId(GenerateIfcGuid());
    relSiteBuilding->setOwnerHistory(ownerHist);
    relSiteBuilding->setRelatingObject(site);
    relSiteBuilding->setRelatedObjects(buildingSet);

    IfcUtil::IfcBaseClass *serialized = nullptr;
    try {
      serialized = IfcGeom::serialise(schema->name(), shape, true);
    } catch (...) {
    }
    if (!serialized) {
      try {
        serialized = IfcGeom::serialise(schema->name(), shape, false);
      } catch (...) {
      }
    }
    if (!serialized) {
      try {
        serialized = IfcGeom::tesselate(schema->name(), shape, 2.0);
      } catch (...) {
      }
    }

    if (!serialized)
      return false;

    file.addEntity(serialized);
    if (serialized->declaration().is("IfcProductDefinitionShape")) {
      auto pds = (Ifc4x3_add2::IfcProductDefinitionShape *)serialized;
      if (pds->Representations()) {
        auto reps = pds->Representations();
        for (auto it = reps->begin(); it != reps->end(); ++it) {
          auto rep = *it;
          if (rep) {
            rep->setContextOfItems(context);
            file.addEntity(rep);
            if (rep->Items()) {
              auto repItems = rep->Items();
              for (auto it2 = repItems->begin(); it2 != repItems->end();
                   ++it2) {
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
      proxy->setRepresentation(
          (Ifc4x3_add2::IfcProductDefinitionShape *)serialized);
    }

    auto productSet =
        boost::make_shared<aggregate_of<Ifc4x3_add2::IfcProduct>>();
    productSet->push(proxy);

    auto relContained =
        CreateEntity<Ifc4x3_add2::IfcRelContainedInSpatialStructure>(file);
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

    ReplaceAll(content, "\\\\X2\\\\", "\\X2\\");
    ReplaceAll(content, "\\\\X0\\\\", "\\X0\\");

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

static std::wstring Utf8ToWstring(const std::string& utf8) {
    std::wstring wstr;
    size_t i = 0;
    while (i < utf8.size()) {
        unsigned char c = utf8[i];
        if (c < 0x80) {
            wstr.push_back(c);
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 < utf8.size()) {
                wchar_t val = ((c & 0x1F) << 6) | (utf8[i + 1] & 0x3F);
                wstr.push_back(val);
            }
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 < utf8.size()) {
                wchar_t val = ((c & 0x0F) << 12) | ((utf8[i + 1] & 0x3F) << 6) | (utf8[i + 2] & 0x3F);
                wstr.push_back(val);
            }
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 < utf8.size()) {
                unsigned int cp = ((c & 0x07) << 18) | ((utf8[i + 1] & 0x3F) << 12) | 
                                  ((utf8[i + 2] & 0x3F) << 6) | (utf8[i + 3] & 0x3F);
                if (cp >= 0x10000) {
                    cp -= 0x10000;
                    wstr.push_back((wchar_t)((cp >> 10) + 0xD800));
                    wstr.push_back((wchar_t)((cp & 0x3FF) + 0xDC00));
                } else {
                    wstr.push_back((wchar_t)cp);
                }
            }
            i += 4;
        } else {
            i += 1;
        }
    }
    return wstr;
}

std::string
IfcExportService::ToStdString(const TCollection_ExtendedString &extStr) {
  Standard_Integer len = extStr.Length();
  if (len == 0) return "";

  // 1. Check if all characters are within Latin-1/UTF-8 byte range (high 8 bits are 0)
  bool isLatin1OrUtf8Bytes = true;
  for (Standard_Integer i = 1; i <= len; ++i) {
      Standard_ExtCharacter ch = extStr.Value(i);
      if ((ch & 0xFF00) != 0) {
          isLatin1OrUtf8Bytes = false;
          break;
      }
  }

  // 2. Decode bytes/UTF-16 to wstring
  std::wstring wstr;
  if (isLatin1OrUtf8Bytes) {
      std::string utf8Str;
      utf8Str.reserve(len);
      for (Standard_Integer i = 1; i <= len; ++i) {
          Standard_ExtCharacter ch = extStr.Value(i);
          utf8Str.push_back((char)(ch & 0xFF));
      }
      wstr = Utf8ToWstring(utf8Str);
  } else {
      wstr.resize(len);
      for (Standard_Integer i = 1; i <= len; ++i) {
          wstr[i - 1] = (wchar_t)extStr.Value(i);
      }
  }

  // 3. Format as IFC hexadecimal unicode escape string (\X2\...\X0\)
  std::string result = "";
  bool inUnicodeMode = false;
  for (size_t i = 0; i < wstr.size(); ++i) {
      wchar_t wch = wstr[i];
      if (wch >= 32 && wch <= 126) {
          if (inUnicodeMode) {
              result += "\\X0\\";
              inUnicodeMode = false;
          }
          result += (char)wch;
      } else {
          if (!inUnicodeMode) {
              result += "\\X2\\";
              inUnicodeMode = true;
          }
          char hexBuf[8];
          snprintf(hexBuf, sizeof(hexBuf), "%04X", (unsigned int)wch);
          result += hexBuf;
      }
  }
  
  if (inUnicodeMode) {
      result += "\\X0\\";
  }
  
  return result;
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

static void ReplaceAll(std::string& str, const std::string& from, const std::string& to) {
    if (from.empty()) return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

void IfcExportService::ExportXcafLabel(const TDF_Label& label,
                                     IfcParse::IfcFile& file,
                                     Ifc4x3_add2::IfcObjectDefinition* parentIfc,
                                     Ifc4x3_add2::IfcSpatialElement* spatialContainer,
                                     Ifc4x3_add2::IfcObjectPlacement* parentPlacement,
                                     Ifc4x3_add2::IfcOwnerHistory* ownerHist,
                                     Ifc4x3_add2::IfcGeometricRepresentationContext* context,
                                     int& exportedCount,
                                     std::map<std::string, Ifc4x3_add2::IfcProductDefinitionShape *>& protoCache)
{
    Handle(TDocStd_Document) doc = TDocStd_Document::Get(label);
    if (doc.IsNull()) return;
    Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
    Handle(XCAFDoc_LayerTool) layerTool = XCAFDoc_DocumentTool::LayerTool(doc->Main());
    Handle(XCAFDoc_ColorTool) colorTool = XCAFDoc_DocumentTool::ColorTool(doc->Main());
    Handle(XCAFDoc_VisMaterialTool) matTool = XCAFDoc_DocumentTool::VisMaterialTool(doc->Main());

    // 1. 判定是否为实例（Component）或自由几何，提取其原型和相对坐标
    TDF_Label protoLabel;
    gp_Trsf localTrsf;
    if (shapeTool->IsComponent(label)) {
        if (!shapeTool->GetReferredShape(label, protoLabel)) return;
        localTrsf = shapeTool->GetLocation(label).Transformation();
    } else {
        protoLabel = label;
        localTrsf = shapeTool->GetLocation(label).Transformation();
    }

    // 2. 提取名称
    std::string name = "Unnamed Component";
    Handle(TDataStd_Name) nameAttr;
    if (label.FindAttribute(TDataStd_Name::GetID(), nameAttr)) {
        name = ToStdString(nameAttr->Get());
    }

    // 3. 提取类型 (通过图层名称前缀)
    std::string type = "Proxy";
    auto layerNames = layerTool->GetLayers(label);
    if (!layerNames.IsNull() && layerNames->Length() > 0) {
        std::string layerStr = ToStdString(layerNames->Value(1));
        if (layerStr.size() > 6 && layerStr.substr(layerStr.size() - 6) == "_Layer") {
            type = layerStr.substr(0, layerStr.size() - 6);
        }
    }

    // 如果原型是一个装配体，我们在这里递归展开其所有子节点并直接路由空间关系
    if (shapeTool->IsAssembly(protoLabel)) {
        // 创建装配体在三维空间中的 Local Placement 节点
        auto origin = CreateEntity<Ifc4x3_add2::IfcCartesianPoint>(file);
        gp_Pnt pnt = localTrsf.TranslationPart();
        origin->setCoordinates(std::vector<double>{pnt.X(), pnt.Y(), pnt.Z()});

        gp_Dir transformedX(1.0, 0.0, 0.0);
        transformedX.Transform(localTrsf);
        gp_Dir transformedZ(0.0, 0.0, 1.0);
        transformedZ.Transform(localTrsf);

        auto dirZ = CreateEntity<Ifc4x3_add2::IfcDirection>(file);
        dirZ->setDirectionRatios(std::vector<double>{transformedZ.X(), transformedZ.Y(), transformedZ.Z()});
        auto dirX = CreateEntity<Ifc4x3_add2::IfcDirection>(file);
        dirX->setDirectionRatios(std::vector<double>{transformedX.X(), transformedX.Y(), transformedX.Z()});

        auto axis2 = CreateEntity<Ifc4x3_add2::IfcAxis2Placement3D>(file);
        axis2->setLocation(origin);
        axis2->setAxis(dirZ);
        axis2->setRefDirection(dirX);

        auto localPlacement = CreateEntity<Ifc4x3_add2::IfcLocalPlacement>(file);
        localPlacement->setPlacementRelTo(parentPlacement);
        localPlacement->setRelativePlacement(axis2);

        // 如果该装配体有对应名称（且类型属于桥梁等大空间节点），可按需创建 spatial 关联；
        // 否则直接递归把其下子构件打平或嵌套挂载在当前 localPlacement 坐标下
        TDF_LabelSequence subComponents;
        shapeTool->GetComponents(protoLabel, subComponents);
        for (int i = 1; i <= subComponents.Length(); ++i) {
            ExportXcafLabel(subComponents.Value(i), file, parentIfc, spatialContainer, localPlacement,
                            ownerHist, context, exportedCount, protoCache);
        }
        return;
    }

    // 4. 创建 Local Placement
    auto origin = CreateEntity<Ifc4x3_add2::IfcCartesianPoint>(file);
    gp_Pnt pnt = localTrsf.TranslationPart();
    origin->setCoordinates(std::vector<double>{pnt.X(), pnt.Y(), pnt.Z()});

    gp_Dir transformedX(1.0, 0.0, 0.0);
    transformedX.Transform(localTrsf);
    gp_Dir transformedZ(0.0, 0.0, 1.0);
    transformedZ.Transform(localTrsf);

    auto dirZ = CreateEntity<Ifc4x3_add2::IfcDirection>(file);
    dirZ->setDirectionRatios(std::vector<double>{transformedZ.X(), transformedZ.Y(), transformedZ.Z()});
    auto dirX = CreateEntity<Ifc4x3_add2::IfcDirection>(file);
    dirX->setDirectionRatios(std::vector<double>{transformedX.X(), transformedX.Y(), transformedX.Z()});

    auto axis2 = CreateEntity<Ifc4x3_add2::IfcAxis2Placement3D>(file);
    axis2->setLocation(origin);
    axis2->setAxis(dirZ);
    axis2->setRefDirection(dirX);

    auto localPlacement = CreateEntity<Ifc4x3_add2::IfcLocalPlacement>(file);
    localPlacement->setPlacementRelTo(parentPlacement);
    localPlacement->setRelativePlacement(axis2);

    // 5. 创建 IFC Product 实体
    std::string guid = GenerateIfcGuid();
    Ifc4x3_add2::IfcProduct *product = nullptr;

    if (type == "Footing" || type == "BridgeFoundation" || type == "Foundation") {
        auto obj = CreateEntity<Ifc4x3_add2::IfcFooting>(file);
        obj->setGlobalId(guid);
        obj->setOwnerHistory(ownerHist);
        obj->setName(name);
        obj->setObjectType(type);
        obj->setObjectPlacement(localPlacement);
        product = obj;
    } else if (type == "Girder" || type == "Beam") {
        auto obj = CreateEntity<Ifc4x3_add2::IfcBeam>(file);
        obj->setGlobalId(guid);
        obj->setOwnerHistory(ownerHist);
        obj->setName(name);
        obj->setObjectType(type);
        obj->setObjectPlacement(localPlacement);
        product = obj;
    } else if (type == "Pier" || type == "Column") {
        auto obj = CreateEntity<Ifc4x3_add2::IfcColumn>(file);
        obj->setGlobalId(guid);
        obj->setOwnerHistory(ownerHist);
        obj->setName(name);
        obj->setObjectType(type);
        obj->setObjectPlacement(localPlacement);
        product = obj;
    } else {
        auto proxy = CreateEntity<Ifc4x3_add2::IfcBuildingElementProxy>(file);
        proxy->setGlobalId(guid);
        proxy->setOwnerHistory(ownerHist);
        proxy->setName(name);
        proxy->setObjectType(type);
        proxy->setObjectPlacement(localPlacement);
        product = proxy;
    }

    exportedCount++;

    // 6. 添加几何表示（零件级块实例复用）
    TCollection_AsciiString protoEntry;
    TDF_Tool::Entry(protoLabel, protoEntry);
    std::string protoKey = protoEntry.ToCString();

    Ifc4x3_add2::IfcProductDefinitionShape *pds = nullptr;
    if (protoCache.find(protoKey) != protoCache.end()) {
        pds = protoCache[protoKey];
    } else {
        TopoDS_Shape shape;
        if (shapeTool->GetShape(protoLabel, shape) && !shape.IsNull()) {
            try {
                const IfcParse::schema_definition *schema = &Ifc4x3_add2::get_schema();
                IfcParse::register_schema(const_cast<IfcParse::schema_definition *>(schema));

                IfcUtil::IfcBaseClass *serialized = nullptr;
                try {
                    serialized = IfcGeom::serialise("IFC4X3_ADD2", shape, false);
                } catch (...) {}

                if (!serialized) {
                    try {
                        serialized = IfcGeom::tesselate("IFC4X3_ADD2", shape, 2.0);
                    } catch (...) {}
                }

                if (serialized) {
                    file.addEntity(serialized);
                    if (serialized->declaration().is("IfcProductDefinitionShape")) {
                        pds = (Ifc4x3_add2::IfcProductDefinitionShape *)serialized;
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
                                            if (*it2) file.addEntity(*it2);
                                        }
                                    }
                                }
                            }
                        }
                        protoCache[protoKey] = pds;
                    }
                }
            } catch (...) {}
        }
    }

    if (pds) {
        product->setRepresentation(pds);
    }

    // 7. 处理图层导出
    if (!layerNames.IsNull() && layerNames->Length() > 0) {
        std::string layerName = ToStdString(layerNames->Value(1));
        auto ifcLayer = CreateEntity<Ifc4x3_add2::IfcPresentationLayerAssignment>(file);
        ifcLayer->setName(layerName);

        auto layerLayeredItems = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcLayeredItem>>();
        if (pds && pds->Representations()) {
            for (auto rep : *pds->Representations()) {
                layerLayeredItems->push(rep);
            }
        }
        ifcLayer->setAssignedItems(layerLayeredItems);
    }

    // 8. 颜色材质分配
    Quantity_Color rgbColor;
    double alpha = 1.0;
    bool hasMaterialColor = false;

    Handle(XCAFDoc_VisMaterial) visMat = matTool->GetShapeMaterial(protoLabel);
    if (!visMat.IsNull()) {
        Quantity_ColorRGBA rgba = visMat->BaseColor();
        rgbColor = rgba.GetRGB();
        alpha = rgba.Alpha();
        hasMaterialColor = true;
    } else {
        Quantity_Color quantityColor;
        if (colorTool->GetColor(protoLabel, XCAFDoc_ColorGen, quantityColor)) {
            rgbColor = quantityColor;
            hasMaterialColor = true;
        }
    }

    if (hasMaterialColor) {
        auto ifcColor = CreateEntity<Ifc4x3_add2::IfcColourRgb>(file);
        ifcColor->setRed(rgbColor.Red());
        ifcColor->setGreen(rgbColor.Green());
        ifcColor->setBlue(rgbColor.Blue());

        auto styleSpec = CreateEntity<Ifc4x3_add2::IfcSurfaceStyleRendering>(file);
        styleSpec->setSurfaceColour(ifcColor);
        if (alpha < 0.99) {
            styleSpec->setTransparency(1.0 - alpha);
        }

        auto styleStyles = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcSurfaceStyleElementSelect>>();
        styleStyles->push(styleSpec);

        auto surfStyle = CreateEntity<Ifc4x3_add2::IfcSurfaceStyle>(file);
        surfStyle->setName(name + "_Style");
        surfStyle->setSide(Ifc4x3_add2::IfcSurfaceSide::IfcSurfaceSide_BOTH);
        surfStyle->setStyles(styleStyles);

        auto styledItemStyles = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcPresentationStyle>>();
        styledItemStyles->push(surfStyle);

        auto styledItem = CreateEntity<Ifc4x3_add2::IfcStyledItem>(file);
        styledItem->setStyles(styledItemStyles);

        if (pds && pds->Representations() && pds->Representations()->size() > 0) {
            auto rep = *pds->Representations()->begin();
            if (rep && rep->Items() && rep->Items()->size() > 0) {
                auto item = *rep->Items()->begin();
                styledItem->setItem(item);
            }
        }
    }

    // 9. 空间树关系分配
    auto childSet = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcObjectDefinition>>();
    childSet->push(product);

    if (!product->declaration().is("IfcSpatialElement")) {
        auto productSet = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcProduct>>();
        productSet->push(product);

        auto relContained = CreateEntity<Ifc4x3_add2::IfcRelContainedInSpatialStructure>(file);
        relContained->setGlobalId(GenerateIfcGuid());
        relContained->setOwnerHistory(ownerHist);
        relContained->setRelatedElements(productSet);
        relContained->setRelatingStructure(spatialContainer);
    }

    auto relAgg = CreateEntity<Ifc4x3_add2::IfcRelAggregates>(file);
    relAgg->setGlobalId(GenerateIfcGuid());
    relAgg->setOwnerHistory(ownerHist);
    relAgg->setRelatingObject(parentIfc);
    relAgg->setRelatedObjects(childSet);
}

Ifc4x3_add2::IfcProduct *
IfcExportService::CreateIfcProduct(const Handle(BrNode_adObject) & adObj,
                                   IfcParse::IfcFile &file,
                                   Ifc4x3_add2::IfcOwnerHistory *ownerHist,
                                   Ifc4x3_add2::IfcLocalPlacement *placement) {
  std::string type = ToStdString(adObj->GetObjectType());
  std::string name = ToStdString(adObj->GetName());
  std::string guid = GenerateIfcGuid();

  if (type == "Footing" || type == "BridgeFoundation" || type == "Foundation") {
    auto obj = CreateEntity<Ifc4x3_add2::IfcFooting>(file);
    obj->setGlobalId(guid);
    obj->setOwnerHistory(ownerHist);
    obj->setName(name);
    obj->setObjectType(type);
    obj->setObjectPlacement(placement);
    return obj;
  } else if (type == "Girder" || type == "Beam") {
    auto obj = CreateEntity<Ifc4x3_add2::IfcBeam>(file);
    obj->setGlobalId(guid);
    obj->setOwnerHistory(ownerHist);
    obj->setName(name);
    obj->setObjectType(type);
    obj->setObjectPlacement(placement);
    return obj;
  } else if (type == "Pier" || type == "Column" || type == "BridgePier") {
    auto obj = CreateEntity<Ifc4x3_add2::IfcColumn>(file);
    obj->setGlobalId(guid);
    obj->setOwnerHistory(ownerHist);
    obj->setName(name);
    obj->setObjectType(type);
    obj->setObjectPlacement(placement);
    return obj;
  } else if (type == "Pile" || type == "SinglePile") {
    auto obj = CreateEntity<Ifc4x3_add2::IfcPile>(file);
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

  TopoDS_Shape shape = geoDef->GetShape();
  if (shape.IsNull())
    return;

  // 1. Precise Geometry Instancing via OpenCASCADE TShape Pointer
  void *tshapePtr = shape.TShape().get();
  if (g_geomInstanceCache.find(tshapePtr) != g_geomInstanceCache.end()) {
    auto cachedRep = g_geomInstanceCache[tshapePtr];
    if (cachedRep) {
      product->setRepresentation(cachedRep);
      return; // Perfect zero-cost geometrical instancing share
    }
  }

  try {
    const IfcParse::schema_definition *schema = &Ifc4x3_add2::get_schema();
    IfcParse::register_schema(
        const_cast<IfcParse::schema_definition *>(schema));

    IfcUtil::IfcBaseClass *serialized = nullptr;

    // Stable serialization fallback sequence (aligned 100% with
    // brep_to_ifc.cpp)
    try {
      serialized = IfcGeom::serialise("IFC4X3_ADD2", shape, false);
    } catch (...) {
    }

    if (!serialized) {
      try {
        serialized = IfcGeom::tesselate("IFC4X3_ADD2", shape, 2.0);
      } catch (...) {
      }
    }

    if (!serialized)
      return;

    // 2. High-fidelity entity registration and ContextOfItems binding (aligned
    // with brep_to_ifc.cpp)
    file.addEntity(serialized);

    if (serialized->declaration().is("IfcProductDefinitionShape")) {
      auto pds = (Ifc4x3_add2::IfcProductDefinitionShape *)serialized;
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
                for (auto it2 = repItems->begin(); it2 != repItems->end();
                     ++it2) {
                  try {
                    if (*it2) {
                      file.addEntity(*it2);
                    }
                  } catch (...) {
                  }
                }
              }
            }
          } catch (...) {
          }
        }
      }

      // Directly assign the complete serialized representation shape!
      product->setRepresentation(pds);

      // Cache successful representation for instancing via its unique TShape
      // address
      g_geomInstanceCache[tshapePtr] = pds;
    }
  } catch (...) {
  }
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

  // 1. Calculate Local Placement (including precise Euler rotations rx, ry, rz)
  auto origin = CreateEntity<Ifc4x3_add2::IfcCartesianPoint>(file);
  origin->setCoordinates(std::vector<double>{0.0, 0.0, 0.0});

  gp_Dir dirZDir(0.0, 0.0, 1.0);
  gp_Dir dirXDir(1.0, 0.0, 0.0);

  Handle(ActAPI_IUserParameter) p =
      adObj->Parameter(BrNode_adObject::PID_ObjectPlacement);
  Handle(ActData_RealArrayParameter) typedP =
      ActData_ParameterFactory::AsRealArray(p);
  if (!typedP.IsNull() && typedP->NbElements() >= 3) {
    origin->setCoordinates(std::vector<double>{
        typedP->GetElement(0), typedP->GetElement(1), typedP->GetElement(2)});

    if (typedP->NbElements() >= 6) {
      double rx = typedP->GetElement(3);
      double ry = typedP->GetElement(4);
      double rz = typedP->GetElement(5);

      gp_Trsf rot;
      if (std::abs(rz) > 1e-6) {
        gp_Trsf r;
        r.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)),
                      rz * 3.14159265358979323846 / 180.0);
        rot.Multiply(r);
      }
      if (std::abs(ry) > 1e-6) {
        gp_Trsf r;
        r.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(0, 1, 0)),
                      ry * 3.14159265358979323846 / 180.0);
        rot.Multiply(r);
      }
      if (std::abs(rx) > 1e-6) {
        gp_Trsf r;
        r.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(1, 0, 0)),
                      rx * 3.14159265358979323846 / 180.0);
        rot.Multiply(r);
      }

      gp_Dir transformedX(1.0, 0.0, 0.0);
      transformedX.Transform(rot);
      gp_Dir transformedZ(0.0, 0.0, 1.0);
      transformedZ.Transform(rot);

      dirXDir = transformedX;
      dirZDir = transformedZ;
    }
  }

  auto dirZ = CreateEntity<Ifc4x3_add2::IfcDirection>(file);
  dirZ->setDirectionRatios(
      std::vector<double>{dirZDir.X(), dirZDir.Y(), dirZDir.Z()});

  auto dirX = CreateEntity<Ifc4x3_add2::IfcDirection>(file);
  dirX->setDirectionRatios(
      std::vector<double>{dirXDir.X(), dirXDir.Y(), dirXDir.Z()});

  auto axis2 = CreateEntity<Ifc4x3_add2::IfcAxis2Placement3D>(file);
  axis2->setLocation(origin);
  axis2->setAxis(dirZ);
  axis2->setRefDirection(dirX);

  auto localPlacement = CreateEntity<Ifc4x3_add2::IfcLocalPlacement>(file);
  localPlacement->setPlacementRelTo(parentPlacement);
  localPlacement->setRelativePlacement(axis2);

  // 2. Create IFC Product
  Ifc4x3_add2::IfcProduct *product =
      CreateIfcProduct(adObj, file, ownerHist, localPlacement);

  exportedCount++;

  // 3. Add Geometry
  AddGeometryToProduct(adObj, file, product, context);

  // 3.1. Layer and Color/Material Assignment
  auto pds = (Ifc4x3_add2::IfcProductDefinitionShape*)product->Representation();
  
  // Assign presentation layer
  std::string modelType = ToStdString(adObj->GetObjectType());
  if (!modelType.empty()) {
      std::string layerName = modelType + "_Layer";
      auto ifcLayer = CreateEntity<Ifc4x3_add2::IfcPresentationLayerAssignment>(file);
      ifcLayer->setName(layerName);

      auto layerLayeredItems = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcLayeredItem>>();
      if (pds && pds->Representations()) {
          for (auto rep : *pds->Representations()) {
              layerLayeredItems->push(rep);
          }
      }
      ifcLayer->setAssignedItems(layerLayeredItems);
  }

  // Parse color and material properties
  Quantity_Color rgbColor;
  double alpha = 1.0;
  bool hasMaterialColor = false;

  NCollection_Sequence<Handle(BrNode_adPropertySet)> psets = adObj->GetPropertySetsList();
  for (int i = 1; i <= psets.Length(); ++i) {
      Handle(BrNode_adPropertySet) ps = psets.Value(i);
      if (ps.IsNull()) continue;
      std::string psName = ToStdString(ps->GetName());
      if (psName == "Pset_MaterialPBR") {
          NCollection_Sequence<Handle(BrNode_adProperty)> props = ps->GetPropertiesList();
          for (int j = 1; j <= props.Length(); ++j) {
              Handle(BrNode_adProperty) p = props.Value(j);
              if (p.IsNull()) continue;
              std::string key = ToStdString(p->GetPropertyName());
              std::string val = ToStdString(p->GetPropertyValue());
              if (key == "BaseColor") {
                  std::stringstream ss(val);
                  std::string segment;
                  std::vector<double> colors;
                  while (std::getline(ss, segment, ',')) {
                      try {
                          colors.push_back(std::stod(segment));
                      } catch (...) {}
                  }
                  if (colors.size() >= 3) {
                      rgbColor.SetValues(colors[0], colors[1], colors[2], Quantity_TOC_RGB);
                      hasMaterialColor = true;
                  }
              } else if (key == "Transparency") {
                  try {
                      alpha = 1.0 - std::stod(val);
                  } catch (...) {}
              }
          }
      }
  }

  if (hasMaterialColor) {
      auto ifcColor = CreateEntity<Ifc4x3_add2::IfcColourRgb>(file);
      ifcColor->setRed(rgbColor.Red());
      ifcColor->setGreen(rgbColor.Green());
      ifcColor->setBlue(rgbColor.Blue());

      auto styleSpec = CreateEntity<Ifc4x3_add2::IfcSurfaceStyleRendering>(file);
      styleSpec->setSurfaceColour(ifcColor);
      if (alpha < 0.99) {
          styleSpec->setTransparency(1.0 - alpha);
      }

      auto styleStyles = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcSurfaceStyleElementSelect>>();
      styleStyles->push(styleSpec);

      auto surfStyle = CreateEntity<Ifc4x3_add2::IfcSurfaceStyle>(file);
      surfStyle->setName(ToStdString(adObj->GetName()) + "_Style");
      surfStyle->setSide(Ifc4x3_add2::IfcSurfaceSide::IfcSurfaceSide_BOTH);
      surfStyle->setStyles(styleStyles);

      auto styledItemStyles = boost::make_shared<aggregate_of<Ifc4x3_add2::IfcPresentationStyle>>();
      styledItemStyles->push(surfStyle);

      auto styledItem = CreateEntity<Ifc4x3_add2::IfcStyledItem>(file);
      styledItem->setStyles(styledItemStyles);

      if (pds && pds->Representations() && pds->Representations()->size() > 0) {
          auto rep = *pds->Representations()->begin();
          if (rep && rep->Items() && rep->Items()->size() > 0) {
              auto item = *rep->Items()->begin();
              styledItem->setItem(item);
          }
      }
  }

  // 4. Link to Parent (Spatial Aggregation vs Contained in Spatial Structure)
  auto childSet =
      boost::make_shared<aggregate_of<Ifc4x3_add2::IfcObjectDefinition>>();
  childSet->push(product);

  // 4. Link to Parent & Spatial Container
  // 核心修复：仅当当前产品不是空间元素，且它的父级是空间元素（即它是顶级产品）时，才直接包含在空间容器中。
  // 子级构件通过聚合嵌套关系隐式包含，不应重复直接关联空间容器，否则会导致 Viewer 将其拉平按类型分类。
  if (!product->declaration().is("IfcSpatialElement") && parentIfc->declaration().is("IfcSpatialElement")) {
    auto productSet =
        boost::make_shared<aggregate_of<Ifc4x3_add2::IfcProduct>>();
    productSet->push(product);

    auto relContained =
        CreateEntity<Ifc4x3_add2::IfcRelContainedInSpatialStructure>(file);
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
  Ifc4x3_add2::IfcSpatialElement *nextSpatial = spatialContainer;
  if (product->declaration().is("IfcSpatialElement")) {
    nextSpatial = (Ifc4x3_add2::IfcSpatialElement *)product;
  }

  NCollection_Sequence<Handle(BrNode_adObject)> children =
      adObj->GetSubObjectsList();
  for (int i = 1; i <= children.Length(); ++i) {
    TraverseAndExport(children.Value(i), file, product, nextSpatial,
                      localPlacement, ownerHist, context, exportedCount);
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
    ownerHist->setChangeAction(
        Ifc4x3_add2::IfcChangeActionEnum::IfcChangeAction_NOCHANGE);
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

    auto context =
        CreateEntity<Ifc4x3_add2::IfcGeometricRepresentationContext>(file);
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
    site->setCompositionType(
        Ifc4x3_add2::IfcElementCompositionEnum::IfcElementComposition_ELEMENT);

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
    building->setCompositionType(
        Ifc4x3_add2::IfcElementCompositionEnum::IfcElementComposition_ELEMENT);

    auto buildingSet =
        boost::make_shared<aggregate_of<Ifc4x3_add2::IfcObjectDefinition>>();
    buildingSet->push(building);
    auto relSiteBuilding = CreateEntity<Ifc4x3_add2::IfcRelAggregates>(file);
    relSiteBuilding->setGlobalId(GenerateIfcGuid());
    relSiteBuilding->setOwnerHistory(ownerHist);
    relSiteBuilding->setRelatingObject(site);
    relSiteBuilding->setRelatedObjects(buildingSet);

    // Traverse ActiveData adObject tree directly
    int exportedCount = 0;
    Handle(ActAPI_INode) rootBase = model->GetRootNode();
    if (!rootBase.IsNull()) {
      Handle(ActAPI_IChildIterator) it = rootBase->GetChildIterator();
      for (; it->More(); it->Next()) {
        Handle(BrNode_adObject) obj = Handle(BrNode_adObject)::DownCast(it->Value());
        if (!obj.IsNull()) {
          TraverseAndExport(obj, file, building, building, buildingPlacement,
                            ownerHist, context, exportedCount);
        }
      }
    }
    std::cout << "[IfcExportService] adObject tree exported " << exportedCount
              << " nodes. (VERSION: 2026-06-05-ADOBJECT-DRIVEN)" << std::endl;

    std::stringstream ss;
    ss << file;
    std::string content = ss.str();

    // Stream rewrite schema to IFC4 for general compatibility
    size_t pos = content.find("FILE_SCHEMA(('IFC4X3_ADD2'));");
    if (pos != std::string::npos) {
      content.replace(pos, 29, "FILE_SCHEMA(('IFC4'));");
    }

    ReplaceAll(content, "\\\\X2\\\\", "\\X2\\");
    ReplaceAll(content, "\\\\X0\\\\", "\\X0\\");

    std::ofstream f(filename);
    if (f.is_open()) {
      f << content;
      f.close();
      std::cout << "[IfcExportService] File saved: " << filename << std::endl;
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
