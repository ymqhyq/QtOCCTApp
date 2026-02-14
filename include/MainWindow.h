#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QCheckBox>
#include <QDockWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>

class OCCTWidget;
class ShxTextGenerator;
class QProcess;
class QLabel;
class QTextEdit;
class PythonSyntaxHighlighter;

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
  void onMousePositionChanged(double x, double y, double z);

private:
  void createFunctionalPanel();
  void setupCadQueryUi();
  void initializeCqProcess();
  void processCqOutput();

  OCCTWidget *m_occtWidget;
  QDockWidget *m_functionalPanel;
  QDockWidget *m_dockCq;
  QTextEdit *m_cqScriptEditor;
  QPushButton *m_drawLineButton;
  QCheckBox *m_solidTextCheckbox;
  std::unique_ptr<ShxTextGenerator> m_shxGenerator;
  QLabel *m_coordLabel;
  QProcess *m_cqProcess;
  PythonSyntaxHighlighter *m_highlighter;
};

#endif // MAINWINDOW_H