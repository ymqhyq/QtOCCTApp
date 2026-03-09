#include "../include/MainWindow.h"
#include "../include/OCCTWidget.h"
#include "SARibbonApplicationButton.h"
#include "SARibbonBar.h"
#include "SARibbonCategory.h"
#include "SARibbonPanel.h"
#include <BRepBuilderAPI_Transform.hxx>

#include "../include/PythonSyntaxHighlighter.h"
#include "../include/ShxTextGenerator.h"
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

MainWindow::MainWindow(QWidget *parent)
    : SARibbonMainWindow(parent), m_occtWidget(new OCCTWidget(this)),
      m_solidTextCheckbox(nullptr), m_coordLabel(nullptr),
      m_highlighter(nullptr), m_currentMaterial(Graphic3d_NOM_PLASTIC),
      m_propertyDock(nullptr), m_propertyWidget(nullptr),
      m_propertyLayout(nullptr), m_currentModelType("BridgePier2") {
  setWindowTitle("Qt OCCT Application - Schema Enabled");
  setMinimumSize(1024, 768);
  showMaximized();

  // Set the OCCT widget as the central widget
  setCentralWidget(m_occtWidget);

  // Create the ribbon
  createRibbon();
  setupCadQueryUi();

  QStatusBar *sBar = new QStatusBar(this);
  setStatusBar(sBar);

  // 创建坐标显示标签
  m_coordLabel = new QLabel("X: 0.000  Y: 0.000  Z: 0.000", this);
  m_coordLabel->setMinimumWidth(280);
  sBar->addPermanentWidget(m_coordLabel);

  // 连接鼠标位置信号
  connect(m_occtWidget, &OCCTWidget::mousePositionChanged, this,
          &MainWindow::onMousePositionChanged);

  // 连接对象选中信号
  connect(m_occtWidget, &OCCTWidget::objectSelected, this,
          &MainWindow::onObjectSelected);

  initializeCqNetwork();
}

MainWindow::~MainWindow() {
  // Network connections handle their own cleanup
}

void MainWindow::createRibbon() {
  SARibbonBar *ribbon = ribbonBar();
  ribbon->applicationButton()->setText("QtOCCTApp");

  // Create "Main Tools" category
  SARibbonCategory *categoryMain = ribbon->addCategoryPage("Main Tools");

  // Basic Tasks Panel
  SARibbonPanel *panelBasic = categoryMain->addPanel("Basic");

  QAction *drawAction =
      new QAction(QIcon(":/resources/icons/draw_line.svg"), "Draw Line", this);
  connect(drawAction, &QAction::triggered, this,
          &MainWindow::onDrawLineClicked);
  panelBasic->addLargeAction(drawAction);

  QAction *randLineAction = new QAction(QIcon(":/resources/icons/random.svg"),
                                        "Generate 10k Lines", this);
  connect(randLineAction, &QAction::triggered,
          [this]() { m_occtWidget->generateRandomLines(10000); });
  panelBasic->addLargeAction(randLineAction);

  QAction *exportStepAction =
      new QAction(QIcon(":/resources/icons/export.svg"), "Export STEP", this);
  connect(exportStepAction, &QAction::triggered, this,
          &MainWindow::onExportStepClicked);
  panelBasic->addLargeAction(exportStepAction);

  QAction *exportGltfAction =
      new QAction(QIcon(":/resources/icons/export.svg"), "Export GLTF", this);
  connect(exportGltfAction, &QAction::triggered, this,
          &MainWindow::onExportGltfClicked);
  panelBasic->addLargeAction(exportGltfAction);

  // View Panel
  SARibbonPanel *panelView = categoryMain->addPanel("View");
  QAction *fitAllAction =
      new QAction(QIcon(":/resources/icons/fit_all.svg"), "Fit All", this);
  connect(fitAllAction, &QAction::triggered,
          [this]() { m_occtWidget->fitAll(); });
  panelView->addLargeAction(fitAllAction);

  // Text Panel
  SARibbonPanel *panelText = categoryMain->addPanel("Text");
  QAction *shxTextAction = new QAction(QIcon(":/resources/icons/text_shx.svg"),
                                       "Add SHX Text", this);
  connect(shxTextAction, &QAction::triggered, this, &MainWindow::onAddShxText);
  panelText->addLargeAction(shxTextAction);

  QAction *brepTextAction = new QAction(
      QIcon(":/resources/icons/text_brep.svg"), "Add BRep Text", this);
  connect(brepTextAction, &QAction::triggered, [this]() {
    bool isSolid = m_solidTextCheckbox->isChecked();
    double angle = 45.0; // Keep the angle test
    m_occtWidget->add3DText("京沪D1K234+869.75", 50.0, gp_Pnt(0, 0, 0), isSolid,
                            angle);
  });
  panelText->addLargeAction(brepTextAction);

  m_solidTextCheckbox = new QCheckBox("Test Solid Text", this);
  m_solidTextCheckbox->setChecked(true); // Default to solid
  connect(m_solidTextCheckbox, &QCheckBox::stateChanged, [this](int state) {
    m_occtWidget->setTextsSolid(state == Qt::Checked);
  });
  panelText->addWidget(m_solidTextCheckbox, SARibbonPanelItem::Small);

  // Bridge Generation Category
  SARibbonCategory *categoryBridge = ribbon->addCategoryPage("Bridge Tools");
  SARibbonPanel *panelBridge = categoryBridge->addPanel("Bridge");

  QLabel *heightLabel = new QLabel("墩高(mm):", this);
  m_pierHeightSpinBox = new QDoubleSpinBox(this);
  m_pierHeightSpinBox->setRange(10.0, 50000.0);
  m_pierHeightSpinBox->setValue(12000.0);
  panelBridge->addWidget(heightLabel, SARibbonPanelItem::Small);
  panelBridge->addWidget(m_pierHeightSpinBox, SARibbonPanelItem::Small);

  QAction *bridgePierAction =
      new QAction(QIcon(":/resources/icons/bridge_pier.svg"), "绘制桥墩", this);
  connect(bridgePierAction, &QAction::triggered, [this]() {
    m_currentModelType = "BridgePier2";
    onDrawBridgePier();
  });
  panelBridge->addLargeAction(bridgePierAction);

  QAction *fullBridgePierAction = new QAction(
      QIcon(":/resources/icons/bridge_pier.svg"), "完全体桥墩", this);
  connect(fullBridgePierAction, &QAction::triggered, [this]() {
    m_currentModelType = "BridgePier2";
    onDrawFullBridgePier();
  });
  panelBridge->addLargeAction(fullBridgePierAction);

  QAction *annotatePierAction =
      new QAction(QIcon(":/resources/icons/dimension.svg"), "标注承台", this);
  connect(annotatePierAction, &QAction::triggered, this,
          &MainWindow::onAnnotateBridgePierFooting);
  panelBridge->addLargeAction(annotatePierAction);

  QAction *fullBridgeAction = new QAction(
      QIcon(":/resources/icons/full_bridge.svg"), "全桥(100墩)", this);
  connect(fullBridgeAction, &QAction::triggered, [this]() {
    m_occtWidget->clearAll();
    m_isBatchProcessing = true;
    m_isAssembling = false; // 确保不会误入极速装配分支
    m_currentPierIndex = 0;
    m_bridgePierCount = 100;
    m_bridgePierSpacing = 31600.0; // 31.6m spacing (31.5m girder + 10cm gap)
    m_currentMaterial = Graphic3d_NOM_STONE;
    m_completedTasks = 0;
    m_batchParts.clear();

    m_batchQueue.clear();
    for (int i = 0; i < m_bridgePierCount; ++i) {
      m_batchQueue.enqueue(i);
    }

    statusBar()->showMessage(
        QString("全部并发生成中: 共 %1 个桥墩已全部发送至微服务...")
            .arg(m_bridgePierCount));
    m_batchTimer.start(); // 开始计时

    // 一次性发出所有任务，服务端异步并发处理
    while (!m_batchQueue.isEmpty()) {
      dispatchTask();
    }
  });
  panelBridge->addLargeAction(fullBridgeAction);

  QAction *fastAssemAction = new QAction(
      QIcon(":/resources/icons/fast_assembly.svg"), "全桥-C++极速", this);
  connect(fastAssemAction, &QAction::triggered, [this]() {
    m_occtWidget->clearAll();
    m_isAssembling = true;
    m_isBatchProcessing = false;
    m_bridgePierCount = 300;
    m_bridgePierSpacing = 31600.0; // 31.6m spacing (31.5m girder + 10cm gap)
    m_completedTasks = 0;
    m_assemblyParts.clear();
    m_batchQueue.clear();
    for (int i = 0; i < 9; ++i) { // 桩、承台、墩身、托盘、垫石x2、支座x2、箱梁
      m_batchQueue.enqueue(i);
    }

    statusBar()->showMessage(QString("准备基础构件中: 正在调用后台微服务..."));
    m_batchTimer.start();

    // 发送并发拼装任务请求
    int initialTasks = qMin(9, m_batchQueue.size());
    for (int i = 0; i < initialTasks; ++i) {
      dispatchTask();
    }
  });
  panelBridge->addLargeAction(fastAssemAction);

  SARibbonPanel *panelSubCrops = categoryBridge->addPanel("Sub-components");

  QAction *tuopanAction =
      new QAction(QIcon(":/resources/icons/tuopan.svg"), "顶帽与托盘", this);
  connect(tuopanAction, &QAction::triggered, [this]() {
    m_currentModelType = "TuopanDingmao";
    m_cqScriptEditor->setText(readScript(m_currentModelType));
    onRunCqScript();
  });
  panelSubCrops->addSmallAction(tuopanAction);

  QAction *dunshenAction =
      new QAction(QIcon(":/resources/icons/dunshen.svg"), "墩身", this);
  connect(dunshenAction, &QAction::triggered, [this]() {
    m_currentModelType = "Dunshen";
    m_cqScriptEditor->setText(readScript(m_currentModelType));
    onRunCqScript();
  });
  panelSubCrops->addSmallAction(dunshenAction);

  QAction *chengtaiAction =
      new QAction(QIcon(":/resources/icons/chengtai.svg"), "承台", this);
  connect(chengtaiAction, &QAction::triggered, [this]() {
    m_currentModelType = "Chengtai";
    m_cqScriptEditor->setText(readScript(m_currentModelType));
    onRunCqScript();
  });
  panelSubCrops->addSmallAction(chengtaiAction);

  QAction *pileAction =
      new QAction(QIcon(":/resources/icons/pile.svg"), "桩基础", this);
  connect(pileAction, &QAction::triggered, [this]() {
    m_currentModelType = "Pile";
    m_cqScriptEditor->setText(readScript(m_currentModelType));
    onRunCqScript();
  });
  panelSubCrops->addSmallAction(pileAction);

  QAction *girderAction =
      new QAction(QIcon(":/resources/icons/girder.svg"), "箱梁", this);
  connect(girderAction, &QAction::triggered, [this]() {
    m_currentModelType = "girder";
    m_cqScriptEditor->setText(readScript(m_currentModelType));
    onRunCqScript();
  });
  panelSubCrops->addSmallAction(girderAction);

  QAction *foundationAction = new QAction(
      QIcon(":/resources/icons/foundation.svg"), "避雷针基础", this);
  connect(foundationAction, &QAction::triggered, this,
          &MainWindow::onDrawFoundation);
  panelSubCrops->addSmallAction(foundationAction);

  QAction *bedStoneAction =
      new QAction(QIcon(":/resources/icons/bed_stone.svg"), "垫石", this);
  connect(bedStoneAction, &QAction::triggered, this,
          &MainWindow::onDrawBedStone);
  panelSubCrops->addSmallAction(bedStoneAction);

  QAction *bearingAction =
      new QAction(QIcon(":/resources/icons/bearing.svg"), "支座", this);
  connect(bearingAction, &QAction::triggered, this, &MainWindow::onDrawBearing);
  panelSubCrops->addSmallAction(bearingAction);
}

void MainWindow::onDrawLineClicked() {
  // Enable line drawing mode in the OCCT widget
  m_occtWidget->setDrawLineMode(true);
  m_occtWidget->setFocus(); // Make sure the OCCT widget has focus to receive
                            // mouse events
}

void MainWindow::onAddShxText() {
  if (!m_shxGenerator) {
    m_shxGenerator = std::make_unique<ShxTextGenerator>();
    // Use the confirmed fonts TTT.shx (shapes) and hztxt.SHX (BigFont)
    bool ok = m_shxGenerator->loadFont("d:/QtOCCTApp/TTT.shx");
    bool okBig = m_shxGenerator->loadBigFont("d:/QtOCCTApp/hztxt.SHX");

    if (!ok || !okBig) {
      QMessageBox::warning(this, "Font Load Error",
                           "Could not load TTT.shx or hztxt.SHX.");
      return;
    }
  }

  std::string text = "京沪D1K323+569.30";
  gp_Pnt pos(0, 0, 0);
  double textHeight = 200.0;
  double angle = 0.0;
  double widthFactor = 1.0;

  auto result =
      m_shxGenerator->generateText(text, pos, textHeight, angle, widthFactor);
  m_occtWidget->addShape(result.first, Quantity_Color(Quantity_NOC_YELLOW));
}

void MainWindow::onMousePositionChanged(double x, double y, double z) {
  QString coordText = QString("X: %1  Y: %2  Z: %3")
                          .arg(x, 0, 'f', 3)
                          .arg(y, 0, 'f', 3)
                          .arg(z, 0, 'f', 3);
  m_coordLabel->setText(coordText);
}

void MainWindow::onDrawBridgePier() {
  m_occtWidget->drawBridgePier();
  statusBar()->showMessage("桥墩模型已生成", 3000);
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

  // 定义待请求的脚本列表
  // 索引 0-7 分别对应：桩、承台、墩身、托盘、垫石1、垫石2、支座1、支座2
  m_batchQueue.clear();
  for (int i = 0; i < 9; ++i) {
    m_batchQueue.enqueue(i);
  }

  statusBar()->showMessage("正在通过微服务分项构建全要素桥墩...");
  m_batchTimer.start();

  // 手动触发初始任务请求
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
  m_currentModelType = "bed_stone";
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

void MainWindow::setupCadQueryUi() {
  m_dockCq = new QDockWidget("脚本", this);
  m_dockCq->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea |
                            Qt::BottomDockWidgetArea);

  QWidget *content = new QWidget(m_dockCq);
  QVBoxLayout *layout = new QVBoxLayout(content);

  m_cqScriptEditor = new QTextEdit(content);
  // Default Example Script
  m_cqScriptEditor->setText(
      "import cadquery as cq\n"
      "# Create a simple box\n"
      "result = cq.Workplane('XY').box(100, 100, 100).edges().chamfer(10)\n"
      "# result variable is automatically exported\n");
  layout->addWidget(m_cqScriptEditor);

  // Apply Syntax Highlighter
  m_highlighter = new PythonSyntaxHighlighter(m_cqScriptEditor->document());

  // Optional: Set a monospace font for better code look
  QFont font("Consolas", 10);
  font.setStyleHint(QFont::Monospace);
  m_cqScriptEditor->setFont(font);

  SARibbonBar *ribbon = ribbonBar();
  SARibbonCategory *categoryScripts = ribbon->addCategoryPage("脚本 (Scripts)");
  SARibbonPanel *panelScripts = categoryScripts->addPanel("示例代码");

  QAction *runBtn =
      new QAction(QIcon(":/resources/icons/random.svg"), "切角立方体", this);
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

  QAction *holedPlateBtn =
      new QAction(QIcon(":/resources/icons/random.svg"), "带孔板", this);
  connect(holedPlateBtn, &QAction::triggered, [this]() {
    // 使用 Chrome 预设配合深色，可以模拟出更真实的冷钢轨质感
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

  // 桥墩按钮
  QAction *bridgePierCqBtn =
      new QAction(QIcon(":/resources/icons/bridge_pier.svg"), "桥墩示例", this);
  connect(bridgePierCqBtn, &QAction::triggered, [this]() {
    m_currentMaterial = Graphic3d_NOM_PLASTIC;
    m_cqScriptEditor->setText(
        "# 流线型桥墩 - CadQuery/OCP 脚本\n"
        "from OCP.gp import gp_Pnt, gp_Dir, gp_Pln\n"
        "from OCP.GC import GC_MakeArcOfCircle, GC_MakeSegment\n"
        "from OCP.BRepBuilderAPI import (\n"
        "    BRepBuilderAPI_MakeEdge, BRepBuilderAPI_MakeWire,\n"
        "    BRepBuilderAPI_MakeFace)\n"
        "from OCP.BRepOffsetAPI import BRepOffsetAPI_ThruSections\n"
        "from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox\n"
        "from OCP.BRepFeat import BRepFeat_MakePrism\n"
        "from OCP.BRepLib import BRepLib\n"
        "from OCP.BRep import BRep_Builder\n"
        "from OCP.TopoDS import TopoDS_Compound\n"
        "\n"
        "def make_wire(P1, P2, P3, P1_1, P2_1, P3_1,\n"
        "              P1_2, P2_2, P3_2, P1_3, P2_3, P3_3, P3_4,\n"
        "              P1_5, P2_5, P3_5, P1_6, P2_6, P3_6, P3_7):\n"
        "    from OCP.GC import GC_MakeArcOfCircle, GC_MakeSegment\n"
        "    from OCP.BRepBuilderAPI import BRepBuilderAPI_MakeEdge, "
        "BRepBuilderAPI_MakeWire\n"
        "    edges = [\n"
        "        BRepBuilderAPI_MakeEdge(GC_MakeArcOfCircle(P1_1, P2_1, "
        "P3_1).Value()).Edge(),\n"
        "        BRepBuilderAPI_MakeEdge(GC_MakeSegment(P3_1, "
        "P1_6).Value()).Edge(),\n"
        "        BRepBuilderAPI_MakeEdge(GC_MakeArcOfCircle(P1_6, P2_6, "
        "P3_6).Value()).Edge(),\n"
        "        BRepBuilderAPI_MakeEdge(GC_MakeArcOfCircle(P3_6, P3_7, "
        "P3_5).Value()).Edge(),\n"
        "        BRepBuilderAPI_MakeEdge(GC_MakeArcOfCircle(P3_5, P2_5, "
        "P1_5).Value()).Edge(),\n"
        "        BRepBuilderAPI_MakeEdge(GC_MakeSegment(P1_5, "
        "P3).Value()).Edge(),\n"
        "        BRepBuilderAPI_MakeEdge(GC_MakeArcOfCircle(P3, P2, "
        "P1).Value()).Edge(),\n"
        "        BRepBuilderAPI_MakeEdge(GC_MakeSegment(P1, "
        "P1_2).Value()).Edge(),\n"
        "        BRepBuilderAPI_MakeEdge(GC_MakeArcOfCircle(P1_2, P2_2, "
        "P3_2).Value()).Edge(),\n"
        "        BRepBuilderAPI_MakeEdge(GC_MakeArcOfCircle(P3_2, P3_4, "
        "P3_3).Value()).Edge(),\n"
        "        BRepBuilderAPI_MakeEdge(GC_MakeArcOfCircle(P3_3, P2_3, "
        "P1_3).Value()).Edge(),\n"
        "        BRepBuilderAPI_MakeEdge(GC_MakeSegment(P1_3, "
        "P1_1).Value()).Edge(),\n"
        "    ]\n"
        "    mw = BRepBuilderAPI_MakeWire()\n"
        "    for e in edges:\n"
        "        mw.Add(e)\n"
        "    return mw.Wire()\n"
        "\n"
        "def section(z, xr, yr, xr2, yr2, r_inner):\n"
        "    \"\"\"Generate section points at height z\"\"\"\n"
        "    return make_wire(\n"
        "        gp_Pnt(xr, -yr, z), gp_Pnt(xr+xr2-xr, 0, z), gp_Pnt(xr, yr, "
        "z),\n"
        "        gp_Pnt(-xr, -yr, z), gp_Pnt(-(xr+xr2-xr), 0, z), gp_Pnt(-xr, "
        "yr, z),\n"
        "        gp_Pnt(2, -yr, z), gp_Pnt(1.29, -(yr-0.26), z), gp_Pnt(1, "
        "-(yr-1), z),\n"
        "        gp_Pnt(-2, -yr, z), gp_Pnt(-1.29, -(yr-0.26), z), gp_Pnt(-1, "
        "-(yr-1), z),\n"
        "        gp_Pnt(0, -(yr-r_inner), z),\n"
        "        gp_Pnt(2, yr, z), gp_Pnt(1.29, yr-0.26, z), gp_Pnt(1, yr-1, "
        "z),\n"
        "        gp_Pnt(-2, yr, z), gp_Pnt(-1.29, yr-0.26, z), gp_Pnt(-1, "
        "yr-1, z),\n"
        "        gp_Pnt(0, yr-r_inner, z))\n"
        "\n"
        "# 截面1: Z=0 (托盘底部)\n"
        "aWire = make_wire(\n"
        "    gp_Pnt(16,-14,0), gp_Pnt(30,0,0), gp_Pnt(16,14,0),\n"
        "    gp_Pnt(-16,-14,0), gp_Pnt(-30,0,0), gp_Pnt(-16,14,0),\n"
        "    gp_Pnt(2,-14,0), gp_Pnt(1.29,-13.74,0), gp_Pnt(1,-13,0),\n"
        "    gp_Pnt(-2,-14,0), gp_Pnt(-1.29,-13.74,0), gp_Pnt(-1,-13,0),\n"
        "    gp_Pnt(0,-12,0),\n"
        "    gp_Pnt(2,14,0), gp_Pnt(1.29,13.74,0), gp_Pnt(1,13,0),\n"
        "    gp_Pnt(-2,14,0), gp_Pnt(-1.29,13.74,0), gp_Pnt(-1,13,0),\n"
        "    gp_Pnt(0,12,0))\n"
        "\n"
        "# 截面2: Z=27.5 (托盘顶部)\n"
        "bWire = make_wire(\n"
        "    gp_Pnt(24,-15,27.5), gp_Pnt(39,0,27.5), gp_Pnt(24,15,27.5),\n"
        "    gp_Pnt(-24,-15,27.5), gp_Pnt(-39,0,27.5), gp_Pnt(-24,15,27.5),\n"
        "    gp_Pnt(2,-15,27.5), gp_Pnt(1.29,-14.71,27.5), "
        "gp_Pnt(1,-14,27.5),\n"
        "    gp_Pnt(-2,-15,27.5), gp_Pnt(-1.29,-14.71,27.5), "
        "gp_Pnt(-1,-14,27.5),\n"
        "    gp_Pnt(0,-13,27.5),\n"
        "    gp_Pnt(2,15,27.5), gp_Pnt(1.29,14.71,27.5), gp_Pnt(1,14,27.5),\n"
        "    gp_Pnt(-2,15,27.5), gp_Pnt(-1.29,14.71,27.5), "
        "gp_Pnt(-1,14,27.5),\n"
        "    gp_Pnt(0,13,27.5))\n"
        "\n"
        "# 截面3: Z=13.75 (中间过渡)\n"
        "mWire = make_wire(\n"
        "    gp_Pnt(17.88,-14.095,13.75), gp_Pnt(31.905,0,13.75), "
        "gp_Pnt(17.88,14.095,13.75),\n"
        "    gp_Pnt(-17.88,-14.095,13.75), gp_Pnt(-31.905,0,13.75), "
        "gp_Pnt(-17.88,14.095,13.75),\n"
        "    gp_Pnt(2,-14.095,13.75), gp_Pnt(1.29,-13.8,13.75), "
        "gp_Pnt(1,-13.1,13.75),\n"
        "    gp_Pnt(-2,-14.095,13.75), gp_Pnt(-1.29,-13.8,13.75), "
        "gp_Pnt(-1,-13.1,13.75),\n"
        "    gp_Pnt(0,-12.1,13.75),\n"
        "    gp_Pnt(2,14.095,13.75), gp_Pnt(1.29,13.8,13.75), "
        "gp_Pnt(1,13.1,13.75),\n"
        "    gp_Pnt(-2,14.095,13.75), gp_Pnt(-1.29,13.8,13.75), "
        "gp_Pnt(-1,13.1,13.75),\n"
        "    gp_Pnt(0,12.1,13.75))\n"
        "\n"
        "# 截面4: Z=30 (顶帽)\n"
        "dWire = make_wire(\n"
        "    gp_Pnt(24,-15,30), gp_Pnt(39,0,30), gp_Pnt(24,15,30),\n"
        "    gp_Pnt(-24,-15,30), gp_Pnt(-39,0,30), gp_Pnt(-24,15,30),\n"
        "    gp_Pnt(2,-15,30), gp_Pnt(1.29,-14.71,30), gp_Pnt(1,-14,30),\n"
        "    gp_Pnt(-2,-15,30), gp_Pnt(-1.29,-14.71,30), gp_Pnt(-1,-14,30),\n"
        "    gp_Pnt(0,-13,30),\n"
        "    gp_Pnt(2,15,30), gp_Pnt(1.29,14.71,30), gp_Pnt(1,14,30),\n"
        "    gp_Pnt(-2,15,30), gp_Pnt(-1.29,14.71,30), gp_Pnt(-1,14,30),\n"
        "    gp_Pnt(0,13,30))\n"
        "\n"
        "# 截面5: Z=-120 (墩身底部)\n"
        "sWire = make_wire(\n"
        "    gp_Pnt(16,-16.67,-120), gp_Pnt(32.67,0,-120), "
        "gp_Pnt(16,16.67,-120),\n"
        "    gp_Pnt(-16,-16.67,-120), gp_Pnt(-32.67,0,-120), "
        "gp_Pnt(-16,16.67,-120),\n"
        "    gp_Pnt(2,-16.67,-120), gp_Pnt(1.29,-16.37,-120), "
        "gp_Pnt(1,-15.67,-120),\n"
        "    gp_Pnt(-2,-16.67,-120), gp_Pnt(-1.29,-16.37,-120), "
        "gp_Pnt(-1,-15.67,-120),\n"
        "    gp_Pnt(0,-14.67,-120),\n"
        "    gp_Pnt(2,16.67,-120), gp_Pnt(1.29,16.37,-120), "
        "gp_Pnt(1,15.67,-120),\n"
        "    gp_Pnt(-2,16.67,-120), gp_Pnt(-1.29,16.37,-120), "
        "gp_Pnt(-1,15.67,-120),\n"
        "    gp_Pnt(0,14.67,-120))\n"
        "\n"
        "# === 流线型托盘 (ThruSections) ===\n"
        "from OCP.BRepOffsetAPI import BRepOffsetAPI_ThruSections\n"
        "tuopan = BRepOffsetAPI_ThruSections(True, False)\n"
        "tuopan.AddWire(aWire)\n"
        "tuopan.AddWire(mWire)\n"
        "tuopan.AddWire(bWire)\n"
        "tuopan.Build()\n"
        "S = tuopan.Shape()\n"
        "\n"
        "# === 顶帽 ===\n"
        "dingmao = BRepOffsetAPI_ThruSections(True, False)\n"
        "dingmao.AddWire(bWire)\n"
        "dingmao.AddWire(dWire)\n"
        "dingmao.Build()\n"
        "S1 = dingmao.Shape()\n"
        "\n"
        "# === 裁剪 (Prism切割) ===\n"
        "from OCP.BRepBuilderAPI import BRepBuilderAPI_MakeEdge, "
        "BRepBuilderAPI_MakeWire, BRepBuilderAPI_MakeFace\n"
        "from OCP.BRepFeat import BRepFeat_MakePrism\n"
        "from OCP.BRepLib import BRepLib\n"
        "from OCP.BRepOffsetAPI import BRepOffsetAPI_ThruSections\n"
        "from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox\n"
        "from OCP.BRep import BRep_Builder\n"
        "from OCP.TopoDS import TopoDS_Compound\n"
        "D = gp_Dir(0, 1, 0)\n"
        "MW = BRepBuilderAPI_MakeWire()\n"
        "pts = [(-7.5,-100,30), (-7.5,-100,27), (-5.5,-100,25),\n"
        "       (5.5,-100,25), (7.5,-100,27), (7.5,-100,30), (-7.5,-100,30)]\n"
        "for i in range(len(pts)-1):\n"
        "    MW.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(*pts[i]), "
        "gp_Pnt(*pts[i+1])).Edge())\n"
        "\n"
        "zox_pln = gp_Pln(gp_Pnt(0,0,0), gp_Dir(0,1,0))\n"
        "FP = BRepBuilderAPI_MakeFace(zox_pln, MW.Wire()).Shape()\n"
        "BRepLib.BuildCurves3d_s(FP)\n"
        "F = BRepBuilderAPI_MakeFace(zox_pln).Face()\n"
        "\n"
        "MKP = BRepFeat_MakePrism(S, FP, F, D, 0, True)\n"
        "MKP.Perform(1000.0)\n"
        "res = MKP.Shape()\n"
        "\n"
        "MKP1 = BRepFeat_MakePrism(S1, FP, F, D, 0, True)\n"
        "MKP1.Perform(1000.0)\n"
        "res1 = MKP1.Shape()\n"
        "\n"
        "# === 墩身 ===\n"
        "dunshen = BRepOffsetAPI_ThruSections(True, False)\n"
        "dunshen.AddWire(sWire)\n"
        "dunshen.AddWire(aWire)\n"
        "dunshen.Build()\n"
        "S2 = dunshen.Shape()\n"
        "\n"
        "# === 承台 (两层底座) ===\n"
        "S3 = BRepPrimAPI_MakeBox(gp_Pnt(-38.41,-22.22,-130), 76.82, 44.44, "
        "10).Shape()\n"
        "S4 = BRepPrimAPI_MakeBox(gp_Pnt(-44.79,-29.53,-140), 89.59, 59.05, "
        "10).Shape()\n"
        "\n"
        "# === 组合为 Compound ===\n"
        "builder = BRep_Builder()\n"
        "compound = TopoDS_Compound()\n"
        "builder.MakeCompound(compound)\n"
        "for s in [res, res1, S2, S3, S4]:\n"
        "    builder.Add(compound, s)\n"
        "\n"
        "result = compound\n"
        "material = 'plastic'\n");
    onRunCqScript();
  });
  panelScripts->addLargeAction(bridgePierCqBtn);

  // 桥墩2按钮 - 使用 CadQuery Workplane 链式语法
  QAction *bridgePier2Btn = new QAction(
      QIcon(":/resources/icons/bridge_pier.svg"), "桥墩示例2", this);
  connect(bridgePier2Btn, &QAction::triggered, [this]() {
    m_currentMaterial = Graphic3d_NOM_PLASTIC;
    m_cqScriptEditor->setText(
        "# 流线型桥墩 - CadQuery Workplane 链式语法\n"
        "import cadquery as cq\n"
        "\n"
        "def draw(wp, xr, yr, px, nmy, ney, iy):\n"
        "    \"\"\"绘制桥墩截面轮廓\"\"\"\n"
        "    return (wp\n"
        "        .moveTo(-xr, -yr)\n"
        "        .threePointArc((-px, 0), (-xr, yr))\n"
        "        .lineTo(-2, yr)\n"
        "        .threePointArc((-1.29, nmy), (-1, ney))\n"
        "        .threePointArc((0, iy), (1, ney))\n"
        "        .threePointArc((1.29, nmy), (2, yr))\n"
        "        .lineTo(xr, yr)\n"
        "        .threePointArc((px, 0), (xr, -yr))\n"
        "        .lineTo(2, -yr)\n"
        "        .threePointArc((1.29, -nmy), (1, -ney))\n"
        "        .threePointArc((0, -iy), (-1, -ney))\n"
        "        .threePointArc((-1.29, -nmy), (-2, -yr))\n"
        "        .close())\n"
        "\n"
        "# === 流线型托盘 (Z=0 -> 13.75 -> 2750) ===\n"
        "w = cq.Workplane('XY')\n"
        "w = draw(w, 1600, 1400, 3000, 1374, 1300, 1200)\n"
        "w = draw(w.workplane(offset=1375),\n"
        "         1788, 1409.5, 3190.5, 1380, 1310, 1210)\n"
        "w = draw(w.workplane(offset=1375),\n"
        "         2400, 1500, 3900, 1471, 1400, 1300)\n"
        "tuopan = w.loft()\n"
        "\n"
        "# === 顶帽 (Z=2750 -> 3000) ===\n"
        "w = cq.Workplane('XY').workplane(offset=2750)\n"
        "w = draw(w, 2400, 1500, 3900, 1471, 1400, 1300)\n"
        "w = draw(w.workplane(offset=250),\n"
        "         2400, 1500, 3900, 1471, 1400, 1300)\n"
        "dingmao = w.loft()\n"
        "\n"
        "# === 裁剪 (梯形槽道沿Y方向) ===\n"
        "cutter = (cq.Workplane('XZ')\n"
        "    .moveTo(-750, 3000).lineTo(-750, 2700)\n"
        "    .lineTo(-550, 2500).lineTo(550, 2500)\n"
        "    .lineTo(750, 2700).lineTo(750, 3000)\n"
        "    .close().extrude(50000, both=True))\n"
        "tuopan = tuopan.cut(cutter)\n"
        "dingmao = dingmao.cut(cutter)\n"
        "\n"
        "pierHeight = globals().get('pierHeight', 12000.0)\n"
        "# === 墩身 ===\n"
        "w = cq.Workplane('XY').workplane(offset=-pierHeight)\n"
        "w = draw(w, 1600, 1667, 3267, 1637, 1567, 1467)\n"
        "w = draw(w.workplane(offset=pierHeight),\n"
        "         1600, 1400, 3000, 1374, 1300, 1200)\n"
        "dunshen = w.loft()\n"
        "\n"
        "# === 承台 (两层底座) ===\n"
        "ct1 = cq.Workplane('XY').workplane(offset=-(pierHeight + "
        "500)).box(7682, 4444, 1000)\n"
        "ct2 = cq.Workplane('XY').workplane(offset=-(pierHeight + "
        "1500)).box(8959, 5905, 1000)\n"
        "# === Assembly 装配 (桩共享同一几何) ===\n"
        "pile = cq.Workplane('XY').circle(500).extrude(6000)\n"
        "assy = cq.Assembly()\n"
        "assy.add(tuopan, name='tuopan')\n"
        "assy.add(dingmao, name='dingmao')\n"
        "assy.add(dunshen, name='dunshen')\n"
        "assy.add(ct1, name='ct1')\n"
        "assy.add(ct2, name='ct2')\n"
        "for xi in [-2500, 0, 2500]:\n"
        "    for yi in [-1500, 1500]:\n"
        "        assy.add(pile,\n"
        "            loc=cq.Location((xi, yi, -(pierHeight + 8000))),\n"
        "            name=f'pile_{xi}_{yi}')\n"
        "result = assy.toCompound()\n"
        "material = 'plastic'\n");
    onRunCqScript();
  });
  panelScripts->addLargeAction(bridgePier2Btn);

  SARibbonPanel *panelRun = categoryScripts->addPanel("运行");
  QAction *runScriptBtn =
      new QAction(QIcon(":/resources/icons/random.svg"), "运行当前脚本", this);
  connect(runScriptBtn, &QAction::triggered, this, &MainWindow::onRunCqScript);
  panelRun->addLargeAction(runScriptBtn);

  content->setLayout(layout);
  m_dockCq->setWidget(content);
  addDockWidget(Qt::RightDockWidgetArea, m_dockCq);

  // --- 初始化属性面板 ---
  m_propertyDock = new QDockWidget("属性", this);
  m_propertyDock->setAllowedAreas(Qt::RightDockWidgetArea |
                                  Qt::LeftDockWidgetArea);

  m_propertyWidget = new QWidget();
  m_propertyLayout = new QVBoxLayout(m_propertyWidget);
  m_propertyLayout->setAlignment(Qt::AlignTop);

  QScrollArea *scroll = new QScrollArea();
  scroll->setWidgetResizable(true);
  scroll->setWidget(m_propertyWidget);

  m_propertyDock->setWidget(scroll);
  addDockWidget(Qt::LeftDockWidgetArea, m_propertyDock);

  // 初始状态
  onObjectSelected(QVariantMap());
}

void MainWindow::initializeCqNetwork() {
  m_networkManager =
      QSharedPointer<QNetworkAccessManager>(new QNetworkAccessManager());
  m_networkManager->setProxy(QNetworkProxy::NoProxy);
}

void MainWindow::onRunCqScript() {
  QString code = m_cqScriptEditor->toPlainText();
  if (code.isEmpty())
    return;

  QJsonObject args;
  args["pierHeight"] = m_pierHeightSpinBox->value();

  sendScriptToMicroservice(code, args, -1, m_currentModelType);
}

void MainWindow::sendScriptToMicroservice(const QString &code,
                                          const QJsonObject &args,
                                          int assemblyIndex,
                                          const QString &modelType) {
  QJsonObject req;
  req["code"] = code;
  req["args"] = args;
  req["model_type"] = modelType;
  req["format"] = "brep";

  QJsonDocument doc(req);
  QByteArray postData = doc.toJson();
  qDebug() << "Sending request to microservice:" << postData;

  QNetworkRequest request(QUrl("http://127.0.0.1:8000/api/v1/model/generate"));
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  QNetworkReply *reply = m_networkManager->post(request, postData);

  // Track assemblyIndex alongside the reply so callback knows how to process it
  reply->setProperty("assemblyIndex", assemblyIndex);

  connect(reply, &QNetworkReply::finished, [this, reply]() {
    int assemblyIdx = reply->property("assemblyIndex").toInt();
    this->onCqNetworkReply(reply, assemblyIdx);
  });

  if (assemblyIndex == -1) {
    QApplication::setOverrideCursor(Qt::WaitCursor);
  }
}

void MainWindow::dispatchTask(int) {
  if (m_batchQueue.isEmpty())
    return;
  int index = m_batchQueue.dequeue();

  QString modelName;
  QJsonObject args;
  args["pierHeight"] = m_pierHeightSpinBox->value();

  if (m_isAssembling) {
    // Map index to script name for full bridge pier assembly
    switch (index) {
    case 0:
      modelName = "Pile";
      break;
    case 1:
      modelName = "Chengtai";
      break;
    case 2:
      modelName = "Dunshen";
      break;
    case 3:
      modelName = "TuopanDingmao";
      break;
    case 4: // bed_stone 1
    case 5: // bed_stone 2
      modelName = "bed_stone";
      break;
    case 6: // bearing 1
    case 7: // bearing 2
      modelName = "bearing";
      break;
    case 8: // girder
      modelName = "girder";
      break;
    default:
      qWarning() << "Unknown assembly index:" << index;
      return;
    }

    QString code = readScript(modelName);
    sendScriptToMicroservice(code, args, index, modelName);
  } else {
    // 独立批量生成全桥 (通过 C++ 循环位移并各自使用 BridgePier2 脚本)
    modelName = "BridgePier2"; // 实际使用的脚本模板名
    QString code = readScript(modelName);

    // 注意：在这里发送 args，其中 yOffset 是由 C++ 计算出来的毫米值
    args["yOffset"] = index * m_bridgePierSpacing;

    sendScriptToMicroservice(code, args, index, modelName);
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
      if (m_completedTasks == 8) {
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

  // --- JHB 协议解析 ---
  if (data.size() < 4) {
    qWarning() << "JHB 数据过短";
    return;
  }

  uint32_t jsonLen = 0;
  memcpy(&jsonLen, data.constData(), 4); // 小端序

  if (data.size() < (int)(4 + jsonLen)) {
    qWarning() << "JHB 元数据长度异常";
    return;
  }

  QByteArray jsonData = data.mid(4, jsonLen);
  QByteArray brepData = data.mid(4 + jsonLen);

  QJsonDocument doc = QJsonDocument::fromJson(jsonData);
  QVariantMap metadata = doc.object().toVariantMap();

  if (m_isAssembling) {
    TopoDS_Shape shape = m_occtWidget->readBrepFromMemory(brepData);
    while (m_assemblyParts.size() <= assemblyIndex) {
      m_assemblyParts.append(
          {TopoDS_Shape(), Graphic3d_NOM_PLASTIC, QVariantMap()});
    }
    Graphic3d_NameOfMaterial mat = Graphic3d_NOM_STONE;
    if (assemblyIndex == 6 || assemblyIndex == 7)
      mat = Graphic3d_NOM_STEEL;

    m_assemblyParts[assemblyIndex] = {shape, mat, metadata};
    m_completedTasks++;

    if (m_completedTasks == 9) {
      m_occtWidget->buildFullBridgeFromParts(m_assemblyParts, m_bridgePierCount,
                                             m_bridgePierSpacing);
      statusBar()->showMessage("全桥拼装完成", 5000);
      m_isAssembling = false;
      m_occtWidget->fitAll();
    } else {
      dispatchTask();
    }
  } else if (m_isBatchProcessing) {
    TopoDS_Shape shape = m_occtWidget->readBrepFromMemory(brepData);
    if (!shape.IsNull()) {
      m_batchParts.append({shape, m_currentMaterial, metadata});
    }
    m_completedTasks++;
    statusBar()->showMessage(QString("正在并发生成: %1/%2")
                                 .arg(m_completedTasks)
                                 .arg(m_bridgePierCount));

    if (m_completedTasks == m_bridgePierCount) {
      m_isBatchProcessing = false;
      m_occtWidget->buildFullBridgeFromBatch(m_batchParts);
      m_occtWidget->fitAll();
      statusBar()->showMessage(
          QString("全桥生成完毕. 耗时: %1 ms").arg(m_batchTimer.elapsed()),
          10000);
    }
  } else {
    m_occtWidget->clearAll();
    TopoDS_Shape shape = m_occtWidget->readBrepFromMemory(brepData);
    if (!shape.IsNull()) {
      m_occtWidget->displayShape(shape, m_currentMaterial, true, metadata);
    }
    statusBar()->showMessage("模型生成成功", 3000);
  }
}

void MainWindow::onObjectSelected(const QVariantMap &metadata) {
  // 清空当前布局
  QLayoutItem *child;
  while ((child = m_propertyLayout->takeAt(0)) != nullptr) {
    if (child->widget()) {
      child->widget()->deleteLater();
    }
    delete child;
  }

  if (metadata.isEmpty()) {
    QLabel *emptyLabel = new QLabel("未选中任何构件");
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setStyleSheet("color: #888; margin-top: 20px;");
    m_propertyLayout->addWidget(emptyLabel);
    return;
  }

  qDebug() << "Selected object metadata:"
           << QJsonDocument::fromVariant(metadata).toJson();

  // 渲染标题
  QString modelType = metadata.value("modelType").toString();
  QString displayName = metadata.value("name", modelType).toString();
  if (displayName.isEmpty())
    displayName = "构件实例";

  QLabel *title = new QLabel(QString("<b>属性: %1</b>").arg(displayName));
  title->setStyleSheet(
      "font-size: 14px; border-bottom: 1px solid #ccc; padding-bottom: 5px;");
  m_propertyLayout->addWidget(title);

  // 渲染参数列表 (基于 Schema)
  QVariantMap schema = metadata.value("schema").toMap();
  QVariantMap currentArgs = metadata.value("args").toMap();

  if (schema.isEmpty()) {
    m_propertyLayout->addWidget(new QLabel("<i>暂无可用属性</i>"));
    for (auto it = currentArgs.begin(); it != currentArgs.end(); ++it) {
      if (it.key() == "yOffset")
        continue;
      m_propertyLayout->addWidget(new QLabel(
          QString("%1: %2").arg(it.key()).arg(it.value().toString())));
    }
  } else {
    // 改为遍历有序的 fields 列表
    QVariantList fields = schema.value("fields").toList();
    for (const QVariant &fieldVar : fields) {
      QVariantMap fieldInfo = fieldVar.toMap();
      QString key = fieldInfo.value("key").toString();
      QString labelText = fieldInfo.value("label", key).toString();
      QString unit = fieldInfo.value("unit", "").toString();
      if (!unit.isEmpty())
        labelText += QString(" (%1)").arg(unit);

      QLabel *label = new QLabel(labelText);
      label->setStyleSheet("margin-top: 8px; font-weight: bold;");
      m_propertyLayout->addWidget(label);

      QString type = fieldInfo.value("type").toString();
      QString access = fieldInfo.value("access", "inout").toString();
      // 兼容用户在 YAML 中定义的 'out' 和 'output'
      bool readOnly = (access == "out" || access == "output");

      if (type == "float" || type == "int") {
        QDoubleSpinBox *dsb = new QDoubleSpinBox();
        dsb->setObjectName(key); // 设置控件名方便后续查询
        dsb->setRange(-1000000, 1000000);
        dsb->setSingleStep(100.0); // 设置调整步长为 0.1m (100mm)
        double val = currentArgs.contains(key)
                         ? currentArgs.value(key).toDouble()
                         : fieldInfo.value("default", 0.0).toDouble();
        dsb->setValue(val);
        if (readOnly) {
          dsb->setReadOnly(true);
          dsb->setStyleSheet("background-color: #f0f0f0; color: #666;");
        }
        m_propertyLayout->addWidget(dsb);
      } else {
        QString val = currentArgs.contains(key)
                          ? currentArgs.value(key).toString()
                          : fieldInfo.value("default", "").toString();
        QLineEdit *edit = new QLineEdit(val);
        edit->setObjectName(key); // 设置控件名方便后续查询
        if (readOnly) {
          edit->setReadOnly(true);
          edit->setStyleSheet("background-color: #f0f0f0; color: #666;");
        }
        m_propertyLayout->addWidget(edit);
      }
    }
  }

  m_propertyLayout->addSpacing(20);
  QPushButton *updateBtn = new QPushButton("重新生成模型");
  updateBtn->setStyleSheet(
      "background-color: #0078d4; color: white; border: none; padding: 8px; "
      "font-weight: bold; border-radius: 4px;");

  // 绑定点击事件：收集参数并重新触发建模
  connect(updateBtn, &QPushButton::clicked, [this, modelType]() {
    QJsonObject newArgs;
    // 遍历布局找到所有的输入控件
    for (int i = 0; i < m_propertyLayout->count(); ++i) {
      QWidget *w = m_propertyLayout->itemAt(i)->widget();
      if (!w)
        continue;
      QString key = w->objectName();
      if (key.isEmpty())
        continue;

      if (QDoubleSpinBox *dsb = qobject_cast<QDoubleSpinBox *>(w)) {
        if (!dsb->isReadOnly()) {
          newArgs[key] = dsb->value();
        }
      } else if (QLineEdit *le = qobject_cast<QLineEdit *>(w)) {
        if (!le->isReadOnly()) {
          newArgs[key] = le->text();
        }
      }
    }

    qDebug() << "Updating model with args:" << newArgs;

    // 注入全局参数，确保脚本（如墩身、承台）能正常运行
    newArgs["pierHeight"] = m_pierHeightSpinBox->value();

    QString code = readScript(modelType);
    if (!code.isEmpty()) {
      // 如果是在装配图中选中的构件，目前逻辑是作为独立构件重新生成预览。
      // 注意：这里 assemblyIndex 传入 -1 表示这不是装配任务的一部分。
      sendScriptToMicroservice(code, newArgs, -1, modelType);
      statusBar()->showMessage(QString("正在重新生成: %1").arg(modelType),
                               3000);
    }
  });

  m_propertyLayout->addWidget(updateBtn);
  m_propertyLayout->addStretch();
}

QString MainWindow::readScript(const QString &modelName) {
  QString path = QDir::currentPath() + "/cq_script/" + modelName + ".py";
  QFile file(path);
  if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return QString::fromUtf8(file.readAll());
  }
  return QString("# 找不到脚本文件: %1").arg(path);
}

void MainWindow::onExportStepClicked() {
  QString filename = QFileDialog::getSaveFileName(
      this, "导出为 STEP 文件", "", "STEP 文件 (*.step *.stp);;所有文件 (*.*)");

  if (!filename.isEmpty()) {
    m_occtWidget->exportToSTEP(filename);
  }
}

void MainWindow::onExportGltfClicked() {
  QString filename = QFileDialog::getSaveFileName(
      this, "导出为 GLTF 文件", "", "GLTF 文件 (*.gltf *.glb);;所有文件 (*.*)");

  if (!filename.isEmpty()) {
    m_occtWidget->exportToGLTF(filename);
  }
}
