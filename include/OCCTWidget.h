#ifndef OCCTWIDGET_H
#define OCCTWIDGET_H

#include <QMouseEvent>
#include <QWidget>

#include <AIS_InteractiveContext.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>

#include <AIS_Point.hxx>
#include <AIS_Shape.hxx>
#include <Geom_CartesianPoint.hxx>
#include <Geom_Line.hxx>
#include <Quantity_Color.hxx>

#include <list>

// Forward declaration
class AspectWindow;

class Line;

class OCCTWidget : public QWidget {
  Q_OBJECT

public:
  explicit OCCTWidget(QWidget *parent = nullptr);
  ~OCCTWidget();

  void addLine(const gp_Pnt &start, const gp_Pnt &end);
  void
  addShape(const TopoDS_Shape &shape,
           const Quantity_Color &color = Quantity_Color(Quantity_NOC_WHITE));
  void selectLine(const gp_Pnt &point);
  void setDrawLineMode(bool enabled) { m_drawLineMode = enabled; }
  void generateRandomLines(int count);
  void add3DText(const QString &text, double height, const gp_Pnt &position,
                 bool isSolid = false, double angle = 0.0);
  void setTextsSolid(bool isSolid);
  void fitAll(); // 缩放到全部视图范围

private:
  TopoDS_Shape makeTextShape(const QString &text, double height,
                             const gp_Pnt &position, double angle = 0.0,
                             const QString &fontName = QString());
  TopoDS_Shape makeTextShape2(const QString &text, double height,
                              const gp_Pnt &position, double angle = 0.0,
                              const QString &fontName = QString());

signals:
  void lineSelected();
  void mousePositionChanged(double x, double y, double z);

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;

private:
  Standard_Integer m_xPos;
  Standard_Integer m_yPos;

  QPaintEngine *paintEngine() const override { return nullptr; }

private:
  void initOCCT();
  void updateView();

  Handle(V3d_Viewer) m_viewer;
  Handle(V3d_View) m_view;
  Handle(AIS_InteractiveContext) m_context;
  Handle(OpenGl_GraphicDriver) m_graphicDriver;
  AspectWindow *m_aspectWindow;

  std::list<Handle(AIS_Shape)> m_lines;
  Handle(AIS_Shape) m_selectedLine;

  bool m_drawLineMode;
  gp_Pnt m_firstPoint;
  bool m_firstPointSet;
  Handle(AIS_Shape) m_dynamicLine;

  bool Get3DPoint(int userX, int userY, gp_Pnt &outPoint);
};

#endif // OCCTWIDGET_H