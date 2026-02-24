#include "../include/MainWindow.h"
#include "../include/OCCTWidget.h"

#include "../include/PythonSyntaxHighlighter.h"
#include "../include/ShxTextGenerator.h"
#include <QApplication>
#include <QCheckBox>
#include <QDebug>
#include <QDir>
#include <QDockWidget>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QStatusBar>
#include <QTemporaryFile>
#include <QTextEdit>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_occtWidget(new OCCTWidget(this)),
      m_functionalPanel(nullptr), m_drawLineButton(nullptr),
      m_solidTextCheckbox(nullptr), m_coordLabel(nullptr),
      m_highlighter(nullptr), m_currentMaterial(Graphic3d_NOM_PLASTIC) {
  setWindowTitle("Qt OCCT Application");
  setMinimumSize(800, 600);

  // Set the OCCT widget as the central widget
  setCentralWidget(m_occtWidget);

  // Create the functional panel
  createFunctionalPanel();
  setupCadQueryUi();

  // Create menu and toolbar
  QMenuBar *menuBar = new QMenuBar(this);
  QMenu *fileMenu = menuBar->addMenu("File");
  fileMenu->addAction("Exit", this, &QMainWindow::close);
  setMenuBar(menuBar);

  QStatusBar *sBar = new QStatusBar(this);
  setStatusBar(sBar);

  // 创建坐标显示标签
  m_coordLabel = new QLabel("X: 0.000  Y: 0.000  Z: 0.000", this);
  m_coordLabel->setMinimumWidth(280);
  sBar->addPermanentWidget(m_coordLabel);

  // 连接鼠标位置信号
  // 连接鼠标位置信号
  // 连接鼠标位置信号
  connect(m_occtWidget, &OCCTWidget::mousePositionChanged, this,
          &MainWindow::onMousePositionChanged);

  initializeCqProcess();
}

MainWindow::~MainWindow() {}

void MainWindow::createFunctionalPanel() {
  // Create dock widget for functional panel
  m_functionalPanel = new QDockWidget("Functional Panel", this);
  m_functionalPanel->setAllowedAreas(Qt::LeftDockWidgetArea |
                                     Qt::RightDockWidgetArea);

  // Create panel content
  QWidget *panelContent = new QWidget(m_functionalPanel);
  QVBoxLayout *layout = new QVBoxLayout(panelContent);

  // Add draw line button
  m_drawLineButton = new QPushButton("Draw Line", panelContent);
  connect(m_drawLineButton, &QPushButton::clicked, this,
          &MainWindow::onDrawLineClicked);
  layout->addWidget(m_drawLineButton);

  // Add Generate Random Lines button
  QPushButton *randomLinesBtn =
      new QPushButton("Generate 10k Lines", panelContent);
  connect(randomLinesBtn, &QPushButton::clicked,
          [this]() { m_occtWidget->generateRandomLines(10000); });
  layout->addWidget(randomLinesBtn);

  // Add SHX Text button
  QPushButton *shxTextBtn = new QPushButton("Add SHX Text", panelContent);
  connect(shxTextBtn, &QPushButton::clicked, this, &MainWindow::onAddShxText);
  layout->addWidget(shxTextBtn);

  // Add Solid Text Checkbox
  m_solidTextCheckbox = new QCheckBox("Test Solid Text", panelContent);
  m_solidTextCheckbox->setChecked(true); // Default to solid
  connect(m_solidTextCheckbox, &QCheckBox::stateChanged, [this](int state) {
    m_occtWidget->setTextsSolid(state == Qt::Checked);
  });
  layout->addWidget(m_solidTextCheckbox);

  // Add BRep Text button (uses Checkbox state)
  QPushButton *brepTextBtn = new QPushButton("Add BRep Text", panelContent);
  connect(brepTextBtn, &QPushButton::clicked, [this]() {
    bool isSolid = m_solidTextCheckbox->isChecked();
    double angle = 45.0; // Keep the angle test
    m_occtWidget->add3DText("京沪D1K234+869.75", 50.0, gp_Pnt(0, 0, 0), isSolid,
                            angle);
  });
  layout->addWidget(brepTextBtn);

  // Add Fit All button - 缩放到全部视图范围
  QPushButton *fitAllBtn = new QPushButton("Fit All (缩放全部)", panelContent);
  connect(fitAllBtn, &QPushButton::clicked,
          [this]() { m_occtWidget->fitAll(); });
  layout->addWidget(fitAllBtn);

  // Add Bridge Pier button - 绘制流线型桥墩
  QPushButton *bridgePierBtn =
      new QPushButton("绘制桥墩 (Bridge Pier)", panelContent);
  bridgePierBtn->setStyleSheet(
      "background-color: #1a5276; color: white; font-weight: bold;"
      "padding: 6px; border-radius: 4px;");
  connect(bridgePierBtn, &QPushButton::clicked, this,
          &MainWindow::onDrawBridgePier);
  layout->addWidget(bridgePierBtn);

  // Add other potential functionality buttons here
  layout->addStretch(); // Add stretch to push buttons to the top

  panelContent->setLayout(layout);
  m_functionalPanel->setWidget(panelContent);

  // Add the dock widget to the main window
  addDockWidget(Qt::LeftDockWidgetArea, m_functionalPanel);
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

void MainWindow::setupCadQueryUi() {
  m_dockCq = new QDockWidget("CadQuery Editor", this);
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

  QHBoxLayout *btnLayout = new QHBoxLayout();

  QPushButton *runBtn = new QPushButton("切角立方体", content);
  connect(runBtn, &QPushButton::clicked, [this]() {
    m_currentMaterial = Graphic3d_NOM_PLASTIC;
    m_cqScriptEditor->setText(
        "import cadquery as cq\n"
        "# Create a simple box\n"
        "result = cq.Workplane('XY').box(100, 100, 100).edges().chamfer(10)\n"
        "material = 'plastic'\n");
    onRunCqScript();
  });
  btnLayout->addWidget(runBtn);

  QPushButton *holedPlateBtn = new QPushButton("带孔板", content);
  connect(holedPlateBtn, &QPushButton::clicked, [this]() {
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
  btnLayout->addWidget(holedPlateBtn);

  // 桥墩按钮
  QPushButton *bridgePierCqBtn = new QPushButton("桥墩", content);
  bridgePierCqBtn->setStyleSheet(
      "background-color: #1a5276; color: white; font-weight: bold;");
  connect(bridgePierCqBtn, &QPushButton::clicked, [this]() {
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
  btnLayout->addWidget(bridgePierCqBtn);

  // 桥墩2按钮 - 使用 CadQuery Workplane 链式语法
  QPushButton *bridgePier2Btn = new QPushButton("桥墩2", content);
  bridgePier2Btn->setStyleSheet(
      "background-color: #1b4f72; color: white; font-weight: bold;");
  connect(bridgePier2Btn, &QPushButton::clicked, [this]() {
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
        "# === 流线型托盘 (Z=0 -> 13.75 -> 27.5) ===\n"
        "w = cq.Workplane('XY')\n"
        "w = draw(w, 16, 14, 30, 13.74, 13, 12)\n"
        "w = draw(w.workplane(offset=13.75),\n"
        "         17.88, 14.095, 31.905, 13.8, 13.1, 12.1)\n"
        "w = draw(w.workplane(offset=13.75),\n"
        "         24, 15, 39, 14.71, 14, 13)\n"
        "tuopan = w.loft()\n"
        "\n"
        "# === 顶帽 (Z=27.5 -> 30) ===\n"
        "w = cq.Workplane('XY').workplane(offset=27.5)\n"
        "w = draw(w, 24, 15, 39, 14.71, 14, 13)\n"
        "w = draw(w.workplane(offset=2.5),\n"
        "         24, 15, 39, 14.71, 14, 13)\n"
        "dingmao = w.loft()\n"
        "\n"
        "# === 裁剪 (梯形槽道沿Y方向) ===\n"
        "cutter = (cq.Workplane('XZ')\n"
        "    .moveTo(-7.5, 30).lineTo(-7.5, 27)\n"
        "    .lineTo(-5.5, 25).lineTo(5.5, 25)\n"
        "    .lineTo(7.5, 27).lineTo(7.5, 30)\n"
        "    .close().extrude(500, both=True))\n"
        "tuopan = tuopan.cut(cutter)\n"
        "dingmao = dingmao.cut(cutter)\n"
        "\n"
        "# === 墩身 (Z=-120 -> 0) ===\n"
        "w = cq.Workplane('XY').workplane(offset=-120)\n"
        "w = draw(w, 16, 16.67, 32.67, 16.37, 15.67, 14.67)\n"
        "w = draw(w.workplane(offset=120),\n"
        "         16, 14, 30, 13.74, 13, 12)\n"
        "dunshen = w.loft()\n"
        "\n"
        "# === 承台 (两层底座) ===\n"
        "ct1 = cq.Workplane('XY').workplane(offset=-125).box(76.82, 44.44, "
        "10)\n"
        "ct2 = cq.Workplane('XY').workplane(offset=-135).box(89.59, 59.05, "
        "10)\n"
        "\n"
        "# === 组合所有部件 (Compound, 无布尔运算) ===\n"
        "result = cq.Compound.makeCompound(\n"
        "    [tuopan.val(), dingmao.val(), dunshen.val(),\n"
        "     ct1.val(), ct2.val()])\n"
        "material = 'plastic'\n");
    onRunCqScript();
  });
  btnLayout->addWidget(bridgePier2Btn);

  QPushButton *runScriptBtn = new QPushButton("运行当前脚本 (Run)", content);
  runScriptBtn->setStyleSheet(
      "background-color: #2d5a27; color: white; font-weight: bold;");
  connect(runScriptBtn, &QPushButton::clicked, this,
          &MainWindow::onRunCqScript);
  btnLayout->addWidget(runScriptBtn);

  layout->addLayout(btnLayout);

  content->setLayout(layout);
  m_dockCq->setWidget(content);

  addDockWidget(Qt::RightDockWidgetArea, m_dockCq);
}

void MainWindow::initializeCqProcess() {
  m_cqProcess = new QProcess(this);

  QString pythonExe = qgetenv("MY_PYTHON_EXE");
  if (pythonExe.isEmpty()) {
    pythonExe = "python";
  }

  m_cqProcess->setProgram(pythonExe);
  m_cqProcess->setArguments(
      {QCoreApplication::applicationDirPath() + "/run_cq.py"});

  // Merge stderr to stdout to catch everything in order (or handle separately
  // if preferred)
  m_cqProcess->setProcessChannelMode(QProcess::MergedChannels);

  connect(m_cqProcess, &QProcess::readyReadStandardOutput, this,
          &MainWindow::processCqOutput);

  m_cqProcess->start();
}

void MainWindow::onRunCqScript() {
  // Execute CadQuery script
  QString code = m_cqScriptEditor->toPlainText();
  if (code.isEmpty())
    return;

  // Save to temporary file or specific file
  QString scriptPath = QDir::currentPath() + "/temp_script.py";
  QString outputPath = QDir::currentPath() + "/temp_output.brep";

  QFile scriptFile(scriptPath);
  if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QMessageBox::critical(this, "Error",
                          "Cannot create temporary script file.");
    return;
  }
  scriptFile.write(code.toUtf8());
  scriptFile.close();

  // If process is dead, restart it
  if (m_cqProcess->state() != QProcess::Running) {
    m_cqProcess->start();
    m_cqProcess->waitForStarted();
  }

  // Send request to daemon
  // Format: scriptPath|outputPath\n
  QString request = scriptPath + "|" + outputPath + "\n";
  m_cqProcess->write(request.toUtf8());

  QApplication::setOverrideCursor(Qt::WaitCursor);
  // We don't wait here synchronously anymore in a blocking way for the whole
  // process, but we wait for response via signal/slot or simple loop if we want
  // blocking UI. For simplicity to match previous behavior (blocking UI until
  // done), we can use a local loop:

  // Actually, let's keep it async UI but with WaitCursor, logic moves to
  // processCqOutput BUT: To keep existing flow simple without refactoring
  // everything to async state machine, we can just waitForReadyRead in a loop
  // until we get result.

  // HOWEVER, persistent process is best used asynchronously.
  // For now, let's rely on processCqOutput to handle the result.
  // Note: multiple requests overlapping might be an issue if we don't track
  // state, but for this single-user tool it's likely fine.
}

void MainWindow::processCqOutput() {
  while (m_cqProcess->canReadLine()) {
    QString line = QString::fromUtf8(m_cqProcess->readLine()).trimmed();

    if (line == "READY") {
      // Daemon started
      continue;
    }

    if (line.startsWith("DEBUG:")) {
      qDebug() << "CQ DEBUG:" << line;
      // 临时开启弹窗以便排查问题
      // QMessageBox::information(this, "Debug Info", line);
      statusBar()->showMessage(line, 5000);
      continue;
    }

    // Check for result
    // Our protocol: SUCCESS or SUCCESS|MATERIAL or ERROR: ...
    if (line.startsWith("SUCCESS")) {
      QApplication::restoreOverrideCursor();

      // Check for material override from script
      QStringList parts = line.split('|');
      if (parts.size() > 1) {
        QString matStr = parts[1].trimmed().toUpper();
        statusBar()->showMessage("脚本指定材质: " + matStr, 5000);
        if (matStr == "STEEL")
          m_currentMaterial = Graphic3d_NOM_STEEL;
        else if (matStr == "CHROME")
          m_currentMaterial = Graphic3d_NOM_CHROME;
        else if (matStr == "ALUMINIUM" || matStr == "ALUMINUM")
          m_currentMaterial = Graphic3d_NOM_ALUMINIUM;
        else if (matStr == "BRASS")
          m_currentMaterial = Graphic3d_NOM_BRASS;
        else if (matStr == "GOLD")
          m_currentMaterial = Graphic3d_NOM_GOLD;
        else if (matStr == "BRONZE")
          m_currentMaterial = Graphic3d_NOM_BRONZE;
        else if (matStr == "PLASTIC")
          m_currentMaterial = Graphic3d_NOM_PLASTIC;
        else if (matStr == "GLASS")
          m_currentMaterial = Graphic3d_NOM_GLASS;
      }

      QString outputPath = QDir::currentPath() + "/temp_output.brep";
      // Load Result
      m_occtWidget->clearAll();
      m_occtWidget->loadBrepFile(outputPath, m_currentMaterial);
      statusBar()->showMessage("Model built successfully.", 3000);
    } else if (line.startsWith("ERROR:") || line.startsWith("EXCEPTION:") ||
               line.startsWith("FATAL:")) {
      QApplication::restoreOverrideCursor();
      // Read rest of error if multi-line? (Our current run_cq.py sends single
      // line response usually, but exception might be long. We replaced newline
      // with ' || ' in python script to ensure single line.)
      QMessageBox::critical(this, "Script Error", line);
    }
  }
}
