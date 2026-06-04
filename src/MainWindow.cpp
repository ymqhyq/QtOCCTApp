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
#include <gp_Trsf.hxx>
#include <BRepBuilderAPI_Transform.hxx>

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

  if (IfcExportService::Export(m_currentModel, filename.toStdString())) {
    QMessageBox::information(this, "Export IFC", "Successfully exported to " + filename);
  } else {
    QMessageBox::critical(this, "Export IFC", "Failed to export IFC. Please check logs.");
  }
}
