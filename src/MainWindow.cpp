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
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_Transform.hxx>
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
#include "DataModel.h"
#include "BrNode_adObject.h"
#include "GeometryService.h"
#include "BrNode_adGeometry.h"
#include "BrNode_adGeometricDef.h"
#include <cmath>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_LayerTool.hxx>
#include <Standard_Failure.hxx>

MainWindow::MainWindow(QWidget *parent)
    : SARibbonMainWindow(parent), m_occtWidget(new OCCTWidget(this)),
      m_solidTextCheckbox(nullptr), m_pbrCheckbox(nullptr), m_coordLabel(nullptr),
      m_highlighter(nullptr), m_currentMaterial(Graphic3d_NOM_PLASTIC),
      m_propertyDock(nullptr), m_modelExplorerDock(nullptr), m_propertyWidget(nullptr),
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

  QAction *importBrepAction = new QAction(QIcon(":/resources/icons/random.svg"), "Import BREP", this);
  connect(importBrepAction, &QAction::triggered, this, &MainWindow::onImportBrep);
  panelBasic->addLargeAction(importBrepAction);

  QAction *importIfcAction = new QAction(QIcon(":/resources/icons/random.svg"), "Import IFC", this);
  connect(importIfcAction, &QAction::triggered, this, &MainWindow::onImportIfc);
  panelBasic->addLargeAction(importIfcAction);

  QAction *exportStepAction = new QAction(QIcon(":/resources/icons/export.svg"), "Export STEP", this);
  connect(exportStepAction, &QAction::triggered, this, &MainWindow::onExportStepClicked);
  panelBasic->addLargeAction(exportStepAction);

  QAction *exportGltfAction = new QAction(QIcon(":/resources/icons/export.svg"), "Export GLTF", this);
  connect(exportGltfAction, &QAction::triggered, this, &MainWindow::onExportGltfClicked);
  panelBasic->addLargeAction(exportGltfAction);

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

  QAction *fullBridgePierAction = new QAction(QIcon(":/resources/icons/bridge_pier.svg"), "Draw Full Bridge Pier", this);
  connect(fullBridgePierAction, &QAction::triggered, [this]() {
    m_currentModelType = "BridgePier2";
    onDrawFullBridgePier();
  });
  panelBridge->addLargeAction(fullBridgePierAction);

  QAction *annotatePierAction = new QAction(QIcon(":/resources/icons/dimension.svg"), "Annotate Pile Cap", this);
  connect(annotatePierAction, &QAction::triggered, this, &MainWindow::onAnnotateBridgePierFooting);
  panelBridge->addLargeAction(annotatePierAction);

  QAction *fullBridgeAction = new QAction(QIcon(":/resources/icons/full_bridge.svg"), "Full Bridge (100 Piers)", this);
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

  QAction *fastAssemAction = new QAction(QIcon(":/resources/icons/fast_assembly.svg"), "Full Bridge (300 Piers, Fast)", this);
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

  SARibbonPanel *panelSubCrops = categoryBridge->addPanel("Sub-components");

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

  // Set initial dock widths for a balanced look
  QList<QDockWidget*> docks;
  docks << m_modelExplorerDock << m_propertyDock;
  QList<int> sizes;
  sizes << 300 << 350;
  resizeDocks(docks, sizes, Qt::Horizontal);

  connect(m_modelExplorerDock, &ModelExplorerPanel::nodeSelected, this, &MainWindow::onExplorerNodeSelected);

  // Move dock titles/tabs to the bottom for a cleaner CAD-like look
  setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::South);
  setTabPosition(Qt::RightDockWidgetArea, QTabWidget::South);
}

void MainWindow::onExplorerNodeSelected(Handle(BrNode_adObject) node) {
    if (node.IsNull()) return;

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
    onObjectSelected(metaMap);

    // 2. Highlighting and Focusing in OCCT View
    m_occtWidget->selectAndCenterObject("_adNodeId", node->GetId().ToCString());
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
    if (metadata.contains("_adNodeId")) {
        QString nodeId = metadata["_adNodeId"].toString();
        if (!m_currentModel.IsNull()) {
            Handle(ActAPI_INode) node = m_currentModel->FindNode(nodeId.toStdString().c_str());
            Handle(BrNode_adObject) adObj = Handle(BrNode_adObject)::DownCast(node);
            if (!adObj.IsNull()) {
                onExplorerNodeSelected(adObj);
                return;
            }
        }
    }

    // Clear layout
    QLayoutItem *child;
    while ((child = m_propertyLayout->takeAt(0)) != nullptr) {
        if (child->widget()) child->widget()->deleteLater();
        else if (child->layout()) {
             // Deep clear for nested layouts
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
    // Map: GroupName -> Map<PropertyName, Value>
    QMap<QString, QMap<QString, QString>> groups;
    
    for (auto it = metadata.begin(); it != metadata.end(); ++it) {
        QString fullKey = it.key();
        QString val = it.value().toString();
        
        if (fullKey.startsWith("_")) {
            groups["Basic Information"][fullKey.mid(1)] = val;
        } else if (fullKey.contains(".")) {
            int dotIdx = fullKey.indexOf('.');
            QString group = fullKey.left(dotIdx);
            QString key = fullKey.mid(dotIdx + 1);
            
            if (group.startsWith("Pset_")) group = group.mid(5);
            
            // Normalize Geometry and Material names for sorting
            if (group.contains("Geometry", Qt::CaseInsensitive)) group = "Geometry (" + group + ")";
            else if (group.contains("Material", Qt::CaseInsensitive)) group = "Material (" + group + ")";

            groups[group][key] = val;
        } else {
            groups["Attributes"][fullKey] = val;
        }
    }

    // Sort groups according to priority: Basic Information > Geometry > Material > Others
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

    // Create UI elements for each group in sorted order
    for (const QString& groupName : sortedGroupKeys) {
        // Section Header Widget
        QWidget* headerContainer = new QWidget();
        headerContainer->setStyleSheet("background-color: #333; border-left: 4px solid #00aaff; border-radius: 2px; margin-top: 5px;");
        QHBoxLayout* headerLayout = new QHBoxLayout(headerContainer);
        headerLayout->setContentsMargins(8, 4, 8, 4);
        
        QLabel* headerLabel = new QLabel(groupName.toUpper());
        headerLabel->setStyleSheet("color: #00aaff; font-weight: bold; font-size: 11px;");
        headerLayout->addWidget(headerLabel);
        
        m_propertyLayout->addWidget(headerContainer);
        
        // Property Grid
        QFrame* groupFrame = new QFrame();
        groupFrame->setStyleSheet("background-color: transparent;");
        QGridLayout* grid = new QGridLayout(groupFrame);
        grid->setContentsMargins(10, 5, 10, 10);
        grid->setSpacing(8);
        grid->setColumnStretch(0, 1);
        grid->setColumnStretch(1, 2);

        // Sort properties within the group
        QStringList propKeys = groups[groupName].keys();
        if (groupName.contains("Geometry", Qt::CaseInsensitive)) {
            // Priority: ModelNumber (模型ID) > Length (长) > Width (宽) > Height (高)
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
            
            QLabel* valLabel = new QLabel(propValue);
            valLabel->setStyleSheet("color: #eee; font-size: 12px; font-family: 'Consolas', monospace;");
            valLabel->setWordWrap(true);
            valLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

            grid->addWidget(propLabel, row, 0, Qt::AlignTop);
            grid->addWidget(valLabel, row, 1, Qt::AlignTop);
            row++;
        }
        m_propertyLayout->addWidget(groupFrame);
        
        // Separator
        QFrame* line = new QFrame();
        line->setFrameShape(QFrame::HLine);
        line->setStyleSheet("background-color: #222;");
        m_propertyLayout->addWidget(line);
    }
    
    m_propertyLayout->addStretch();
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

    std::vector<GeometryService::VisualShape> visualShapes;

    // 开启 Command 事务并在事务中构建几何并挂载到 XCAF 树上
    m_currentModel->OpenCommand();
    try {
        // 显式在事务内初始化 XCAF 的各种 Tools 以避免懒加载崩溃
        XCAFDoc_DocumentTool::ShapeTool(m_currentModel->Document()->Main());
        XCAFDoc_DocumentTool::ColorTool(m_currentModel->Document()->Main());
        XCAFDoc_DocumentTool::LayerTool(m_currentModel->Document()->Main());

        GeometryService geoService(m_currentModel);

        Handle(ActAPI_INode) rootBase = m_currentModel->GetRootNode();
        Handle(ActAPI_IChildIterator) it = rootBase->GetChildIterator();
        for (; it->More(); it->Next()) {
            Handle(BrNode_adObject) obj = Handle(BrNode_adObject)::DownCast(it->Value());
            if (!obj.IsNull()) {
                geoService.TraverseAndBuild(obj, visualShapes);
            }
        }
        m_currentModel->CommitCommand();
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
