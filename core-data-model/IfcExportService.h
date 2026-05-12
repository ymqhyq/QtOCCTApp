#ifndef IFCEXPORTSERVICE_H
#define IFCEXPORTSERVICE_H

#include <string>
#include "BrNode_adObject.h"
#include "DataModel.h"

#define HAS_SCHEMA_4x3_add2
#include <ifcparse/Ifc4x3_add2.h>
#include <ifcparse/IfcFile.h>

class IfcExportService {
public:
    static bool Export(const Handle(DataModel)& model, const std::string& filename);

private:
    static void TraverseAndExport(const Handle(BrNode_adObject)& adObj, 
                                 IfcParse::IfcFile& file,
                                 Ifc4x3_add2::IfcObjectDefinition* parentIfc,
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
