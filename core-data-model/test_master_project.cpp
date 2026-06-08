#include <iostream>
#include <fstream>
#include <cassert>
#include <filesystem>

// OCCT & XCAF
#include <TDocStd_Document.hxx>
#include <TDocStd_Application.hxx>
#include <BinXCAFDrivers.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFDoc_LayerTool.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Compound.hxx>
#include <TopExp_Explorer.hxx>
#include <BRep_Tool.hxx>
#include <gp_Pnt.hxx>
#include <Quantity_Color.hxx>
#include <TDF_LabelSequence.hxx>

// Project Classes
#include "ProjectManager.h"
#include "RwSlope2DGeometryBuilder.h"
#include "generated/DataFactory.h"
#include "generated/BrNode_adDrawing2D.h"
#include "generated/BrNode_adSlopeIndication.h"
#include "generated/BrNode_adObject.h"
#include "generated/BrNode_adModelRoot.h"

// -----------------------------------------------------------------------------
// GTest-like Unit Test Macro Definitions
// -----------------------------------------------------------------------------
#define TEST(TestSuite, TestName) void TestSuite##_##TestName()
#define RUN_TEST(TestSuite, TestName) do { \
    std::cout << "[ RUN      ] " << #TestSuite << "." << #TestName << std::endl; \
    TestSuite##_##TestName(); \
    std::cout << "[       OK ] " << #TestSuite << "." << #TestName << std::endl; \
} while(0)

#define EXPECT_EQ(val1, val2) do { \
    if ((val1) != (val2)) { \
        std::cerr << "[  FAILED  ] EXPECT_EQ(" << #val1 << ", " << #val2 << ") failed!" \
                  << " Line: " << __LINE__ << " Expected: " << (val2) << " Actual: " << (val1) << std::endl; \
        exit(1); \
    } \
} while(0)

#define ASSERT_TRUE(condition) do { \
    if (!(condition)) { \
        std::cerr << "[  FAILED  ] ASSERT_TRUE(" << #condition << ") failed! Line: " << __LINE__ << std::endl; \
        exit(1); \
    } \
} while(0)

// -----------------------------------------------------------------------------
// Helper: Create parallel lines (shoulder and toe) for slope
// -----------------------------------------------------------------------------
struct SlopeWires {
    TopoDS_Wire shoulder;
    TopoDS_Wire toe;
};

SlopeWires CreateSlopeWires(double length, double height, double slopeRatio) {
    SlopeWires wires;

    // 1. Create shoulder feature line (high = height, length = length)
    gp_Pnt sStart(0.0, 0.0, height);
    gp_Pnt sEnd(length, 0.0, height);
    TopoDS_Edge sEdge = BRepBuilderAPI_MakeEdge(sStart, sEnd).Edge();
    wires.shoulder = BRepBuilderAPI_MakeWire(sEdge).Wire();

    // 2. Create toe feature line (translate Y by height * slopeRatio)
    double toeY = height * slopeRatio;
    gp_Pnt tStart(0.0, toeY, 0.0);
    gp_Pnt tEnd(length, toeY, 0.0);
    TopoDS_Edge tEdge = BRepBuilderAPI_MakeEdge(tStart, tEnd).Edge();
    wires.toe = BRepBuilderAPI_MakeWire(tEdge).Wire();

    return wires;
}

// -----------------------------------------------------------------------------
// Unit Test 1: Verify RwSlope2DGeometryBuilder geometry generation and XDE save
// -----------------------------------------------------------------------------
TEST(SlopeGeometryBuilderTest, GenerateAndSaveSlope)
{
    // 1. Create slope feature lines: length 20m, height 8m, ratio 1:1.5
    double length = 20000.0;    // 20m
    double height = 8000.0;     // 8m
    double slopeRatio = 1.5;    // toe Y = 12m (12000.0)
    
    SlopeWires Wires = CreateSlopeWires(length, height, slopeRatio);

    // 2. Init builder: spacing 2m, long 60%, short 30%
    double spacing = 2000.0;
    Handle(RwSlope2DGeometryBuilder) builder = new RwSlope2DGeometryBuilder(
        Wires.shoulder, 
        Wires.toe, 
        spacing, 
        0.6, 
        0.3
    );

    // 3. Build geometry compound and verify teeth count
    TopoDS_Shape totalShape = builder->Build();
    ASSERT_TRUE(!totalShape.IsNull());

    // Count teeth edges
    int edgeCount = 0;
    TopExp_Explorer exp(builder->GetTeethCompound(), TopAbs_EDGE);
    for (; exp.More(); exp.Next()) {
        edgeCount++;
    }
    // 20m (20000mm) / 2m (2000mm) + 1 = 11 lines
    EXPECT_EQ(edgeCount, 11);

    // 4. Create in-memory XCAF doc and save, verify layer and color
    Handle(TDocStd_Application) app = new TDocStd_Application();
    BinXCAFDrivers::DefineFormat(app);
    Handle(TDocStd_Document) doc;
    app->NewDocument("BinXCAF", doc);
    
    Standard_Boolean saved = builder->SaveToXDE(doc);
    ASSERT_TRUE(saved);

    // Verify layers
    Handle(XCAFDoc_LayerTool) layerTool = XCAFDoc_DocumentTool::LayerTool(doc->Main());
    Handle(XCAFDoc_ColorTool) colorTool = XCAFDoc_DocumentTool::ColorTool(doc->Main());

    TDF_Label shoulderLayerLabel = layerTool->FindLayer("Layer_RoadShoulder");
    TDF_Label toeLayerLabel = layerTool->FindLayer("Layer_SlopeToe_Dashed");
    TDF_Label teethLayerLabel = layerTool->FindLayer("Layer_SlopeTeeth");

    ASSERT_TRUE(!shoulderLayerLabel.IsNull());
    ASSERT_TRUE(!toeLayerLabel.IsNull());
    ASSERT_TRUE(!teethLayerLabel.IsNull());

    // Verify colors
    Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
    TDF_LabelSequence shapes;
    shapeTool->GetShapes(shapes);
    
    // Should have 3 top-level sub shapes (shoulder, toe, teeth)
    EXPECT_EQ(shapes.Length(), 3);
}

// -----------------------------------------------------------------------------
// Unit Test 2: Verify multi-doc lazy sync and 3D/2D linkage
// -----------------------------------------------------------------------------
TEST(ProjectManagerTest, MultiDocLinkageAndSync)
{
    // Clean environment and create test directory
    std::string testProjDir = "./temp_debug";
    std::filesystem::remove_all(testProjDir);
    std::filesystem::create_directories(testProjDir + "/models");
    std::filesystem::create_directories(testProjDir + "/drawings");

    std::string masterPath = testProjDir + "/master.cbf";
    std::string modelPath3D = "models/subgrade_3d.cbf";
    std::string drawingPath2D = "drawings/plan_view.cbf";

    // 1. Init ProjectManager and create/load master doc
    ProjectManager* pm = new ProjectManager();
    Standard_Boolean opened = pm->OpenMasterProject(masterPath);
    ASSERT_TRUE(opened);

    // 2. Load and init 3D model sub doc, write 3D slope
    Handle(TDocStd_Document) modelDoc = pm->GetOrLoadSubDocument(modelPath3D);
    ASSERT_TRUE(!modelDoc.IsNull());
    
    std::cout << "[DEBUG] Creating model3D..." << std::endl;
    Handle(DataModel) model3D = new DataModel(modelDoc);
    std::cout << "[DEBUG] model3D created." << std::endl;
    
    std::cout << "[DEBUG] Calling OpenCommand..." << std::endl;
    model3D->OpenCommand();
    std::cout << "[DEBUG] OpenCommand finished." << std::endl;
    
    // Init a 3D slope object named "Slope_1" using DataFactory
    std::cout << "[DEBUG] Calling CreateObject..." << std::endl;
    Handle(ActAPI_INode) baseNode = DataFactory::CreateObject(model3D, "SubgradeSlope");
    std::cout << "[DEBUG] CreateObject finished: " << baseNode.get() << std::endl;
    ASSERT_TRUE(!baseNode.IsNull());
    Handle(BrNode_adObject) slope3DObj = Handle(BrNode_adObject)::DownCast(baseNode);
    std::cout << "[DEBUG] slope3DObj cast finished." << std::endl;
    slope3DObj->SetName("Slope_1");
    slope3DObj->SetGlobalID("GUID_Slope_3D_1");
    std::cout << "[DEBUG] SetName and SetGlobalID finished." << std::endl;
    
    TCollection_ExtendedString slopeGuid = slope3DObj->GetGlobalID();
    
    // Attach to OCAF root
    Handle(BrNode_adModelRoot) root3D = Handle(BrNode_adModelRoot)::DownCast(model3D->GetRootNode());
    root3D->AddSubObjects(slope3DObj);
    model3D->CommitCommand();

    // 3. Load and config 2D drawing sub doc, create adSlopeIndication
    Handle(TDocStd_Document) drawingDoc = pm->GetOrLoadSubDocument(drawingPath2D);
    ASSERT_TRUE(!drawingDoc.IsNull());

    Handle(DataModel) drawingModel = new DataModel(drawingDoc);
    drawingModel->OpenCommand();

    Handle(BrNode_adDrawing2D) drawing2D = drawingModel->AddadDrawing2D();
    drawing2D->SetName("Plan_Sheet_A");

    Handle(BrNode_adSlopeIndication) slope2D = drawingModel->AddadSlopeIndication();
    slope2D->SetName("Slope_2D_Representation");
    slope2D->SetTargetModelDoc(modelPath3D.c_str());
    slope2D->SetTargetObjectID(slopeGuid);
    slope2D->SetSpacing(2000.0);
    slope2D->SetLongLineRatio(0.6);
    slope2D->SetShortLineRatio(0.3);

    // Set initial feature lines to 20m
    SlopeWires initWires = CreateSlopeWires(20000.0, 8000.0, 1.5);
    slope2D->SetShoulderLine(initWires.shoulder);
    slope2D->SetToeLine(initWires.toe);

    drawing2D->AddRepresentations(slope2D);

    // Attach drawing to root
    Handle(BrNode_adModelRoot) root2D = Handle(BrNode_adModelRoot)::DownCast(drawingModel->GetRootNode());
    root2D->AddDrawings(drawing2D);
    drawingModel->CommitCommand();

    // First lazy sync, generate 20m slope indication and save
    Standard_Boolean synced1 = pm->Sync2DDrawing(drawing2D);
    ASSERT_TRUE(synced1);
    
    // Check initial count
    int initTeethCount = 0;
    TopExp_Explorer exp1(slope2D->GetGeneratedShape(), TopAbs_EDGE);
    for (; exp1.More(); exp1.Next()) {
        initTeethCount++;
    }
    // Contains 11 teeth + 1 shoulder + 1 toe = 13
    EXPECT_EQ(initTeethCount, 13);

    Standard_Boolean saved = pm->SaveAll();
    ASSERT_TRUE(saved);

    // 4. Parameter modification and linkage: simulate 3D model feature lines change to 30m
    // Update feature lines to 30m to trigger lazy sync
    SlopeWires updatedWires = CreateSlopeWires(30000.0, 8000.0, 1.5); // 30m
    
    drawingModel->OpenCommand();
    slope2D->SetShoulderLine(updatedWires.shoulder);
    slope2D->SetToeLine(updatedWires.toe);
    drawingModel->CommitCommand();

    // Re-run Lazy Sync, detect geometry changes and recalculate
    Standard_Boolean synced2 = pm->Sync2DDrawing(drawing2D);
    ASSERT_TRUE(synced2);

    // Assert: generated 2D slope indication successfully updated
    int updatedTeethCount = 0;
    TopExp_Explorer exp2(slope2D->GetGeneratedShape(), TopAbs_EDGE);
    for (; exp2.More(); exp2.Next()) {
        updatedTeethCount++;
    }
    // 30m / 2m + 1 = 16 teeth + 1 shoulder + 1 toe = 18
    EXPECT_EQ(updatedTeethCount, 18);

    // Save again
    ASSERT_TRUE(pm->SaveAll());

    // 5. Clean up test directory
    std::filesystem::remove_all(testProjDir);
    delete pm;
}

#include <Standard_Failure.hxx>

// -----------------------------------------------------------------------------
// Test entry function
// -----------------------------------------------------------------------------
int main(int argc, char** argv)
{
    std::cout << "=========================================" << std::endl;
    std::cout << "   Running test_master_project Unit Tests " << std::endl;
    std::cout << "=========================================" << std::endl;

    try {
        RUN_TEST(SlopeGeometryBuilderTest, GenerateAndSaveSlope);
        RUN_TEST(ProjectManagerTest, MultiDocLinkageAndSync);
    }
    catch (const Standard_Failure& f) {
        std::cerr << "\n[  FAILED  ] OCCT Exception: " << f.GetMessageString() << std::endl;
        return 1;
    }
    catch (const std::exception& e) {
        std::cerr << "\n[  FAILED  ] Std Exception: " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "\n[  FAILED  ] Some unit tests failed with unknown exception!" << std::endl;
        return 1;
    }

    std::cout << "\n[  PASSED  ] All tests completed successfully." << std::endl;
    return 0;
}
