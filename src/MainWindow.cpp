#include "../include/MainWindow.h"
#include "../include/OCCTWidget.h"
#include "SARibbonApplicationButton.h"
#include "SARibbonBar.h"
#include "SARibbonCategory.h"
#include "SARibbonPanel.h"
#include <BRepBuilderAPI_Transform.hxx>

#include "../include/PythonSyntaxHighlighter.h"
#include "../include/ShxTextGenerator.h"
#include "../include/ModelExplorerPanel.h"
#include "../include/ComponentLibraryPanel.h"
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include "RwSlopeGeometryBuilder.h"
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>

namespace {
    struct SlopeWires { TopoDS_Wire shoulder; TopoDS_Wire toe; };
    static SlopeWires CreateSlopeWiresLocal(double L, double H, double ratio) {
        SlopeWires res;
        double offset = H * ratio;
        gp_Pnt p1(0, offset, H);
        gp_Pnt p2(L, offset, H);
        res.shoulder = BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(p1, p2));
        
        gp_Pnt p3(0, 0, 0);
        gp_Pnt p4(L, 0, 0);
        res.toe = BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(p3, p4));
        return res;
    }
}
#include "IfcExportService.h"
#include <ifcparse/IfcFile.h>
#include <ifcgeom/Iterator.h>
#include <ifcgeom/kernels/opencascade/OpenCascadeKernel.h>
#include <ifcgeom/kernels/opencascade/OpenCascadeConversionResult.h>
#include <ifcgeom/kernels/opencascade/base_utils.h>
#include <TopoDS_Compound.hxx>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStatusBar>
#include <QTextEdit>
#include <QUuid>
#include <QVBoxLayout>
#include <QDirIterator>
#include <QProcess>
#include <QFileInfo>
#include <BinXCAFDrivers.hxx>
#include <TDocStd_Application.hxx>
#include <TDocStd_Document.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFDoc_VisMaterialTool.hxx>
#include <XCAFDoc_VisMaterial.hxx>
#include <XCAFDoc_VisMaterialPBR.hxx>
#include <TDF_LabelSequence.hxx>
#include <sstream>

// core-data-model headers
#include <ActData_BasePartition.h>
#include <ActData_RealArrayParameter.h>
#include "DataModel.h"
#include "BrNode_adObject.h"
#include "GeometryService.h"
#include "SceneDataExtractor.h"
#include "BrNode_adGeometry.h"
#include "BrNode_adGeometricDef.h"
#include "ProjectManager.h"
#include "generated/BrNode_adSubDocRef.h"
#include "generated/BrNode_adModelRoot.h"
#include "generated/BrNode_adDrawing2D.h"
#include "generated/BrNode_adSlopeIndication.h"
#include <cmath>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_LayerTool.hxx>
#include <Standard_Failure.hxx>
#include <ActAPI_IPartition.h>
#include <ActData_BasePartition.h>

namespace {
    std::vector<Handle(BrNode_adObject)> getTopLevelObjects(const Handle(DataModel)& model) {
        std::vector<Handle(BrNode_adObject)> topLevelObjs;
        if (model.IsNull()) return topLevelObjs;

        // 优先从 Topology 分区中遍历并筛选顶级对象
        Handle(ActAPI_IPartition) topologyPart = model->Partition(2);
        if (!topologyPart.IsNull()) {
            for (ActData_BasePartition::Iterator it(topologyPart); it.More(); it.Next()) {
                Handle(BrNode_adObject) obj = Handle(BrNode_adObject)::DownCast(it.Value());
                if (!obj.IsNull()) {
                    Handle(ActAPI_INode) parent = obj->GetParentNode();
                    if (parent.IsNull() || !parent->IsKind(STANDARD_TYPE(BrNode_adObject))) {
                        topLevelObjs.push_back(obj);
                    }
                }
            }
        }

        // 如果 Partition 2 中没有任何构件，降级从根节点子节点获取
        if (topLevelObjs.empty()) {
            Handle(ActAPI_INode) rootBase = model->GetRootNode();
            if (!rootBase.IsNull()) {
                Handle(ActAPI_IChildIterator) childIt = rootBase->GetChildIterator();
                for (; childIt->More(); childIt->Next()) {
                    Handle(BrNode_adObject) obj = Handle(BrNode_adObject)::DownCast(childIt->Value());
                    if (!obj.IsNull()) {
                        topLevelObjs.push_back(obj);
                    }
                }
            }
        }

        return topLevelObjs;
    }
}

static const char* const CHECKBOX_STYLE = 
    "QCheckBox { color: #333333; font-size: 12px; }"
    "QCheckBox::indicator {"
    "  width: 14px;"
    "  height: 14px;"
    "  border: 1.5px solid #555555;"
    "  background-color: #ffffff;"
    "  border-radius: 2px;"
    "}"
    "QCheckBox::indicator:hover {"
    "  border: 1.5px solid #0078d7;"
    "}"
    "QCheckBox::indicator:checked {"
    "  image: url(:/resources/icons/check.svg);"
    "}";

MainWindow::MainWindow(QWidget *parent)
    : SARibbonMainWindow(parent), m_occtWidget(new OCCTWidget(this)),
      m_occtWidget2D(new OCCTWidget(this)), m_splitter(new QSplitter(Qt::Horizontal, this)),
      m_projectManager(nullptr),
      m_solidTextCheckbox(nullptr), m_pbrCheckbox(nullptr), m_coordLabel(nullptr),
      m_highlighter(nullptr), m_currentMaterial(Graphic3d_NOM_PLASTIC),
      m_propertyDock(nullptr), m_modelExplorerDock(nullptr), m_componentLibraryDock(nullptr), m_propertyWidget(nullptr),
      m_propertyLayout(nullptr), m_currentModelType("BridgePier2") {
  setWindowTitle("Qt OCCT Application - Schema Enabled");
  setWindowIcon(QIcon(":/resources/icons/app_logo.png"));
  setMinimumSize(1024, 768);
  showMaximized();
  m_tempProjDir = nullptr;
  m_loadedMasterPath = "";

  m_occtWidget2D->setAs2DView();
  m_occtWidget2D->hide();

  m_splitter->addWidget(m_occtWidget);
  m_splitter->addWidget(m_occtWidget2D);
  m_splitter->setCollapsible(0, false);
  m_splitter->setCollapsible(1, true);
  setCentralWidget(m_splitter);

  setupCadQueryUi();
  createRibbon();
  createScriptsCategory();
  createTestCategory();
  initializeCqNetwork();

  QStatusBar *sBar = new QStatusBar(this);
  setStatusBar(sBar);

  m_coordLabel = new QLabel("X: 0.000  Y: 0.000  Z: 0.000", this);
  m_coordLabel->setMinimumWidth(280);
  sBar->addPermanentWidget(m_coordLabel);

  connect(m_occtWidget, &OCCTWidget::mousePositionChanged, this, &MainWindow::onMousePositionChanged);
  connect(m_occtWidget, &OCCTWidget::objectSelected, this, &MainWindow::onObjectSelected);
  connect(m_occtWidget2D, &OCCTWidget::objectSelected, this, &MainWindow::onObjectSelected);
  connect(m_occtWidget, &OCCTWidget::propertyDragged, this, [this](const QString& nodeId, const QString& propertyName, double newValue) {
    onPropertyValueChanged(nodeId, propertyName, QString::number(newValue));
  });
  connect(m_occtWidget2D, &OCCTWidget::propertyDragged, this, [this](const QString& nodeId, const QString& propertyName, double newValue) {
    onPropertyValueChanged(nodeId, propertyName, QString::number(newValue));
  });

  onObjectSelected(QVariantMap());
}

MainWindow::~MainWindow() {
  if (m_projectManager) {
    delete m_projectManager;
  }
}

void MainWindow::closeEvent(QCloseEvent *event) {
  // 提前清理 OpenGL 资源，防止 eglMakeCurrent() 失败
  if (m_occtWidget) {
    m_occtWidget->cleanup();
  }
  if (m_occtWidget2D) {
    m_occtWidget2D->cleanup();
  }
  
  // 释放共享的图形驱动
  OCCTWidget::releaseSharedDriver();
  
  event->accept();
}

void MainWindow::createRibbon() {
  SARibbonBar *ribbon = ribbonBar();
  // 1. 隐藏最左侧的应用按钮 (删除 qtocctapp 面板入口)
  ribbon->applicationButton()->setVisible(false);

  // 2. "Main Tools" 修改为 "主页"
  SARibbonCategory *categoryMain = ribbon->addCategoryPage("主页");

  // "Basic" 面板修改为 "项目"，包含"打开"和"关闭"按钮
  SARibbonPanel *panelProject = categoryMain->addPanel("项目");

  // 打开 (原 Load ASI 移至第一位)
  QAction *loadAsiAction = new QAction(QIcon(":/resources/icons/open.svg"), "打开", this);
  connect(loadAsiAction, &QAction::triggered, this, &MainWindow::onLoadAsiModel);
  panelProject->addLargeAction(loadAsiAction);

  // 关闭 (新按钮)
  QAction *closeAction = new QAction(QIcon(":/resources/icons/close.svg"), "关闭", this);
  connect(closeAction, &QAction::triggered, this, &MainWindow::onCloseModel);
  panelProject->addLargeAction(closeAction);

  // 增加 "导入" 分组并归类
  SARibbonPanel *panelImport = categoryMain->addPanel("导入");
  QAction *importBrepAction = new QAction(QIcon(":/resources/icons/import.svg"), "导入 BREP", this);
  connect(importBrepAction, &QAction::triggered, this, &MainWindow::onImportBrep);
  panelImport->addLargeAction(importBrepAction);

  QAction *importIfcAction = new QAction(QIcon(":/resources/icons/import.svg"), "导入 IFC", this);
  connect(importIfcAction, &QAction::triggered, this, &MainWindow::onImportIfc);
  panelImport->addLargeAction(importIfcAction);

  // 增加 "导出" 分组并归类
  SARibbonPanel *panelExport = categoryMain->addPanel("导出");
  QAction *exportIfcAction = new QAction(QIcon(":/resources/icons/export.svg"), "导出 IFC", this);
  connect(exportIfcAction, &QAction::triggered, this, &MainWindow::onExportIfcClicked);
  panelExport->addLargeAction(exportIfcAction);

  QAction *exportStepAction = new QAction(QIcon(":/resources/icons/export.svg"), "导出 STEP", this);
  connect(exportStepAction, &QAction::triggered, this, &MainWindow::onExportStepClicked);
  panelExport->addLargeAction(exportStepAction);

  QAction *exportGltfAction = new QAction(QIcon(":/resources/icons/export.svg"), "导出 GLTF", this);
  connect(exportGltfAction, &QAction::triggered, this, &MainWindow::onExportGltfClicked);
  panelExport->addLargeAction(exportGltfAction);

  QAction *exportRdeAction = new QAction(QIcon(":/resources/icons/export.svg"), "导出 RDE", this);
  connect(exportRdeAction, &QAction::triggered, this, &MainWindow::onExportRdeClicked);
  panelExport->addLargeAction(exportRdeAction);

  // View 修改为 "视图"
  SARibbonPanel *panelView = categoryMain->addPanel("视图");
  QAction *fitAllAction = new QAction(QIcon(":/resources/icons/fit_all.svg"), "适应屏幕", this);
  connect(fitAllAction, &QAction::triggered, [this]() { m_occtWidget->fitAll(); });
  panelView->addLargeAction(fitAllAction);

  m_pbrCheckbox = new QCheckBox("启用 PBR", this);
  m_pbrCheckbox->setStyleSheet(CHECKBOX_STYLE);
  m_pbrCheckbox->setChecked(false);
  connect(m_pbrCheckbox, &QCheckBox::stateChanged, [this](int state) {
    m_occtWidget->setUsePbr(state == Qt::Checked);
  });
  panelView->addWidget(m_pbrCheckbox, SARibbonPanelItem::Small);

  panelView->addSeparator();

  QAction* toggleExplorer = m_modelExplorerDock->toggleViewAction();
  toggleExplorer->setText("模型结构");
  panelView->addSmallAction(toggleExplorer);

  QAction* toggleComponent = m_componentLibraryDock->toggleViewAction();
  toggleComponent->setText("构件库");
  panelView->addSmallAction(toggleComponent);

  QAction* toggleProperty = m_propertyDock->toggleViewAction();
  toggleProperty->setText("属性面板");
  panelView->addSmallAction(toggleProperty);

  QAction* toggleScript = m_dockCq->toggleViewAction();
  toggleScript->setText("脚本控制台");
  panelView->addSmallAction(toggleScript);

  // 4. "Bridge Tools" 修改为 "桥梁"
  SARibbonCategory *categoryBridge = ribbon->addCategoryPage("桥梁");
  SARibbonPanel *panelBridge = categoryBridge->addPanel("桥梁");

  QLabel *heightLabel = new QLabel("高度 (mm):", this);
  m_pierHeightSpinBox = new QDoubleSpinBox(this);
  m_pierHeightSpinBox->setRange(10.0, 50000.0);
  m_pierHeightSpinBox->setValue(12000.0);
  panelBridge->addWidget(heightLabel, SARibbonPanelItem::Small);
  panelBridge->addWidget(m_pierHeightSpinBox, SARibbonPanelItem::Small);

  QAction *bridgePierAction = new QAction(QIcon(":/resources/icons/bridge_pier.svg"), "绘制桥墩", this);
  connect(bridgePierAction, &QAction::triggered, [this]() {
    m_currentModelType = "BridgePier2";
    onDrawBridgePier();
  });
  panelBridge->addLargeAction(bridgePierAction);

  QAction *fullBridgePierAction = new QAction(QIcon(":/resources/icons/bridge_pier.svg"), "绘制完整桥墩", this);
  connect(fullBridgePierAction, &QAction::triggered, [this]() {
    m_currentModelType = "BridgePier2";
    onDrawFullBridgePier();
  });
  panelBridge->addLargeAction(fullBridgePierAction);

  QAction *annotatePierAction = new QAction(QIcon(":/resources/icons/dimension.svg"), "标注承台", this);
  connect(annotatePierAction, &QAction::triggered, this, &MainWindow::onAnnotateBridgePierFooting);
  panelBridge->addLargeAction(annotatePierAction);

  QAction *fullBridgeAction = new QAction(QIcon(":/resources/icons/full_bridge.svg"), "完整桥梁 (100桥墩)", this);
  connect(fullBridgeAction, &QAction::triggered, [this]() {
    m_occtWidget->clearAll();
    m_isBatchProcessing = true;
    m_isAssembling = false;
    m_currentPierIndex = 0;
    m_bridgePierCount = 100;
    m_bridgePierSpacing = 31600.0;
    m_currentMaterial = Graphic3d_NOM_STONE;
    m_completedTasks = 0;
    m_batchParts.clear();

    m_batchQueue.clear();
    for (int i = 0; i < m_bridgePierCount; ++i) {
      m_batchQueue.enqueue(i);
    }

    statusBar()->showMessage(QString("全部并发生成中, 共 %1 个桥墩已发送至微服务...").arg(m_bridgePierCount));
    m_batchTimer.start();
    while (!m_batchQueue.isEmpty()) {
      dispatchTask();
    }
  });
  panelBridge->addLargeAction(fullBridgeAction);

  QAction *fastAssemAction = new QAction(QIcon(":/resources/icons/fast_assembly.svg"), "快速装配 (300桥墩)", this);
  connect(fastAssemAction, &QAction::triggered, [this]() {
    m_occtWidget->clearAll();
    m_isAssembling = true;
    m_isBatchProcessing = false;
    m_bridgePierCount = 300;
    m_bridgePierSpacing = 31600.0;
    m_completedTasks = 0;
    m_assemblyParts.clear();
    m_batchQueue.clear();
    for (int i = 0; i < 9; ++i) {
      m_batchQueue.enqueue(i);
    }

    statusBar()->showMessage(QString("准备基础构件中, 正在调用后台微服务..."));
    m_batchTimer.start();

    int initialTasks = qMin(9, m_batchQueue.size());
    for (int i = 0; i < initialTasks; ++i) {
      dispatchTask();
    }
  });
  panelBridge->addLargeAction(fastAssemAction);

  SARibbonPanel *panelSubCrops = categoryBridge->addPanel("子构件");

  QAction *tuopanAction = new QAction(QIcon(":/resources/icons/tuopan.svg"), "顶帽与托盘", this);
  connect(tuopanAction, &QAction::triggered, [this]() {
    m_currentModelType = "PierTray";
    m_cqScriptEditor->setText(readScript(m_currentModelType));
    onRunCqScript();
  });
  panelSubCrops->addSmallAction(tuopanAction);

  QAction *dunshenAction = new QAction(QIcon(":/resources/icons/dunshen.svg"), "墩身", this);
  connect(dunshenAction, &QAction::triggered, [this]() {
    m_currentModelType = "PierBody";
    m_cqScriptEditor->setText(readScript(m_currentModelType));
    onRunCqScript();
  });
  panelSubCrops->addSmallAction(dunshenAction);

  QAction *chengtaiAction = new QAction(QIcon(":/resources/icons/chengtai.svg"), "承台", this);
  connect(chengtaiAction, &QAction::triggered, [this]() {
    m_currentModelType = "PileCap";
    m_cqScriptEditor->setText(readScript(m_currentModelType));
    onRunCqScript();
  });
  panelSubCrops->addSmallAction(chengtaiAction);

  QAction *pileAction = new QAction(QIcon(":/resources/icons/pile.svg"), "桩基础", this);
  connect(pileAction, &QAction::triggered, [this]() {
    m_currentModelType = "Pile";
    m_cqScriptEditor->setText(readScript(m_currentModelType));
    onRunCqScript();
  });
  panelSubCrops->addSmallAction(pileAction);

  QAction *girderAction = new QAction(QIcon(":/resources/icons/girder.svg"), "箱梁", this);
  connect(girderAction, &QAction::triggered, [this]() {
    m_currentModelType = "Girder";
    m_cqScriptEditor->setText(readScript(m_currentModelType));
    onRunCqScript();
  });
  panelSubCrops->addSmallAction(girderAction);

  QAction *foundationAction = new QAction(QIcon(":/resources/icons/foundation.svg"), "避雷针基础", this);
  connect(foundationAction, &QAction::triggered, this, &MainWindow::onDrawFoundation);
  panelSubCrops->addSmallAction(foundationAction);

  QAction *bedStoneAction = new QAction(QIcon(":/resources/icons/bed_stone.svg"), "垫石", this);
  connect(bedStoneAction, &QAction::triggered, this, &MainWindow::onDrawBedStone);
  panelSubCrops->addSmallAction(bedStoneAction);

  QAction *bearingAction = new QAction(QIcon(":/resources/icons/bearing.svg"), "支座", this);
  connect(bearingAction, &QAction::triggered, this, &MainWindow::onDrawBearing);
  panelSubCrops->addSmallAction(bearingAction);

}

void MainWindow::createTestCategory() {
  SARibbonBar *ribbon = ribbonBar();
  // 3. 增加“测试”面板
  SARibbonCategory *categoryTest = ribbon->addCategoryPage("测试");
  SARibbonPanel *panelTest = categoryTest->addPanel("测试功能");

  QAction *drawAction = new QAction(QIcon(":/resources/icons/draw_line.svg"), "绘制线段", this);
  connect(drawAction, &QAction::triggered, this, &MainWindow::onDrawLineClicked);
  panelTest->addLargeAction(drawAction);

  QAction *randLineAction = new QAction(QIcon(":/resources/icons/random.svg"), "生成万条线", this);
  connect(randLineAction, &QAction::triggered, [this]() { m_occtWidget->generateRandomLines(10000); });
  panelTest->addLargeAction(randLineAction);

  QAction *shxTextAction = new QAction(QIcon(":/resources/icons/text_shx.svg"), "添加 SHX 文本", this);
  connect(shxTextAction, &QAction::triggered, this, &MainWindow::onAddShxText);
  panelTest->addLargeAction(shxTextAction);

  m_solidTextCheckbox = new QCheckBox("测试实体文本", this);
  m_solidTextCheckbox->setStyleSheet(CHECKBOX_STYLE);
  m_solidTextCheckbox->setChecked(true);
  connect(m_solidTextCheckbox, &QCheckBox::stateChanged, [this](int state) {
    m_occtWidget->setTextsSolid(state == Qt::Checked);
  });
  panelTest->addWidget(m_solidTextCheckbox, SARibbonPanelItem::Small);

  QAction *loadMasterAction = new QAction(QIcon(":/resources/icons/open.svg"), "加载 Master CBF", this);
  connect(loadMasterAction, &QAction::triggered, this, &MainWindow::onLoadMasterCbf);
  panelTest->addLargeAction(loadMasterAction);
}

void MainWindow::setupCadQueryUi() {
  m_dockCq = new QDockWidget("CadQuery", this);
  m_dockCq->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);

  QWidget *content = new QWidget(m_dockCq);
  QVBoxLayout *layout = new QVBoxLayout(content);

  m_cqScriptEditor = new QTextEdit(content);
  m_cqScriptEditor->setText("import cadquery as cq\nresult = cq.Workplane('XY').box(100, 100, 100).edges().chamfer(10)\n");
  layout->addWidget(m_cqScriptEditor);

  m_highlighter = new PythonSyntaxHighlighter(m_cqScriptEditor->document());
  QFont font("Consolas", 10);
  font.setStyleHint(QFont::Monospace);
  m_cqScriptEditor->setFont(font);

  content->setLayout(layout);
  m_dockCq->setWidget(content);
  addDockWidget(Qt::RightDockWidgetArea, m_dockCq);

  m_propertyDock = new QDockWidget("Properties", this);
  m_propertyDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
  m_propertyWidget = new QWidget();
  m_propertyLayout = new QVBoxLayout(m_propertyWidget);
  m_propertyLayout->setAlignment(Qt::AlignTop);
  QScrollArea *scroll = new QScrollArea();
  scroll->setWidgetResizable(true);
  scroll->setWidget(m_propertyWidget);
  m_propertyDock->setWidget(scroll);
  addDockWidget(Qt::RightDockWidgetArea, m_propertyDock);
  
  // Tabify Properties and CadQuery on the right
  tabifyDockWidget(m_dockCq, m_propertyDock);
  m_propertyDock->raise(); // Show Properties by default

  m_modelExplorerDock = new ModelExplorerPanel(this);
  addDockWidget(Qt::LeftDockWidgetArea, m_modelExplorerDock);

  m_componentLibraryDock = new ComponentLibraryPanel(this);
  addDockWidget(Qt::LeftDockWidgetArea, m_componentLibraryDock);
  connect(m_componentLibraryDock, &ComponentLibraryPanel::componentSelected, this, &MainWindow::onComponentSelected);

  // Set initial dock widths for a balanced look
  QList<QDockWidget*> docks;
  docks << m_modelExplorerDock << m_propertyDock;
  QList<int> sizes;
  sizes << 300 << 300;
  resizeDocks(docks, sizes, Qt::Horizontal);
  
  // 默认不启动脚本窗口与构件库面板
  m_dockCq->hide();
  m_componentLibraryDock->hide();

  connect(m_modelExplorerDock, &ModelExplorerPanel::nodeSelected, this, &MainWindow::onExplorerNodeSelected);

  // Move dock titles/tabs to the bottom for a cleaner CAD-like look
  setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::South);
  setTabPosition(Qt::RightDockWidgetArea, QTabWidget::South);
}

void MainWindow::createScriptsCategory() {
  SARibbonBar *ribbon = ribbonBar();
  SARibbonCategory *categoryScripts = ribbon->addCategoryPage("脚本 (Scripts)");
  SARibbonPanel *panelScripts = categoryScripts->addPanel("示例代码");

  QAction *runBtn = new QAction(QIcon(":/resources/icons/random.svg"), "切角立方体", this);
  connect(runBtn, &QAction::triggered, [this]() {
    m_currentMaterial = Graphic3d_NOM_PLASTIC;
    m_cqScriptEditor->setText(
        "import cadquery as cq\n"
        "# Create a simple box\n"
        "result = cq.Workplane('XY').box(100, 100, 100).edges().chamfer(10)\n"
        "material = 'plastic'\n");
    onRunCqScript();
  });
  panelScripts->addLargeAction(runBtn);

  QAction *holedPlateBtn = new QAction(QIcon(":/resources/icons/random.svg"), "带孔板", this);
  connect(holedPlateBtn, &QAction::triggered, [this]() {
    m_currentMaterial = Graphic3d_NOM_CHROME;
    m_cqScriptEditor->setText("import cadquery as cq\n"
                              "height = 60.0\n"
                              "width = 80.0\n"
                              "thickness = 10.0\n"
                              "diameter = 22.0\n"
                              "\n"
                              "result = (\n"
                              "    cq.Workplane('XY')\n"
                              "    .box(height, width, thickness)\n"
                              "    .faces('>Z')\n"
                              "    .workplane()\n"
                              "    .hole(diameter)\n"
                              ")\n"
                              "material = 'gold'\n");
    onRunCqScript();
  });
  panelScripts->addLargeAction(holedPlateBtn);

  QAction *bridgePierCqBtn = new QAction(QIcon(":/resources/icons/bridge_pier.svg"), "桥墩示例", this);
  connect(bridgePierCqBtn, &QAction::triggered, [this]() {
    m_currentMaterial = Graphic3d_NOM_PLASTIC;
    m_cqScriptEditor->setText(readScript("bridge_pier_cq1"));
    onRunCqScript();
  });
  panelScripts->addLargeAction(bridgePierCqBtn);

  QAction *bridgePier2Btn = new QAction(QIcon(":/resources/icons/bridge_pier.svg"), "桥墩示例2", this);
  connect(bridgePier2Btn, &QAction::triggered, [this]() {
    m_currentMaterial = Graphic3d_NOM_PLASTIC;
    m_cqScriptEditor->setText(readScript("bridge_pier_cq2"));
    onRunCqScript();
  });
  panelScripts->addLargeAction(bridgePier2Btn);

  SARibbonPanel *panelRun = categoryScripts->addPanel("运行");
  QAction *runScriptBtn = new QAction(QIcon(":/resources/icons/random.svg"), "运行当前脚本", this);
  connect(runScriptBtn, &QAction::triggered, this, &MainWindow::onRunCqScript);
  panelRun->addLargeAction(runScriptBtn);
}

void MainWindow::onExplorerNodeSelected(Handle(BrNode_adObject) node) {
    if (node.IsNull()) {
        qDebug() << "[MainWindow] onExplorerNodeSelected: ERROR: node is Null!";
        return;
    }
    qDebug() << "[MainWindow] onExplorerNodeSelected entered for nodeId:" << node->GetId().ToCString();

    auto convertToUtf8 = [](const TCollection_ExtendedString& extStr) -> QString {
        QByteArray bytes;
        const Standard_ExtCharacter* p = extStr.ToExtString();
        for (int i = 0; i < extStr.Length(); ++i) {
            bytes.append((char)(p[i] & 0xFF));
        }
        return QString::fromUtf8(bytes);
    };

    // 1. Extract properties from the node for the Property Panel
    QVariantMap metaMap;
    // Basic info
    metaMap["_Name"] = convertToUtf8(node->GetName());
    metaMap["_Type"] = convertToUtf8(node->GetObjectType());
    metaMap["_adNodeId"] = QString(node->GetId().ToCString());

    // Iterate through all PropertySets
    NCollection_Sequence<Handle(BrNode_adPropertySet)> psets = node->GetPropertySetsList();
    for (int i = 1; i <= psets.Length(); ++i) {
        Handle(BrNode_adPropertySet) ps = psets.Value(i);
        if (ps.IsNull()) continue;
        
        QString psName = convertToUtf8(ps->GetName());
        
        NCollection_Sequence<Handle(BrNode_adProperty)> props = ps->GetPropertiesList();
        for (int j = 1; j <= props.Length(); ++j) {
            Handle(BrNode_adProperty) p = props.Value(j);
            if (p.IsNull()) continue;
            
            QString key = convertToUtf8(p->GetPropertyName());
            QString val = convertToUtf8(p->GetPropertyValue());
            metaMap[psName + "." + key] = val;
        }
    }

    // Refresh Property Panel
    updatePropertyPanelUI(metaMap);

    // 2. Highlighting and Focusing in OCCT View
    QString globalId = convertToUtf8(node->GetGlobalID());
    m_occtWidget->selectAndCenterObject("_adNodeId", globalId);
    m_occtWidget2D->selectAndCenterObject("_adNodeId", globalId);

    // 3. 检查是否有边坡相关的长度属性，显示拖拽手柄
    qDebug() << "[MainWindow] onExplorerNodeSelected: Calling updateStretchHandles...";
    updateStretchHandles(metaMap, node);
    qDebug() << "[MainWindow] onExplorerNodeSelected finished.";
}

void MainWindow::initializeCqNetwork() {
  m_networkManager = QSharedPointer<QNetworkAccessManager>(new QNetworkAccessManager());
  m_networkManager->setProxy(QNetworkProxy::NoProxy);
}

void MainWindow::onDrawLineClicked() {
  m_occtWidget->setDrawLineMode(true);
  m_occtWidget->setFocus();
}

void MainWindow::onAddShxText() {
  if (!m_shxGenerator) {
    m_shxGenerator = std::make_unique<ShxTextGenerator>();
    m_shxGenerator->loadFont("d:/QtOCCTApp/TTT.shx");
    m_shxGenerator->loadBigFont("d:/QtOCCTApp/hztxt.SHX");
  }
  std::string text = "京沪D1K323+569.30";
  gp_Pnt pos(0, 0, 0);
  auto result = m_shxGenerator->generateText(text, pos, 200.0, 0.0, 1.0);
  m_occtWidget->addShape(result.first, Quantity_Color(Quantity_NOC_YELLOW));
}

void MainWindow::onMousePositionChanged(double x, double y, double z) {
  m_coordLabel->setText(QString("X: %1  Y: %2  Z: %3").arg(x, 0, 'f', 3).arg(y, 0, 'f', 3).arg(z, 0, 'f', 3));
}

void MainWindow::onDrawBridgePier() {
  m_occtWidget->drawBridgePier();
}

void MainWindow::onObjectSelected(const QVariantMap &metadata) {
    qDebug() << "[MainWindow] onObjectSelected entered. Input metadata isEmpty:" << metadata.isEmpty();
    if (metadata.isEmpty()) {
        qDebug() << "[MainWindow] onObjectSelected: metadata is empty, hiding handles.";
        m_occtWidget->hideLengthHandle();
        m_occtWidget2D->hideLengthHandle();
        updatePropertyPanelUI(QVariantMap());
        return;
    }

    QVariantMap flattenedMeta;
    std::function<void(const QString&, const QVariant&)> flatten = [&](const QString& prefix, const QVariant& v) {
        if (v.type() == QVariant::Map) {
            QVariantMap m = v.toMap();
            for (auto it = m.begin(); it != m.end(); ++it) {
                QString newKey = prefix.isEmpty() ? it.key() : prefix + "." + it.key();
                flatten(newKey, it.value());
            }
        } else {
            flattenedMeta[prefix] = v;
        }
    };
    flatten("", metadata);

    qDebug() << "[MainWindow] onObjectSelected: Flattened metadata keys:" << flattenedMeta.keys();
    for (auto k : flattenedMeta.keys()) {
        qDebug() << "  " << k << "=>" << flattenedMeta[k].toString();
    }

    if (flattenedMeta.contains("_adNodeId")) {
        QString nodeId = flattenedMeta["_adNodeId"].toString();
        qDebug() << "[MainWindow] onObjectSelected: _adNodeId exists:" << nodeId;
        if (m_projectManager) {
            std::vector<std::pair<Handle(ActAPI_INode), Handle(DataModel)>> matchingNodes = 
                m_projectManager->FindNodesAcrossModels(nodeId.toStdString());
            qDebug() << "[MainWindow] onObjectSelected: FindNodesAcrossModels results size:" << matchingNodes.size();
            if (!matchingNodes.empty()) {
                Handle(BrNode_adObject) adObj = Handle(BrNode_adObject)::DownCast(matchingNodes[0].first);
                qDebug() << "[MainWindow] onObjectSelected: DownCast adObj isNull:" << adObj.IsNull();
                if (!adObj.IsNull()) {
                    qDebug() << "[MainWindow] onObjectSelected: Delegating to onExplorerNodeSelected for NodeId:" << adObj->GetId().ToCString();
                    onExplorerNodeSelected(adObj);
                    return;
                }
            } else {
                qDebug() << "[MainWindow] onObjectSelected: FindNodesAcrossModels found no match for NodeId:" << nodeId;
            }
        } else {
            qDebug() << "[MainWindow] onObjectSelected: ERROR: m_projectManager is Null!";
        }
    } else {
        qDebug() << "[MainWindow] onObjectSelected: No _adNodeId found in metadata.";
    }

    updatePropertyPanelUI(metadata);
}

void MainWindow::updatePropertyPanelUI(const QVariantMap &metadata) {
    // 优先对元数据进行扁平化操作，得到 flattenedMeta
    QVariantMap flattenedMeta;
    std::function<void(const QString&, const QVariant&)> flatten = [&](const QString& prefix, const QVariant& v) {
        if (v.type() == QVariant::Map) {
            QVariantMap m = v.toMap();
            for (auto it = m.begin(); it != m.end(); ++it) {
                QString newKey = prefix.isEmpty() ? it.key() : prefix + "." + it.key();
                flatten(newKey, it.value());
            }
        } else {
            flattenedMeta[prefix] = v;
        }
    };
    flatten("", metadata);

    // Clear layout
    QLayoutItem *child;
    while ((child = m_propertyLayout->takeAt(0)) != nullptr) {
        if (child->widget()) child->widget()->deleteLater();
        else if (child->layout()) {
             QLayout* subLayout = child->layout();
             QLayoutItem* subChild;
             while ((subChild = subLayout->takeAt(0)) != nullptr) {
                 if (subChild->widget()) subChild->widget()->deleteLater();
                 delete subChild;
             }
         }
        delete child;
    }

    if (metadata.isEmpty()) {
        QLabel* emptyLabel = new QLabel("No object selected");
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet("color: #666; padding: 20px; font-style: italic;");
        m_propertyLayout->addWidget(emptyLabel);
        return;
    }

    // Organize by groups
    QMap<QString, QMap<QString, QString>> groups;

    for (auto it = flattenedMeta.begin(); it != flattenedMeta.end(); ++it) {
        QString fullKey = it.key();
        QString val = it.value().toString();
        
        if (fullKey.startsWith("_")) {
            groups["Basic Information"][fullKey.mid(1)] = val;
        } else if (fullKey.contains(".")) {
            int dotIdx = fullKey.indexOf('.');
            QString group = fullKey.left(dotIdx);
            QString key = fullKey.mid(dotIdx + 1);
            
            if (group.startsWith("Pset_")) group = group.mid(5);
            
            if (group.contains("Geometry", Qt::CaseInsensitive)) group = "Geometry (" + group + ")";
            else if (group.contains("Material", Qt::CaseInsensitive)) group = "Material (" + group + ")";

            groups[group][key] = val;
        } else {
            groups["Attributes"][fullKey] = val;
        }
    }

    // Sort groups
    QStringList groupPriority = {"Basic Information", "Geometry", "Material", "Attributes"};
    QStringList sortedGroupKeys = groups.keys();
    std::sort(sortedGroupKeys.begin(), sortedGroupKeys.end(), [&](const QString& a, const QString& b) {
        auto getPriority = [&](const QString& name) {
            for (int i = 0; i < groupPriority.size(); ++i) {
                if (name.contains(groupPriority[i], Qt::CaseInsensitive)) return i;
            }
            return static_cast<int>(groupPriority.size());
        };
        int pA = getPriority(a);
        int pB = getPriority(b);
        if (pA != pB) return pA < pB;
        return a < b;
    });

    for (const QString& groupName : sortedGroupKeys) {
        QWidget* headerContainer = new QWidget();
        headerContainer->setStyleSheet("background-color: #333; border-left: 4px solid #00aaff; border-radius: 2px; margin-top: 5px;");
        QHBoxLayout* headerLayout = new QHBoxLayout(headerContainer);
        headerLayout->setContentsMargins(8, 4, 8, 4);
        
        QLabel* headerLabel = new QLabel(groupName.toUpper());
        headerLabel->setStyleSheet("color: #00aaff; font-weight: bold; font-size: 11px;");
        headerLayout->addWidget(headerLabel);
        
        m_propertyLayout->addWidget(headerContainer);
        
        QFrame* groupFrame = new QFrame();
        groupFrame->setStyleSheet("background-color: transparent;");
        QGridLayout* grid = new QGridLayout(groupFrame);
        grid->setContentsMargins(10, 5, 10, 10);
        grid->setSpacing(8);
        grid->setColumnStretch(0, 1);
        grid->setColumnStretch(1, 2);

        QStringList propKeys = groups[groupName].keys();
        if (groupName.contains("Geometry", Qt::CaseInsensitive)) {
            QStringList propPriority = {"ModelNumber", "Length", "Width", "Height"};
            std::sort(propKeys.begin(), propKeys.end(), [&](const QString& a, const QString& b) {
                auto getPropPriority = [&](const QString& name) {
                    for (int i = 0; i < propPriority.size(); ++i) {
                        if (name == propPriority[i]) return i;
                    }
                    return static_cast<int>(propPriority.size());
                };
                int pA = getPropPriority(a);
                int pB = getPropPriority(b);
                if (pA != pB) return pA < pB;
                return a < b;
            });
        }

        int row = 0;
        for (const QString& propKey : propKeys) {
            QString propValue = groups[groupName][propKey];
            QLabel* propLabel = new QLabel(propKey);
            propLabel->setStyleSheet("color: #999; font-size: 12px;");
            
            QWidget* valWidget = nullptr;
            
            if (groupName.contains("Geometry") && 
               (propKey == "Length" || propKey == "Height" || propKey == "Width" || propKey == "SlopeRatio" || propKey == "Spacing" || propKey == "LongLineRatio" || propKey == "ShortLineRatio")) {
                QDoubleSpinBox* spinBox = new QDoubleSpinBox();
                spinBox->setRange(0.0, 1000000.0);
                spinBox->setDecimals(2);
                spinBox->setSingleStep(100.0);
                spinBox->setValue(propValue.toDouble());
                spinBox->setStyleSheet("color: #eee; background-color: #2a2a2a; border: 1px solid #444; border-radius: 2px; padding: 2px;");
                valWidget = spinBox;
                
                QString nodeId = metadata["_adNodeId"].toString();
                connect(spinBox, &QDoubleSpinBox::editingFinished, this, [this, spinBox, nodeId, propKey]() {
                    double newVal = spinBox->value();
                    QTimer::singleShot(0, this, [this, nodeId, propKey, newVal]() {
                        onPropertyValueChanged(nodeId, propKey, QString::number(newVal));
                    });
                });
            } else {
                QLabel* valLabel = new QLabel(propValue);
                valLabel->setStyleSheet("color: #eee; font-size: 12px; font-family: 'Consolas', monospace;");
                valLabel->setWordWrap(true);
                valLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
                valWidget = valLabel;
            }

            grid->addWidget(propLabel, row, 0, Qt::AlignTop);
            grid->addWidget(valWidget, row, 1, Qt::AlignTop);
            row++;
        }
        m_propertyLayout->addWidget(groupFrame);
        
        QFrame* line = new QFrame();
        line->setFrameShape(QFrame::HLine);
        line->setStyleSheet("background-color: #222;");
        m_propertyLayout->addWidget(line);
    }
    
    m_propertyLayout->addStretch();
}

void MainWindow::updateStretchHandles(const QVariantMap &flattenedMeta, Handle(BrNode_adObject) node) {
    if (node.IsNull()) return;
    auto convertToUtf8 = [](const TCollection_ExtendedString& extStr) -> QString {
        QByteArray bytes;
        const Standard_ExtCharacter* p = extStr.ToExtString();
        for (int i = 0; i < extStr.Length(); ++i) {
            bytes.append((char)(p[i] & 0xFF));
        }
        return QString::fromUtf8(bytes);
    };
    QString nodeId = convertToUtf8(node->GetGlobalID());

    QVariantMap full3DMeta = m_occtWidget->findMetadataByNodeId(nodeId);

    double currentLength = -1;
    if (flattenedMeta.contains("Pset_SlopeGeometry.Length")) currentLength = flattenedMeta["Pset_SlopeGeometry.Length"].toDouble();
    else if (flattenedMeta.contains("Geometry.Length")) currentLength = flattenedMeta["Geometry.Length"].toDouble();
    else if (flattenedMeta.contains("Length")) currentLength = flattenedMeta["Length"].toDouble();
    else if (full3DMeta.contains("Pset_SlopeGeometry.Length")) currentLength = full3DMeta["Pset_SlopeGeometry.Length"].toDouble();
    else if (full3DMeta.contains("Geometry.Length")) currentLength = full3DMeta["Geometry.Length"].toDouble();
    else if (full3DMeta.contains("Length")) currentLength = full3DMeta["Length"].toDouble();

    double height = 8000.0;
    double slopeRatio = 1.5;
    if (flattenedMeta.contains("Pset_SlopeGeometry.Height")) height = flattenedMeta["Pset_SlopeGeometry.Height"].toDouble();
    else if (flattenedMeta.contains("Geometry.Height")) height = flattenedMeta["Geometry.Height"].toDouble();
    else if (flattenedMeta.contains("Height")) height = flattenedMeta["Height"].toDouble();
    else if (full3DMeta.contains("Pset_SlopeGeometry.Height")) height = full3DMeta["Pset_SlopeGeometry.Height"].toDouble();
    else if (full3DMeta.contains("Geometry.Height")) height = full3DMeta["Geometry.Height"].toDouble();
    else if (full3DMeta.contains("Height")) height = full3DMeta["Height"].toDouble();

    if (flattenedMeta.contains("Pset_SlopeGeometry.SlopeRatio")) slopeRatio = flattenedMeta["Pset_SlopeGeometry.SlopeRatio"].toDouble();
    else if (flattenedMeta.contains("Geometry.SlopeRatio")) slopeRatio = flattenedMeta["Geometry.SlopeRatio"].toDouble();
    else if (flattenedMeta.contains("SlopeRatio")) slopeRatio = flattenedMeta["SlopeRatio"].toDouble();
    else if (full3DMeta.contains("Pset_SlopeGeometry.SlopeRatio")) slopeRatio = full3DMeta["Pset_SlopeGeometry.SlopeRatio"].toDouble();
    else if (full3DMeta.contains("Geometry.SlopeRatio")) slopeRatio = full3DMeta["Geometry.SlopeRatio"].toDouble();
    else if (full3DMeta.contains("SlopeRatio")) slopeRatio = full3DMeta["SlopeRatio"].toDouble();

    qDebug() << "[MainWindow] updateStretchHandles: nodeId =" << nodeId << ", currentLength =" << currentLength << ", height =" << height << ", slopeRatio =" << slopeRatio;

    if (currentLength > 0) {
        gp_Pnt handlePos2D_start(0, 0, 0);
        gp_Pnt handlePos2D_end(currentLength, 0, 0);
        
        gp_Pnt handlePos3D_start = handlePos2D_start;
        gp_Pnt handlePos3D_end = handlePos2D_end;
        
        gp_Trsf trsf;
        QVariantList trsfList;
        if (flattenedMeta.contains("_globalTrsf")) {
            trsfList = flattenedMeta["_globalTrsf"].toList();
        } else if (full3DMeta.contains("_globalTrsf")) {
            trsfList = full3DMeta["_globalTrsf"].toList();
        }
        
        if (trsfList.size() == 12) {
            trsf.SetValues(trsfList[0].toDouble(), trsfList[1].toDouble(), trsfList[2].toDouble(), trsfList[3].toDouble(),
                           trsfList[4].toDouble(), trsfList[5].toDouble(), trsfList[6].toDouble(), trsfList[7].toDouble(),
                           trsfList[8].toDouble(), trsfList[9].toDouble(), trsfList[10].toDouble(), trsfList[11].toDouble());
            handlePos3D_start.Transform(trsf);
            handlePos3D_end.Transform(trsf);
            qDebug() << "[MainWindow] updateStretchHandles: Applied global trsf to 3D handle positions. Start:" 
                     << handlePos3D_start.X() << handlePos3D_start.Y() << handlePos3D_start.Z();
        } else {
            qDebug() << "[MainWindow] updateStretchHandles: WARNING: _globalTrsf size is not 12, size =" << trsfList.size();
        }
        
        gp_Trsf identityTrsf;
        m_occtWidget2D->showLengthHandle(handlePos2D_start, handlePos2D_end, currentLength, nodeId, identityTrsf, height, slopeRatio);
        m_occtWidget->showLengthHandle(handlePos3D_start, handlePos3D_end, currentLength, nodeId, trsf, height, slopeRatio);
    } else {
        m_occtWidget2D->hideLengthHandle();
        m_occtWidget->hideLengthHandle();
    }
}



void MainWindow::onLoadAsiModel() {
    QString fileName = QFileDialog::getOpenFileName(this, "Open", "", "Project Packages (*.rde);;Master Projects (*.cbf);;ASI Files (*.asi *.asi.cbf);;All Files (*.*)");
    if (fileName.isEmpty()) return;

    if (fileName.endsWith(".rde", Qt::CaseInsensitive) || fileName.endsWith("master.cbf", Qt::CaseInsensitive)) {
        openProjectFile(fileName);
        return;
    }

    m_occtWidget->clearAll();
    if (!m_currentModel.IsNull()) {
        m_currentModel.Nullify();
    }

    m_currentModel = new DataModel();
    if (!m_currentModel->Open(fileName.toStdString().c_str())) {
        QMessageBox::critical(this, "Error", "Failed to open file: " + fileName);
        m_currentModel.Nullify();
        return;
    }

    std::vector<SceneDataExtractor::VisualShape> visualShapes;

    try {
        SceneDataExtractor::Extract(m_currentModel, visualShapes);
    } catch (Standard_Failure& e) {
        m_currentModel->AbortCommand();
        QString errMsg = QString("OCCT Exception: %1").arg(e.GetMessageString());
        QMessageBox::warning(this, "Warning", "Failed to construct XCAF geometry assembly.\n" + errMsg);
    } catch (const std::exception& e) {
        m_currentModel->AbortCommand();
        QString errMsg = QString("Standard Exception: %1").arg(e.what());
        QMessageBox::warning(this, "Warning", "Failed to construct XCAF geometry assembly.\n" + errMsg);
    } catch (...) {
        m_currentModel->AbortCommand();
        QMessageBox::warning(this, "Warning", "Failed to construct XCAF geometry assembly.\nUnknown error occurred.");
    }

    // 转换 json 为 QVariantMap 的辅助 Lambdas
    auto convertJsonToQVariantMap = [](const nlohmann::json& j) -> QVariantMap {
        std::function<QVariantMap(const nlohmann::json&)> convert = [&](const nlohmann::json& js) -> QVariantMap {
            QVariantMap map;
            for (auto it = js.begin(); it != js.end(); ++it) {
                QString key = QString::fromStdString(it.key());
                if (it.value().is_string()) {
                    map[key] = QString::fromStdString(it.value().get<std::string>());
                } else if (it.value().is_number_float()) {
                    map[key] = it.value().get<double>();
                } else if (it.value().is_number_integer()) {
                    map[key] = it.value().get<int>();
                } else if (it.value().is_boolean()) {
                    map[key] = it.value().get<bool>();
                } else if (it.value().is_object()) {
                    map[key] = convert(it.value());
                } else if (it.value().is_array()) {
                    QVariantList list;
                    for (const auto& item : it.value()) {
                        if (item.is_number()) list.append(item.get<double>());
                        else if (item.is_string()) list.append(QString::fromStdString(item.get<std::string>()));
                    }
                    map[key] = list;
                }
            }
            return map;
        };
        return convert(j);
    };

    // 渲染普通展示：直接用 visualShapes 一键渲染，并建立元数据绑定！
    m_occtWidget->clearAll();
    for (const auto& vs : visualShapes) {
        if (vs.shape.IsNull()) continue;
        
        // 应用绝对坐标变换
        TopoDS_Shape transformedShape = vs.shape;
        try {
            BRepBuilderAPI_Transform trans(vs.shape, vs.transform);
            transformedShape = trans.Shape();
        } catch (...) {
            transformedShape = vs.shape;
        }

        QVariantMap meta = convertJsonToQVariantMap(vs.metadata);
        
        // 确定构件的颜色
        Quantity_Color color(Quantity_NOC_GRAY75);
        if (meta.contains("Pset_MaterialPBR")) {
            QVariantMap pbr = meta["Pset_MaterialPBR"].toMap();
            if (pbr.contains("BaseColor")) {
                QVariantList colorList = pbr["BaseColor"].toList();
                if (colorList.size() >= 3) {
                    color = Quantity_Color(colorList[0].toDouble(), 
                                           colorList[1].toDouble(), 
                                           colorList[2].toDouble(), 
                                           Quantity_TOC_RGB);
                }
            }
        }
        
        m_occtWidget->displayShape(transformedShape, Graphic3d_NOM_PLASTIC, color, false, meta);
    }

    m_occtWidget->fitAll();
    
    m_modelExplorerDock->setModel(m_currentModel);
    statusBar()->showMessage("Model loaded: " + fileName, 3000);
}

void MainWindow::onLoadMasterCbf() {
    QString fileName = QFileDialog::getOpenFileName(this, "Open Master Project", "", "CBF/RDE Files (master.cbf *.rde);;All Files (*.*)");
    if (fileName.isEmpty()) return;

    openProjectFile(fileName);
}

bool MainWindow::loadMasterCbf(const QString &fileName) {
    qDebug() << "[GUI] loadMasterCbf: Opening master project:" << fileName;

    m_occtWidget->clearAll();
    m_occtWidget2D->clearAll();
    m_occtWidget2D->hide();

    if (m_projectManager) {
        delete m_projectManager;
        m_projectManager = nullptr;
    }

    if (!m_currentModel.IsNull()) {
        m_currentModel.Nullify();
    }

    m_projectManager = new ProjectManager();
    if (!m_projectManager->OpenMasterProject(fileName.toStdString())) {
        QMessageBox::critical(this, "Error", "Failed to open master project: " + fileName);
        delete m_projectManager;
        m_projectManager = nullptr;
        return false;
    }
    qDebug() << "[GUI] loadMasterCbf: Master project opened successfully.";

    m_currentModel = m_projectManager->GetMasterModel();
    if (m_currentModel.IsNull()) {
        QMessageBox::critical(this, "Error", "Master project has no root data model.");
        return false;
    }

    Handle(BrNode_adModelRoot) rootNode = Handle(BrNode_adModelRoot)::DownCast(m_currentModel->GetRootNode());
    if (rootNode.IsNull()) {
        qDebug() << "[GUI] loadMasterCbf: ERROR: Root node is NULL!";
        return false;
    }
    
    // Helper lambda for converting ExtendedString to QString
    auto convertToUtf8 = [](const TCollection_ExtendedString& extStr) -> QString {
        QByteArray bytes;
        const Standard_ExtCharacter* p = extStr.ToExtString();
        for (int i = 0; i < extStr.Length(); ++i) {
            bytes.append((char)(p[i] & 0xFF));
        }
        return QString::fromUtf8(bytes);
    };

    qDebug() << "[GUI] loadMasterCbf: Root node name:" << convertToUtf8(rootNode->GetName());

    // Traverse sub-documents
    NCollection_Sequence<Handle(BrNode_adSubDocRef)> subDocs = rootNode->GetSubDocRefsList();
        if (subDocs.IsEmpty() && !m_currentModel.IsNull()) {
            Handle(ActAPI_IPartition) part = m_currentModel->Partition(2);
            if (!part.IsNull()) {
                for (ActData_BasePartition::Iterator pit(part); pit.More(); pit.Next()) {
                    Handle(BrNode_adSubDocRef) child = Handle(BrNode_adSubDocRef)::DownCast(pit.Value());
                    if (!child.IsNull()) {
                        Handle(ActAPI_INode) p = child->GetParentNode();
                        if (!p.IsNull() && p->GetId() == rootNode->GetId()) {
                            subDocs.Append(child);
                        }
                    }
                }
            }
        }
    qDebug() << "[GUI] loadMasterCbf: SubDocRefs count:" << subDocs.Length();
    std::string path3D = "";
    std::string path2D = "";

    for (int i = 1; i <= subDocs.Length(); ++i) {
        Handle(BrNode_adSubDocRef) refNode = subDocs.Value(i);
        if (refNode.IsNull()) continue;

        QString docType = convertToUtf8(refNode->GetDocType());
        QString docPath = convertToUtf8(refNode->GetDocPath());
        qDebug() << "[GUI] loadMasterCbf: SubDocRef index:" << i << "Type:" << docType << "Path:" << docPath;

        if (docType == "3DModel") {
            path3D = docPath.toStdString();
        } else if (docType == "2DDrawing") {
            path2D = docPath.toStdString();
        }
    }

    // Fallback names if empty
    if (path3D.empty()) {
        path3D = "models/subgrade_3d.cbf";
    }
    if (path2D.empty()) {
        path2D = "drawings/plan_view.cbf";
    }
    qDebug() << "[GUI] loadMasterCbf: path3D:" << QString::fromStdString(path3D);
    qDebug() << "[GUI] loadMasterCbf: path2D:" << QString::fromStdString(path2D);

    // 检测子文档的物理文件是否存在
    QFileInfo masterInfo(fileName);
    QDir masterDir = masterInfo.dir();
    QString absPath3D = masterDir.absoluteFilePath(QString::fromStdString(path3D));
    QString absPath2D = masterDir.absoluteFilePath(QString::fromStdString(path2D));
    bool has3DFile = QFile::exists(absPath3D);
    bool has2DFile = QFile::exists(absPath2D);
    qDebug() << "[GUI] loadMasterCbf: absPath3D =" << absPath3D << "exists =" << has3DFile;
    qDebug() << "[GUI] loadMasterCbf: absPath2D =" << absPath2D << "exists =" << has2DFile;

    // 将路径调试信息写入 D:/QtOCCTApp/debug_paths.log
    {
        QFile dbgFile("D:/QtOCCTApp/debug_paths.log");
        if (dbgFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream outStream(&dbgFile);
            outStream << "======== LoadMasterCbf Debug ========\n";
            outStream << "fileName: " << fileName << "\n";
            outStream << "path3D: " << QString::fromStdString(path3D) << " (len=" << path3D.length() << ")\n";
            outStream << "path2D: " << QString::fromStdString(path2D) << " (len=" << path2D.length() << ")\n";
            outStream << "absPath3D: " << absPath3D << "\n";
            outStream << "absPath2D: " << absPath2D << "\n";
            outStream << "has3DFile: " << (has3DFile ? "TRUE" : "FALSE") << "\n";
            outStream << "has2DFile: " << (has2DFile ? "TRUE" : "FALSE") << "\n";
            outStream << "=====================================\n";
            dbgFile.close();
        }
    }


    std::vector<SceneDataExtractor::VisualShape> visualShapes;

    // Set up UI layout based on available documents
    if (has2DFile) {
        m_occtWidget2D->show();
        QList<int> sizes;
        sizes << width() / 2 << width() / 2;
        m_splitter->setSizes(sizes);
    } else {
        m_occtWidget2D->hide();
        QList<int> sizes;
        sizes << width() << 0;
        m_splitter->setSizes(sizes);
    }

    // Call unified refresh to extract and display 3D geometries and 2D drawings
    refreshViews();

    m_modelExplorerDock->setModel(m_currentModel);
    statusBar()->showMessage("Master project loaded: " + fileName, 3000);

    // 将加载结果写入 D:/QtOCCTApp/debug_paths.log
    {
        QFile dbgFile("D:/QtOCCTApp/debug_paths.log");
        if (dbgFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream outStream(&dbgFile);
            outStream << "======== LoadMasterCbf Result ========\n";
            outStream << "visualShapes.size(): " << visualShapes.size() << "\n";
            outStream << "has3DFile: " << (has3DFile ? "TRUE" : "FALSE") << "\n";
            outStream << "has2DFile: " << (has2DFile ? "TRUE" : "FALSE") << "\n";
            outStream << "m_projectManager is NULL: " << (m_projectManager == nullptr ? "TRUE" : "FALSE") << "\n";
            if (m_projectManager) {
                Handle(TDocStd_Document) m3d = m_projectManager->GetOrLoadSubDocument(path3D);
                outStream << "modelDoc (3D) is NULL: " << (m3d.IsNull() ? "TRUE" : "FALSE") << "\n";
                Handle(TDocStd_Document) m2d = m_projectManager->GetOrLoadSubDocument(path2D);
                outStream << "drawingDoc (2D) is NULL: " << (m2d.IsNull() ? "TRUE" : "FALSE") << "\n";
            }
            outStream << "======================================\n";
            dbgFile.close();
        }
    }

    m_loadedMasterPath = fileName;
    return true;
}

void MainWindow::onImportBrep() {
    QString fileName = QFileDialog::getOpenFileName(this, "Import BREP", "", "BREP Files (*.brep *.occ);;All Files (*.*)");
    if (fileName.isEmpty()) return;

    TopoDS_Shape shape;
    BRep_Builder builder;
    if (!BRepTools::Read(shape, fileName.toStdString().c_str(), builder)) {
        QMessageBox::critical(this, "Error", "Failed to read BREP file: " + fileName);
        return;
    }

    if (shape.IsNull()) {
        QMessageBox::warning(this, "Warning", "The imported shape is empty.");
        return;
    }

    // Add metadata with filename
    QVariantMap meta;
    meta["Basic Information.Name"] = QFileInfo(fileName).fileName();
    meta["Basic Information.Path"] = fileName;

    m_occtWidget->addShape(shape, Quantity_Color(Quantity_NOC_GRAY70), Graphic3d_NOM_PLASTIC, meta);
    m_occtWidget->fitAll();
    
    statusBar()->showMessage("BREP imported: " + fileName, 3000);
}

void MainWindow::onImportIfc() {
    QString fileName = QFileDialog::getOpenFileName(this, "Import IFC", "", "IFC Files (*.ifc);;All Files (*.*)");
    if (fileName.isEmpty()) return;

    statusBar()->showMessage("Importing IFC file: " + fileName + "...");
    QApplication::setOverrideCursor(Qt::WaitCursor);

    try {
        // 1. Initialize translation settings
        ifcopenshell::geometry::Settings settings;
        
        // 2. Open IFC File
        IfcParse::IfcFile file(fileName.toStdString());
        if (!file.good()) {
            QApplication::restoreOverrideCursor();
            QMessageBox::critical(this, "Error", "Failed to parse IFC file: " + fileName);
            statusBar()->clearMessage();
            return;
        }

        // 3. Create OpenCascade geometric kernel
        auto kernel = std::make_unique<IfcGeom::OpenCascadeKernel>(settings);

        // 4. Initialize Geometry Iterator
        IfcGeom::Iterator iterator(std::move(kernel), settings, &file);
        if (!iterator.initialize()) {
            QApplication::restoreOverrideCursor();
            QMessageBox::critical(this, "Error", "Failed to initialize IFC geometry iterator. No 3D shapes found.");
            statusBar()->clearMessage();
            return;
        }

        // 5. Gather and reconstruct compound shape
        BRep_Builder builder;
        TopoDS_Compound compound;
        builder.MakeCompound(compound);
        bool hasGeometry = false;
        int parsedCount = 0;

        do {
            IfcGeom::Element* elem = iterator.get();
            if (elem) {
                IfcGeom::BRepElement* brepElem = dynamic_cast<IfcGeom::BRepElement*>(elem);
                if (brepElem) {
                    auto compound_ptr = brepElem->geometry().as_compound();
                    auto cascade_compound = dynamic_cast<ifcopenshell::geometry::OpenCascadeShape*>(compound_ptr);
                    if (cascade_compound) {
                        TopoDS_Shape base_shape = cascade_compound->shape();
                        
                        // Apply placement transformation
                        const auto& trsf = brepElem->transformation().data();
                        TopoDS_Shape transformed = IfcGeom::util::apply_transformation(base_shape, *trsf);
                        
                        builder.Add(compound, transformed);
                        hasGeometry = true;
                        parsedCount++;
                    }
                    delete compound_ptr; // Release as_compound memory allocation
                }
            }
        } while (iterator.next());

        QApplication::restoreOverrideCursor();

        if (!hasGeometry) {
            QMessageBox::warning(this, "Warning", "The imported IFC file contains no valid 3D shapes.");
            statusBar()->clearMessage();
            return;
        }

        // 6. Display compound shape in OCCT Widget and set metadata
        QVariantMap meta;
        meta["Basic Information.Name"] = QFileInfo(fileName).fileName();
        meta["Basic Information.Path"] = fileName;
        meta["Basic Information.ParsedElements"] = QString::number(parsedCount);

        m_occtWidget->addShape(compound, Quantity_Color(Quantity_NOC_GRAY70), Graphic3d_NOM_PLASTIC, meta);
        m_occtWidget->fitAll();

        statusBar()->showMessage(QString("IFC imported: %1 (%2 shapes)").arg(QFileInfo(fileName).fileName()).arg(parsedCount), 5000);

    } catch (const std::exception& e) {
        QApplication::restoreOverrideCursor();
        QMessageBox::critical(this, "Error", QString("Exception during IFC import:\n%1").arg(e.what()));
        statusBar()->clearMessage();
    } catch (...) {
        QApplication::restoreOverrideCursor();
        QMessageBox::critical(this, "Error", "Unknown exception occurred during IFC import.");
        statusBar()->clearMessage();
    }
}

void MainWindow::onRunCqScript() {
    QString code = m_cqScriptEditor->toPlainText();
    if (code.isEmpty()) return;
    QJsonObject args;
    args["pierHeight"] = m_pierHeightSpinBox->value();
    sendScriptToMicroservice(code, args, -1, m_currentModelType);
}

void MainWindow::sendScriptToMicroservice(const QString &code, const QJsonObject &args, int assemblyIndex, const QString &modelType) {
    QJsonObject req;
    req["code"] = code;
    req["args"] = args;
    req["model_type"] = modelType;
    req["format"] = "cbf";

    QJsonDocument doc(req);
    QByteArray postData = doc.toJson();
    qDebug() << "Sending request to microservice:" << postData;

    QNetworkRequest request(QUrl("http://127.0.0.1:8000/api/v1/model/generate"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = m_networkManager->post(request, postData);
    reply->setProperty("assemblyIndex", assemblyIndex);

    connect(reply, &QNetworkReply::finished, [this, reply]() {
        int assemblyIdx = reply->property("assemblyIndex").toInt();
        this->onCqNetworkReply(reply, assemblyIdx);
    });

    if (assemblyIndex == -1) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
    }
}

void MainWindow::onCqNetworkReply(QNetworkReply *reply, int assemblyIndex) {
    QApplication::restoreOverrideCursor();
    if (reply->error() != QNetworkReply::NoError) {
        QByteArray errData = reply->readAll();
        QString errMsg = reply->errorString();
        if (!errData.isEmpty()) {
            errMsg += "\n详细信息: " + QString::fromUtf8(errData);
        }
        QMessageBox::critical(this, "Network Error", errMsg);

        if (m_isAssembling) {
            m_completedTasks++;
            if (m_completedTasks == 9) {
                statusBar()->showMessage("脚本拼装中断", 5000);
                m_isAssembling = false;
            } else {
                dispatchTask();
            }
        }
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    if (data.size() < 4) {
        qWarning() << "JHB data too short";
        return;
    }

    uint32_t jsonLen = 0;
    memcpy(&jsonLen, data.constData(), 4);
    if (data.size() < (int)(4 + jsonLen)) {
        qWarning() << "JHB metadata length mismatch";
        return;
    }

    QByteArray jsonData = data.mid(4, jsonLen);
    QByteArray cbfData = data.mid(4 + jsonLen);

    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    QVariantMap metadata = doc.object().toVariantMap();

    Handle(TDocStd_Application) app = new TDocStd_Application();
    BinXCAFDrivers::DefineFormat(app);

    Handle(TDocStd_Document) tempDoc;
    app->NewDocument("BinXCAF", tempDoc);

    std::string byteStream = cbfData.toStdString();
    std::istringstream iss(byteStream, std::ios::binary);
    if (app->Open(iss, tempDoc) != PCDM_RS_OK) {
        qWarning() << "Failed to deserialize CBF byte stream in onCqNetworkReply";
        return;
    }

    Handle(XCAFDoc_ShapeTool) tempShapeTool = XCAFDoc_DocumentTool::ShapeTool(tempDoc->Main());
    if (tempShapeTool.IsNull()) return;

    TDF_LabelSequence tempShapes;
    tempShapeTool->GetShapes(tempShapes);
    if (tempShapes.IsEmpty()) return;

    TDF_Label srcProtoLabel = tempShapes.Value(1);
    TopoDS_Shape shape;
    if (!tempShapeTool->GetShape(srcProtoLabel, shape) || shape.IsNull()) {
        qWarning() << "Failed to extract shape from CBF document";
        return;
    }

    Handle(XCAFDoc_VisMaterialTool) tempVisMatTool = XCAFDoc_DocumentTool::VisMaterialTool(tempDoc->Main());
    if (!tempVisMatTool.IsNull()) {
        Handle(XCAFDoc_VisMaterial) aVisMat = tempVisMatTool->GetShapeMaterial(srcProtoLabel);
        if (!aVisMat.IsNull() && aVisMat->HasPbrMaterial()) {
            XCAFDoc_VisMaterialPBR aPbrMat = aVisMat->PbrMaterial();
            if (aPbrMat.IsDefined) {
                QVariantMap pbrMap;
                QVariantList colorList;
                colorList << aPbrMat.BaseColor.GetRGB().Red() 
                          << aPbrMat.BaseColor.GetRGB().Green() 
                          << aPbrMat.BaseColor.GetRGB().Blue();
                pbrMap["BaseColor"] = colorList;
                pbrMap["Metallic"] = aPbrMat.Metallic;
                pbrMap["Roughness"] = aPbrMat.Roughness;
                pbrMap["IOR"] = aPbrMat.RefractionIndex;
                metadata["Pset_MaterialPBR"] = pbrMap;
            }
        }
    }

    if (m_isAssembling) {
        while (m_assemblyParts.size() <= assemblyIndex) {
            m_assemblyParts.append({TopoDS_Shape(), Graphic3d_NOM_PLASTIC, QVariantMap()});
        }
        Graphic3d_NameOfMaterial mat = Graphic3d_NOM_STONE;
        if (assemblyIndex == 6 || assemblyIndex == 7)
            mat = Graphic3d_NOM_STEEL;

        m_assemblyParts[assemblyIndex] = {shape, mat, metadata};
        m_completedTasks++;

        if (m_completedTasks == 9) {
            m_occtWidget->buildFullBridgeFromParts(m_assemblyParts, m_bridgePierCount, m_bridgePierSpacing);
            statusBar()->showMessage(QString("全桥拼装完成，用时 %1 ms").arg(m_batchTimer.elapsed()), 10000);
            m_isAssembling = false;
            m_occtWidget->fitAll();
        } else {
            dispatchTask();
        }
    } else if (m_isBatchProcessing) {
        if (!shape.IsNull()) {
            m_batchParts.append({shape, m_currentMaterial, metadata});
        }
        m_completedTasks++;
        statusBar()->showMessage(QString("正在并发生成: %1/%2").arg(m_completedTasks).arg(m_bridgePierCount));

        if (m_completedTasks == m_bridgePierCount) {
            m_isBatchProcessing = false;
            m_occtWidget->buildFullBridgeFromBatch(m_batchParts);
            m_occtWidget->fitAll();
            statusBar()->showMessage(QString("全桥生成完毕. 耗时: %1 ms").arg(m_batchTimer.elapsed()), 10000);
        }
    } else {
        m_occtWidget->clearAll();
        m_occtWidget->displayShape(shape, Graphic3d_NOM_PLASTIC, true, metadata);
        statusBar()->showMessage("模型生成成功", 3000);
    }
}

QString MainWindow::readScript(const QString &modelName) {
    QStringList searchPaths;
    QString fileName = modelName + ".py";
    searchPaths << QApplication::applicationDirPath() + "/cq_script/" + fileName;
    searchPaths << QApplication::applicationDirPath() + "/../cq_script/" + fileName;
    searchPaths << QApplication::applicationDirPath() + "/../../cq_script/" + fileName;
    searchPaths << QDir::currentPath() + "/cq_script/" + fileName;
    searchPaths << QDir::currentPath() + "/../cq_script/" + fileName;

    for (const QString &path : searchPaths) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qDebug() << "Successfully loaded script from:" << path;
            return QString::fromUtf8(file.readAll());
        }
    }
    qWarning() << "Failed to find script file" << fileName << "in searched paths:" << searchPaths;
    return "";
}

void MainWindow::onDrawFullBridgePier() {
    m_occtWidget->clearAll();
    m_isAssembling = true;
    m_isBatchProcessing = false;
    m_bridgePierCount = 1;
    m_bridgePierSpacing = 0.0;
    m_completedTasks = 0;
    m_batchParts.clear();
    m_assemblyParts.clear();

    m_batchQueue.clear();
    for (int i = 0; i < 9; ++i) {
        m_batchQueue.enqueue(i);
    }

    statusBar()->showMessage("正在通过微服务分项构建全要素桥墩...");
    m_batchTimer.start();

    int initialCount = qMin(5, m_batchQueue.size());
    for (int i = 0; i < initialCount; ++i) {
        dispatchTask();
    }
}

void MainWindow::onAnnotateBridgePierFooting() {
    m_occtWidget->annotateBridgePierFooting();
    statusBar()->showMessage("已添加承台长宽高尺寸标注", 3000);
}

void MainWindow::onDrawFoundation() {
    m_currentModelType = "lightning_rod_foundation";
    m_cqScriptEditor->setText(readScript(m_currentModelType));
    onRunCqScript();
    statusBar()->showMessage("避雷针基础脚本已加载并运行", 3000);
}

void MainWindow::onDrawBedStone() {
    m_currentModelType = "BedStone";
    m_cqScriptEditor->setText(readScript(m_currentModelType));
    onRunCqScript();
    statusBar()->showMessage("垫石脚本已加载并运行", 3000);
}

void MainWindow::onDrawBearing() {
    m_currentModelType = "bearing";
    m_cqScriptEditor->setText(readScript(m_currentModelType));
    onRunCqScript();
    statusBar()->showMessage("支座脚本已加载并运行", 3000);
}

void MainWindow::onExportStepClicked() {
    QString filename = QFileDialog::getSaveFileName(this, "导出为 STEP 文件", "", "STEP 文件 (*.step *.stp);;所有文件 (*.*)");
    if (!filename.isEmpty()) {
        m_occtWidget->exportToSTEP(filename);
    }
}

void MainWindow::onExportGltfClicked() {
    QString filename = QFileDialog::getSaveFileName(this, "导出为 GLTF 文件", "", "GLTF 文件 (*.gltf *.glb);;所有文件 (*.*)");
    if (!filename.isEmpty()) {
        m_occtWidget->exportToGLTF(filename);
    }
}

void MainWindow::dispatchTask(int) {
    if (m_batchQueue.isEmpty()) return;
    int index = m_batchQueue.dequeue();

    QString modelName;
    QJsonObject args;
    args["pierHeight"] = m_pierHeightSpinBox->value();

    if (m_isAssembling) {
        switch (index) {
            case 0: modelName = "Pile"; break;
            case 1: modelName = "PileCap"; break;
            case 2: modelName = "PierBody"; break;
            case 3: modelName = "PierTray"; break;
            case 4:
            case 5: modelName = "BedStone"; break;
            case 6:
            case 7: modelName = "bearing"; break;
            case 8: modelName = "Girder"; break;
            default:
                qWarning() << "Unknown assembly index:" << index;
                return;
        }
        QString code = readScript(modelName);
        sendScriptToMicroservice(code, args, index, modelName);
    } else {
        modelName = "BridgePier2";
        QString code = readScript(modelName);
        args["yOffset"] = index * m_bridgePierSpacing;
        sendScriptToMicroservice(code, args, index, modelName);
    }
}

void MainWindow::onExportIfcClicked() {
  if (m_currentModel.IsNull()) {
    QMessageBox::warning(this, "Export IFC", "No model loaded.");
    return;
  }

  QString filename = QFileDialog::getSaveFileName(this, "Export IFC 4x3", "", "IFC Files (*.ifc)");
  if (filename.isEmpty()) return;

  if (IfcExportService::Export(m_currentModel, filename.toStdString())) {
    QMessageBox::information(this, "Export IFC", "Successfully exported to " + filename);
  } else {
    QMessageBox::critical(this, "Export IFC", "Failed to export IFC. Please check logs.");
  }
}

void MainWindow::onCloseModel() {
    m_occtWidget->clearAll();
    m_occtWidget2D->clearAll();
    m_occtWidget2D->hide();
    m_modelExplorerDock->setModel(nullptr);
    m_currentModel.Nullify();
    updatePropertyPanelUI(QVariantMap());
    m_loadedMasterPath = "";
    if (m_tempProjDir) {
        delete m_tempProjDir;
        m_tempProjDir = nullptr;
    }
    if (m_projectManager) {
        delete m_projectManager;
        m_projectManager = nullptr;
    }
}

void MainWindow::onPropertyValueChanged(const QString& nodeId, const QString& propertyName, const QString& newValue) {
    qDebug() << "[GUI] onPropertyValueChanged called for Node:" << nodeId << "Prop:" << propertyName << "NewVal:" << newValue;
    
    if (!m_projectManager) {
        qDebug() << "[GUI] ERROR: m_projectManager is null";
        return;
    }
    
    std::vector<std::pair<Handle(ActAPI_INode), Handle(DataModel)>> matchingNodes = m_projectManager->FindNodesAcrossModels(nodeId.toStdString());
    if (matchingNodes.empty()) {
        qDebug() << "[GUI] ERROR: Could not find node with ID" << nodeId;
        return;
    }
    
    Handle(BrNode_adProperty) targetProp;
    Handle(DataModel) model;
    Handle(ActAPI_INode) node;
    
    for (const auto& pair : matchingNodes) {
        Handle(ActAPI_INode) candidateNode = pair.first;
        Handle(DataModel) candidateModel = pair.second;
        
        Handle(BrNode_adObject) adObj = Handle(BrNode_adObject)::DownCast(candidateNode);
        if (adObj.IsNull() || candidateModel.IsNull()) continue;
        
        // Find property
        NCollection_Sequence<Handle(BrNode_adPropertySet)> psets = adObj->GetPropertySetsList();
        for (int i = 1; i <= psets.Length() && targetProp.IsNull(); ++i) {
            Handle(BrNode_adPropertySet) pset = psets.Value(i);
            if (pset.IsNull()) continue;
            NCollection_Sequence<Handle(BrNode_adProperty)> props = pset->GetPropertiesList();
            for (int j = 1; j <= props.Length(); ++j) {
                Handle(BrNode_adProperty) prop = props.Value(j);
                if (!prop.IsNull()) {
                    QString pName = QString::fromUtf16((const char16_t*)prop->GetPropertyName().ToExtString());
                    if (pName == propertyName) {
                        targetProp = prop;
                        model = candidateModel;
                        node = candidateNode;
                        qDebug() << "[GUI] Property MATCH found in model with ID" << nodeId;
                        break;
                    }
                }
            }
        }
        if (!targetProp.IsNull()) break;
    }
    
    if (targetProp.IsNull()) {
        // If not found in property sets, check if it's a direct property of a 2D node (e.g. adSlopeIndication)
        Handle(BrNode_adSlopeIndication) slope2D = Handle(BrNode_adSlopeIndication)::DownCast(node);
        if (!slope2D.IsNull()) {
            if (propertyName == "Length") {
                QString targetGuid = QString::fromUtf16((const char16_t*)slope2D->GetTargetObjectID().ToExtString());
                onPropertyValueChanged(targetGuid, propertyName, newValue);
                return;
            }
            model->OpenCommand();
            if (propertyName == "Spacing") slope2D->SetSpacing(newValue.toDouble());
            else if (propertyName == "LongLineRatio") slope2D->SetLongLineRatio(newValue.toDouble());
            else if (propertyName == "ShortLineRatio") slope2D->SetShortLineRatio(newValue.toDouble());
            model->CommitCommand();
            refreshViews();
        }
        return;
    }
    
    // Update value
    QString oldVal = QString::fromUtf16((const char16_t*)targetProp->GetPropertyValue().ToExtString());
    if (oldVal == newValue || (oldVal.toDouble() == newValue.toDouble())) {
        qDebug() << "[GUI] Property" << propertyName << "unchanged, skipping rebuild.";
        return;
    }
    
    model->OpenCommand();
    targetProp->SetPropertyValue(TCollection_ExtendedString(newValue.toStdString().c_str()));
    model->CommitCommand();
    
    qDebug() << "[GUI] Property" << propertyName << "updated to" << newValue << "for node" << nodeId;
    
    // Re-build geometry and refresh views
    Handle(BrNode_adObject) adObj = Handle(BrNode_adObject)::DownCast(node);
    if (!adObj.IsNull() && QString::fromUtf16((const char16_t*)adObj->GetObjectType().ToExtString()) == "SubgradeSlope") {
        double L = 20000.0, H = 8000.0, Ratio = 1.5;
        NCollection_Sequence<Handle(BrNode_adPropertySet)> psets = adObj->GetPropertySetsList();
        for (int i = 1; i <= psets.Length(); ++i) {
            Handle(BrNode_adPropertySet) pset = psets.Value(i);
            if (!pset.IsNull() && QString::fromUtf16((const char16_t*)pset->GetName().ToExtString()) == "Pset_SlopeGeometry") {
                NCollection_Sequence<Handle(BrNode_adProperty)> props = pset->GetPropertiesList();
                for (int k = 1; k <= props.Length(); ++k) {
                    QString pn = QString::fromUtf16((const char16_t*)props.Value(k)->GetPropertyName().ToExtString());
                    QString pv = QString::fromUtf16((const char16_t*)props.Value(k)->GetPropertyValue().ToExtString());
                    if (pn == "Length") L = pv.toDouble();
                    else if (pn == "Height") H = pv.toDouble();
                    else if (pn == "Ratio") Ratio = pv.toDouble();
                }
                break;
            }
        }
        
        SlopeWires initWires3D = CreateSlopeWiresLocal(L, H, Ratio);
        Handle(RwSlopeGeometryBuilder) builder3D = new RwSlopeGeometryBuilder(
            initWires3D.shoulder, initWires3D.toe, 2000.0, 0.6, 0.3
        );
        TopoDS_Shape shape3D = builder3D->Build(RwBuilder::Rep_3D_Solid);
        
        model->OpenCommand();
        Handle(BrNode_adGeometry) geoNode = Handle(BrNode_adGeometry)::DownCast(adObj->GetGeometry());
        if (!geoNode.IsNull() && !geoNode->GetGeometryRef().IsNull()) {
            Handle(BrNode_adGeometricDef) geoDef = Handle(BrNode_adGeometricDef)::DownCast(geoNode->GetGeometryRef());
            if (!geoDef.IsNull()) {
                geoDef->SetShape(shape3D);
            }
        }
        model->CommitCommand();
    }
    
    refreshViews();
}

void MainWindow::refreshViews() {
    if (!m_projectManager || m_currentModel.IsNull()) return;
    
    qDebug() << "[GUI] Refreshing views...";
    
    // Rebuild and display 3D geometries
    m_occtWidget->clearAll();
    
    // In master mode, we need to gather from loaded sub-models
    std::string path3D, path2D;
    std::vector<SceneDataExtractor::VisualShape> visualShapes;
    Handle(BrNode_adModelRoot) rootNode = Handle(BrNode_adModelRoot)::DownCast(m_currentModel->GetRootNode());
    if (!rootNode.IsNull()) {
        NCollection_Sequence<Handle(BrNode_adSubDocRef)> subDocs = rootNode->GetSubDocRefsList();
        if (subDocs.IsEmpty() && !m_currentModel.IsNull()) {
            Handle(ActAPI_IPartition) part = m_currentModel->Partition(2);
            if (!part.IsNull()) {
                for (ActData_BasePartition::Iterator pit(part); pit.More(); pit.Next()) {
                    Handle(BrNode_adSubDocRef) child = Handle(BrNode_adSubDocRef)::DownCast(pit.Value());
                    if (!child.IsNull()) {
                        Handle(ActAPI_INode) p = child->GetParentNode();
                        if (!p.IsNull() && p->GetId() == rootNode->GetId()) {
                            subDocs.Append(child);
                        }
                    }
                }
            }
        }
        for (int i = 1; i <= subDocs.Length(); ++i) {
            Handle(BrNode_adSubDocRef) refNode = subDocs.Value(i);
            if (!refNode.IsNull()) {
                QString docType = QString::fromUtf16((const char16_t*)refNode->GetDocType().ToExtString());
                QString docPath = QString::fromUtf16((const char16_t*)refNode->GetDocPath().ToExtString());
                if (docType == "3DModel") path3D = docPath.toStdString();
                else if (docType == "2DDrawing") path2D = docPath.toStdString();
            }
        }
    }
    
    if (path3D.empty()) path3D = "models/subgrade_3d.cbf";
    if (path2D.empty()) path2D = "drawings/plan_view.cbf";
    
    m_projectManager->GetOrLoadSubDocument(path3D);
    Handle(DataModel) model3D = m_projectManager->GetSubModel(path3D);
    if (!model3D.IsNull()) {
        model3D->OpenCommand();
        try {
            qDebug() << "[GUI] Extracting 3D geometries for rendering...";
            SceneDataExtractor::Extract(model3D, visualShapes);
            qDebug() << "[GUI] 3D geometries extracted, visualShapes count:" << visualShapes.size();
        } catch (...) {
            qDebug() << "[GUI] Exception during 3D geometry rebuild";
        }
        model3D->CommitCommand();
        qDebug() << "[GUI] CommitCommand done for 3D model.";
        
    }
    // Display in 3D widget
    auto convertJsonToQVariantMap = [](const nlohmann::json& j) -> QVariantMap {
        std::function<QVariantMap(const nlohmann::json&)> convert = [&](const nlohmann::json& js) -> QVariantMap {
            QVariantMap map;
            for (auto it = js.begin(); it != js.end(); ++it) {
                QString key = QString::fromStdString(it.key());
                if (it.value().is_string()) map[key] = QString::fromStdString(it.value().get<std::string>());
                else if (it.value().is_number_float()) map[key] = it.value().get<double>();
                else if (it.value().is_number_integer()) map[key] = it.value().get<int>();
                else if (it.value().is_boolean()) map[key] = it.value().get<bool>();
                else if (it.value().is_object()) map[key] = convert(it.value());
                else if (it.value().is_array()) {
                    QVariantList list;
                    for (const auto& item : it.value()) {
                        if (item.is_number()) list.append(item.get<double>());
                        else if (item.is_string()) list.append(QString::fromStdString(item.get<std::string>()));
                    }
                    map[key] = list;
                }
            }
            return map;
        };
        return convert(j);
    };

    for (const auto& vs : visualShapes) {
        if (vs.shape.IsNull()) continue;
        TopoDS_Shape transformedShape = vs.shape;
        try {
            BRepBuilderAPI_Transform trans(vs.shape, vs.transform);
            transformedShape = trans.Shape();
        } catch (...) {}

        QVariantMap meta = convertJsonToQVariantMap(vs.metadata);
        QVariantList trsfList;
        trsfList << vs.transform.Value(1, 1) << vs.transform.Value(1, 2) << vs.transform.Value(1, 3) << vs.transform.Value(1, 4)
                 << vs.transform.Value(2, 1) << vs.transform.Value(2, 2) << vs.transform.Value(2, 3) << vs.transform.Value(2, 4)
                 << vs.transform.Value(3, 1) << vs.transform.Value(3, 2) << vs.transform.Value(3, 3) << vs.transform.Value(3, 4);
        meta["_globalTrsf"] = trsfList;
        Quantity_Color color(Quantity_NOC_GRAY75);
        if (meta.contains("Pset_MaterialPBR")) {
            QVariantMap pbr = meta["Pset_MaterialPBR"].toMap();
            if (pbr.contains("BaseColor")) {
                QVariantList colorList = pbr["BaseColor"].toList();
                if (colorList.size() >= 3) {
                    color = Quantity_Color(colorList[0].toDouble(), colorList[1].toDouble(), colorList[2].toDouble(), Quantity_TOC_RGB);
                }
            }
        }
        qDebug() << "[GUI] displayShape for shape index";
        m_occtWidget->displayShape(transformedShape, Graphic3d_NOM_PLASTIC, color, false, meta);
    }
    qDebug() << "[GUI] Calling fitAll on 3D widget";
    m_occtWidget->fitAll();
    
    qDebug() << "[GUI] Starting Sync and refresh 2D view";
    // Sync and refresh 2D view
    m_projectManager->GetOrLoadSubDocument(path2D);
    Handle(DataModel) drawingModel = m_projectManager->GetSubModel(path2D);
    if (!drawingModel.IsNull()) {
        Handle(ActAPI_IPartition) topoPart = drawingModel->Partition(2);
        if (!topoPart.IsNull()) {
            for (ActData_BasePartition::Iterator it(topoPart); it.More(); it.Next()) {
                Handle(BrNode_adDrawing2D) drawNode = Handle(BrNode_adDrawing2D)::DownCast(it.Value());
                if (!drawNode.IsNull()) {
                    m_projectManager->Sync2DDrawing(drawNode);
                    break;
                }
            }
        }
        
        // Reload into 2D view
        Handle(TDocStd_Document) drawingDoc = m_projectManager->GetOrLoadSubDocument(path2D);
        if (!drawingDoc.IsNull()) {
            m_occtWidget2D->clearAll();
            
            // Render 2D geometry from active data models
            if (!topoPart.IsNull()) {
                for (ActData_BasePartition::Iterator it(topoPart); it.More(); it.Next()) {
                    Handle(BrNode_adRepresentation2D) repNode = Handle(BrNode_adRepresentation2D)::DownCast(it.Value());
                    if (!repNode.IsNull()) {
                        TopoDS_Shape shape = repNode->GetGeneratedShape();
                        if (!shape.IsNull()) {
                            QVariantMap meta;
                            meta["name"] = QString::fromUtf16((const char16_t*)repNode->GetName().ToExtString());
                            
                            // Check if it is a slope indication and get the target 3D object GUID
                            Handle(BrNode_adSlopeIndication) slopeNode = Handle(BrNode_adSlopeIndication)::DownCast(repNode);
                            if (!slopeNode.IsNull()) {
                                QString targetGuid = QString::fromUtf16((const char16_t*)slopeNode->GetTargetObjectID().ToExtString());
                                if (!targetGuid.isEmpty()) {
                                    meta["_adNodeId"] = targetGuid;
                                }
                            }
                            
                            m_occtWidget2D->displayShape(shape, Graphic3d_NOM_PLASTIC, true, meta);
                        }
                    }
                }
            }
            
            // 为二维示坡线添加长度标注
            Handle(BrNode_adObject) found3DObj;
            if (!model3D.IsNull()) {
                Handle(ActAPI_IPartition) topologyPart = model3D->Partition(2);
                if (!topologyPart.IsNull()) {
                    for (ActData_BasePartition::Iterator it(topologyPart); it.More(); it.Next()) {
                        Handle(BrNode_adObject) obj = Handle(BrNode_adObject)::DownCast(it.Value());
                        if (!obj.IsNull() && QString::fromUtf16((const char16_t*)obj->GetObjectType().ToExtString()) == "SubgradeSlope") {
                            found3DObj = obj;
                            break;
                        }
                    }
                }
            }
            if (!found3DObj.IsNull()) {
                double L = 20000.0;
                NCollection_Sequence<Handle(BrNode_adPropertySet)> psets = found3DObj->GetPropertySetsList();
                for (int p = 1; p <= psets.Length(); ++p) {
                    Handle(BrNode_adPropertySet) pset = psets.Value(p);
                    if (!pset.IsNull() && QString::fromUtf16((const char16_t*)pset->GetName().ToExtString()) == "Pset_SlopeGeometry") {
                        NCollection_Sequence<Handle(BrNode_adProperty)> props = pset->GetPropertiesList();
                        for (int k = 1; k <= props.Length(); ++k) {
                            if (QString::fromUtf16((const char16_t*)props.Value(k)->GetPropertyName().ToExtString()) == "Length") {
                                L = QString::fromUtf16((const char16_t*)props.Value(k)->GetPropertyValue().ToExtString()).toDouble();
                                break;
                            }
                        }
                        break;
                    }
                }
                
                gp_Pnt p1(0, 0, 0);
                gp_Pnt p2(L, 0, 0);
                gp_Pln plane(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1));
                // 向上偏移标注
                m_occtWidget2D->addLengthDimension(p1, p2, plane, -1500.0);
            }
            
            m_occtWidget2D->fitAll();
        }
    }
    
    m_modelExplorerDock->setModel(m_currentModel);
}

void MainWindow::onComponentSelected(const QString& category, const QString& name) {
    statusBar()->showMessage(tr("正在加载构件: %1 ...").arg(name));
    
    if (category == "parametric") {
        sendScriptToMicroservice("", QJsonObject(), -1, name);
    } else {
        QJsonObject args;
        args["ComponentFile"] = name;
        sendScriptToMicroservice("", args, -1, "NonParametric");
    }
}

bool MainWindow::openProjectFile(const QString &fileName) {
    if (fileName.endsWith(".rde", Qt::CaseInsensitive)) {
        qDebug() << "Opening RDE package:" << fileName;
        
        if (m_tempProjDir) {
            delete m_tempProjDir;
            m_tempProjDir = nullptr;
        }
        
        m_tempProjDir = new QTemporaryDir();
        if (!m_tempProjDir->isValid()) {
            QMessageBox::critical(this, "Error", "Failed to create temporary directory for extraction.");
            delete m_tempProjDir;
            m_tempProjDir = nullptr;
            return false;
        }
        
        QString tempPath = m_tempProjDir->path();
        qDebug() << "Temporary directory created:" << tempPath;
        
        if (!extractRdeFile(fileName, tempPath)) {
            QMessageBox::critical(this, "Error", "Failed to extract RDE package.");
            delete m_tempProjDir;
            m_tempProjDir = nullptr;
            return false;
        }
        
        QString masterPath = tempPath + "/master.cbf";
        if (!QFile::exists(masterPath)) {
            QDirIterator it(tempPath, QStringList() << "master.cbf", QDir::Files, QDirIterator::Subdirectories);
            if (it.hasNext()) {
                masterPath = it.next();
            } else {
                QMessageBox::critical(this, "Error", "Could not find master.cbf inside the RDE package.");
                delete m_tempProjDir;
                m_tempProjDir = nullptr;
                return false;
            }
        }
        
        return loadMasterCbf(masterPath);
    } else {
        if (m_tempProjDir) {
            delete m_tempProjDir;
            m_tempProjDir = nullptr;
        }
        return loadMasterCbf(fileName);
    }
}

bool MainWindow::extractRdeFile(const QString &rdePath, const QString &destDir) {
    QProcess process;
    QStringList arguments;
    arguments << "-xf" << QDir::toNativeSeparators(rdePath) 
              << "-C" << QDir::toNativeSeparators(destDir);
    
    qDebug() << "Extracting archive using tar:" << "tar" << arguments;
    process.start("tar", arguments);
    if (!process.waitForFinished(10000)) {
        qWarning() << "Failed to extract RDE file: tar process timeout";
        return false;
    }
    if (process.exitCode() != 0) {
        qWarning() << "tar exited with code:" << process.exitCode() << process.readAllStandardError();
        return false;
    }
    return true;
}

bool MainWindow::packageToRde(const QString &sourceDir, const QString &rdePath) {
    QString tempZip = QDir::tempPath() + "/temp_rde_pkg.zip";
    if (QFile::exists(tempZip)) {
        QFile::remove(tempZip);
    }
    
    QProcess process;
    QStringList arguments;
    arguments << "-cf" << QDir::toNativeSeparators(tempZip)
              << "-C" << QDir::toNativeSeparators(sourceDir)
              << ".";
              
    qDebug() << "Packaging using tar:" << "tar" << arguments;
    process.start("tar", arguments);
    if (!process.waitForFinished(15000)) {
        qWarning() << "Failed to package: tar process timeout";
        return false;
    }
    if (process.exitCode() != 0) {
        qWarning() << "tar package exited with code:" << process.exitCode() << process.readAllStandardError();
        return false;
    }
    
    if (QFile::exists(rdePath)) {
        QFile::remove(rdePath);
    }
    
    bool success = QFile::copy(tempZip, rdePath);
    QFile::remove(tempZip);
    return success;
}

void MainWindow::onExportRdeClicked() {
    if (m_loadedMasterPath.isEmpty()) {
        QMessageBox::warning(this, "Export RDE", "No master project currently loaded to package.");
        return;
    }
    
    QString savePath = QFileDialog::getSaveFileName(this, "Export RDE Package", "", "RDE Packages (*.rde)");
    if (savePath.isEmpty()) return;
    
    QFileInfo fileInfo(m_loadedMasterPath);
    QString sourceDir = fileInfo.absolutePath();
    
    QApplication::setOverrideCursor(Qt::WaitCursor);
    bool success = packageToRde(sourceDir, savePath);
    QApplication::restoreOverrideCursor();
    
    if (success) {
        QMessageBox::information(this, "Export RDE", "Successfully packaged master project to " + savePath);
    } else {
        QMessageBox::critical(this, "Export RDE", "Failed to package project. Please check if tar utility is available.");
    }
}

