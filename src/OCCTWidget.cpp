#include "../include/OCCTWidget.h"
#include "../include/AspectWindow.h"
#include "../include/Line.h"

#include <Aspect_DisplayConnection.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRep_Tool.hxx>
#include <Graphic3d_Camera.hxx>
#include <IntAna_Quadric.hxx>
#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QShowEvent>
#include <Quantity_Color.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Lin.hxx>
#include <gp_Pln.hxx>

OCCTWidget::OCCTWidget(QWidget *parent)
    : QWidget(parent), m_viewer(nullptr), m_view(nullptr), m_context(nullptr),
      m_graphicDriver(nullptr), m_aspectWindow(nullptr),
      m_selectedLine(nullptr), m_drawLineMode(false), m_firstPointSet(false) {
  setFocusPolicy(Qt::StrongFocus);

  // Set required attributes for OCCT integration
  setAttribute(Qt::WA_PaintOnScreen);
  setAttribute(Qt::WA_NoSystemBackground);
  setMouseTracking(true);
  setBackgroundRole(QPalette::NoRole);

  initOCCT();
}

void OCCTWidget::initOCCT() {
  try {
    Handle(Aspect_DisplayConnection) aDisplayConnection =
        new Aspect_DisplayConnection();
    m_graphicDriver = new OpenGl_GraphicDriver(aDisplayConnection);
    // 重要：设置图形驱动器的选项
    // m_graphicDriver->ChangeOptions().buffersNoSwap = Standard_True;
    // m_graphicDriver->ChangeOptions().glslWarnings = Standard_False;
    // Create Viewer
    m_viewer = new V3d_Viewer(m_graphicDriver);
    m_viewer->SetDefaultLights();
    m_viewer->SetLightOn();

    // Create View
    m_view = m_viewer->CreateView();

    // Create AIS Interactive Context
    m_context = new AIS_InteractiveContext(m_viewer);

    // Improve ease of picking/highlighting
    m_context->SetPixelTolerance(10); // Easier to hit lines

    // Create custom aspect window
    m_aspectWindow = new AspectWindow(this);
    m_view->SetWindow(m_aspectWindow);
    if (!m_aspectWindow->IsMapped()) {
      m_aspectWindow->Map();
    }

    // Set up the view
    // Set up the view for 2D (Top Orthographic)
    m_view->Camera()->SetProjectionType(
        Graphic3d_Camera::Projection_Orthographic);
    m_view->SetProj(V3d_Zpos);

    // 将视图中心对准世界坐标原点 (0,0,0)
    m_view->Camera()->SetCenter(gp_Pnt(0, 0, 0));
    m_view->SetScale(100.0); // 设置合适的初始缩放比例

    updateView();
  } catch (const std::exception &e) {
    // 在调试版本中，我们可以输出错误信息
    // 但在实际应用中，你可能想使用其他日志机制
  } catch (...) {
    // 捕获所有异常
  }
}

OCCTWidget::~OCCTWidget() {
  if (m_aspectWindow) {
    delete m_aspectWindow;
  }
}

void OCCTWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  if (!m_view.IsNull()) {
    m_view->Redraw();
  }
}

void OCCTWidget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);

  if (!m_view.IsNull()) {
    m_view->MustBeResized();
    m_view->Redraw();
  }
}

bool OCCTWidget::Get3DPoint(int userX, int userY, gp_Pnt &outPoint) {
  if (m_view.IsNull())
    return false;

  // Handle DPI scaling
  qreal pixelRatio = devicePixelRatio();
  Standard_Integer x = static_cast<Standard_Integer>(userX * pixelRatio);
  Standard_Integer y = static_cast<Standard_Integer>(userY * pixelRatio);

  Standard_Real x_eye, y_eye, z_eye, x_dir, y_dir, z_dir;
  m_view->ConvertWithProj(x, y, x_eye, y_eye, z_eye, x_dir, y_dir, z_dir);

  // Manual intersection with Plane Z=0
  if (Abs(z_dir) > 1e-6) {
    Standard_Real t = -z_eye / z_dir;
    outPoint.SetCoord(x_eye + t * x_dir, y_eye + t * y_dir, z_eye + t * z_dir);

    // --- Snapping Logic ---
    Standard_Real minDistance = 1.0; // Snapping threshold in World Units
    gp_Pnt snapPoint;
    bool foundSnap = false;

    for (const auto &aisShape : m_lines) {
      if (aisShape.IsNull())
        continue;

      TopExp_Explorer exp(aisShape->Shape(), TopAbs_VERTEX);
      for (; exp.More(); exp.Next()) {
        const TopoDS_Vertex &vertex = TopoDS::Vertex(exp.Current());
        gp_Pnt p = BRep_Tool::Pnt(vertex);

        // Convert world point to screen space to check pixel distance?
        // Or just check 3D distance since we are on flat plane logic mostly.
        // Let's check 3D distance for now.
        if (outPoint.Distance(p) < minDistance) {
          if (!foundSnap ||
              outPoint.Distance(p) < outPoint.Distance(snapPoint)) {
            snapPoint = p;
            minDistance = outPoint.Distance(p); // Keep finding closest
            foundSnap = true;
          }
        }
      }
    }

    if (foundSnap) {
      outPoint = snapPoint;
      // Optional: You could draw a marker here
    }
    // ----------------------

    return true;
  }
  return false;
}

void OCCTWidget::mousePressEvent(QMouseEvent *event) {
  if (m_context.IsNull()) {
    return;
  }

  gp_Pnt clickPoint;
  bool hasIntersection =
      Get3DPoint(event->pos().x(), event->pos().y(), clickPoint);

  // Track mouse position for panning
  m_xPos = event->pos().x();
  m_yPos = event->pos().y();

  // Check if we are panning (Left Button + Not Drawing/Selecting or perhaps
  // just Left Button? User asked to CHANGE to Left Button. However, Left Button
  // is also used for Drawing and Picking. Usually, Left Button = Pick/Draw,
  // Middle/Right = Pan/Rotate. If User wants Left Button Pan, we must
  // differentiate context. Assuming: Left Drag = Pan, Left Click = Select/Draw
  // But we trigger Draw on Press.

  // Let's implement: If Draw Mode is OFF, Left Drag = Pan?
  // Or maybe User wants it always? If always, how to Draw?
  // Assuming User wants to REPLACE the Middle Button logic with Left Button
  // logic for Panning BUT we must allow Click for Selection/Drawing. So we only
  // Pan in Move if Left Button is held.

  // In Press, we just initialize. logic in Move handles the dragging.
  // We shouldn't return early here if we want to allow Click actions on Release
  // or Press. But previous code returned early for Middle Button.

  // Let's just update tracking here. Actual conflict resolution is in Mouse
  // Move vs Press logic. If we want Pan on Left Drag, we need to know if it's a
  // Click or Drag. For now, let's keep the return early ONLY if we decide this
  // press is EXCLUSIVELY for panning. But if Left Button is overloaded, we
  // can't return early.

  // Removing the early return for the button check to allow mixed usage (Drag
  // to Pan, Click to Select) or we check modifiers?

  // Simple implementation as requested: Change Middle to Left in Move logic.
  // In Press, we don't block.

  // Original code had:
  /*
  if (event->buttons() & Qt::MiddleButton) {
      return;
  }
  */

  // We remove the blocking check so execution continues to picking logic
  // (ClickPoint calculation). Panning will happen in Move event.

  if (!hasIntersection) {
    return;
  }

  if (m_drawLineMode) {
    // First click - set the first point
    if (!m_firstPointSet) {
      m_firstPoint = clickPoint;
      m_firstPointSet = true;
    } else {
      // Second click - create the line
      addLine(m_firstPoint, clickPoint);
      m_firstPointSet = false;
      m_drawLineMode = false; // Exit line drawing mode

      // Remove dynamic line
      if (!m_dynamicLine.IsNull()) {
        m_context->Remove(m_dynamicLine, Standard_True);
        m_dynamicLine.Nullify();
      }
    }
  } else {
    // Select a line if clicked on one
    // Logic remains similar but using original coordinates for MoveTo
    qreal pixelRatio = devicePixelRatio();
    m_context->MoveTo(static_cast<int>(event->pos().x() * pixelRatio),
                      static_cast<int>(event->pos().y() * pixelRatio), m_view,
                      Standard_True);

    m_context->Select(Standard_True);
  }
  update();
}

void OCCTWidget::mouseReleaseEvent(QMouseEvent *event) {
  // Handle release if needed
  Q_UNUSED(event);
}

void OCCTWidget::wheelEvent(QWheelEvent *event) {
  if (m_view.IsNull()) {
    return;
  }

  // Handle DPI scaling
  qreal pixelRatio = devicePixelRatio();
  Standard_Integer x =
      static_cast<Standard_Integer>(event->position().x() * pixelRatio);
  Standard_Integer y =
      static_cast<Standard_Integer>(event->position().y() * pixelRatio);

  m_view->StartZoomAtPoint(x, y);

  // Zoom based on wheel delta
  // A standard wheel step is 120 (or 15 degrees in eighths)
  Standard_Real delta = event->angleDelta().y();
  if (delta > 0) {
    m_view->ZoomAtPoint(x, y, x + 10, y + 10); // Zoom in - Reduced sensitivity
  } else {
    m_view->ZoomAtPoint(x, y, x - 10, y - 10); // Zoom out - Reduced sensitivity
  }

  // Or simpler approach if ZoomAtPoint behaves oddly:
  // Standard_Real aZoomFactor = (delta > 0) ? 1.1 : 0.9;
  // m_view->SetScale(m_view->Scale() * aZoomFactor);

  m_view->Redraw();
}

void OCCTWidget::mouseMoveEvent(QMouseEvent *event) {
  if (m_context.IsNull())
    return;

  // 获取鼠标位置对应的3D坐标并发射信号
  gp_Pnt mouseWorldPos;
  if (Get3DPoint(event->pos().x(), event->pos().y(), mouseWorldPos)) {
    emit mousePositionChanged(mouseWorldPos.X(), mouseWorldPos.Y(),
                              mouseWorldPos.Z());
  }

  // Check for Panning (Left Button and NOT in Draw Mode)
  // If we are in Draw Mode, Left Drag updates the line preview.
  // If we are NOT in Draw Mode, Left Drag pans.
  if ((event->buttons() & Qt::LeftButton) && !m_drawLineMode) {
    // Pan the view
    if (!m_view.IsNull()) {
      m_view->Pan(event->pos().x() - m_xPos, m_yPos - event->pos().y());
      m_xPos = event->pos().x();
      m_yPos = event->pos().y();
      m_view->Redraw();
      return; // Consume event if panning
    }
  }

  // Update position for next move
  m_xPos = event->pos().x();
  m_yPos = event->pos().y();

  if (m_drawLineMode && m_firstPointSet) {
    gp_Pnt currentPoint;
    if (Get3DPoint(event->pos().x(), event->pos().y(), currentPoint)) {
      // Redraw dynamic line
      if (!m_dynamicLine.IsNull()) {
        m_context->Remove(m_dynamicLine, Standard_False);
        m_dynamicLine.Nullify();
      }

      try {
        TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(m_firstPoint, currentPoint);
        m_dynamicLine = new AIS_Shape(edge);
        m_context->SetColor(m_dynamicLine, Quantity_Color(Quantity_NOC_YELLOW),
                            Standard_False);
        m_context->Display(m_dynamicLine, Standard_True);
      } catch (...) {
        // Ignore errors during dynamic drawing
      }
    }
  } else {
    // Standard highlighting
    qreal pixelRatio = devicePixelRatio();
    m_context->MoveTo(static_cast<int>(event->pos().x() * pixelRatio),
                      static_cast<int>(event->pos().y() * pixelRatio), m_view,
                      Standard_True);
  }
}

void OCCTWidget::addLine(const gp_Pnt &start, const gp_Pnt &end) {
  if (!m_context.IsNull()) {
    // Create edge from the two points
    TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(start, end);

    // Create AIS_Shape for visualization
    Handle(AIS_Shape) lineShape = new AIS_Shape(edge);

    // Set the color to green
    m_context->SetColor(lineShape, Quantity_Color(Quantity_NOC_GREEN),
                        Standard_True);

    // Display the shape
    m_context->Display(lineShape, Standard_True);

    // Store the line for reference
    m_lines.push_back(lineShape);

    updateView();
  }
}

void OCCTWidget::addShape(const TopoDS_Shape &shape,
                          const Quantity_Color &color) {
  if (!m_context.IsNull()) {
    Handle(AIS_Shape) aisShape = new AIS_Shape(shape);
    m_context->SetColor(aisShape, color, Standard_True);
    m_context->Display(aisShape, Standard_True);
    m_lines.push_back(aisShape); // Track it
    updateView();
  }
}

void OCCTWidget::selectLine(const gp_Pnt &point) {
  // Find the closest line to the point and change its color to red
  // This is a simplified implementation
}

void OCCTWidget::updateView() {
  if (!m_view.IsNull()) {
    m_view->ZFitAll(); // Adjust clipping planes only
    // m_view->FitAll(); // Don't refit camera to all objects
    m_view->Redraw();
  }
}

void OCCTWidget::generateRandomLines(int count) {
  if (m_context.IsNull())
    return;

  srand(0); // Fixed seed for reproducibility or time(NULL) for random

  for (int i = 0; i < count; ++i) {
    // Random 3D P1 and P2 in range [-500, 500]
    double x1 = (rand() % 1000) - 500;
    double y1 = (rand() % 1000) - 500;
    double z1 = (rand() % 100) - 50; // Slight z variation

    double x2 = (rand() % 1000) - 500;
    double y2 = (rand() % 1000) - 500;
    double z2 = (rand() % 100) - 50;

    gp_Pnt start(x1, y1, z1);
    gp_Pnt end(x2, y2, z2);

    // Create edge
    TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(start, end);
    Handle(AIS_Shape) lineShape = new AIS_Shape(edge);

    // Random Color
    // Safer color:
    Quantity_Color col((rand() % 100) / 100.0, (rand() % 100) / 100.0,
                       (rand() % 100) / 100.0, Quantity_TOC_RGB);

    m_context->SetColor(lineShape, col, Standard_False);

    // Display without update
    m_context->Display(lineShape, Standard_False);

    m_lines.push_back(lineShape);
  }

  m_view->ZFitAll();
  m_view->FitAll(); // Fit all for this initial generation
  m_view->Redraw();
}

#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRep_Builder.hxx>
#include <Bnd_Box.hxx>
#include <Font_FontMgr.hxx>
#include <NCollection_String.hxx>
#include <StdPrs_BRepFont.hxx>
#include <StdPrs_BRepTextBuilder.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Trsf.hxx>
// Helper to get string advance width approximation
double GetCharAdvance(const TopoDS_Shape &charShape, double height) {
  if (charShape.IsNull())
    return height * 0.5; // Default spacing for spaces
  Bnd_Box bbox;
  BRepBndLib::Add(charShape, bbox);
  double xMin, yMin, zMin, xMax, yMax, zMax;
  bbox.Get(xMin, yMin, zMin, xMax, yMax, zMax);
  return (xMax - xMin) + height * 0.1; // Width + 10% spacing
}

TopoDS_Shape OCCTWidget::makeTextShape(const QString &text, double height,
                                       const gp_Pnt &position, double angle,
                                       const QString &fontName) {
  // Font managers
  StdPrs_BRepFont fontAscii;
  StdPrs_BRepFont fontChinese;

  // Default Font Scheme: Mixed Arial + FangSong
  // If a specific fontName is provided, use it for both if possible, or mixed
  // if not. For now, we stick to the Requirement: "Default Arial and SimFang
  // mixed scheme" If fontName is empty, use default scheme.

  bool useCustomFont = !fontName.isEmpty();
  bool asciiLoaded = false;
  bool chineseLoaded = false;

  if (useCustomFont) {
    asciiLoaded =
        fontAscii.Init(fontName.toUtf8().constData(), Font_FA_Regular, height);
    chineseLoaded = asciiLoaded; // Use same font for both
  } else {
    // Initialize default scheme
    // English: Arial
    asciiLoaded = fontAscii.Init("Arial", Font_FA_Regular, height);
    if (!asciiLoaded)
      asciiLoaded =
          fontAscii.Init(Font_NOF_SANS_SERIF, Font_FA_Regular, height);

    // Chinese: SimFang (FangSong)
    if (fontChinese.Init("FangSong", Font_FA_Regular, height))
      chineseLoaded = true;
    else if (fontChinese.Init("SimFang", Font_FA_Regular, height))
      chineseLoaded = true;
    else if (fontChinese.Init("仿宋", Font_FA_Regular, height))
      chineseLoaded = true;

    if (!chineseLoaded)
      chineseLoaded = fontChinese.Init(Font_NOF_SERIF, Font_FA_Regular, height);
  }

  if (!asciiLoaded && !chineseLoaded)
    return TopoDS_Shape();

  TopoDS_Compound resultShape;
  BRep_Builder builder;
  builder.MakeCompound(resultShape);

  double currentX = 0.0;
  StdPrs_BRepTextBuilder aBuilder;

  for (int i = 0; i < text.length(); ++i) {
    QChar qChar = text.at(i);
    ushort unicode = qChar.unicode();

    // Font Selection Logic
    StdPrs_BRepFont *pFont = nullptr;

    if (useCustomFont) {
      pFont = &fontAscii;
    } else {
      // Mixed Scheme
      if (unicode < 128) {
        if (asciiLoaded)
          pFont = &fontAscii;
        else if (chineseLoaded)
          pFont = &fontChinese;
      } else {
        if (chineseLoaded)
          pFont = &fontChinese;
        else if (asciiLoaded)
          pFont = &fontAscii;
      }
    }

    if (!pFont)
      continue; // Should not happen if initialization check passed

    TopoDS_Shape charShape;
    if (qChar == ' ') {
      currentX += height * 0.3; // Space advance
      continue;
    } else {
      // Generate Shape
      QString charStr(qChar);
      NCollection_String occtStr(charStr.toUtf8().constData());
      charShape = aBuilder.Perform(*pFont, occtStr);
    }

    if (!charShape.IsNull()) {
      // Translate to current line position (pre-rotation)
      gp_Trsf aTrsf;
      aTrsf.SetTranslation(gp_Vec(currentX, 0, 0));
      BRepBuilderAPI_Transform aTransform(charShape, aTrsf);
      TopoDS_Shape placedChar = aTransform.Shape();

      builder.Add(resultShape, placedChar);

      // Calculate advance
      currentX += GetCharAdvance(charShape, height);
    }
  }

  // Apply global position and rotation
  gp_Trsf aFinalTrsf;
  // 1. Rotate around Origin (0,0,0) of the text
  gp_Trsf aRot;
  aRot.SetRotation(gp::OZ(), angle * M_PI / 180.0);

  // 2. Translate to Position
  gp_Trsf aTrans;
  aTrans.SetTranslation(gp_Vec(gp::Origin(), position));

  aFinalTrsf = aTrans * aRot;

  BRepBuilderAPI_Transform aFinalTransform(resultShape, aFinalTrsf);
  return aFinalTransform.Shape();
}

void OCCTWidget::add3DText(const QString &text, double height,
                           const gp_Pnt &position, bool isSolid, double angle) {
  if (m_context.IsNull())
    return;

  // Call encapsulated method with default mixed font (empty string)
  TopoDS_Shape textShape =
      makeTextShape(text, height, position, angle, QString());

  if (!textShape.IsNull()) {
    // Create AIS Interactive Object
    Handle(AIS_Shape) aisShape = new AIS_Shape(textShape);

    // Set color
    m_context->SetColor(aisShape, Quantity_Color(Quantity_NOC_YELLOW),
                        Standard_False);

    // Set display mode: 0 = Wireframe (Hollow), 1 = Shaded (Solid)
    m_context->SetDisplayMode(aisShape, isSolid ? 1 : 0, Standard_False);

    // Display
    m_context->Display(aisShape, Standard_True);
    m_lines.push_back(aisShape);

    // Update view
    updateView();
  }
}

void OCCTWidget::setTextsSolid(bool isSolid) {
  if (m_context.IsNull())
    return;

  // Use display mode to control solid (Shaded=1) vs hollow (Wireframe=0)
  Standard_Integer mode = isSolid ? 1 : 0;

  // Iterate over all managed objects (m_lines stores them)
  // In a real app we might want to filter only text objects.
  for (const auto &aisShape : m_lines) {
    if (!aisShape.IsNull()) {
      // Check if already displayed to avoid errors? SetDisplayMode is safe.
      m_context->SetDisplayMode(aisShape, mode, Standard_False);
    }
  }

  m_context->UpdateCurrentViewer();
}

void OCCTWidget::fitAll() {
  if (!m_view.IsNull()) {
    m_view->FitAll();
    m_view->ZFitAll();
    m_view->Redraw();
  }
}