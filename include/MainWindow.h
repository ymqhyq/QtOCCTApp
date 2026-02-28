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

#include <TopoDS_Shape.hxx>

#include <memory>

class OCCTWidget;
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
  void onDrawBridgePier();            // 绘制桥墩
  void onAnnotateBridgePierFooting(); // 标注标注桥墩承台尺寸
  void onMousePositionChanged(double x, double y, double z);

  // Microservice Connection
  void onCqNetworkReply(QNetworkReply *reply, int assemblyIndex);

private:
  void createRibbon();
  void setupCadQueryUi();
  void initializeCqNetwork();
  void sendScriptToMicroservice(const QString &code, const QJsonObject &args,
                                int assemblyIndex);
  void dispatchTask(int dummy = 0);
  QString readScript(const QString &modelName);

  OCCTWidget *m_occtWidget;
  QDockWidget *m_dockCq;
  QTextEdit *m_cqScriptEditor;
  QDoubleSpinBox *m_pierHeightSpinBox;
  QCheckBox *m_solidTextCheckbox;
  std::unique_ptr<ShxTextGenerator> m_shxGenerator;
  QLabel *m_coordLabel;
  QSharedPointer<QNetworkAccessManager> m_networkManager;
  QQueue<int> m_batchQueue;
  int m_completedTasks = 0;
  PythonSyntaxHighlighter *m_highlighter;
  Graphic3d_NameOfMaterial m_currentMaterial;
  bool m_fullBridgeMode = false;
  bool m_isBatchProcessing = false;
  int m_currentPierIndex = 0;
  int m_bridgePierCount = 100;
  double m_bridgePierSpacing = 340.0;
  QElapsedTimer m_batchTimer;
  bool m_isAssembling = false;
  QList<QPair<TopoDS_Shape, Graphic3d_NameOfMaterial>> m_assemblyParts;
};

#endif // MAINWINDOW_H