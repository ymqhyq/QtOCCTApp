#include "../include/MainWindow.h"
#include "../include/OCCTWidget.h"

#include "../include/ShxTextGenerator.h"
#include <QCheckBox>
#include <QDockWidget>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_occtWidget(new OCCTWidget(this)),
      m_functionalPanel(nullptr), m_drawLineButton(nullptr),
      m_solidTextCheckbox(nullptr), m_coordLabel(nullptr) {
  setWindowTitle("Qt OCCT Application");
  setMinimumSize(800, 600);

  // Set the OCCT widget as the central widget
  setCentralWidget(m_occtWidget);

  // Create the functional panel
  createFunctionalPanel();

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
  connect(m_occtWidget, &OCCTWidget::mousePositionChanged, this,
          &MainWindow::onMousePositionChanged);
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
    // Hardcoded paths for demo
    bool loaded = m_shxGenerator->loadFont("D:/QwenCodeWS/txt.shx");
    bool loadedBig = m_shxGenerator->loadBigFont("D:/QwenCodeWS/hztxt.SHX");

    if (!loaded || !loadedBig) {
      QMessageBox::warning(this, "Font Load Error",
                           "Could not load D:/QwenCodeWS/txt.shx or "
                           "hztxt.SHX.\nPlease ensure files exist.");
    }
  }

  // User Requested Test: "京沪D1K235+500.369", H=200, W=0.8, A=0
  std::string text = "京：:。.";
  gp_Pnt pos(0, 0, 0);

  // Generate Text
  auto result = m_shxGenerator->generateText(text, pos, 200.0, 0.0, 1.0);
  m_occtWidget->addShape(result.first, Quantity_Color(Quantity_NOC_YELLOW));

  // Zoom handled by addShape
}

void MainWindow::onMousePositionChanged(double x, double y, double z) {
  QString coordText = QString("X: %1  Y: %2  Z: %3")
                          .arg(x, 0, 'f', 3)
                          .arg(y, 0, 'f', 3)
                          .arg(z, 0, 'f', 3);
  m_coordLabel->setText(coordText);
}
