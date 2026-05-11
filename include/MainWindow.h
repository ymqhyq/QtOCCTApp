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

#include <TopoDS_Shape.hxx>

#include <memory>

#include "OCCTWidget.h"

#include "DataModel.h"

class ShxTextGenerator;
class QLabel;
class QTextEdit;
class PythonSyntaxHighlighter;
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
  void onDrawBridgePier();            // 缁樺埗妗ュⅸ
  void onDrawFullBridgePier();        // 缁樺埗瀹屽叏浣撴ˉ澧?
  void onAnnotateBridgePierFooting(); // 鏍囨敞鏍囨敞妗ュⅸ鎵垮彴灏哄
  void onDrawFoundation();            // 缁樺埗閬块浄閽堝熀纭€
  void onDrawBedStone();              // 缁樺埗鍨煶
  void onDrawBearing();               // 缁樺埗鏀骇
  void onExportStepClicked();         // 瀵煎嚭涓篠TEP
  void onExportGltfClicked();         // 瀵煎嚭涓篏LTF
  void onMousePositionChanged(double x, double y, double z);
  void onObjectSelected(const QVariantMap &metadata);
  void onLoadAsiModel();             // 鍔犺浇ASI妯″瀷骞舵樉绀?

  // Microservice Connection
  void onCqNetworkReply(QNetworkReply *reply, int assemblyIndex);

private:
  void createRibbon();
  void setupCadQueryUi();
  void initializeCqNetwork();
  void sendScriptToMicroservice(const QString &code, const QJsonObject &args,
                                int assemblyIndex,
                                const QString &modelType = QString());
  void dispatchTask(int dummy = 0);
  QString readScript(const QString &modelName);

  OCCTWidget *m_occtWidget;
  QDockWidget *m_dockCq;
  QDockWidget *m_propertyDock;
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
};

#endif // MAINWINDOW_H