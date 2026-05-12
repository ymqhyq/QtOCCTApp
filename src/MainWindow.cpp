#include "../include/MainWindow.h"
#include "../include/OCCTWidget.h"
#include "SARibbonApplicationButton.h"
#include "SARibbonBar.h"
#include "SARibbonCategory.h"
#include "SARibbonPanel.h"
#include <BRepBuilderAPI_Transform.hxx>

#include "../include/PythonSyntaxHighlighter.h"
#include "../include/ShxTextGenerator.h"
#include "IfcExportService.h"
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
#include <gp_Trsf.hxx>
#include <BRepBuilderAPI_Transform.hxx>

// core-data-model headers
#include "DataModel.h"
#include "BrNode_adObject.h"
#include "GeometryService.h"
#include "BrNode_adGeometry.h"
#include "BrNode_adGeometricDef.h"
#include <cmath>

MainWindow::MainWindow(QWidget *parent)
    : SARibbonMainWindow(parent), m_occtWidget(new OCCTWidget(this)),
      m_solidTextCheckbox(nullptr), m_pbrCheckbox(nullptr), m_coordLabel(nullptr),
      m_highlighter(nullptr), m_currentMaterial(Graphic3d_NOM_PLASTIC),
      m_propertyDock(nullptr), m_propertyWidget(nullptr),
      m_propertyLayout(nullptr), m_currentModelType("BridgePier2") {
  setWindowTitle("Qt OCCT Application - Schema Enabled");
  setMinimumSize(1024, 768);
  showMaximized();

  setCentralWidget(m_occtWidget);

  createRibbon();
  setupCadQueryUi();
  initializeCqNetwork();

  QStatusBar *sBar = new QStatusBar(this);
  setStatusBar(sBar);

  m_coordLabel = new QLabel("X: 0.000  Y: 0.000  Z: 0.000", this);
  m_coordLabel->setMinimumWidth(280);
  sBar->addPermanentWidget(m_coordLabel);

  connect(m_occtWidget, &OCCTWidget::mousePositionChanged, this, &MainWindow::onMousePositionChanged);
  connect(m_occtWidget, &OCCTWidget::objectSelected, this, &MainWindow::onObjectSelected);

  onObjectSelected(QVariantMap());
}

MainWindow::~MainWindow() {}

void MainWindow::createRibbon() {
  SARibbonBar *ribbon = ribbonBar();
  ribbon->applicationButton()->setText("QtOCCTApp");

  SARibbonCategory *categoryMain = ribbon->addCategoryPage("Main Tools");
  SARibbonPanel *panelBasic = categoryMain->addPanel("Basic");

  QAction *drawAction = new QAction(QIcon(":/resources/icons/draw_line.svg"), "Draw Line", this);
  connect(drawAction, &QAction::triggered, this, &MainWindow::onDrawLineClicked);
  panelBasic->addLargeAction(drawAction);

  QAction *randLineAction = new QAction(QIcon(":/resources/icons/random.svg"), "Generate 10k Lines", this);
  connect(randLineAction, &QAction::triggered, [this]() { m_occtWidget->generateRandomLines(10000); });
  panelBasic->addLargeAction(randLineAction);

  QAction *exportIfcAction = new QAction(QIcon(":/resources/icons/fit_all.svg"), "Export IFC", this);
  connect(exportIfcAction, &QAction::triggered, this, &MainWindow::onExportIfcClicked);
  panelBasic->addLargeAction(exportIfcAction);

  QAction *loadAsiAction = new QAction(QIcon(":/resources/icons/random.svg"), "Load ASI", this);
  connect(loadAsiAction, &QAction::triggered, this, &MainWindow::onLoadAsiModel);
  panelBasic->addLargeAction(loadAsiAction);

  SARibbonPanel *panelView = categoryMain->addPanel("View");
  QAction *fitAllAction = new QAction(QIcon(":/resources/icons/fit_all.svg"), "Fit All", this);
  connect(fitAllAction, &QAction::triggered, [this]() { m_occtWidget->fitAll(); });
  panelView->addLargeAction(fitAllAction);

  m_pbrCheckbox = new QCheckBox("Enable PBR", this);
  m_pbrCheckbox->setChecked(false);
  connect(m_pbrCheckbox, &QCheckBox::stateChanged, [this](int state) {
    m_occtWidget->setUsePbr(state == Qt::Checked);
  });
  panelView->addWidget(m_pbrCheckbox, SARibbonPanelItem::Small);

  SARibbonPanel *panelText = categoryMain->addPanel("Text");
  QAction *shxTextAction = new QAction(QIcon(":/resources/icons/text_shx.svg"), "Add SHX Text", this);
  connect(shxTextAction, &QAction::triggered, this, &MainWindow::onAddShxText);
  panelText->addLargeAction(shxTextAction);

  m_solidTextCheckbox = new QCheckBox("Test Solid Text", this);
  m_solidTextCheckbox->setChecked(true);
  connect(m_solidTextCheckbox, &QCheckBox::stateChanged, [this](int state) {
    m_occtWidget->setTextsSolid(state == Qt::Checked);
  });
  panelText->addWidget(m_solidTextCheckbox, SARibbonPanelItem::Small);

  SARibbonCategory *categoryBridge = ribbon->addCategoryPage("Bridge Tools");
  SARibbonPanel *panelBridge = categoryBridge->addPanel("Bridge");

  QLabel *heightLabel = new QLabel("Height (mm):", this);
  m_pierHeightSpinBox = new QDoubleSpinBox(this);
  m_pierHeightSpinBox->setRange(10.0, 50000.0);
  m_pierHeightSpinBox->setValue(12000.0);
  panelBridge->addWidget(heightLabel, SARibbonPanelItem::Small);
  panelBridge->addWidget(m_pierHeightSpinBox, SARibbonPanelItem::Small);

  QAction *bridgePierAction = new QAction(QIcon(":/resources/icons/bridge_pier.svg"), "Draw Bridge Pier", this);
  connect(bridgePierAction, &QAction::triggered, [this]() {
    m_currentModelType = "BridgePier2";
    onDrawBridgePier();
  });
  panelBridge->addLargeAction(bridgePierAction);
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
  addDockWidget(Qt::LeftDockWidgetArea, m_propertyDock);
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
    // Clear layout
    QLayoutItem *child;
    while ((child = m_propertyLayout->takeAt(0)) != nullptr) {
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }

    if (metadata.isEmpty()) {
        m_propertyLayout->addWidget(new QLabel("No object selected"));
        return;
    }

    for (auto it = metadata.begin(); it != metadata.end(); ++it) {
        m_propertyLayout->addWidget(new QLabel(QString("<b>%1:</b> %2").arg(it.key()).arg(it.value().toString())));
    }
}

void MainWindow::onLoadAsiModel() {
    QString fileName = QFileDialog::getOpenFileName(this, "Open", "", "ASI Files (*.asi *.asi.cbf);;All Files (*.*)");
    if (fileName.isEmpty()) return;

    m_occtWidget->clearAll();
    if (!m_currentModel.IsNull()) {
        m_currentModel->Release();
        m_currentModel.Nullify();
    }

    m_currentModel = new DataModel();
    if (!m_currentModel->Open(fileName.toStdString().c_str())) {
        QMessageBox::critical(this, "Error", "Failed to open file: " + fileName);
        m_currentModel.Nullify();
        return;
    }

    GeometryService geoService(m_currentModel);
    std::vector<GeometryService::VisualShape> visualShapes;

    Handle(ActAPI_INode) rootBase = m_currentModel->GetRootNode();
    Handle(ActAPI_IChildIterator) it = rootBase->GetChildIterator();
    for (; it->More(); it->Next()) {
        Handle(BrNode_adObject) obj = Handle(BrNode_adObject)::DownCast(it->Value());
        if (!obj.IsNull()) {
            geoService.TraverseAndBuild(obj, visualShapes);
        }
    }

    for (const auto& vs : visualShapes) {
        BRepBuilderAPI_Transform transformer(vs.shape, vs.transform);
        if (transformer.IsDone()) {
            // Convert nlohmann::json to QVariantMap via JSON string
            std::string metaStr = vs.metadata.dump();
            QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(metaStr));
            QVariantMap metaMap = doc.toVariant().toMap();

            m_occtWidget->addShape(transformer.Shape(), Quantity_Color(Quantity_NOC_GRAY70), Graphic3d_NOM_PLASTIC, metaMap);
        }
    }
    m_occtWidget->fitAll();
    statusBar()->showMessage("Model loaded: " + fileName, 3000);
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
    req["format"] = "brep";

    QNetworkRequest request(QUrl("http://127.0.0.1:3500/v1.0/invoke/modeling-service/method/api/v1/model/generate"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply *reply = m_networkManager->post(request, QJsonDocument(req).toJson());
    connect(reply, &QNetworkReply::finished, [this, reply]() { this->onCqNetworkReply(reply, -1); });
}

void MainWindow::onCqNetworkReply(QNetworkReply *reply, int) {
    if (reply->error() == QNetworkReply::NoError) {
        // Simple visualization for now
        statusBar()->showMessage("Model generated", 3000);
    }
    reply->deleteLater();
}

QString MainWindow::readScript(const QString &modelName) {
    QFile file(QDir::currentPath() + "/cq_script/" + modelName + ".py");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) return QString::fromUtf8(file.readAll());
    return "";
}

void MainWindow::onDrawFullBridgePier() {}
void MainWindow::onAnnotateBridgePierFooting() {}
void MainWindow::onDrawFoundation() {}
void MainWindow::onDrawBedStone() {}
void MainWindow::onDrawBearing() {}
void MainWindow::onExportStepClicked() {}
void MainWindow::onExportGltfClicked() {}
void MainWindow::dispatchTask(int) {}

void MainWindow::onExportIfcClicked() {
  if (m_currentModel.IsNull()) {
    QMessageBox::warning(this, "Export IFC", "No model loaded.");
    return;
  }

  QString filename = QFileDialog::getSaveFileName(this, "Export IFC 4x3", "", "IFC Files (*.ifc)");
  if (filename.isEmpty()) return;

  QMessageBox::information(this, "Export IFC", "Triggering Export...");
  if (IfcExportService::Export(m_currentModel, filename.toStdString())) {
    QMessageBox::information(this, "Export IFC", "Successfully exported to " + filename);
  } else {
    QMessageBox::critical(this, "Export IFC", "Failed to export IFC. Please check logs.");
  }
}
