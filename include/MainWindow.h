#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "SARibbonMainWindow.h"
#include <QCheckBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPair>
#include <QPushButton>
#include <QQueue>
#include <QScrollArea>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QSplitter>
#include <QTemporaryDir>

#include <TopoDS_Shape.hxx>

#include <memory>

#include "OCCTWidget.h"
#include "DataModel.h"

class ShxTextGenerator;
class QLabel;
class QTextEdit;
class PythonSyntaxHighlighter;
class ModelExplorerPanel;
class ComponentLibraryPanel;
#include <Graphic3d_NameOfMaterial.hxx>

class MainWindow : public SARibbonMainWindow {
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

public slots:
  void onRunCqScript();

private slots:
  void onDrawLineClicked();
  void onAddShxText();
  void onDrawBridgePier();            // 绘制桥墩
  void onDrawFullBridgePier();        // 绘制完全体桥墩
  void onAnnotateBridgePierFooting(); // 标注标注桥墩承台尺寸
  void onDrawFoundation();            // 绘制避雷针基础
  void onDrawBedStone();              // 绘制垫石
  void onDrawBearing();               // 绘制支座
  void onExportStepClicked();         // 导出为STEP
  void onExportGltfClicked();         // 导出为GLTF
  void onExportIfcClicked();
  void onMousePositionChanged(double x, double y, double z);
  void onExplorerNodeSelected(Handle(BrNode_adObject) node);
  void onObjectSelected(const QVariantMap &metadata);
  void onLoadAsiModel();             // 加载ASI模型并显示
  void onLoadMasterCbf();            // 加载Master CBF并显示3D/2D
  void onImportBrep();               // 导入BREP文件
  void onImportIfc();                // 导入IFC文件并显示
  void onCloseModel();               // 关闭/清理模型视图
  void onComponentSelected(const QString& category, const QString& name);
  void onPropertyValueChanged(const QString& nodeId, const QString& propertyName, const QString& newValue);
  void onExportRdeClicked();         // 导出为RDE打包文件
  void refreshViews();

  // Microservice Connection
  void onCqNetworkReply(QNetworkReply *reply, int assemblyIndex);

protected:
  void closeEvent(QCloseEvent *event) override;

private:
  void updatePropertyPanelUI(const QVariantMap &metadata);
  void updateStretchHandles(const QVariantMap &flattenedMeta, Handle(BrNode_adObject) node);
  void createRibbon();
  void setupCadQueryUi();
  void createScriptsCategory();
  void initializeCqNetwork();
  void createTestCategory();
  bool openProjectFile(const QString &fileName);
  bool loadMasterCbf(const QString &fileName);
  bool extractRdeFile(const QString &rdePath, const QString &destDir);
  bool packageToRde(const QString &sourceDir, const QString &rdePath);
  void sendScriptToMicroservice(const QString &code, const QJsonObject &args,
                                int assemblyIndex,
                                const QString &modelType = QString());
  void dispatchTask(int dummy = 0);
  QString readScript(const QString &modelName);

  OCCTWidget *m_occtWidget;
  OCCTWidget *m_occtWidget2D;
  QSplitter *m_splitter;
  class ProjectManager *m_projectManager;
  QDockWidget *m_dockCq;
  QDockWidget *m_propertyDock;
  ModelExplorerPanel *m_modelExplorerDock;
  ComponentLibraryPanel *m_componentLibraryDock;
  QWidget *m_propertyWidget;
  QVBoxLayout *m_propertyLayout;
  QTextEdit *m_cqScriptEditor;
  QDoubleSpinBox *m_pierHeightSpinBox;
  QCheckBox *m_solidTextCheckbox;
  QCheckBox *m_pbrCheckbox;
  std::unique_ptr<ShxTextGenerator> m_shxGenerator;
  QLabel *m_coordLabel;
  QSharedPointer<QNetworkAccessManager> m_networkManager;
  QQueue<int> m_batchQueue;
  int m_completedTasks = 0;
  PythonSyntaxHighlighter *m_highlighter;
  Graphic3d_NameOfMaterial m_currentMaterial;
  bool m_fullBridgeMode = false;
  bool m_isBatchProcessing = false;
  QString m_currentModelType;
  int m_currentPierIndex = 0;
  int m_bridgePierCount = 100;
  double m_bridgePierSpacing = 340.0;
  QElapsedTimer m_batchTimer;
  bool m_isAssembling = false;
  QList<OCCTWidget::AssemblyPart> m_assemblyParts;
  QList<OCCTWidget::AssemblyPart> m_batchParts;
  Handle(DataModel) m_currentModel;
  QTemporaryDir *m_tempProjDir;
  QString m_loadedMasterPath;
};

#endif // MAINWINDOW_H