#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QCheckBox>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QList>
#include <QMainWindow>
#include <QPushButton>
#include <QQueue>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>

class OCCTWidget;
class ShxTextGenerator;
class QProcess;
class QLabel;
class QTextEdit;
class PythonSyntaxHighlighter;
#include <Graphic3d_NameOfMaterial.hxx>

class MainWindow : public QMainWindow {
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

private:
  void createFunctionalPanel();
  void setupCadQueryUi();
  void initializeCqProcess();
  void processCqOutput();
  void dispatchTask(QProcess *proc);
  QString getBridgePier2Script(double yOffset = 0.0);

  OCCTWidget *m_occtWidget;
  QDockWidget *m_functionalPanel;
  QDockWidget *m_dockCq;
  QTextEdit *m_cqScriptEditor;
  QPushButton *m_drawLineButton;
  QCheckBox *m_solidTextCheckbox;
  std::unique_ptr<ShxTextGenerator> m_shxGenerator;
  QLabel *m_coordLabel;
  QList<QProcess *> m_cqProcessList;
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
};

#endif // MAINWINDOW_H