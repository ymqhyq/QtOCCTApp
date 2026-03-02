#include "../include/OCCTWidget.h"
#include "../include/AspectWindow.h"
#include "../include/Line.h"

#include <Aspect_DisplayConnection.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <Graphic3d_Camera.hxx>
#include <IntAna_Quadric.hxx>
#include <Prs3d_DimensionAspect.hxx>
#include <Prs3d_LineAspect.hxx>
#include <Prs3d_TextAspect.hxx>
#include <PrsDim_LengthDimension.hxx>
#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QShowEvent>
#include <Quantity_Color.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Lin.hxx>
#include <gp_Pln.hxx>
#include <gp_Trsf.hxx>

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
    // m_graphicDriver->ChangeOptions().buffersNoSwap = true;
    // m_graphicDriver->ChangeOptions().glslWarnings = false;
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
    m_context->SetDisplayMode(AIS_Shaded, true);

    // Create custom aspect window
    m_aspectWindow = new AspectWindow(this);
    m_view->SetWindow(m_aspectWindow);
    if (!m_aspectWindow->IsMapped()) {
      m_aspectWindow->Map();
    }

    // Set up the view
    // Set up the view for 3D (Perspective)
    m_view->Camera()->SetProjectionType(
        Graphic3d_Camera::Projection_Perspective);
    m_view->SetProj(V3d_XposYnegZpos); // Iso view

    // 将视图中心对准世界坐标原点 (0,0,0)
    m_view->Camera()->SetCenter(gp_Pnt(0, 0, 0));
    m_view->SetScale(100.0); // 设置合适的初始缩放比例

    // 开启 Phong 着色以获得更好的金属高光效果 (像素级光照)
    m_view->SetShadingModel(V3d_PHONG);

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
  if (m_context.IsNull())
    return;

  // Start rotation
  // Start rotation
  if (event->button() == Qt::RightButton) {
    if (!m_view.IsNull()) {
      m_view->StartRotation(event->pos().x(), event->pos().y());
      m_startX = event->pos().x();
      m_startY = event->pos().y();
    }
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

  /*
  if (!hasIntersection) {
    return;
  }
  */

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
        m_context->Remove(m_dynamicLine, true);
        m_dynamicLine.Nullify();
      }
    }
  } else if (event->button() == Qt::LeftButton) {
    // Select a line if clicked on one
    // Logic remains similar but using original coordinates for MoveTo
    qreal pixelRatio = devicePixelRatio();
    m_context->MoveTo(static_cast<int>(event->pos().x() * pixelRatio),
                      static_cast<int>(event->pos().y() * pixelRatio), m_view,
                      true);

    m_context->Select(true);
  }
  update();
}

void OCCTWidget::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::RightButton) {
    int dx = event->pos().x() - m_startX;
    int dy = event->pos().y() - m_startY;
    if (std::abs(dx) < 10 && std::abs(dy) < 10) {
      if (!m_context.IsNull()) {
        qreal pixelRatio = devicePixelRatio();
        m_context->MoveTo(static_cast<int>(event->pos().x() * pixelRatio),
                          static_cast<int>(event->pos().y() * pixelRatio),
                          m_view, true);

        m_context->Select(true);

        m_context->InitSelected();
        qDebug() << "MoreSelected:" << m_context->MoreSelected();
        if (m_context->MoreSelected()) {
          qDebug() << "Menu will be shown";
          QMenu contextMenu(this);
          QAction *deleteAction =
              contextMenu.addAction(QString::fromUtf8("删除模型"));

          QAction *selectedAction =
              contextMenu.exec(event->globalPosition().toPoint());
          if (selectedAction == deleteAction) {
            std::list<Handle(AIS_InteractiveObject)> objectsToRemove;
            m_context->InitSelected();
            while (m_context->MoreSelected()) {
              objectsToRemove.push_back(m_context->SelectedInteractive());
              m_context->NextSelected();
            }
            m_context->ClearSelected(Standard_False);

            for (auto obj : objectsToRemove) {
              Handle(AIS_Shape) shapeObj = Handle(AIS_Shape)::DownCast(obj);
              if (!shapeObj.IsNull()) {
                m_lines.remove(shapeObj);
              }
              m_context->Remove(obj, Standard_False);
            }
            m_context->UpdateCurrentViewer();
          }
        }
      }
    }
  }
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
  // Check for Rotation (Right Button or Left + Ctrl)
  if ((event->buttons() & Qt::RightButton) ||
      ((event->buttons() & Qt::LeftButton) &&
       (event->modifiers() & Qt::ControlModifier))) {
    if (!m_view.IsNull()) {
      // Sensitivity factor
      double sensitivity = 2.5;

      // We calculate a virtual position that is further away from start than
      // real mouse
      int dx = event->pos().x() - m_startX;
      int dy = event->pos().y() - m_startY;

      int virtualX = m_startX + static_cast<int>(dx * sensitivity);
      int virtualY = m_startY + static_cast<int>(dy * sensitivity);

      m_view->Rotation(virtualX, virtualY);
      return;
    }
  }

  // Check for Panning (Middle Button or Left + Shift)
  if ((event->buttons() & Qt::MiddleButton) ||
      ((event->buttons() & Qt::LeftButton) &&
       (event->modifiers() & Qt::ShiftModifier))) {
    if (!m_view.IsNull()) {
      m_view->Pan(event->pos().x() - m_xPos, m_yPos - event->pos().y());
      m_xPos = event->pos().x();
      m_yPos = event->pos().y();
      return;
    }
  }

  // Original Panning Logic (Left Button without modifiers) - KEEP or REMOVE?
  // Let's keep it for now as "Pan" if no mode is active, but maybe restrict it?
  // User asked for "Increase rotation function". Standard CAD:
  // Middle=Pan, Right/Ctrl+Left=Rotate, Wheel=Zoom.
  // Old logic had Left=Pan if not drawing. Let's keep it but prioritize
  // Rotation.
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
      qreal pixelRatio = devicePixelRatio();
      m_view->Pan(static_cast<Standard_Integer>((event->pos().x() - m_xPos) *
                                                pixelRatio),
                  static_cast<Standard_Integer>((m_yPos - event->pos().y()) *
                                                pixelRatio));
      m_xPos = event->pos().x();
      m_yPos = event->pos().y();
      update();
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
        m_context->Remove(m_dynamicLine, false);
        m_dynamicLine.Nullify();
      }

      try {
        TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(m_firstPoint, currentPoint);
        m_dynamicLine = new AIS_Shape(edge);
        m_context->SetColor(m_dynamicLine, Quantity_Color(Quantity_NOC_YELLOW),
                            false);
        m_context->Display(m_dynamicLine, true);
      } catch (...) {
        // Ignore errors during dynamic drawing
      }
    }
  } else {
    // Standard highlighting
    qreal pixelRatio = devicePixelRatio();
    m_context->MoveTo(static_cast<int>(event->pos().x() * pixelRatio),
                      static_cast<int>(event->pos().y() * pixelRatio), m_view,
                      true);
  }
}

void OCCTWidget::addLine(const gp_Pnt &start, const gp_Pnt &end) {
  if (!m_context.IsNull()) {
    // Create edge from the two points
    TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(start, end);

    // Create AIS_Shape for visualization
    Handle(AIS_Shape) lineShape = new AIS_Shape(edge);

    // Set the color to green
    m_context->SetColor(lineShape, Quantity_Color(Quantity_NOC_GREEN), true);

    // Display the shape
    m_context->Display(lineShape, true);

    // Store the line for reference
    m_lines.push_back(lineShape);

    updateView();
  }
}

void OCCTWidget::addShape(const TopoDS_Shape &shape,
                          const Quantity_Color &color,
                          Graphic3d_NameOfMaterial material) {
  if (!m_context.IsNull()) {
    Handle(AIS_Shape) aisShape = new AIS_Shape(shape);

    // 1. 设置显示模式为 Shaded (1)
    m_context->SetDisplayMode(aisShape, 1, false);

    // 2. 先设置材质
    m_context->SetMaterial(aisShape, material, false);

    // 3. 后设置颜色 (确保覆盖材质自带颜色)
    m_context->SetColor(aisShape, color, false);

    // 4. 显示
    m_context->Display(aisShape, false);

    m_lines.push_back(aisShape);
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

    m_context->SetColor(lineShape, col, false);

    // Display without update
    m_context->Display(lineShape, false);

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
TopoDS_Shape OCCTWidget::makeTextShape(const QString &text, double height,
                                       const gp_Pnt &position, double angle,
                                       const QString &fontName) {
  // Use SimFang as default for mixed Chinese/English if fontName is empty
  QString actualFont = fontName.isEmpty() ? "SimFang" : fontName;

  StdPrs_BRepFont font;
  if (!font.Init(actualFont.toUtf8().constData(), Font_FA_Regular, height)) {
    // Fallback if SimFang not found
    if (!font.Init("FangSong", Font_FA_Regular, height)) {
      if (!font.Init(Font_NOF_SANS_SERIF, Font_FA_Regular, height)) {
        return TopoDS_Shape();
      }
    }
  }

  TopoDS_Shape textShape;
  StdPrs_BRepTextBuilder builder;
  NCollection_String occtStr(text.toUtf8().constData());

  try {
    textShape = builder.Perform(font, occtStr);
  } catch (...) {
    return TopoDS_Shape();
  }

  if (textShape.IsNull())
    return TopoDS_Shape();

  // Apply transformations
  gp_Trsf aFinalTrsf;

  // 1. Rotation
  gp_Trsf aRot;
  aRot.SetRotation(gp::OZ(), angle * M_PI / 180.0);

  // 2. Translation
  gp_Trsf aTrans;
  aTrans.SetTranslation(gp_Vec(gp::Origin(), position));

  aFinalTrsf = aTrans * aRot;

  BRepBuilderAPI_Transform aFinalTransform(textShape, aFinalTrsf);
  return aFinalTransform.Shape();
}
double GetCharAdvance(const TopoDS_Shape &charShape, double height) {
  if (charShape.IsNull())
    return height * 0.5; // Default spacing for spaces
  Bnd_Box bbox;
  BRepBndLib::Add(charShape, bbox);
  double xMin, yMin, zMin, xMax, yMax, zMax;
  bbox.Get(xMin, yMin, zMin, xMax, yMax, zMax);
  return (xMax - xMin) + height * 0.1; // Width + 10% spacing
}

TopoDS_Shape OCCTWidget::makeTextShape2(const QString &text, double height,
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
    m_context->SetColor(aisShape, Quantity_Color(Quantity_NOC_YELLOW), false);

    // Set display mode: 0 = Wireframe (Hollow), 1 = Shaded (Solid)
    m_context->SetDisplayMode(aisShape, isSolid ? 1 : 0, false);

    // Display
    m_context->Display(aisShape, true);
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
      m_context->SetDisplayMode(aisShape, mode, false);
    }
  }

  m_context->UpdateCurrentViewer();
}

void OCCTWidget::fitAll() {
  if (!m_view.IsNull()) {
    m_view->FitAll();
    m_view->ZFitAll();
    if (!m_context.IsNull()) {
      m_context->UpdateCurrentViewer();
    }
    m_view->Redraw();
  }
}

void OCCTWidget::loadBrepFile(const QString &filename,
                              Graphic3d_NameOfMaterial material) {
  TopoDS_Shape shape;
  BRep_Builder builder;

  // Convert QString to const char*
  QByteArray ba = filename.toLocal8Bit();
  const char *c_str = ba.data();

  // Read the file
  if (!BRepTools::Read(shape, c_str, builder)) {
    // Failed to read, handle error (maybe just log or ignore for now)
    return;
  }

  // Important: we need to handle potential failure.
  // And actually, BRepTools::Read returns Standard_Boolean (true on success).
  // Let's verify documentation if possible, but usually yes.

  // 智能色彩映射：让材质看起来更真实
  Quantity_Color finalColor;
  switch (material) {
  case Graphic3d_NOM_GOLD:
    finalColor = Quantity_NOC_GOLD1;
    break;
  case Graphic3d_NOM_BRASS:
    finalColor = Quantity_NOC_DARKKHAKI;
    break;
  case Graphic3d_NOM_BRONZE:
    finalColor = Quantity_NOC_CHOCOLATE1;
    break;
  case Graphic3d_NOM_CHROME:
  case Graphic3d_NOM_STEEL:
  case Graphic3d_NOM_ALUMINIUM:
    finalColor = Quantity_NOC_GRAY30; // 重工业深灰色
    break;
  case Graphic3d_NOM_PLASTIC:
    finalColor = Quantity_NOC_YELLOW; // 塑料默认明黄
    break;
  case Graphic3d_NOM_GLASS:
    finalColor = Quantity_NOC_LIGHTBLUE;
    break;
  default:
    finalColor = Quantity_NOC_GRAY75;
    break;
  }

  addShape(shape, finalColor, material);
  fitAll();
}

void OCCTWidget::loadBrepFileDeferred(const QString &filename,
                                      Graphic3d_NameOfMaterial material,
                                      double yOffset) {
  TopoDS_Shape shape;
  BRep_Builder builder;

  QByteArray ba = filename.toLocal8Bit();
  if (!BRepTools::Read(shape, ba.data(), builder))
    return;

  // 如果有 Y 轴偏移，对形状做平移变换
  if (std::abs(yOffset) > 1e-6) {
    gp_Trsf trsf;
    trsf.SetTranslation(gp_Vec(0, yOffset, 0));
    shape = shape.Moved(TopLoc_Location(trsf));
  }

  Quantity_Color finalColor;
  switch (material) {
  case Graphic3d_NOM_GOLD:
    finalColor = Quantity_NOC_GOLD1;
    break;
  case Graphic3d_NOM_BRASS:
    finalColor = Quantity_NOC_DARKKHAKI;
    break;
  case Graphic3d_NOM_BRONZE:
    finalColor = Quantity_NOC_CHOCOLATE1;
    break;
  case Graphic3d_NOM_CHROME:
  case Graphic3d_NOM_STEEL:
  case Graphic3d_NOM_ALUMINIUM:
    finalColor = Quantity_NOC_GRAY30;
    break;
  case Graphic3d_NOM_PLASTIC:
    finalColor = Quantity_NOC_YELLOW;
    break;
  case Graphic3d_NOM_GLASS:
    finalColor = Quantity_NOC_LIGHTBLUE;
    break;
  default:
    finalColor = Quantity_NOC_GRAY75;
    break;
  }

  if (!m_context.IsNull()) {
    Handle(AIS_Shape) aisShape = new AIS_Shape(shape);
    m_context->SetDisplayMode(aisShape, 1, false);
    m_context->SetMaterial(aisShape, material, false);
    m_context->SetColor(aisShape, finalColor, false);
    m_context->Display(aisShape, false);
    m_lines.push_back(aisShape);
    // 不调用 updateView() 和 fitAll()，由调用方最终统一刷新
  }
}

void OCCTWidget::loadBrepAsFullBridge(const QString &filename, int count,
                                      double spacing,
                                      Graphic3d_NameOfMaterial material) {
  if (m_context.IsNull())
    return;

  // 读取原始 BREP
  TopoDS_Shape baseShape;
  BRep_Builder builder;
  QByteArray ba = filename.toLocal8Bit();
  if (!BRepTools::Read(baseShape, ba.data(), builder))
    return;

  // 颜色映射 (与 loadBrepFile 保持一致)
  Quantity_Color color;
  switch (material) {
  case Graphic3d_NOM_GOLD:
    color = Quantity_NOC_GOLD1;
    break;
  case Graphic3d_NOM_BRASS:
    color = Quantity_NOC_DARKKHAKI;
    break;
  case Graphic3d_NOM_BRONZE:
    color = Quantity_NOC_CHOCOLATE1;
    break;
  case Graphic3d_NOM_CHROME:
  case Graphic3d_NOM_STEEL:
  case Graphic3d_NOM_ALUMINIUM:
    color = Quantity_NOC_GRAY30;
    break;
  case Graphic3d_NOM_PLASTIC:
    color = Quantity_NOC_YELLOW;
    break;
  case Graphic3d_NOM_GLASS:
    color = Quantity_NOC_LIGHTBLUE;
    break;
  default:
    color = Quantity_NOC_GRAY75;
    break;
  }

  // 创建 count 个独立 AIS_Shape，沿 Y 方向间距 spacing
  for (int i = 0; i < count; ++i) {
    gp_Trsf trsf;
    trsf.SetTranslation(gp_Vec(0, i * spacing, 0));
    TopoDS_Shape moved = baseShape.Moved(TopLoc_Location(trsf));

    Handle(AIS_Shape) ais = new AIS_Shape(moved);
    m_context->SetDisplayMode(ais, 1, false);
    m_context->SetColor(ais, color, false);
    m_context->SetMaterial(ais, material, false);
    m_context->Display(ais, false);
    m_lines.push_back(ais);
  }

  m_context->UpdateCurrentViewer();
  fitAll();
}

void OCCTWidget::clearAll() {
  if (m_context.IsNull())
    return;

  // Remove all shapes stored in m_lines
  for (const auto &aisShape : m_lines) {
    if (!aisShape.IsNull()) {
      m_context->Remove(aisShape, false); // false to defer update
    }
  }
  m_lines.clear();

  for (const auto &dim : m_dimensions) {
    if (!dim.IsNull()) {
      m_context->Remove(dim, false);
    }
  }
  m_dimensions.clear();

  // Optional: remove m_dynamicLine if needed
  if (!m_dynamicLine.IsNull()) {
    m_context->Remove(m_dynamicLine, false);
    m_dynamicLine.Nullify();
  }

  m_context->UpdateCurrentViewer();
}

void OCCTWidget::annotateBridgePierFooting() {
  if (m_context.IsNull())
    return;

  // 根据脚本设置: 最下层承台位于 Z=-140 到
  // -130，长(X)=89.59，宽(Y)=59.05，高(Z)=10
  double length = 89.59;
  double width = 59.05;
  double height = 10.0;

  double minX = -length / 2.0;
  double maxX = length / 2.0;
  double minY = -width / 2.0;
  double maxY = width / 2.0;
  double minZ = -140.0;
  double maxZ = -130.0;

  // X方向标注 (长) - 放置在底部前边缘 (-Y 方向偏移)
  gp_Pnt pL1(minX, minY, minZ);
  gp_Pnt pL2(maxX, minY, minZ);
  gp_Pln plnL(gp_Pnt(0, minY, minZ), gp_Dir(0, 0, 1));
  Handle(PrsDim_LengthDimension) dimL =
      new PrsDim_LengthDimension(pL1, pL2, plnL);
  dimL->SetFlyout(-35.0);

  // Y方向标注 (宽) - 放置在底部右边缘 (+X 方向偏移)
  gp_Pnt pW1(maxX, minY, minZ);
  gp_Pnt pW2(maxX, maxY, minZ);
  gp_Pln plnW(gp_Pnt(maxX, 0, minZ), gp_Dir(0, 0, 1));
  Handle(PrsDim_LengthDimension) dimW =
      new PrsDim_LengthDimension(pW1, pW2, plnW);
  dimW->SetFlyout(35.0);

  // Z方向标注 (高) - 放置在前右边缘 (+X 方向偏移)
  gp_Pnt pH1(maxX, minY, minZ);
  gp_Pnt pH2(maxX, minY, maxZ);
  gp_Pln plnH(gp_Pnt(maxX, minY, 0), gp_Dir(0, -1, 0));
  Handle(PrsDim_LengthDimension) dimH =
      new PrsDim_LengthDimension(pH1, pH2, plnH);
  dimH->SetFlyout(35.0);

  // 统一设置样式并显示
  Handle(PrsDim_LengthDimension) dims[] = {dimL, dimW, dimH};
  for (int i = 0; i < 3; ++i) {
    Handle(Prs3d_DimensionAspect) aspect = dims[i]->DimensionAspect();
    if (!aspect.IsNull()) {
      // 文字大小
      aspect->TextAspect()->SetHeight(12.0);
      aspect->TextAspect()->SetColor(Quantity_NOC_BLACK);
      // 标注线颜色
      aspect->LineAspect()->SetColor(Quantity_NOC_BLACK);
      // 箭头和文字放在外侧
      aspect->SetArrowOrientation(Prs3d_DAO_External);
      aspect->SetTextHorizontalPosition(Prs3d_DTHP_Left);
      aspect->SetTextVerticalPosition(Prs3d_DTVP_Above);
    }
    m_context->Display(dims[i], false);
    m_dimensions.push_back(dims[i]);
  }

  m_context->UpdateCurrentViewer();
}

// ========== 桥墩绘制 ==========
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepFeat_MakePrism.hxx>
#include <BRepLib.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GC_MakeSegment.hxx>
#include <Geom_TrimmedCurve.hxx>

void OCCTWidget::drawBridgePier() {
  if (m_context.IsNull())
    return;

  // 清空已有模型
  clearAll();

  //******流线型托盘******
  gp_Pnt P1(16, -14, 0);
  gp_Pnt P2(30, 0, 0);
  gp_Pnt P3(16, 14, 0);
  gp_Pnt P1_1(-16, -14, 0);
  gp_Pnt P2_1(-30, 0, 0);
  gp_Pnt P3_1(-16, 14, 0);
  gp_Pnt P1_2(2, -14, 0);
  gp_Pnt P2_2(1.29, -13.74, 0);
  gp_Pnt P3_2(1, -13, 0);
  gp_Pnt P1_3(-2, -14, 0);
  gp_Pnt P2_3(-1.29, -13.74, 0);
  gp_Pnt P3_3(-1, -13, 0);
  gp_Pnt P3_4(0, -12, 0);
  gp_Pnt P1_5(2, 14, 0);
  gp_Pnt P2_5(1.29, 13.74, 0);
  gp_Pnt P3_5(1, 13, 0);
  gp_Pnt P1_6(-2, 14, 0);
  gp_Pnt P2_6(-1.29, 13.74, 0);
  gp_Pnt P3_6(-1, 13, 0);
  gp_Pnt P3_7(0, 12, 0);

  // --- 截面1 (底部 Z=0) ---
  Handle(Geom_TrimmedCurve) anArcOfCircle1 =
      GC_MakeArcOfCircle(P1_1, P2_1, P3_1);
  Handle(Geom_TrimmedCurve) aSegment1 = GC_MakeSegment(P3_1, P1_6);
  Handle(Geom_TrimmedCurve) anArcOfCircle2 =
      GC_MakeArcOfCircle(P1_6, P2_6, P3_6);
  Handle(Geom_TrimmedCurve) anArcOfCircle3 =
      GC_MakeArcOfCircle(P3_6, P3_7, P3_5);
  Handle(Geom_TrimmedCurve) anArcOfCircle4 =
      GC_MakeArcOfCircle(P3_5, P2_5, P1_5);
  Handle(Geom_TrimmedCurve) aSegment2 = GC_MakeSegment(P1_5, P3);
  Handle(Geom_TrimmedCurve) anArcOfCircle5 = GC_MakeArcOfCircle(P3, P2, P1);
  Handle(Geom_TrimmedCurve) aSegment3 = GC_MakeSegment(P1, P1_2);
  Handle(Geom_TrimmedCurve) anArcOfCircle6 =
      GC_MakeArcOfCircle(P1_2, P2_2, P3_2);
  Handle(Geom_TrimmedCurve) anArcOfCircle7 =
      GC_MakeArcOfCircle(P3_2, P3_4, P3_3);
  Handle(Geom_TrimmedCurve) anArcOfCircle8 =
      GC_MakeArcOfCircle(P3_3, P2_3, P1_3);
  Handle(Geom_TrimmedCurve) aSegment4 = GC_MakeSegment(P1_3, P1_1);

  TopoDS_Edge anEdge1 = BRepBuilderAPI_MakeEdge(anArcOfCircle1);
  TopoDS_Edge anEdge2 = BRepBuilderAPI_MakeEdge(aSegment1);
  TopoDS_Edge anEdge3 = BRepBuilderAPI_MakeEdge(anArcOfCircle2);
  TopoDS_Edge anEdge4 = BRepBuilderAPI_MakeEdge(anArcOfCircle3);
  TopoDS_Edge anEdge5 = BRepBuilderAPI_MakeEdge(anArcOfCircle4);
  TopoDS_Edge anEdge6 = BRepBuilderAPI_MakeEdge(aSegment2);
  TopoDS_Edge anEdge7 = BRepBuilderAPI_MakeEdge(anArcOfCircle5);
  TopoDS_Edge anEdge8 = BRepBuilderAPI_MakeEdge(aSegment3);
  TopoDS_Edge anEdge9 = BRepBuilderAPI_MakeEdge(anArcOfCircle6);
  TopoDS_Edge anEdge10 = BRepBuilderAPI_MakeEdge(anArcOfCircle7);
  TopoDS_Edge anEdge11 = BRepBuilderAPI_MakeEdge(anArcOfCircle8);
  TopoDS_Edge anEdge12 = BRepBuilderAPI_MakeEdge(aSegment4);

  TopoDS_Wire aWire =
      BRepBuilderAPI_MakeWire(anEdge1, anEdge2, anEdge3, anEdge4);
  aWire = BRepBuilderAPI_MakeWire(aWire, anEdge5);
  aWire = BRepBuilderAPI_MakeWire(aWire, anEdge6);
  aWire = BRepBuilderAPI_MakeWire(aWire, anEdge7);
  aWire = BRepBuilderAPI_MakeWire(aWire, anEdge8);
  aWire = BRepBuilderAPI_MakeWire(aWire, anEdge9);
  aWire = BRepBuilderAPI_MakeWire(aWire, anEdge10);
  aWire = BRepBuilderAPI_MakeWire(aWire, anEdge11);
  aWire = BRepBuilderAPI_MakeWire(aWire, anEdge12);

  // --- 截面2 (顶部 Z=27.5) ---
  P1.SetCoord(24, -15, 27.5);
  P2.SetCoord(39, 0, 27.5);
  P3.SetCoord(24, 15, 27.5);
  P1_1.SetCoord(-24, -15, 27.5);
  P2_1.SetCoord(-39, 0, 27.5);
  P3_1.SetCoord(-24, 15, 27.5);
  P1_2.SetCoord(2, -15, 27.5);
  P2_2.SetCoord(1.29, -14.71, 27.5);
  P3_2.SetCoord(1, -14, 27.5);
  P1_3.SetCoord(-2, -15, 27.5);
  P2_3.SetCoord(-1.29, -14.71, 27.5);
  P3_3.SetCoord(-1, -14, 27.5);
  P3_4.SetCoord(0, -13, 27.5);
  P1_5.SetCoord(2, 15, 27.5);
  P2_5.SetCoord(1.29, 14.71, 27.5);
  P3_5.SetCoord(1, 14, 27.5);
  P1_6.SetCoord(-2, 15, 27.5);
  P2_6.SetCoord(-1.29, 14.71, 27.5);
  P3_6.SetCoord(-1, 14, 27.5);
  P3_7.SetCoord(0, 13, 27.5);

  anArcOfCircle1 = GC_MakeArcOfCircle(P1_1, P2_1, P3_1);
  aSegment1 = GC_MakeSegment(P3_1, P1_6);
  anArcOfCircle2 = GC_MakeArcOfCircle(P1_6, P2_6, P3_6);
  anArcOfCircle3 = GC_MakeArcOfCircle(P3_6, P3_7, P3_5);
  anArcOfCircle4 = GC_MakeArcOfCircle(P3_5, P2_5, P1_5);
  aSegment2 = GC_MakeSegment(P1_5, P3);
  anArcOfCircle5 = GC_MakeArcOfCircle(P3, P2, P1);
  aSegment3 = GC_MakeSegment(P1, P1_2);
  anArcOfCircle6 = GC_MakeArcOfCircle(P1_2, P2_2, P3_2);
  anArcOfCircle7 = GC_MakeArcOfCircle(P3_2, P3_4, P3_3);
  anArcOfCircle8 = GC_MakeArcOfCircle(P3_3, P2_3, P1_3);
  aSegment4 = GC_MakeSegment(P1_3, P1_1);

  anEdge1 = BRepBuilderAPI_MakeEdge(anArcOfCircle1);
  anEdge2 = BRepBuilderAPI_MakeEdge(aSegment1);
  anEdge3 = BRepBuilderAPI_MakeEdge(anArcOfCircle2);
  anEdge4 = BRepBuilderAPI_MakeEdge(anArcOfCircle3);
  anEdge5 = BRepBuilderAPI_MakeEdge(anArcOfCircle4);
  anEdge6 = BRepBuilderAPI_MakeEdge(aSegment2);
  anEdge7 = BRepBuilderAPI_MakeEdge(anArcOfCircle5);
  anEdge8 = BRepBuilderAPI_MakeEdge(aSegment3);
  anEdge9 = BRepBuilderAPI_MakeEdge(anArcOfCircle6);
  anEdge10 = BRepBuilderAPI_MakeEdge(anArcOfCircle7);
  anEdge11 = BRepBuilderAPI_MakeEdge(anArcOfCircle8);
  anEdge12 = BRepBuilderAPI_MakeEdge(aSegment4);

  TopoDS_Wire bWire =
      BRepBuilderAPI_MakeWire(anEdge1, anEdge2, anEdge3, anEdge4);
  bWire = BRepBuilderAPI_MakeWire(bWire, anEdge5);
  bWire = BRepBuilderAPI_MakeWire(bWire, anEdge6);
  bWire = BRepBuilderAPI_MakeWire(bWire, anEdge7);
  bWire = BRepBuilderAPI_MakeWire(bWire, anEdge8);
  bWire = BRepBuilderAPI_MakeWire(bWire, anEdge9);
  bWire = BRepBuilderAPI_MakeWire(bWire, anEdge10);
  bWire = BRepBuilderAPI_MakeWire(bWire, anEdge11);
  bWire = BRepBuilderAPI_MakeWire(bWire, anEdge12);

  // --- 截面3 (中间 Z=13.75) ---
  P1.SetCoord(17.88, -14.095, 13.75);
  P2.SetCoord(31.905, 0, 13.75);
  P3.SetCoord(17.88, 14.095, 13.75);
  P1_1.SetCoord(-17.88, -14.095, 13.75);
  P2_1.SetCoord(-31.905, 0, 13.75);
  P3_1.SetCoord(-17.88, 14.095, 13.75);
  P1_2.SetCoord(2, -14.095, 13.75);
  P2_2.SetCoord(1.29, -13.8, 13.75);
  P3_2.SetCoord(1, -13.1, 13.75);
  P1_3.SetCoord(-2, -14.095, 13.75);
  P2_3.SetCoord(-1.29, -13.8, 13.75);
  P3_3.SetCoord(-1, -13.1, 13.75);
  P3_4.SetCoord(0, -12.1, 13.75);
  P1_5.SetCoord(2, 14.095, 13.75);
  P2_5.SetCoord(1.29, 13.8, 13.75);
  P3_5.SetCoord(1, 13.1, 13.75);
  P1_6.SetCoord(-2, 14.095, 13.75);
  P2_6.SetCoord(-1.29, 13.8, 13.75);
  P3_6.SetCoord(-1, 13.1, 13.75);
  P3_7.SetCoord(0, 12.1, 13.75);

  anArcOfCircle1 = GC_MakeArcOfCircle(P1_1, P2_1, P3_1);
  aSegment1 = GC_MakeSegment(P3_1, P1_6);
  anArcOfCircle2 = GC_MakeArcOfCircle(P1_6, P2_6, P3_6);
  anArcOfCircle3 = GC_MakeArcOfCircle(P3_6, P3_7, P3_5);
  anArcOfCircle4 = GC_MakeArcOfCircle(P3_5, P2_5, P1_5);
  aSegment2 = GC_MakeSegment(P1_5, P3);
  anArcOfCircle5 = GC_MakeArcOfCircle(P3, P2, P1);
  aSegment3 = GC_MakeSegment(P1, P1_2);
  anArcOfCircle6 = GC_MakeArcOfCircle(P1_2, P2_2, P3_2);
  anArcOfCircle7 = GC_MakeArcOfCircle(P3_2, P3_4, P3_3);
  anArcOfCircle8 = GC_MakeArcOfCircle(P3_3, P2_3, P1_3);
  aSegment4 = GC_MakeSegment(P1_3, P1_1);

  anEdge1 = BRepBuilderAPI_MakeEdge(anArcOfCircle1);
  anEdge2 = BRepBuilderAPI_MakeEdge(aSegment1);
  anEdge3 = BRepBuilderAPI_MakeEdge(anArcOfCircle2);
  anEdge4 = BRepBuilderAPI_MakeEdge(anArcOfCircle3);
  anEdge5 = BRepBuilderAPI_MakeEdge(anArcOfCircle4);
  anEdge6 = BRepBuilderAPI_MakeEdge(aSegment2);
  anEdge7 = BRepBuilderAPI_MakeEdge(anArcOfCircle5);
  anEdge8 = BRepBuilderAPI_MakeEdge(aSegment3);
  anEdge9 = BRepBuilderAPI_MakeEdge(anArcOfCircle6);
  anEdge10 = BRepBuilderAPI_MakeEdge(anArcOfCircle7);
  anEdge11 = BRepBuilderAPI_MakeEdge(anArcOfCircle8);
  anEdge12 = BRepBuilderAPI_MakeEdge(aSegment4);

  TopoDS_Wire mWire =
      BRepBuilderAPI_MakeWire(anEdge1, anEdge2, anEdge3, anEdge4);
  mWire = BRepBuilderAPI_MakeWire(mWire, anEdge5);
  mWire = BRepBuilderAPI_MakeWire(mWire, anEdge6);
  mWire = BRepBuilderAPI_MakeWire(mWire, anEdge7);
  mWire = BRepBuilderAPI_MakeWire(mWire, anEdge8);
  mWire = BRepBuilderAPI_MakeWire(mWire, anEdge9);
  mWire = BRepBuilderAPI_MakeWire(mWire, anEdge10);
  mWire = BRepBuilderAPI_MakeWire(mWire, anEdge11);
  mWire = BRepBuilderAPI_MakeWire(mWire, anEdge12);

  // 流线型托盘 - ThruSections 放样
  BRepOffsetAPI_ThruSections tuopan(true, false);
  tuopan.AddWire(aWire);
  tuopan.AddWire(mWire);
  tuopan.AddWire(bWire);
  tuopan.Build();
  TopoDS_Shape S = tuopan.Shape();

  Handle(AIS_Shape) ais = new AIS_Shape(S);
  m_context->SetDisplayMode(ais, 1, false);
  m_context->SetColor(ais, Quantity_NOC_MATRABLUE, false);
  m_context->SetMaterial(ais, Graphic3d_NOM_PLASTIC, false);
  m_context->Display(ais, false);
  m_lines.push_back(ais);

  // --- 顶帽 (Z=30) ---
  P1.SetCoord(24, -15, 30);
  P2.SetCoord(39, 0, 30);
  P3.SetCoord(24, 15, 30);
  P1_1.SetCoord(-24, -15, 30);
  P2_1.SetCoord(-39, 0, 30);
  P3_1.SetCoord(-24, 15, 30);
  P1_2.SetCoord(2, -15, 30);
  P2_2.SetCoord(1.29, -14.71, 30);
  P3_2.SetCoord(1, -14, 30);
  P1_3.SetCoord(-2, -15, 30);
  P2_3.SetCoord(-1.29, -14.71, 30);
  P3_3.SetCoord(-1, -14, 30);
  P3_4.SetCoord(0, -13, 30);
  P1_5.SetCoord(2, 15, 30);
  P2_5.SetCoord(1.29, 14.71, 30);
  P3_5.SetCoord(1, 14, 30);
  P1_6.SetCoord(-2, 15, 30);
  P2_6.SetCoord(-1.29, 14.71, 30);
  P3_6.SetCoord(-1, 14, 30);
  P3_7.SetCoord(0, 13, 30);

  anArcOfCircle1 = GC_MakeArcOfCircle(P1_1, P2_1, P3_1);
  aSegment1 = GC_MakeSegment(P3_1, P1_6);
  anArcOfCircle2 = GC_MakeArcOfCircle(P1_6, P2_6, P3_6);
  anArcOfCircle3 = GC_MakeArcOfCircle(P3_6, P3_7, P3_5);
  anArcOfCircle4 = GC_MakeArcOfCircle(P3_5, P2_5, P1_5);
  aSegment2 = GC_MakeSegment(P1_5, P3);
  anArcOfCircle5 = GC_MakeArcOfCircle(P3, P2, P1);
  aSegment3 = GC_MakeSegment(P1, P1_2);
  anArcOfCircle6 = GC_MakeArcOfCircle(P1_2, P2_2, P3_2);
  anArcOfCircle7 = GC_MakeArcOfCircle(P3_2, P3_4, P3_3);
  anArcOfCircle8 = GC_MakeArcOfCircle(P3_3, P2_3, P1_3);
  aSegment4 = GC_MakeSegment(P1_3, P1_1);

  anEdge1 = BRepBuilderAPI_MakeEdge(anArcOfCircle1);
  anEdge2 = BRepBuilderAPI_MakeEdge(aSegment1);
  anEdge3 = BRepBuilderAPI_MakeEdge(anArcOfCircle2);
  anEdge4 = BRepBuilderAPI_MakeEdge(anArcOfCircle3);
  anEdge5 = BRepBuilderAPI_MakeEdge(anArcOfCircle4);
  anEdge6 = BRepBuilderAPI_MakeEdge(aSegment2);
  anEdge7 = BRepBuilderAPI_MakeEdge(anArcOfCircle5);
  anEdge8 = BRepBuilderAPI_MakeEdge(aSegment3);
  anEdge9 = BRepBuilderAPI_MakeEdge(anArcOfCircle6);
  anEdge10 = BRepBuilderAPI_MakeEdge(anArcOfCircle7);
  anEdge11 = BRepBuilderAPI_MakeEdge(anArcOfCircle8);
  anEdge12 = BRepBuilderAPI_MakeEdge(aSegment4);

  TopoDS_Wire dWire =
      BRepBuilderAPI_MakeWire(anEdge1, anEdge2, anEdge3, anEdge4);
  dWire = BRepBuilderAPI_MakeWire(dWire, anEdge5);
  dWire = BRepBuilderAPI_MakeWire(dWire, anEdge6);
  dWire = BRepBuilderAPI_MakeWire(dWire, anEdge7);
  dWire = BRepBuilderAPI_MakeWire(dWire, anEdge8);
  dWire = BRepBuilderAPI_MakeWire(dWire, anEdge9);
  dWire = BRepBuilderAPI_MakeWire(dWire, anEdge10);
  dWire = BRepBuilderAPI_MakeWire(dWire, anEdge11);
  dWire = BRepBuilderAPI_MakeWire(dWire, anEdge12);

  // 顶帽 - ThruSections 放样
  BRepOffsetAPI_ThruSections dingmao(true, false);
  dingmao.AddWire(bWire);
  dingmao.AddWire(dWire);
  dingmao.Build();
  TopoDS_Shape S1 = dingmao.Shape();

  Handle(AIS_Shape) ais1 = new AIS_Shape(S1);
  m_context->SetDisplayMode(ais1, 1, false);
  m_context->SetColor(ais1, Quantity_NOC_MATRABLUE, false);
  m_context->SetMaterial(ais1, Graphic3d_NOM_PLASTIC, false);
  m_context->Display(ais1, false);
  m_lines.push_back(ais1);

  // --- 裁剪 (Prism切割) ---
  gp_Dir D(0, 1, 0);
  gp_Pnt p1, p2;
  BRepBuilderAPI_MakeWire MW;
  p1 = gp_Pnt(-7.5, -100, 30);
  p2 = gp_Pnt(-7.5, -100, 27);
  MW.Add(BRepBuilderAPI_MakeEdge(p1, p2));
  p1 = p2;
  p2 = gp_Pnt(-5.5, -100, 25);
  MW.Add(BRepBuilderAPI_MakeEdge(p1, p2));
  p1 = p2;
  p2 = gp_Pnt(5.5, -100, 25);
  MW.Add(BRepBuilderAPI_MakeEdge(p1, p2));
  p1 = p2;
  p2 = gp_Pnt(7.5, -100, 27);
  MW.Add(BRepBuilderAPI_MakeEdge(p1, p2));
  p1 = p2;
  p2 = gp_Pnt(7.5, -100, 30);
  MW.Add(BRepBuilderAPI_MakeEdge(p1, p2));
  p1 = p2;
  p2 = gp_Pnt(-7.5, -100, 30);
  MW.Add(BRepBuilderAPI_MakeEdge(p1, p2));

  TopoDS_Shape FP = BRepBuilderAPI_MakeFace(gp_Pln(gp::ZOX()), MW.Wire());
  BRepLib::BuildCurves3d(FP);
  TopoDS_Face F = BRepBuilderAPI_MakeFace(gp_Pln(gp::ZOX()));

  BRepFeat_MakePrism MKP(S, FP, F, D, 0, true);
  MKP.Perform(1000.);
  TopoDS_Shape res = MKP.Shape();

  BRepFeat_MakePrism MKP1(S1, FP, F, D, 0, true);
  MKP1.Perform(1000.);
  TopoDS_Shape res1 = MKP1.Shape();

  ais->Set(res);
  ais1->Set(res1);
  m_context->Redisplay(ais, false);
  m_context->Redisplay(ais1, false);

  // --- 墩身 (Z=-120) ---
  P1.SetCoord(16, -16.67, -120);
  P2.SetCoord(32.67, 0, -120);
  P3.SetCoord(16, 16.67, -120);
  P1_1.SetCoord(-16, -16.67, -120);
  P2_1.SetCoord(-32.67, 0, -120);
  P3_1.SetCoord(-16, 16.67, -120);
  P1_2.SetCoord(2, -16.67, -120);
  P2_2.SetCoord(1.29, -16.37, -120);
  P3_2.SetCoord(1, -15.67, -120);
  P1_3.SetCoord(-2, -16.67, -120);
  P2_3.SetCoord(-1.29, -16.37, -120);
  P3_3.SetCoord(-1, -15.67, -120);
  P3_4.SetCoord(0, -14.67, -120);
  P1_5.SetCoord(2, 16.67, -120);
  P2_5.SetCoord(1.29, 16.37, -120);
  P3_5.SetCoord(1, 15.67, -120);
  P1_6.SetCoord(-2, 16.67, -120);
  P2_6.SetCoord(-1.29, 16.37, -120);
  P3_6.SetCoord(-1, 15.67, -120);
  P3_7.SetCoord(0, 14.67, -120);

  anArcOfCircle1 = GC_MakeArcOfCircle(P1_1, P2_1, P3_1);
  aSegment1 = GC_MakeSegment(P3_1, P1_6);
  anArcOfCircle2 = GC_MakeArcOfCircle(P1_6, P2_6, P3_6);
  anArcOfCircle3 = GC_MakeArcOfCircle(P3_6, P3_7, P3_5);
  anArcOfCircle4 = GC_MakeArcOfCircle(P3_5, P2_5, P1_5);
  aSegment2 = GC_MakeSegment(P1_5, P3);
  anArcOfCircle5 = GC_MakeArcOfCircle(P3, P2, P1);
  aSegment3 = GC_MakeSegment(P1, P1_2);
  anArcOfCircle6 = GC_MakeArcOfCircle(P1_2, P2_2, P3_2);
  anArcOfCircle7 = GC_MakeArcOfCircle(P3_2, P3_4, P3_3);
  anArcOfCircle8 = GC_MakeArcOfCircle(P3_3, P2_3, P1_3);
  aSegment4 = GC_MakeSegment(P1_3, P1_1);

  anEdge1 = BRepBuilderAPI_MakeEdge(anArcOfCircle1);
  anEdge2 = BRepBuilderAPI_MakeEdge(aSegment1);
  anEdge3 = BRepBuilderAPI_MakeEdge(anArcOfCircle2);
  anEdge4 = BRepBuilderAPI_MakeEdge(anArcOfCircle3);
  anEdge5 = BRepBuilderAPI_MakeEdge(anArcOfCircle4);
  anEdge6 = BRepBuilderAPI_MakeEdge(aSegment2);
  anEdge7 = BRepBuilderAPI_MakeEdge(anArcOfCircle5);
  anEdge8 = BRepBuilderAPI_MakeEdge(aSegment3);
  anEdge9 = BRepBuilderAPI_MakeEdge(anArcOfCircle6);
  anEdge10 = BRepBuilderAPI_MakeEdge(anArcOfCircle7);
  anEdge11 = BRepBuilderAPI_MakeEdge(anArcOfCircle8);
  anEdge12 = BRepBuilderAPI_MakeEdge(aSegment4);

  TopoDS_Wire sWire =
      BRepBuilderAPI_MakeWire(anEdge1, anEdge2, anEdge3, anEdge4);
  sWire = BRepBuilderAPI_MakeWire(sWire, anEdge5);
  sWire = BRepBuilderAPI_MakeWire(sWire, anEdge6);
  sWire = BRepBuilderAPI_MakeWire(sWire, anEdge7);
  sWire = BRepBuilderAPI_MakeWire(sWire, anEdge8);
  sWire = BRepBuilderAPI_MakeWire(sWire, anEdge9);
  sWire = BRepBuilderAPI_MakeWire(sWire, anEdge10);
  sWire = BRepBuilderAPI_MakeWire(sWire, anEdge11);
  sWire = BRepBuilderAPI_MakeWire(sWire, anEdge12);

  // 墩身 - ThruSections 放样
  BRepOffsetAPI_ThruSections dunshen(true, false);
  dunshen.AddWire(sWire);
  dunshen.AddWire(aWire);
  dunshen.Build();
  TopoDS_Shape S2 = dunshen.Shape();

  Handle(AIS_Shape) ais2 = new AIS_Shape(S2);
  m_context->SetDisplayMode(ais2, 1, false);
  m_context->SetColor(ais2, Quantity_NOC_MATRABLUE, false);
  m_context->SetMaterial(ais2, Graphic3d_NOM_PLASTIC, false);
  m_context->Display(ais2, false);
  m_lines.push_back(ais2);

  // --- 承台 (两层底座) ---
  TopoDS_Shape S3 =
      BRepPrimAPI_MakeBox(gp_Pnt(-38.41, -22.22, -130), 76.82, 44.44, 10)
          .Shape();
  Handle(AIS_Shape) ais3 = new AIS_Shape(S3);
  m_context->SetDisplayMode(ais3, 1, false);
  m_context->SetColor(ais3, Quantity_NOC_GREEN, false);
  m_context->SetMaterial(ais3, Graphic3d_NOM_PLASTIC, false);
  m_context->Display(ais3, false);
  m_lines.push_back(ais3);

  TopoDS_Shape S4 =
      BRepPrimAPI_MakeBox(gp_Pnt(-44.79, -29.53, -140), 89.59, 59.05, 10)
          .Shape();
  Handle(AIS_Shape) ais4 = new AIS_Shape(S4);
  m_context->SetDisplayMode(ais4, 1, false);
  m_context->SetColor(ais4, Quantity_NOC_GREEN, false);
  m_context->SetMaterial(ais4, Graphic3d_NOM_PLASTIC, false);
  m_context->Display(ais4, false);
  m_lines.push_back(ais4);

  // 刷新视图
  fitAll();
}

void OCCTWidget::drawFullBridgePier() {
  if (m_context.IsNull())
    return;

  clearAll();

  // ====== 核心参数 (单位: mm) ======
  double pierH = 12000.0;         // 墩身高度
  double scale = 100.0;           // 基础比例
  double capTopZ = 30.0 * scale;  // 托盘顶面高度 (3000mm)
  double capBottomZ = 0.0;        // 托盘底面高度 (0mm)
  double bodyBottomZ = -pierH;    // 墩身底面高度 (-12000mm)
  double footingH = 2500.0;       // 承台厚度
  double footingW = 10000.0;      // 承台宽度 (桥横向，沿X轴)
  double footingL = 6000.0;       // 承台长度 (桥纵向，沿Y轴)
  double bedStoneDim = 1200.0;    // 垫石边长
  double bedStoneH = 400.0;       // 垫石高度
  double lateralSpacing = 3300.0; // 垫石中心距 (沿X轴)
  double bearingH = 250.0;        // 支座高度

  // 1. 桩基础 (3x2 阵列)
  double pileR = 750.0;
  double pileL = 10000.0;
  for (double x : {-3500.0, 0.0, 3500.0}) {
    for (double y : {-2000.0, 2000.0}) {
      gp_Pnt pnt(x, y, bodyBottomZ - footingH - pileL);
      TopoDS_Shape pile =
          BRepPrimAPI_MakeCylinder(gp_Ax2(pnt, gp_Dir(0, 0, 1)), pileR, pileL)
              .Shape();
      addShape(pile, Quantity_NOC_GRAY50, Graphic3d_NOM_PLASTIC);
    }
  }

  // 2. 承台
  gp_Pnt footingPnt(-footingW / 2.0, -footingL / 2.0, bodyBottomZ - footingH);
  TopoDS_Shape footing =
      BRepPrimAPI_MakeBox(footingPnt, footingW, footingL, footingH).Shape();
  addShape(footing, Quantity_NOC_GRAY60, Graphic3d_NOM_PLASTIC);

  // 3. 通用截面生成函数
  auto make_pier_wire = [&](double z, double xr, double yr, double xr2) {
    gp_Pnt pt1(xr, -yr, z), pt2(xr2, 0, z), pt3(xr, yr, z);
    gp_Pnt pt1_1(-xr, -yr, z), pt2_1(-xr2, 0, z), pt3_1(-xr, yr, z);
    BRepBuilderAPI_MakeWire mw;
    mw.Add(BRepBuilderAPI_MakeEdge(
        GC_MakeArcOfCircle(pt1_1, pt2_1, pt3_1).Value()));
    mw.Add(BRepBuilderAPI_MakeEdge(pt3_1, pt3));
    mw.Add(BRepBuilderAPI_MakeEdge(GC_MakeArcOfCircle(pt3, pt2, pt1).Value()));
    mw.Add(BRepBuilderAPI_MakeEdge(pt1, pt1_1));
    return mw.Wire();
  };

  // 4. 墩身 (Loft Body)
  BRepOffsetAPI_ThruSections bodyLoft(true, false);
  // 底部截面 (16, 16.67, 32.67) - 原始尺寸
  bodyLoft.AddWire(
      make_pier_wire(bodyBottomZ, 16 * scale, 16.67 * scale, 32.67 * scale));
  // 顶部截面 (必须严格匹配托盘底部 16, 14, 30)
  bodyLoft.AddWire(
      make_pier_wire(capBottomZ, 16 * scale, 14.0 * scale, 30.0 * scale));
  bodyLoft.Build();
  addShape(bodyLoft.Shape(), Quantity_NOC_MATRABLUE, Graphic3d_NOM_PLASTIC);

  // 5. 托盘 (Loft Tray)
  BRepOffsetAPI_ThruSections capLoft(true, false);
  // 底部匹配墩身
  capLoft.AddWire(
      make_pier_wire(capBottomZ, 16 * scale, 14 * scale, 30 * scale));
  // 顶部外扩
  capLoft.AddWire(make_pier_wire(capTopZ, 24 * scale, 15 * scale, 39 * scale));
  capLoft.Build();
  addShape(capLoft.Shape(), Quantity_NOC_MATRABLUE, Graphic3d_NOM_PLASTIC);

  // 6. 垫石与支座 (沿 X 轴对称)
  for (double side : {-1.0, 1.0}) {
    double xPos = (lateralSpacing / 2.0) * side;

    // 垫石 (BedStone)
    gp_Pnt bsStart(xPos - bedStoneDim / 2.0, -bedStoneDim / 2.0, capTopZ);
    TopoDS_Shape bedStone =
        BRepPrimAPI_MakeBox(bsStart, bedStoneDim, bedStoneDim, bedStoneH)
            .Shape();
    addShape(bedStone, Quantity_NOC_WHITE, Graphic3d_NOM_PLASTIC);

    // 支座 (Bearing)
    double brDim = 650.0;
    gp_Pnt brStart(xPos - brDim / 2.0, -brDim / 2.0, capTopZ + bedStoneH);
    TopoDS_Shape bearing =
        BRepPrimAPI_MakeBox(brStart, brDim, brDim, bearingH).Shape();
    addShape(bearing, Quantity_NOC_GRAY20, Graphic3d_NOM_STEEL);
  }

  fitAll();
}

TopoDS_Shape OCCTWidget::readBrepFileToShape(const QString &filename) {
  TopoDS_Shape shape;
  BRep_Builder builder;
  if (!BRepTools::Read(shape, filename.toStdString().c_str(), builder)) {
    qWarning() << "Failed to load deferred BREP file:" << filename;
  }
  return shape;
}

TopoDS_Shape OCCTWidget::readBrepFromMemory(const QByteArray &data) {
  TopoDS_Shape shape;
  BRep_Builder builder;
  std::stringstream ss(std::string(data.constData(), data.length()));
  BRepTools::Read(shape, ss, builder);
  if (shape.IsNull()) {
    qWarning()
        << "BRepTools::Read failed to parse shape from memory! Data size: "
        << data.length();
  }
  return shape;
}

void OCCTWidget::displayShape(const TopoDS_Shape &shape,
                              Graphic3d_NameOfMaterial material) {
  if (shape.IsNull() || m_context.IsNull())
    return;

  Quantity_Color finalColor;
  switch (material) {
  case Graphic3d_NOM_GOLD:
    finalColor = Quantity_NOC_GOLD1;
    break;
  case Graphic3d_NOM_BRASS:
    finalColor = Quantity_NOC_DARKKHAKI;
    break;
  case Graphic3d_NOM_BRONZE:
    finalColor = Quantity_NOC_CHOCOLATE1;
    break;
  case Graphic3d_NOM_CHROME:
  case Graphic3d_NOM_STEEL:
  case Graphic3d_NOM_ALUMINIUM:
    finalColor = Quantity_NOC_GRAY30;
    break;
  case Graphic3d_NOM_PLASTIC:
    finalColor = Quantity_NOC_YELLOW;
    break;
  case Graphic3d_NOM_GLASS:
    finalColor = Quantity_NOC_LIGHTBLUE;
    break;
  default:
    finalColor = Quantity_NOC_GRAY75;
    break;
  }

  Handle(AIS_Shape) aisShape = new AIS_Shape(shape);
  m_context->SetDisplayMode(aisShape, 1, false);
  m_context->SetMaterial(aisShape, material, false);
  m_context->SetColor(aisShape, finalColor, false);
  m_context->Display(aisShape, false);
  m_lines.push_back(aisShape);
  fitAll();
}

void OCCTWidget::buildFullBridgeFromParts(
    const QList<QPair<TopoDS_Shape, Graphic3d_NameOfMaterial>> &parts,
    int count, double spacing) {

  if (parts.isEmpty())
    return;

  // 区分桥墩部分和箱梁部分 (假设最后一个是箱梁，名字是 girder，如果在 parts
  // 里则其索引通常是 4) 这里我们使用一个简单启发式：如果某个part的Bounding
  // Box很长，或者它的索引是最后一个（我们传了5个） 假设传入的 parts = [Tuopan,
  // Dunshen, Chengtai, Pile, Girder]
  bool hasGirder = (parts.size() >= 5);
  int pierPartCount = hasGirder ? parts.size() - 1 : parts.size();

  // 循环 count 次，分别计算 Y 轴 offset 进行桥墩移动
  for (int i = 0; i < count; ++i) {
    double yOff = i * spacing;
    gp_Trsf trsf;
    trsf.SetTranslation(gp_Vec(0, yOff, 0));

    // 为每个桥墩部件进行阵列
    for (int j = 0; j < pierPartCount; ++j) {
      if (parts[j].first.IsNull())
        continue;

      BRepBuilderAPI_Transform childXform(parts[j].first, trsf, true);
      TopoDS_Shape childShape = childXform.Shape();

      Handle(AIS_Shape) aisShape = new AIS_Shape(childShape);
      m_context->SetDisplayMode(aisShape, 1, false);
      m_context->SetMaterial(aisShape, parts[j].second, false);
      m_context->Display(aisShape, false);
      m_lines.push_back(aisShape);
    }

    // 如果有箱梁，且不是最后一跨（或要求最后一跨也有）
    // 通常全桥 100 墩，则有 99 跨梁
    if (hasGirder && i < count - 1) {
      // 梁已经在 YZ 面生成，沿着 X 轴挤出 (L=31500)。
      // 桥墩的布局是沿着 Y 轴分布的。
      // 所以我们需要把梁从 X 轴旋转到 Y 轴。
      // 绕 Z 轴旋转 90 度。
      gp_Trsf rotTrsf;
      rotTrsf.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), M_PI / 2.0);

      gp_Trsf transTrsf;
      // 需要将其抬高到桥墩托盘/顶帽的顶部高度 Z 约等于 3000 (30.0m)
      transTrsf.SetTranslation(gp_Vec(0, yOff, 30.0 * 100.0));

      gp_Trsf girderTrsf = transTrsf * rotTrsf;

      BRepBuilderAPI_Transform girderXform(parts.last().first, girderTrsf,
                                           true);
      TopoDS_Shape girderShape = girderXform.Shape();

      Handle(AIS_Shape) aisShape = new AIS_Shape(girderShape);
      m_context->SetDisplayMode(aisShape, 1, false);
      m_context->SetMaterial(aisShape, Graphic3d_NOM_STEEL,
                             false); // 梁用 Steel
      // 用混凝土颜色代替Steel
      m_context->SetColor(aisShape, Quantity_NOC_GRAY75, false);
      m_context->Display(aisShape, false);
      m_lines.push_back(aisShape);
    }
  }

  fitAll();
}

void OCCTWidget::buildFullBridgeFromShapes(const QList<TopoDS_Shape> &shapes,
                                           Graphic3d_NameOfMaterial material) {

  qDebug() << "buildFullBridgeFromShapes: Received " << shapes.size()
           << " shapes.";

  if (shapes.isEmpty() || m_context.IsNull()) {
    qWarning()
        << "buildFullBridgeFromShapes aborting: Shapes empty or context null.";
    return;
  }

  Quantity_Color finalColor;
  switch (material) {
  case Graphic3d_NOM_GOLD:
    finalColor = Quantity_NOC_GOLD1;
    break;
  case Graphic3d_NOM_BRASS:
    finalColor = Quantity_NOC_DARKKHAKI;
    break;
  case Graphic3d_NOM_BRONZE:
    finalColor = Quantity_NOC_CHOCOLATE1;
    break;
  case Graphic3d_NOM_CHROME:
  case Graphic3d_NOM_STEEL:
  case Graphic3d_NOM_ALUMINIUM:
    finalColor = Quantity_NOC_GRAY30;
    break;
  case Graphic3d_NOM_PLASTIC:
    finalColor = Quantity_NOC_YELLOW;
    break;
  case Graphic3d_NOM_GLASS:
    finalColor = Quantity_NOC_LIGHTBLUE;
    break;
  default:
    finalColor = Quantity_NOC_GRAY75;
    break;
  }

  qDebug() << "buildFullBridgeFromShapes: Processing individual shape AIS "
              "assignment. Count: "
           << shapes.size();

  int processed = 0;
  for (const TopoDS_Shape &shape : shapes) {
    if (!shape.IsNull()) {
      // 先生成网格再显示能够显著提升渲染稳定性和速度
      Handle(AIS_Shape) aisShape = new AIS_Shape(shape);
      m_context->SetDisplayMode(aisShape, 1, false);
      m_context->SetMaterial(aisShape, material, false);
      m_context->SetColor(aisShape, finalColor, false);
      m_context->Display(aisShape, false);
      m_lines.push_back(aisShape);
      processed++;
    }
  }

  qDebug() << "buildFullBridgeFromShapes: Pushed " << processed
           << " AIS_Shapes.";

  // 最后调用一次 fitAll 更新视图
  fitAll();
}