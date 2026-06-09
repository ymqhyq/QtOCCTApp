#ifndef OCCTWIDGET_H
#define OCCTWIDGET_H

#include <QElapsedTimer>
#include <QLabel>
#include <QMouseEvent>
#include <QTimer>
#include <QWidget>

#include <AIS_InteractiveContext.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>
#include <AIS_ViewCube.hxx>

#include <AIS_Point.hxx>
#include <AIS_Shape.hxx>
#include <Geom_CartesianPoint.hxx>
#include <Geom_Line.hxx>
#include <Graphic3d_NameOfMaterial.hxx>
#include <Quantity_Color.hxx>
#include <TDocStd_Document.hxx>

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
  void selectAndCenterObject(const QString& key, const QVariant& value);
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
  void cleanup(); // 主动清理显卡资源
  static void releaseSharedDriver(); // 主动清理共享的底层图形驱动
  void setUsePbr(bool enabled);
  void applyMaterial(const Handle(AIS_InteractiveObject)& aisShape, const QVariantMap& metadata);
  void exportToSTEP(const QString &filename);
  void exportToGLTF(const QString &filename);
  void drawBridgePier();            // 绘制流线型桥墩
  void drawFullBridgePier();        // 绘制完整体桥墩
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
  void loadXcafDocument(const Handle(TDocStd_Document)& doc);
  void setAs2DView();
  void addLengthDimension(const gp_Pnt& p1, const gp_Pnt& p2, const gp_Pln& plane, double flyout);

  // 边坡长度拉伸交互接口
  void showLengthHandle(const gp_Pnt& startPos, const gp_Pnt& endPos, double currentLength, const QString& nodeId, const gp_Trsf& trsf, double height = 8000.0, double slopeRatio = 1.5);
  void hideLengthHandle();
  QVariantMap findMetadataByNodeId(const QString& nodeId) const;

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
  void propertyDragged(const QString& nodeId, const QString& propertyName, double newValue);

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void showEvent(QShowEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private:
  Standard_Integer m_xPos;
  Standard_Integer m_yPos;
  Standard_Integer m_startX;
  Standard_Integer m_startY;

  QPaintEngine *paintEngine() const override { return nullptr; }

private:
  void initOCCT();
  void initViewCube();
  void updateView();

  Handle(V3d_Viewer) m_viewer;
  Handle(V3d_View) m_view;
  Handle(AIS_InteractiveContext) m_context;
  Handle(OpenGl_GraphicDriver) m_graphicDriver;
  Handle(AspectWindow) m_aspectWindow;

  std::list<Handle(AIS_Shape)> m_lines;
  QMap<Handle(AIS_InteractiveObject), QVariantMap> m_objectMetadata;
  std::list<Handle(AIS_InteractiveObject)> m_dimensions;
  Handle(AIS_Shape) m_selectedLine;

  bool m_drawLineMode;
  bool m_usePbr;
  gp_Pnt m_firstPoint;
  bool m_firstPointSet;
  Handle(AIS_Shape) m_dynamicLine;

  bool Get3DPoint(int userX, int userY, gp_Pnt &outPoint);

  QElapsedTimer m_fpsTimer;
  int m_frameCount;
  double m_fps;
  int m_shapeCount;
  QLabel *m_infoLabel;
  QTimer m_refreshTimer;

  Handle(AIS_ViewCube) m_viewCube;
  QSize m_lastSize; // 用于检测 widget 大小变化
  bool m_showViewCube;
  bool m_enableRotation;

  // 长度拉伸交互状态
  Handle(AIS_Shape) m_startHandle;
  Handle(AIS_Shape) m_lengthHandle;
  Handle(AIS_Shape) m_dynamicPreview;
  bool m_isDraggingLength;
  bool m_dragIsStartHandle;
  QString m_dragNodeId;
  double m_dragOriginalLength;
  double m_dragCurrentLength;
  gp_Trsf m_dragTrsf;
  gp_Pnt m_dragStartPos;
  gp_Pnt m_dragEndPos;
  double m_dragHeight;
  double m_dragSlopeRatio;

  void cancelLengthDragging();
};

#endif // OCCTWIDGET_H