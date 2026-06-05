#include <iostream>
#include <fstream>
#include <string>

// OCCT & ActiveData
#include <TDocStd_Application.hxx>
#include <TDocStd_Document.hxx>
#include <BinXCAFDrivers.hxx>

// core-data-model
#include <DataModel.h>
#include <IfcExportService.h>

// IfcOpenShell
#include <ifcparse/IfcSchema.h>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: asicbf_to_ifc <input.asi.cbf> <output.ifc>" << std::endl;
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = argv[2];

    std::cout << "=== ActiveData CBF to IFC Converter ===" << std::endl;
    std::cout << "[INFO] Input file: " << inputPath << std::endl;
    std::cout << "[INFO] Output file: " << outputPath << std::endl;

    // 1. Initialize Application and Register BinXCAF Format
    Handle(TDocStd_Application) app = new TDocStd_Application();
    BinXCAFDrivers::DefineFormat(app);

    // 2. Load DataModel from .asi.cbf
    Handle(DataModel) model = new DataModel();
    if (!model->Open(inputPath.c_str())) {
        std::cerr << "[ERROR] Failed to open input CBF file: " << inputPath << std::endl;
        return 1;
    }
    std::cout << "[OK] Successfully opened input CBF file." << std::endl;

    // 3. Register Schema (must be done in main thread / entry point)
    const IfcParse::schema_definition* schema = IfcParse::schema_by_name("Ifc4x3_add2");
    if (schema) {
        IfcParse::register_schema(const_cast<IfcParse::schema_definition*>(schema));
    } else {
        std::cerr << "[ERROR] Schema Ifc4x3_add2 not found." << std::endl;
        return 1;
    }
    std::cout << "[OK] Registered schema: " << schema->name() << std::endl;

    // 4. Export to IFC
    std::cout << "[INFO] Exporting to IFC..." << std::endl;
    if (IfcExportService::Export(model, outputPath)) {
        std::cout << "[SUCCESS] IFC file saved to: " << outputPath << std::endl;
    } else {
        std::cerr << "[ERROR] IFC export failed." << std::endl;
        return 1;
    }

    return 0;
}
