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
      m_highlighter(nullptr) {
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
    m_cqScriptEditor->setText(
        "import cadquery as cq\n"
        "# Create a simple box\n"
        "result = cq.Workplane('XY').box(100, 100, 100).edges().chamfer(10)\n"
        "# result variable is automatically exported\n");
    onRunCqScript();
  });
  btnLayout->addWidget(runBtn);

  QPushButton *holedPlateBtn = new QPushButton("带孔板", content);
  connect(holedPlateBtn, &QPushButton::clicked, [this]() {
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
                              ")\n");
    onRunCqScript();
  });
  btnLayout->addWidget(holedPlateBtn);

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
      continue;
    }

    // Check for result
    // Our protocol: SUCCESS or ERROR: ... or EXCEPTION: ...
    if (line == "SUCCESS") {
      QApplication::restoreOverrideCursor();

      QString outputPath = QDir::currentPath() + "/temp_output.brep";
      // Load Result
      m_occtWidget->clearAll();
      m_occtWidget->loadBrepFile(outputPath);
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
