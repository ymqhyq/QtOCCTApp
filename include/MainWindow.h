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
class QLabel;

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

private slots:
  void onDrawLineClicked();
  void onAddShxText();
  void onMousePositionChanged(double x, double y, double z);

private:
  void createFunctionalPanel();

  OCCTWidget *m_occtWidget;
  QDockWidget *m_functionalPanel;
  QPushButton *m_drawLineButton;
  QCheckBox *m_solidTextCheckbox;
  std::unique_ptr<ShxTextGenerator> m_shxGenerator;
  QLabel *m_coordLabel;
};

#endif // MAINWINDOW_H