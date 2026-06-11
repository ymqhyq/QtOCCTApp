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
#include <TDF_Tool.hxx>
#include <ActData_BasePartition.h>

// Project Classes
#include "ProjectManager.h"
#include "RwSlopeGeometryBuilder.h"
#include "SceneDataExtractor.h"
#include "generated/DataFactory.h"
#include "generated/BrNode_adDrawing2D.h"
#include "generated/BrNode_adSlopeIndication.h"
#include "generated/BrNode_adObject.h"
#include "generated/BrNode_adModelRoot.h"
#include "generated/BrNode_adProperty.h"
#include "generated/BrNode_adPropertySet.h"

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
// Unit Test 1: Verify RwSlopeGeometryBuilder geometry generation and XDE save
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
    Handle(RwSlopeGeometryBuilder) builder = new RwSlopeGeometryBuilder(
        Wires.shoulder, 
        Wires.toe, 
        spacing, 
        0.6, 
        0.3
    );

    // 3. Build geometry compound and verify teeth count
    TopoDS_Shape totalShape = builder->Build(RwBuilder::Rep_2D_Plan);
    ASSERT_TRUE(!totalShape.IsNull());

    // Count teeth edges
    int edgeCount = 0;
    TopExp_Explorer exp(totalShape, TopAbs_EDGE);
    for (; exp.More(); exp.Next()) {
        edgeCount++;
    }
    // 20m (20000mm) / 2m (2000mm) + 1 = 11 teeth lines + 1 shoulder + 1 toe = 13 lines
    EXPECT_EQ(edgeCount, 13);

    // 4. Create in-memory XCAF doc and save, verify layer and color
    Handle(TDocStd_Application) app = new TDocStd_Application();
    BinXCAFDrivers::DefineFormat(app);
    Handle(TDocStd_Document) doc;
    app->NewDocument("BinXCAF", doc);
    
    Standard_Boolean saved = builder->SaveToXDE(doc, RwBuilder::Rep_2D_Plan, totalShape);
    ASSERT_TRUE(saved);

    // Verify layers
    Handle(XCAFDoc_LayerTool) layerTool = XCAFDoc_DocumentTool::LayerTool(doc->Main());
    Handle(XCAFDoc_ColorTool) colorTool = XCAFDoc_DocumentTool::ColorTool(doc->Main());

    TDF_Label planLayerLabel = layerTool->FindLayer("Layer_Slope2D");
    ASSERT_TRUE(!planLayerLabel.IsNull());

    // Verify colors
    Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
    TDF_LabelSequence shapes;
    shapeTool->GetShapes(shapes);
    
    // Should have 1 top-level sub shape (Slope2DPlan)
    EXPECT_EQ(shapes.Length(), 1);
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

    // Add SubDocRefs to master document root
    Handle(DataModel) masterModel = pm->GetMasterModel();
    ASSERT_TRUE(!masterModel.IsNull());
    masterModel->OpenCommand();
    
    Handle(BrNode_adSubDocRef) ref3D = masterModel->AddadSubDocRef();
    ref3D->SetName("Subgrade_3D_Model");
    ref3D->SetDocPath(modelPath3D.c_str());
    ref3D->SetDocType("3DModel");
    
    Handle(BrNode_adSubDocRef) ref2D = masterModel->AddadSubDocRef();
    ref2D->SetName("Plan_View_Drawing");
    ref2D->SetDocPath(drawingPath2D.c_str());
    ref2D->SetDocType("2DDrawing");
    
    Handle(BrNode_adModelRoot) masterRoot = Handle(BrNode_adModelRoot)::DownCast(masterModel->GetRootNode());
    ASSERT_TRUE(!masterRoot.IsNull());
    masterRoot->AddSubDocRefs(ref3D);
    masterRoot->AddSubDocRefs(ref2D);
    
    masterModel->CommitCommand();

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
    
    // Create 3D geometry and store to modelDoc XCAF
    SlopeWires initWires3D = CreateSlopeWires(20000.0, 8000.0, 1.5);
    Handle(RwSlopeGeometryBuilder) builder3D = new RwSlopeGeometryBuilder(
        initWires3D.shoulder,
        initWires3D.toe,
        2000.0, 0.6, 0.3
    );
    TopoDS_Shape shape3D = builder3D->Build(RwBuilder::Rep_3D_Solid);
    builder3D->SaveToXDE(modelDoc, RwBuilder::Rep_3D_Solid, shape3D);

    // [CRITICAL FIX] Bind the 3D shape into the ActiveData Node Tree
    Handle(BrNode_adGeometricDef) geoDef = model3D->AddadGeometricDef();
    geoDef->SetShape(shape3D);
    Handle(BrNode_adGeometry) geoNode = model3D->AddadGeometry();
    geoNode->SetGeometryRef(geoDef);
    slope3DObj->SetGeometry(geoNode);

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

    // 5. Clean up test directory (disabled to keep generated test files)
    // std::filesystem::remove_all(testProjDir);
    delete pm;

    // Pack to slope.rde: Compress-Archive requires .zip extension
    std::cout << "[DEBUG] Packaging temp_debug to slope.rde..." << std::endl;
    system("powershell -Command \"Compress-Archive -Path ./temp_debug/* -DestinationPath ./slope.zip -Force; Move-Item -Path ./slope.zip -Destination ./slope.rde -Force\"");
}

static std::string ToStdString(const TCollection_ExtendedString &es) {
    std::string result;
    const Standard_ExtCharacter* p = es.ToExtString();
    for (int i = 0; i < es.Length(); ++i) {
        result += (char)(p[i] & 0xFF);
    }
    return result;
}

static std::string GetEntryStr(const TDF_Label& label) {
    TCollection_AsciiString entry;
    TDF_Tool::Entry(label, entry);
    return entry.ToCString();
}

static void PrintNodeTree(const Handle(ActAPI_INode)& node, int depth = 0) {
    if (node.IsNull()) return;
    std::string indent(depth * 2, ' ');
    std::cout << indent << "- Node: " << ToStdString(node->GetName()) 
              << " | Type: " << node->DynamicType()->Name() << std::endl;
    
    // Check property sets
    Handle(BrNode_adObject) obj = Handle(BrNode_adObject)::DownCast(node);
    if (!obj.IsNull()) {
        NCollection_Sequence<Handle(BrNode_adPropertySet)> psets = obj->GetPropertySetsList();
        for (int p = 1; p <= psets.Length(); ++p) {
            Handle(BrNode_adPropertySet) pset = psets.Value(p);
            if (!pset.IsNull()) {
                std::cout << indent << "  [PSet] " << ToStdString(pset->GetName()) << std::endl;
                NCollection_Sequence<Handle(BrNode_adProperty)> props = pset->GetPropertiesList();
                for (int k = 1; k <= props.Length(); ++k) {
                    Handle(BrNode_adProperty) prop = props.Value(k);
                    if (!prop.IsNull()) {
                        std::cout << indent << "    * " << ToStdString(prop->GetPropertyName()) 
                                  << " = " << ToStdString(prop->GetPropertyValue()) << std::endl;
                    }
                }
            }
        }
    }

    // Traverse children
    Handle(ActAPI_IChildIterator) it = node->GetChildIterator();
    if (!it.IsNull()) {
        for (; it->More(); it->Next()) {
            PrintNodeTree(it->Value(), depth + 1);
        }
    }
}

// -----------------------------------------------------------------------------
// Unit Test 3: Diagnose existing slope.rde
// -----------------------------------------------------------------------------
TEST(ProjectManagerTest, DiagnoseSlopeRde)
{
    std::string extractDir = "./temp_slope_extracted";
    std::filesystem::remove_all(extractDir);
    std::filesystem::create_directories(extractDir);

    std::string rdePath = "";
    if (std::filesystem::exists("slope.rde")) {
        rdePath = "slope.rde";
    } else if (std::filesystem::exists("../slope.rde")) {
        rdePath = "../slope.rde";
    } else if (std::filesystem::exists("../../slope.rde")) {
        rdePath = "../../slope.rde";
    }
    std::cout << "[DIAG] Found slope.rde at: " << rdePath << std::endl;
    std::string cmd = "tar -xf " + rdePath + " -C " + extractDir;
    int ret = std::system(cmd.c_str());
    std::cout << "[DIAG] Tar extraction returned: " << ret << std::endl;

    std::string masterPath = extractDir + "/master.cbf";
    if (!std::filesystem::exists(masterPath)) {
        // Search for master.cbf in subdirectories
        for (const auto& entry : std::filesystem::recursive_directory_iterator(extractDir)) {
            if (entry.is_regular_file() && entry.path().filename() == "master.cbf") {
                masterPath = entry.path().string();
                break;
            }
        }
    }

    std::cout << "[DIAG] Master path: " << masterPath << " (exists=" << std::filesystem::exists(masterPath) << ")" << std::endl;

    ProjectManager* pm = new ProjectManager();
    Standard_Boolean opened = pm->OpenMasterProject(masterPath);
    std::cout << "[DIAG] OpenMasterProject: " << (opened ? "SUCCESS" : "FAILED") << std::endl;

    if (opened) {
        Handle(DataModel) masterModel = pm->GetMasterModel();
        if (!masterModel.IsNull()) {
            Handle(BrNode_adModelRoot) rootNode = Handle(BrNode_adModelRoot)::DownCast(masterModel->GetRootNode());
            if (!rootNode.IsNull()) {
                std::cout << "[DIAG] Node Hierarchy:" << std::endl;
                PrintNodeTree(rootNode);
                std::cout << "[DIAG] End Node Hierarchy" << std::endl;
                NCollection_Sequence<Handle(BrNode_adSubDocRef)> subDocs = rootNode->GetSubDocRefsList();
                std::cout << "[DIAG] SubDocRefs count: " << subDocs.Length() << std::endl;
                
                std::string path3D = "models/subgrade_3d.cbf";
                std::string path2D = "drawings/plan_view.cbf";
                for (int i = 1; i <= subDocs.Length(); ++i) {
                    Handle(BrNode_adSubDocRef) refNode = subDocs.Value(i);
                    if (!refNode.IsNull()) {
                        std::cout << "  - SubDocRef " << i 
                                  << " Name: " << ToStdString(refNode->GetName())
                                  << " Type: " << ToStdString(refNode->GetDocType())
                                  << " Path: " << ToStdString(refNode->GetDocPath()) << std::endl;
                        if (ToStdString(refNode->GetDocType()) == "3DModel") {
                            path3D = ToStdString(refNode->GetDocPath());
                        } else if (ToStdString(refNode->GetDocType()) == "2DDrawing") {
                            path2D = ToStdString(refNode->GetDocPath());
                        }
                    }
                }

                // Check and load 3D sub doc
                std::filesystem::path mPath(masterPath);
                std::filesystem::path absSubPath3D = mPath.parent_path() / path3D;
                std::cout << "[DIAG] 3D SubDoc Path: " << absSubPath3D.string() 
                          << " (exists=" << std::filesystem::exists(absSubPath3D) << ")" << std::endl;
                if (std::filesystem::exists(absSubPath3D)) {
                    Handle(TDocStd_Document) subDoc3D = pm->GetOrLoadSubDocument(path3D);
                    std::cout << "  - GetOrLoadSubDocument 3D: " << (subDoc3D.IsNull() ? "NULL" : "VALID") << std::endl;
                    if (!subDoc3D.IsNull()) {
                        Handle(DataModel) subModel3D = pm->GetSubModel(path3D);
                        
                        // 诊断: 提取 3D 模型渲染数据
                        std::cout << "[DIAG] Testing SceneDataExtractor on 3D Model..." << std::endl;
                        std::vector<SceneDataExtractor::VisualShape> visualShapes;
                        SceneDataExtractor::Extract(subModel3D, visualShapes);
                        std::cout << "[DIAG] SceneDataExtractor returned " << visualShapes.size() << " visual shapes." << std::endl;

                        if (!subModel3D.IsNull()) {
                            Handle(ActAPI_INode) subRoot3D = subModel3D->GetRootNode();
                            if (!subRoot3D.IsNull()) {
                                std::cout << "  - 3D Model Node Hierarchy:" << std::endl;
                                PrintNodeTree(subRoot3D);
                                std::cout << "  - End 3D Model Node Hierarchy" << std::endl;
                            }
                            // Also print Partition 2 nodes
                            Handle(ActAPI_IPartition) topoPart3D = subModel3D->Partition(2);
                            if (!topoPart3D.IsNull()) {
                                std::cout << "  - 3D Model Partition 2 (Topology) Nodes:" << std::endl;
                                for (ActData_BasePartition::Iterator nodeIt(topoPart3D); nodeIt.More(); nodeIt.Next()) {
                                    Handle(ActAPI_INode) n = nodeIt.Value();
                                    if (!n.IsNull()) {
                                        Handle(ActAPI_INode) p = n->GetParentNode();
                                        std::cout << "    * Node: " << ToStdString(n->GetName()) 
                                                  << " | Entry: " << GetEntryStr(n->RootLabel())
                                                  << " | Type: " << n->DynamicType()->Name()
                                                  << " | Parent: " << (p.IsNull() ? "NULL" : GetEntryStr(p->RootLabel()).c_str()) << std::endl;
                                    }
                                }
                            }
                        }
                    }
                }

                // Check and load 2D sub doc
                std::filesystem::path absSubPath2D = mPath.parent_path() / path2D;
                std::cout << "[DIAG] 2D SubDoc Path: " << absSubPath2D.string() 
                          << " (exists=" << std::filesystem::exists(absSubPath2D) << ")" << std::endl;
                if (std::filesystem::exists(absSubPath2D)) {
                    Handle(TDocStd_Document) subDoc2D = pm->GetOrLoadSubDocument(path2D);
                    std::cout << "  - GetOrLoadSubDocument 2D: " << (subDoc2D.IsNull() ? "NULL" : "VALID") << std::endl;
                    if (!subDoc2D.IsNull()) {
                        Handle(DataModel) subModel2D = pm->GetSubModel(path2D);
                        if (!subModel2D.IsNull()) {
                            Handle(ActAPI_INode) subRoot2D = subModel2D->GetRootNode();
                            if (!subRoot2D.IsNull()) {
                                std::cout << "  - 2D Drawing Node Hierarchy:" << std::endl;
                                PrintNodeTree(subRoot2D);
                                std::cout << "  - End 2D Drawing Node Hierarchy" << std::endl;
                            }
                            // Also print Partition 2 nodes
                            Handle(ActAPI_IPartition) topoPart2D = subModel2D->Partition(2);
                            if (!topoPart2D.IsNull()) {
                                std::cout << "  - 2D Model Partition 2 (Topology) Nodes:" << std::endl;
                                for (ActData_BasePartition::Iterator nodeIt(topoPart2D); nodeIt.More(); nodeIt.Next()) {
                                    Handle(ActAPI_INode) n = nodeIt.Value();
                                    if (!n.IsNull()) {
                                        Handle(ActAPI_INode) p = n->GetParentNode();
                                        std::cout << "    * Node: " << ToStdString(n->GetName()) 
                                                  << " | Entry: " << GetEntryStr(n->RootLabel())
                                                  << " | Type: " << n->DynamicType()->Name()
                                                  << " | Parent: " << (p.IsNull() ? "NULL" : GetEntryStr(p->RootLabel()).c_str()) << std::endl;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
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
        RUN_TEST(ProjectManagerTest, DiagnoseSlopeRde);
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
