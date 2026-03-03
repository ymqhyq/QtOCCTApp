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
#include <Graphic3d_NameOfMaterial.hxx>
#include <Quantity_Color.hxx>

#include <QMap>
#include <QVariant>
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
           const Quantity_Color &color = Quantity_Color(Quantity_NOC_YELLOW),
           Graphic3d_NameOfMaterial material = Graphic3d_NOM_PLASTIC,
           const QVariantMap &metadata = QVariantMap());
  void selectLine(const gp_Pnt &point);
  void setDrawLineMode(bool enabled) { m_drawLineMode = enabled; }
  void generateRandomLines(int count);
  void add3DText(const QString &text, double height, const gp_Pnt &position,
                 bool isSolid = false, double angle = 0.0);
  void setTextsSolid(bool isSolid);
  void fitAll(); // 缩放到全部视图范围
  void loadBrepFile(const QString &filename,
                    Graphic3d_NameOfMaterial material = Graphic3d_NOM_PLASTIC);
  void clearAll();
  void drawBridgePier();            // 绘制流线型桥墩
  void drawFullBridgePier();        // 绘制完全体桥墩
  void annotateBridgePierFooting(); // 标注桥墩承台尺寸
  void loadBrepAsFullBridge(
      const QString &filename, int count, double spacing,
      Graphic3d_NameOfMaterial material = Graphic3d_NOM_PLASTIC);
  void loadBrepFileDeferred(
      const QString &filename,
      Graphic3d_NameOfMaterial material = Graphic3d_NOM_PLASTIC,
      double yOffset = 0.0);
  TopoDS_Shape readBrepFileToShape(const QString &filename);
  TopoDS_Shape readBrepFromMemory(const QByteArray &data);
  void displayShape(const TopoDS_Shape &shape,
                    Graphic3d_NameOfMaterial material = Graphic3d_NOM_PLASTIC,
                    bool fit = true,
                    const QVariantMap &metadata = QVariantMap());
  void displayShape(const TopoDS_Shape &shape,
                    Graphic3d_NameOfMaterial material,
                    const Quantity_Color &color, bool fit = true,
                    const QVariantMap &metadata = QVariantMap());
  struct AssemblyPart {
    TopoDS_Shape shape;
    Graphic3d_NameOfMaterial material;
    QVariantMap metadata;
  };
  void buildFullBridgeFromParts(const QList<AssemblyPart> &parts, int count,
                                double spacing);
  void buildFullBridgeFromBatch(const QList<AssemblyPart> &parts);

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
  void objectSelected(const QVariantMap &metadata);

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
  Standard_Integer m_startX;
  Standard_Integer m_startY;

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
  QMap<Handle(AIS_InteractiveObject), QVariantMap> m_objectMetadata;
  std::list<Handle(AIS_InteractiveObject)> m_dimensions;
  Handle(AIS_Shape) m_selectedLine;

  bool m_drawLineMode;
  gp_Pnt m_firstPoint;
  bool m_firstPointSet;
  Handle(AIS_Shape) m_dynamicLine;

  bool Get3DPoint(int userX, int userY, gp_Pnt &outPoint);
};

#endif // OCCTWIDGET_H