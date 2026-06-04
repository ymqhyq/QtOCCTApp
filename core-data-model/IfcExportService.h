#ifndef IFCEXPORTSERVICE_H
#define IFCEXPORTSERVICE_H

#include <string>
#include "BrNode_adObject.h"
#include "DataModel.h"

#define HAS_SCHEMA_4x3_add2
#include <ifcparse/Ifc4x3_add2.h>
#include <ifcparse/IfcFile.h>
#include <TopoDS_Shape.hxx>
#include <TDF_Label.hxx>
#include <map>

class IfcExportService {
public:
    // Safe Entity Creation Helper to properly route through IfcOpenShell's storage allocator
    // and naturally bypass the ID=0 bypass bug by forcibly pushing the entity registration.
    template <typename T>
    static T* CreateEntity(IfcParse::IfcFile& file) {
        auto* inst = file.create<T>();
        if (inst) {
            inst->file_ = nullptr; 
            file.addEntity(inst);  
        }
        return inst;
    }

    static bool Export(const Handle(DataModel)& model, const std::string& filename);
    static bool ExportShapeToFile(const TopoDS_Shape& shape, const std::string& filename);

private:
    static void ExportXcafComponent(const TDF_Label& instLabel,
                                    IfcParse::IfcFile& file,
                                    Ifc4x3_add2::IfcObjectDefinition* parentIfc,
                                    Ifc4x3_add2::IfcSpatialElement* spatialContainer,
                                    Ifc4x3_add2::IfcObjectPlacement* parentPlacement,
                                    Ifc4x3_add2::IfcOwnerHistory* ownerHist,
                                    Ifc4x3_add2::IfcGeometricRepresentationContext* context,
                                    int& exportedCount,
                                    std::map<std::string, Ifc4x3_add2::IfcProductDefinitionShape *>& protoCache);

    static void TraverseAndExport(const Handle(BrNode_adObject)& adObj, 
                                 IfcParse::IfcFile& file,
                                 Ifc4x3_add2::IfcObjectDefinition* parentIfc,
                                 Ifc4x3_add2::IfcSpatialElement* spatialContainer,
                                 Ifc4x3_add2::IfcObjectPlacement* parentPlacement,
                                 Ifc4x3_add2::IfcOwnerHistory* ownerHist,
                                 Ifc4x3_add2::IfcGeometricRepresentationContext* context,
                                 int& exportedCount);

    static void AddGeometryToProduct(const Handle(BrNode_adObject)& adObj,
                                     IfcParse::IfcFile& file,
                                     Ifc4x3_add2::IfcProduct* product,
                                     Ifc4x3_add2::IfcGeometricRepresentationContext* context);

    static Ifc4x3_add2::IfcProduct* CreateIfcProduct(const Handle(BrNode_adObject)& adObj,
                                                    IfcParse::IfcFile& file,
                                                    Ifc4x3_add2::IfcOwnerHistory* ownerHist,
                                                    Ifc4x3_add2::IfcLocalPlacement* placement);

    static std::string ToStdString(const TCollection_ExtendedString& extStr);
};

#endif // IFCEXPORTSERVICE_H
