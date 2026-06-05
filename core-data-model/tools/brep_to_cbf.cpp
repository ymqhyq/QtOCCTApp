#include <TDocStd_Application.hxx>
#include <TDocStd_Document.hxx>
#include <BinXCAFDrivers.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Shape.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_VisMaterialTool.hxx>
#include <XCAFDoc_VisMaterial.hxx>
#include <XCAFDoc_VisMaterialPBR.hxx>
#include <TDataStd_AsciiString.hxx>
#include <Quantity_Color.hxx>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Usage: brep_to_cbf <input_brep_path> <output_cbf_path> <param_geo_id> [r] [g] [b] [metallic] [roughness] [ior]" << std::endl;
        return 1;
    }

    std::string brepPath = argv[1];
    std::string cbfPath = argv[2];
    std::string paramGeoId = argv[3];

    double r = -1.0, g = -1.0, b = -1.0;
    double metallic = 0.0, roughness = 0.8, ior = 1.5;
    bool hasPbr = false;

    if (argc >= 7) {
        try {
            r = std::stod(argv[4]);
            g = std::stod(argv[5]);
            b = std::stod(argv[6]);
        } catch (...) {
            r = -1.0;
        }
    }

    if (argc >= 10) {
        try {
            metallic = std::stod(argv[7]);
            roughness = std::stod(argv[8]);
            ior = std::stod(argv[9]);
            hasPbr = true;
        } catch (...) {
            hasPbr = false;
        }
    }

    // 1. 读取 BREP 几何
    TopoDS_Shape shape;
    BRep_Builder builder;
    if (!BRepTools::Read(shape, brepPath.c_str(), builder)) {
        std::cerr << "Failed to read BREP file: " << brepPath << std::endl;
        return 1;
    }

    // 2. 创建 OCAF Application 并定义 BinXCAF 格式
    Handle(TDocStd_Application) app = new TDocStd_Application();
    BinXCAFDrivers::DefineFormat(app);

    // 3. 创建文档
    Handle(TDocStd_Document) doc;
    app->NewDocument("BinXCAF", doc);
    if (doc.IsNull()) {
        std::cerr << "Failed to create new OCAF Document" << std::endl;
        return 1;
    }

    // 4. 初始化 XCAF Tools 并开启事务
    doc->NewCommand();
    Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
    Handle(XCAFDoc_ColorTool) colorTool = XCAFDoc_DocumentTool::ColorTool(doc->Main());
    Handle(XCAFDoc_VisMaterialTool) visMatTool = XCAFDoc_DocumentTool::VisMaterialTool(doc->Main());

    // 5. 注册 Shape 到 XCAF 树（直接作为单体零件挂载，防止被AddShape自动分解为悬空引用）
    TDF_Label protoLabel = shapeTool->NewShape();
    shapeTool->SetShape(protoLabel, shape);

    // 6. 附加 ParamGeoID 属性作为主键
    TDataStd_AsciiString::Set(protoLabel, paramGeoId.c_str());

    // 7. 设置颜色与 PBR 材质
    if (r >= 0.0 && r <= 1.0 && g >= 0.0 && g <= 1.0 && b >= 0.0 && b <= 1.0) {
        Quantity_Color col(r, g, b, Quantity_TOC_RGB);
        colorTool->SetColor(protoLabel, col, XCAFDoc_ColorGen);

        if (hasPbr && !visMatTool.IsNull()) {
            XCAFDoc_VisMaterialPBR pbrMat;
            pbrMat.BaseColor.SetValues(r, g, b, 1.0);
            pbrMat.Metallic = (float)metallic;
            pbrMat.Roughness = (float)roughness;
            pbrMat.RefractionIndex = (float)ior;
            pbrMat.IsDefined = true;

            Handle(XCAFDoc_VisMaterial) visMat = new XCAFDoc_VisMaterial();
            visMat->SetPbrMaterial(pbrMat);

            std::string matName = paramGeoId + "_Material";
            TDF_Label matLabel = visMatTool->AddMaterial(visMat, matName.c_str());
            visMatTool->SetShapeMaterial(protoLabel, matLabel);
        }
    }

    doc->CommitCommand();

    // 8. 序列化保存文档到 CBF 文件
    PCDM_StoreStatus status = app->SaveAs(doc, cbfPath.c_str());
    if (status != PCDM_SS_OK) {
        std::cerr << "Failed to save CBF file: " << cbfPath << " status: " << status << std::endl;
        return 1;
    }

    std::cout << "Successfully generated CBF: " << cbfPath << std::endl;
    return 0;
}
