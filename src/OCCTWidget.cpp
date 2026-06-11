#include <AIS_ListOfInteractive.hxx>
#include <AIS_Shape.hxx>

#include "../include/OCCTWidget.h"
#include "../include/AspectWindow.h"
#include <OpenGl_GraphicDriver.hxx>
#include "../include/Line.h"
#include <Graphic3d_Vec2.hxx>

#include <Aspect_DisplayConnection.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <Graphic3d_Camera.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <gp_Pln.hxx>
#include <gp_Sphere.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Cone.hxx>
#include <IntAna_Quadric.hxx>
#include <Interface_Static.hxx>
#include <Prs3d_DimensionAspect.hxx>
#include <Prs3d_LineAspect.hxx>
#include <Prs3d_TextAspect.hxx>
#include <PrsDim_LengthDimension.hxx>
#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QShowEvent>
#include <QTemporaryFile>
#include <Quantity_Color.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPControl_Writer.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Lin.hxx>
#include <gp_Pln.hxx>
#include <gp_Trsf.hxx>
#include <Graphic3d_ZLayerId.hxx>
#include <XCAFPrs_AISObject.hxx>
#include <TPrsStd_AISPresentation.hxx>
#include <TPrsStd_AISViewer.hxx>
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Name.hxx>
#include <TDF_LabelSequence.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>

OCCTWidget::OCCTWidget(QWidget *parent)
    : QWidget(parent), m_viewer(nullptr), m_view(nullptr), m_context(nullptr),
      m_graphicDriver(nullptr),
      m_selectedLine(nullptr), m_drawLineMode(false), m_firstPointSet(false),
      m_frameCount(0), m_fps(0.0), m_shapeCount(0), m_usePbr(false),
      m_showViewCube(true), m_enableRotation(true),
      m_isDraggingLength(false), m_dragOriginalLength(0.0) {
  setFocusPolicy(Qt::StrongFocus);

  // 鍒濆鍖栦俊鎭彔鍔犳爣绛 
  m_infoLabel = new QLabel(this);
  m_infoLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
  m_infoLabel
      ->winId(); // 寮哄埗鍒涘缓鍘熺敓绐楀彛锛屼互纭繚鍦 WA_PaintOnScreen 琛ㄩ潰涓婂彲瑙 
  m_infoLabel->setStyleSheet(
      "QLabel { background-color: rgba(0, 0, 0, 100); color: cyan; "
      "font-weight: bold; border-radius: 5px; padding: 5px; }");
  m_infoLabel->move(10, 10);
  m_infoLabel->show();

  // 鍚姩瀹氭椂鍒锋柊锛岀‘淇 FPS 鍜屼俊鎭瘡甯у埛鏂 
  connect(&m_refreshTimer, &QTimer::timeout, this, [this]() { update(); });
  m_refreshTimer.start(16); // 绾 60 FPS

  // Set required attributes for OCCT integration
  setAttribute(Qt::WA_PaintOnScreen);
  setAttribute(Qt::WA_NoSystemBackground);
  setMouseTracking(true);
  setBackgroundRole(QPalette::NoRole);

  initOCCT();
  
  // 寤惰繜涓€绉掑啀娆″己鍒惰皟鏁村ぇ灏忥紝纭繚鍒濆 showMaximized 鐘舵€佽姝ｇ‘鎹曡幏
  QTimer::singleShot(100, this, [this]() {
    if (!m_view.IsNull()) {
      m_view->MustBeResized();
      m_view->Redraw();
    }
  });
}

static Handle(OpenGl_GraphicDriver) s_graphicDriver;

void OCCTWidget::releaseSharedDriver() {
  if (!s_graphicDriver.IsNull()) {
    s_graphicDriver.Nullify();
  }
}

void OCCTWidget::initOCCT() {
  try {
    if (s_graphicDriver.IsNull()) {
      Handle(Aspect_DisplayConnection) aDisplayConnection =
          new Aspect_DisplayConnection();
      s_graphicDriver = new OpenGl_GraphicDriver(aDisplayConnection);
    }
    m_graphicDriver = s_graphicDriver;
    // 重要：设置图形驱动的选项
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

    // Create custom AspectWindow that correctly handles Qt DPI scaling
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

    // 灏嗚鍥句腑蹇冨鍑嗕笘鐣屽潗鏍囧師鐐 (0,0,0)
    m_view->Camera()->SetCenter(gp_Pnt(0, 0, 0));
    m_view->SetScale(100.0); // 璁剧疆鍚堥€傜殑鍒濆缂╂斁姣斾緥

    // 1. 鍦ㄥ乏涓嬭鏄剧ず甯﹀皬鏂瑰潡鐨勫潗鏍囪酱 (ZBUFFER 妯″紡)
    m_view->TriedronDisplay(Aspect_TOTP_LEFT_LOWER, Quantity_NOC_GRAY80, 0.1, V3d_ZBUFFER);

    // 鍚敤鎶楅敮榻 (OCCT 7.9+ 鎺ㄨ崘鏂瑰紡)
    m_view->ChangeRenderingParams().IsAntialiasingEnabled = Standard_True;
    
    updateView();
  } catch (const std::exception &e) {
    // 鍦ㄨ皟璇曠増鏈腑锛屾垜浠彲浠ヨ緭鍑洪敊璇俊鎭 
    // 浣嗗湪瀹為檯搴旂敤涓紝浣犲彲鑳芥兂浣跨敤鍏朵粬鏃ュ織鏈哄埗
  } catch (...) {
    // 鎹曡幏鎵€鏈夊紓甯 
  }
}

void OCCTWidget::initViewCube() {
  if (!m_showViewCube || m_context.IsNull() || !m_viewCube.IsNull())
    return;

  try {
    m_viewCube = new AIS_ViewCube();
    m_viewCube->SetSize(100.0);
    m_viewCube->SetBoxColor(Quantity_Color(Quantity_NOC_ANTIQUEWHITE));
    m_viewCube->SetTextColor(Quantity_Color(Quantity_NOC_BLACK));
    m_viewCube->SetInnerColor(Quantity_Color(Quantity_NOC_GRAY50));
    
    // 璁剧疆鎸佷箙鎬э細鍙充笂瑙 
    m_viewCube->SetTransformPersistence(new Graphic3d_TransformPers(
        Graphic3d_TMF_TriedronPers, Aspect_TOTP_RIGHT_UPPER, Graphic3d_Vec2i(100, 100)));

    m_context->Display(m_viewCube, 1, 0, false);
    m_context->Activate(m_viewCube, 0);
  } catch (...) {
    // 蹇界暐寮傚父
  }
}

void OCCTWidget::cleanup() {
  if (!m_context.IsNull()) {
    m_context->RemoveAll(Standard_False);
    m_context.Nullify();
  }
  if (!m_view.IsNull()) {
    m_view->Remove();
    m_view.Nullify();
  }
  if (!m_viewer.IsNull()) {
    m_viewer.Nullify();
  }
  if (!m_graphicDriver.IsNull()) {
    m_graphicDriver.Nullify();
  }
  // 如果是共享的 s_graphicDriver，我们在 MainWindow 中主动将其置空
}

OCCTWidget::~OCCTWidget() {
  // 不在析构函数里强行清理，而是依赖提前调用 cleanup()
  // 以免 eglMakeCurrent() 因为窗口上下文已销毁而失败
}

void OCCTWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  if (!m_view.IsNull()) {
    // 姣忓抚妫€娴 widget 澶у皬鏄惁鍙樺寲锛岃嚜鍔ㄥ悓姝 OCCT 瑙嗗浘澶у皬
    // 瑙ｅ喅 showMaximized() 鍦 setCentralWidget() 涔嬪墠璋冪敤鏃 
    // 绐楀彛甯冨眬鍙樺寲鏈 OCCT 姝ｇ‘鎹曡幏鐨勯棶棰 
    QSize currentSize = size();
    if (currentSize != m_lastSize && currentSize.width() > 0 && currentSize.height() > 0) {
      m_lastSize = currentSize;
      m_view->MustBeResized();
    }

    if (m_viewCube.IsNull()) {
      initViewCube();
    }
    m_view->Redraw();
  }

  // 璁＄畻 FPS
  m_frameCount++;
  if (!m_fpsTimer.isValid()) {
    m_fpsTimer.start();
  } else {
    qint64 elapsed = m_fpsTimer.elapsed();
    if (elapsed >= 1000) {
      m_fps = m_frameCount * 1000.0 / elapsed;
      m_frameCount = 0;
      m_fpsTimer.restart();
    }
  }

  // 缁熻妯″瀷鏁伴噺
  if (!m_context.IsNull()) {
    NCollection_List<Handle(AIS_InteractiveObject)> displayed;
    m_context->DisplayedObjects(displayed);
    m_shapeCount = displayed.Extent();
  }

  // 鏇存柊淇℃伅鍙犲姞鏄剧ず
  if (m_infoLabel) {
    m_infoLabel->raise(); // 纭繚鏍囩濮嬬粓鍦ㄦ渶椤跺眰
    QString info =
        QString("Shapes: %1 | FPS: %2").arg(m_shapeCount).arg(m_fps, 0, 'f', 1);
    m_infoLabel->setText(info);
    m_infoLabel->adjustSize();
  }
}

void OCCTWidget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);

  if (!m_view.IsNull()) {
    m_view->MustBeResized();
    m_view->Redraw();
  }
}

void OCCTWidget::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
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

        // Convert world point to screen space to check pixel distance 
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
  if (m_enableRotation && (event->button() == Qt::RightButton)) {
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

  // 检查是否点击了 ViewCube 交互组件
  qreal pixelRatio = devicePixelRatio();
  m_context->MoveTo(static_cast<int>(event->pos().x() * pixelRatio),
                    static_cast<int>(event->pos().y() * pixelRatio), m_view, true);
  
  if (!m_viewCube.IsNull() && m_context->HasDetected()) {
    Handle(AIS_ViewCubeOwner) aCubeOwner = Handle(AIS_ViewCubeOwner)::DownCast(m_context->DetectedOwner());
    if (!aCubeOwner.IsNull()) {
      m_viewCube->HandleClick(aCubeOwner);
      update();
      return; 
    }
  }

  if (m_isDraggingLength) {
    if (event->button() == Qt::LeftButton) {
      // Confirm drag
      m_isDraggingLength = false;
      
      // Notify main window to update property with the newest length
      emit propertyDragged(m_dragNodeId, "Length", m_dragCurrentLength);
      
      // Clear preview
      if (!m_dynamicPreview.IsNull()) {
        m_context->Remove(m_dynamicPreview, false);
        m_dynamicPreview.Nullify();
      }
      m_context->UpdateCurrentViewer();
      this->update();
      return;
    } else if (event->button() == Qt::RightButton) {
      // Cancel drag
      cancelLengthDragging();
      return;
    }
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
        m_context->Remove(m_dynamicLine, true);
        m_dynamicLine.Nullify();
      }
    }
  } else if (event->button() == Qt::LeftButton) {
    qreal pixelRatio = devicePixelRatio();
    m_context->MoveTo(static_cast<int>(event->pos().x() * pixelRatio),
                      static_cast<int>(event->pos().y() * pixelRatio), m_view,
                      true);

    m_context->Select(true);

    // 鍙戝嚭瀵硅薄閫変腑淇″彿
    m_context->InitSelected();
    if (m_context->MoreSelected()) {
      Handle(AIS_InteractiveObject) selObj = m_context->SelectedInteractive();

      // Check if we clicked either of the length handles
      bool isStart = (!m_startHandle.IsNull() && selObj == m_startHandle);
      bool isEnd = (!m_lengthHandle.IsNull() && selObj == m_lengthHandle);
      
      if (isStart || isEnd) {
        m_isDraggingLength = true;
        m_dragIsStartHandle = isStart;
        m_dragCurrentLength = m_dragOriginalLength;
        
        // 隐藏手柄球进行预览
        m_context->Erase(m_startHandle, false);
        m_context->Erase(m_lengthHandle, false);
        m_context->UpdateCurrentViewer();
        this->update();
        return; // Enter dragging mode
      }

      if (m_objectMetadata.contains(selObj)) {
        emit objectSelected(m_objectMetadata[selObj]);
      } else {
        Handle(XCAFPrs_AISObject) xcafObj = Handle(XCAFPrs_AISObject)::DownCast(selObj);
        if (!xcafObj.IsNull()) {
          TDF_Label label = xcafObj->GetLabel();
          Handle(TDataStd_AsciiString) adNodeIdAttr;
          if (label.FindAttribute(TDataStd_AsciiString::GetID(), adNodeIdAttr)) {
            QVariantMap meta;
            meta["_adNodeId"] = QString(adNodeIdAttr->Get().ToCString());
            emit objectSelected(meta);
          } else {
            QVariantMap basicMeta;
            basicMeta["name"] = selObj->DynamicType()->Name();
            emit objectSelected(basicMeta);
          }
        } else {
          QVariantMap basicMeta;
          basicMeta["name"] = selObj->DynamicType()->Name();
          emit objectSelected(basicMeta);
        }
      }
    } else {
      emit objectSelected(QVariantMap()); // 鍙戦€佺┖琛ㄧず鍙栨秷閫変腑
    }
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
          contextMenu.setAttribute(Qt::WA_TranslucentBackground);
          contextMenu.setStyleSheet(
              "QMenu { background: #2d2d2d; border: 1px solid #555; border-radius: 8px; padding: 4px; }"
              "QMenu::item { color: white; padding: 6px 24px; text-align: center; background: transparent; }"
              "QMenu::item:selected { background: #0078d4; border-radius: 4px; }"
          );
          QAction *deleteAction = contextMenu.addAction(QStringLiteral("删除"));

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
  // Standard_Real aZoomFactor = (delta > 0)   1.1 : 0.9;
  // m_view->SetScale(m_view->Scale() * aZoomFactor);

  m_view->Redraw();
}

void OCCTWidget::mouseMoveEvent(QMouseEvent *event) {
  // Check for Rotation (Right Button or Left + Ctrl)
  if (m_enableRotation && ((event->buttons() & Qt::RightButton) ||
      ((event->buttons() & Qt::LeftButton) &&
       (event->modifiers() & Qt::ControlModifier)))) {
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

  // Original Panning Logic (Left Button without modifiers) - KEEP or REMOVE 
  // Let's keep it for now as "Pan" if no mode is active, but maybe restrict it 
  // User asked for "Increase rotation function". Standard CAD:
  // Middle=Pan, Right/Ctrl+Left=Rotate, Wheel=Zoom.
  // Old logic had Left=Pan if not drawing. Let's keep it but prioritize
  // Rotation.
  gp_Pnt mouseWorldPos;
  if (Get3DPoint(event->pos().x(), event->pos().y(), mouseWorldPos)) {
    emit mousePositionChanged(mouseWorldPos.X(), mouseWorldPos.Y(),
                              mouseWorldPos.Z());
  }

  // Check for Length Dragging
  if (m_isDraggingLength) {
    gp_Pnt currentPoint;
    if (Get3DPoint(event->pos().x(), event->pos().y(), currentPoint)) {
      // 1. 将 3D 世界坐标逆变换回局部坐标系，实现沿 X 轴轨道方向的任意三维投影
      gp_Pnt currentLocalPoint = currentPoint.Transformed(m_dragTrsf.Inverted());
      double localX = currentLocalPoint.X();
      
      double lStart = 0.0;
      double lEnd = m_dragOriginalLength;
      
      if (m_dragIsStartHandle) {
          lStart = localX;
          if (lStart > lEnd - 100.0) lStart = lEnd - 100.0; // 最小预留 100mm 间距
          m_dragCurrentLength = lEnd - lStart;
      } else {
          lEnd = localX;
          if (lEnd < 100.0) lEnd = 100.0; // 最小预留 100mm 长度
          m_dragCurrentLength = lEnd;
      }
      
      // Update dynamic preview
      if (!m_dynamicPreview.IsNull()) {
        m_context->Remove(m_dynamicPreview, false);
        m_dynamicPreview.Nullify();
      }

      try {
        double W = m_dragHeight * m_dragSlopeRatio;
        double H = m_dragHeight;

        // 起点端截面顶点
        gp_Pnt s1(lStart, 0, H);
        gp_Pnt s2(lStart, W, 0);
        gp_Pnt s3(lStart, 0, 0);

        // 终点端截面顶点
        gp_Pnt e1(lEnd, 0, H);
        gp_Pnt e2(lEnd, W, 0);
        gp_Pnt e3(lEnd, 0, 0);

        // 构建端点截面的三角形线框
        BRepBuilderAPI_MakeEdge meS1(s1, s2);
        BRepBuilderAPI_MakeEdge meS2(s2, s3);
        BRepBuilderAPI_MakeEdge meS3(s3, s1);

        BRepBuilderAPI_MakeEdge meE1(e1, e2);
        BRepBuilderAPI_MakeEdge meE2(e2, e3);
        BRepBuilderAPI_MakeEdge meE3(e3, e1);

        // 构建连接起点和终点端截面的三条轴线/脊骨线
        BRepBuilderAPI_MakeEdge meL1(s1, e1);
        BRepBuilderAPI_MakeEdge meL2(s2, e2);
        BRepBuilderAPI_MakeEdge meL3(s3, e3);

        // 打包组合成 Compound 作为三维线框包络
        TopoDS_Compound compound;
        BRep_Builder builder;
        builder.MakeCompound(compound);

        builder.Add(compound, meS1.Edge());
        builder.Add(compound, meS2.Edge());
        builder.Add(compound, meS3.Edge());

        builder.Add(compound, meE1.Edge());
        builder.Add(compound, meE2.Edge());
        builder.Add(compound, meE3.Edge());

        builder.Add(compound, meL1.Edge());
        builder.Add(compound, meL2.Edge());
        builder.Add(compound, meL3.Edge());

        m_dynamicPreview = new AIS_Shape(compound);
        m_context->SetColor(m_dynamicPreview, Quantity_Color(Quantity_NOC_YELLOW), false);
        m_context->SetWidth(m_dynamicPreview, 3.0, false);
        
        // 应用绝对坐标变换
        m_dynamicPreview->SetLocalTransformation(m_dragTrsf);
        
        m_context->Display(m_dynamicPreview, true);
      } catch (...) {
        qDebug() << "[OCCTWidget] mouseMoveEvent: ERROR: Failed to generate ghost preview shape!";
      }
    }
    
    m_xPos = event->pos().x();
    m_yPos = event->pos().y();
    return; // Block other mouse moves
  }

  // Check for Panning (Left Button and NOT in Draw Mode)
  if ((event->buttons() & Qt::LeftButton) && !m_drawLineMode) {
    // Pan the view
    if (!m_view.IsNull()) {
      qreal pixelRatio = devicePixelRatio();
      m_view->Pan(static_cast<Standard_Integer>((event->pos().x() - m_xPos) * pixelRatio),
                  static_cast<Standard_Integer>((m_yPos - event->pos().y()) * pixelRatio));
      m_xPos = event->pos().x();
      m_yPos = event->pos().y();
      m_view->Invalidate();
      m_view->Redraw();
      return; 
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
                          Graphic3d_NameOfMaterial material,
                          const QVariantMap &metadata) {
  if (!m_context.IsNull()) {
    Handle(AIS_Shape) aisShape = new AIS_Shape(shape);

    // 1. 璁剧疆鏄剧ず妯″紡涓 Shaded (1)
    m_context->SetDisplayMode(aisShape, 1, false);

    // 2. 鍏堣缃潗璐 
    m_context->SetMaterial(aisShape, material, false);

    // 3. 鍚庤缃鑹 (纭繚瑕嗙洊鏉愯川鑷甫棰滆壊)
    m_context->SetColor(aisShape, color, false);

    // 4. 鏄剧ず
    m_context->Display(aisShape, false);

    m_lines.push_back(aisShape);
      
      
    if (!metadata.isEmpty()) {
      m_objectMetadata[aisShape] = metadata;
      applyMaterial(aisShape, metadata);
    }
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
    else if (fontChinese.Init("   ", Font_FA_Regular, height))
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
  Standard_Integer mode = 1;

  // Iterate over all managed objects (m_lines stores them)
  // In a real app we might want to filter only text objects.
  for (const auto &aisShape : m_lines) {
    if (!aisShape.IsNull()) {
      // Check if already displayed to avoid errors  SetDisplayMode is safe.
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

  // 鏅鸿兘鑹插僵鏄犲皠锛氳鏉愯川鐪嬭捣鏉ユ洿鐪熷疄
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
    finalColor = Quantity_NOC_GRAY30; // 閲嶅伐涓氭繁鐏拌壊
    break;
  case Graphic3d_NOM_PLASTIC:
    finalColor = Quantity_NOC_YELLOW; // 濉戞枡榛樿鏄庨粍
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

  // 濡傛灉鏈 Y 杞村亸绉伙紝瀵瑰舰鐘跺仛骞崇Щ鍙樻崲
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
    finalColor = Quantity_NOC_GRAY75;
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
      
      
    // 涓嶈皟鐢 updateView() 鍜 fitAll()锛岀敱璋冪敤鏂规渶缁堢粺涓€鍒锋柊
  }
}

void OCCTWidget::loadBrepAsFullBridge(const QString &filename, int count,
                                      double spacing,
                                      Graphic3d_NameOfMaterial material) {
  if (m_context.IsNull())
    return;

  // 璇诲彇鍘熷 BREP
  TopoDS_Shape baseShape;
  BRep_Builder builder;
  QByteArray ba = filename.toLocal8Bit();
  if (!BRepTools::Read(baseShape, ba.data(), builder))
    return;

  // 棰滆壊鏄犲皠 (涓 loadBrepFile 淇濇寔涓€鑷 
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

  // 鍒涘缓 count 涓嫭绔 AIS_Shape锛屾部 Y 鏂瑰悜闂磋窛 spacing
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
  qDebug() << "[OCCTWidget] clearAll: start";
  if (m_context.IsNull())
    return;

  qDebug() << "[OCCTWidget] clearAll: calling RemoveAll";
  m_context->RemoveAll(true);
  
  qDebug() << "[OCCTWidget] clearAll: handling viewCube";
  // 鍏抽敭淇锛歊emoveAll 浼氱Щ闄 ViewCube锛岃繖閲岄渶瑕侀噸鏂版樉绀哄畠
  if (!m_viewCube.IsNull()) {
    m_context->Display(m_viewCube, AIS_Shaded, 0, false);
    m_context->Activate(m_viewCube, 0);
  }

  qDebug() << "[OCCTWidget] clearAll: UpdateCurrentViewer";
  m_context->UpdateCurrentViewer();

  qDebug() << "[OCCTWidget] clearAll: clearing vectors";
  m_lines.clear();
  m_objectMetadata.clear();

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

  qDebug() << "[OCCTWidget] clearAll: final UpdateCurrentViewer";
  m_context->UpdateCurrentViewer();
  qDebug() << "[OCCTWidget] clearAll: done";
}

void OCCTWidget::exportToSTEP(const QString &filename) {
  if (m_context.IsNull())
    return;

  STEPControl_Writer writer;
  Interface_Static::SetCVal("write.step.schema", "AP214");

  // Iterate over all displayed objects and add their shapes to the writer
  NCollection_List<Handle(AIS_InteractiveObject)> displayedObjects;
  m_context->DisplayedObjects(displayedObjects);

  int shapeCount = 0;
  for (NCollection_List<Handle(AIS_InteractiveObject)>::Iterator it(
           displayedObjects);
       it.More(); it.Next()) {
    Handle(AIS_InteractiveObject) obj = it.Value();
    Handle(AIS_Shape) shapeObj = Handle(AIS_Shape)::DownCast(obj);
    if (!shapeObj.IsNull()) {
      IFSelect_ReturnStatus status =
          writer.Transfer(shapeObj->Shape(), STEPControl_AsIs);
      if (status == IFSelect_RetDone) {
        shapeCount++;
      }
    }
  }

  if (shapeCount > 0) {
    IFSelect_ReturnStatus status = writer.Write(filename.toUtf8().constData());
    if (status == IFSelect_RetDone) {
      QMessageBox::information(this, "Info", "Action completed");
    } else {
      QMessageBox::information(this, "Info", "Action completed");
    }
  } else {
    QMessageBox::information(this, "Info", "Action completed");
  }
}

#if __has_include(<RWGltf_CafWriter.hxx>)
#include <Message_ProgressRange.hxx>
#include <RWGltf_CafWriter.hxx>
#if __has_include(<TColStd_IndexedDataMapOfStringString.hxx>)
#include <TColStd_IndexedDataMapOfStringString.hxx>
#else
#include <NCollection_IndexedDataMap.hxx>
#include <TCollection_AsciiString.hxx>
// In OCCT 8.0+, TColStd_IndexedDataMapOfStringString is deprecated and moved
typedef NCollection_IndexedDataMap<TCollection_AsciiString,
                                   TCollection_AsciiString>
    TColStd_IndexedDataMapOfStringString;
#endif
#include <TDocStd_Document.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#endif

void OCCTWidget::exportToGLTF(const QString &filename) {
  if (m_context.IsNull())
    return;

#if __has_include(<RWGltf_CafWriter.hxx>)
  Handle(TDocStd_Document) doc;
  Handle(TDocStd_Application) app = XCAFApp_Application::GetApplication();
  app->NewDocument("BinXCAF", doc);
  Handle(XCAFDoc_ShapeTool) shapeTool =
      XCAFDoc_DocumentTool::ShapeTool(doc->Main());
  Handle(XCAFDoc_ColorTool) colorTool =
      XCAFDoc_DocumentTool::ColorTool(doc->Main());

  NCollection_List<Handle(AIS_InteractiveObject)> displayedObjects;
  m_context->DisplayedObjects(displayedObjects);

  int shapeCount = 0;
  for (NCollection_List<Handle(AIS_InteractiveObject)>::Iterator it(
           displayedObjects);
       it.More(); it.Next()) {
    Handle(AIS_InteractiveObject) obj = it.Value();
    Handle(AIS_Shape) shapeObj = Handle(AIS_Shape)::DownCast(obj);
    if (!shapeObj.IsNull()) {
      TDF_Label label = shapeTool->AddShape(shapeObj->Shape(), false);
      Quantity_Color color;
      if (shapeObj->HasColor()) {
        shapeObj->Color(color);
        colorTool->SetColor(label, color, XCAFDoc_ColorGen);
      }
      shapeCount++;
    }
  }

  if (shapeCount > 0) {
    RWGltf_CafWriter writer(filename.toUtf8().constData(), true);
    TColStd_IndexedDataMapOfStringString fileInfo;
    Message_ProgressRange progress;
    bool status = writer.Perform(doc, fileInfo, progress);
    if (status) {
      QMessageBox::information(this, "Info", "Action completed");
    } else {
      QMessageBox::information(this, "Info", "Action completed");
    }
  } else {
    QMessageBox::information(this, "Info", "Action completed");
  }
#else
  QMessageBox::critical(this, "   ",
                        "    OCCT    /        GLTF        (      "
                        "TKDEGLTF      RWGltf_CafWriter.hxx)  );
#endif
}

void OCCTWidget::annotateBridgePierFooting() {
  if (m_context.IsNull())
    return;

  //          :             Z=-140   
  // -130   (X)=89.59   (Y)=59.05   (Z)=10
  double length = 89.59;
  double width = 59.05;
  double height = 10.0;

  double minX = -length / 2.0;
  double maxX = length / 2.0;
  double minY = -width / 2.0;
  double maxY = width / 2.0;
  double minZ = -140.0;
  double maxZ = -130.0;

  // X       (   -              (-Y       )
  gp_Pnt pL1(minX, minY, minZ);
  gp_Pnt pL2(maxX, minY, minZ);
  gp_Pln plnL(gp_Pnt(0, minY, minZ), gp_Dir(0, 0, 1));
  Handle(PrsDim_LengthDimension) dimL =
      new PrsDim_LengthDimension(pL1, pL2, plnL);
  dimL->SetFlyout(-35.0);

  // Y       (   -              (+X       )
  gp_Pnt pW1(maxX, minY, minZ);
  gp_Pnt pW2(maxX, maxY, minZ);
  gp_Pln plnW(gp_Pnt(maxX, 0, minZ), gp_Dir(0, 0, 1));
  Handle(PrsDim_LengthDimension) dimW =
      new PrsDim_LengthDimension(pW1, pW2, plnW);
  dimW->SetFlyout(35.0);

  // Z       (   -            (+X       )
  gp_Pnt pH1(maxX, minY, minZ);
  gp_Pnt pH2(maxX, minY, maxZ);
  gp_Pln plnH(gp_Pnt(maxX, minY, 0), gp_Dir(0, -1, 0));
  Handle(PrsDim_LengthDimension) dimH =
      new PrsDim_LengthDimension(pH1, pH2, plnH);
  dimH->SetFlyout(35.0);

  //               
  Handle(PrsDim_LengthDimension) dims[] = {dimL, dimW, dimH};
  for (int i = 0; i < 3; ++i) {
    Handle(Prs3d_DimensionAspect) aspect = dims[i]->DimensionAspect();
    if (!aspect.IsNull()) {
      //       
      aspect->TextAspect()->SetHeight(12.0);
      aspect->TextAspect()->SetColor(Quantity_NOC_BLACK);
      //         
      aspect->LineAspect()->SetColor(Quantity_NOC_BLACK);
      //               
      aspect->SetArrowOrientation(Prs3d_DAO_External);
      aspect->SetTextHorizontalPosition(Prs3d_DTHP_Left);
      aspect->SetTextVerticalPosition(Prs3d_DTVP_Above);
    }
    m_context->Display(dims[i], false);
    m_dimensions.push_back(dims[i]);
  }

  m_context->UpdateCurrentViewer();
}

void OCCTWidget::addLengthDimension(const gp_Pnt& p1, const gp_Pnt& p2, const gp_Pln& plane, double flyout) {
  if (m_context.IsNull()) return;
  
  Handle(PrsDim_LengthDimension) dim = new PrsDim_LengthDimension(p1, p2, plane);
  dim->SetFlyout(flyout);
  
  Handle(Prs3d_DimensionAspect) aspect = dim->DimensionAspect();
  if (!aspect.IsNull()) {
    aspect->TextAspect()->SetHeight(12.0);
    aspect->TextAspect()->SetColor(Quantity_NOC_BLACK);
    aspect->LineAspect()->SetColor(Quantity_NOC_BLACK);
    aspect->SetArrowOrientation(Prs3d_DAO_External);
    aspect->SetTextHorizontalPosition(Prs3d_DTHP_Center);
    aspect->SetTextVerticalPosition(Prs3d_DTVP_Above);
  }
  
  m_context->Display(dim, false);
  m_dimensions.push_back(dim);
  m_context->UpdateCurrentViewer();
}

// ==========        ==========
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

  //          
  clearAll();

  //******        *****
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

  // ---    1 (    Z=0) ---
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

  // ---    2 (    Z=27.5) ---
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

  // ---    3 (    Z=13.75) ---
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

  //         - ThruSections    
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

  // ---     (Z=30) ---
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

  //     - ThruSections    
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

  // ---     (Prism   ) ---
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

  // ---     (Z=-120) ---
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

  //     - ThruSections    
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

  // ---     (      ) ---
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

  //       
  fitAll();
}

void OCCTWidget::drawFullBridgePier() {
  //           MainWindow::onDrawFullBridgePier                       
  //      C++                                        
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
  if (data.isEmpty())
    return TopoDS_Shape();

  //        
  std::string dataStr(data.constData(), data.length());
  bool isStep = (dataStr.find("ISO-10303-21") != std::string::npos);

  if (isStep) {
    QTemporaryFile tempFile;
    if (tempFile.open()) {
      tempFile.write(data);
      QString fileName = tempFile.fileName();
      tempFile.close();

      STEPControl_Reader reader;
      IFSelect_ReturnStatus status =
          reader.ReadFile(fileName.toStdString().c_str());
      if (status == IFSelect_RetDone) {
        reader.TransferRoots();
        if (reader.NbShapes() > 0) {
          return reader.OneShape();
        }
      }
      qWarning()
          << "STEPControl_Reader failed to parse STEP from temp file! Status:"
          << (int)status;
    } else {
      qWarning() << "Failed to create temporary file for STEP parsing!";
    }
    return TopoDS_Shape();
  } else {
    //        BREP    
    TopoDS_Shape shape;
    BRep_Builder builder;
    std::stringstream ss(dataStr);
    BRepTools::Read(shape, ss, builder);
    if (shape.IsNull()) {
      qWarning()
          << "BRepTools::Read failed to parse shape from memory! Data size:"
          << data.length();
      //    :      50      
      qWarning() << "Data prefix:" << data.left(50);
    }
    return shape;
  }
}

void OCCTWidget::displayShape(const TopoDS_Shape &shape,
                              Graphic3d_NameOfMaterial material, bool fit,
                              const QVariantMap &metadata) {
  if (shape.IsNull() || m_context.IsNull())
    return;

  Quantity_Color finalColor;
  // ... (switch case logic remains)
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
  case Graphic3d_NOM_STONE:
    finalColor = Quantity_NOC_GRAY80; //                      
    break;
  case Graphic3d_NOM_PLASTIC:
    finalColor = Quantity_NOC_GRAY75;
    break;
  case Graphic3d_NOM_GLASS:
    finalColor = Quantity_NOC_LIGHTBLUE;
    break;
  default:
    finalColor = Quantity_NOC_GRAY75;
    break;
  }

  displayShape(shape, material, finalColor, fit, metadata);
}

void OCCTWidget::displayShape(const TopoDS_Shape &shape,
                              Graphic3d_NameOfMaterial material,
                              const Quantity_Color &color, bool fit,
                              const QVariantMap &metadata) {
  if (shape.IsNull() || m_context.IsNull())
    return;

  Handle(AIS_Shape) aisShape = new AIS_Shape(shape);
  m_context->SetDisplayMode(aisShape, 1, false);
  m_context->SetMaterial(aisShape, material, false);
  m_context->SetColor(aisShape, color, false);
  m_context->Display(aisShape, false);
  m_lines.push_back(aisShape);
      
  m_objectMetadata[aisShape] = metadata;
  applyMaterial(aisShape, metadata);

  if (fit)
    fitAll();
}

void OCCTWidget::buildFullBridgeFromParts(
    const QList<OCCTWidget::AssemblyPart> &parts, int count, double spacing) {

  if (parts.isEmpty())
    return;

  // 0:Pile, 1:PileCap, 2:PierBody, 3:PierTray, 4:Stone1, 5:Stone2, 6:Bearing1,
  // 7:Bearing2, 8:Girder
  int partsAvailable = parts.size();

  // 从元数据中提取墩身高度参数，用于计算各构件 Z 偏移
  double pierHeight = 12000.0;
  if (partsAvailable > 2 && parts[2].metadata.contains("args")) {
    QVariantMap argsMap = parts[2].metadata["args"].toMap();
    if (argsMap.contains("pierHeight")) pierHeight = argsMap["pierHeight"].toDouble();
    else if (argsMap.contains("Height")) pierHeight = argsMap["Height"].toDouble();
  }
  // 承台参数
  double pileCapHeight = 2000.0;

  for (int i = 0; i < count; ++i) {
    double yOff = i * spacing;

    for (int j = 0; j < qMin(8, partsAvailable); ++j) {
      if (parts[j].shape.IsNull())
        continue;

      gp_Trsf pierTrsf;
      gp_Vec offset(0, yOff, 0);

      // 坐标约定: Z=0 为托盘底面 (= 墩身顶面)
      // PierTray (index 3): Z=0 ~ Z=3000, 无需偏移
      // PierBody (index 2): 脚本输出 Z=0~pierHeight, 需移到 Z=-pierHeight~0
      // PileCap  (index 1): 脚本输出 Z=0~2000, 需移到 Z=-(pierHeight+500)
      // Pile     (index 0): 脚本输出向下extrude, 需移到 Z=-(pierHeight+pileCapHeight)
      if (j == 0) // Pile
        offset += gp_Vec(0, 0, -(pierHeight + pileCapHeight));
      if (j == 1) // PileCap
        offset += gp_Vec(0, 0, -(pierHeight + pileCapHeight));
      if (j == 2) // PierBody
        offset += gp_Vec(0, 0, -pierHeight);
      // j == 3: PierTray, 无偏移
      if (j == 4) // BedStone 1
        offset += gp_Vec(-1650.0, 0, 3000.0);
      if (j == 5) // BedStone 2
        offset += gp_Vec(1650.0, 0, 3000.0);
      if (j == 6) // Bearing 1
        offset += gp_Vec(-1650.0, 0, 3400.0);
      if (j == 7) // Bearing 2
        offset += gp_Vec(1650.0, 0, 3400.0);

      pierTrsf.SetTranslation(offset);

      QVariantMap partMeta = parts[j].metadata;
      if (!partMeta.contains("Pset_MaterialPBR")) {
        QVariantMap pbrMap;
        QVariantList colorList;
        if (j >= 4 && j <= 5) {
          colorList << 1.0 << 1.0 << 1.0; // White bed stones
          pbrMap["Roughness"] = 0.8;
          pbrMap["Metallic"] = 0.0;
        } else if (j >= 6 && j <= 7) {
          colorList << 0.3 << 0.3 << 0.3; // Dark gray steel bearings
          pbrMap["Roughness"] = 0.4;
          pbrMap["Metallic"] = 0.8;
        } else {
          colorList << 0.75 << 0.75 << 0.75; // Concrete pier elements
          pbrMap["Roughness"] = 0.8;
          pbrMap["Metallic"] = 0.0;
        }
        pbrMap["BaseColor"] = colorList;
        partMeta["Pset_MaterialPBR"] = pbrMap;
      }

      Handle(AIS_Shape) aisShape = new AIS_Shape(parts[j].shape);
      m_context->SetDisplayMode(aisShape, 1, false);
      aisShape->SetLocalTransformation(pierTrsf);
      m_context->Display(aisShape, false);
      m_lines.push_back(aisShape);
      
      m_objectMetadata[aisShape] = partMeta;
      applyMaterial(aisShape, partMeta);
    }

    if (partsAvailable > 8 && i < count - 1) {
      if (!parts[8].shape.IsNull()) {
        gp_Trsf rot;
        rot.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), M_PI / 2.0);

        gp_Trsf trans;
        trans.SetTranslation(gp_Vec(0, yOff + 50.0, 3650.0)); // Girder sits above bearings

        gp_Trsf girderTrsf = trans * rot;
        
        QVariantMap partMeta = parts[8].metadata;
        if (!partMeta.contains("Pset_MaterialPBR")) {
          QVariantMap pbrMap;
          QVariantList colorList;
          colorList << 0.75 << 0.75 << 0.75; // Gray concrete girder
          pbrMap["BaseColor"] = colorList;
          pbrMap["Roughness"] = 0.8;
          pbrMap["Metallic"] = 0.0;
          partMeta["Pset_MaterialPBR"] = pbrMap;
        }

        Handle(AIS_Shape) aisGirder = new AIS_Shape(parts[8].shape);
        m_context->SetDisplayMode(aisGirder, 1, false);
        aisGirder->SetLocalTransformation(girderTrsf);
        m_context->Display(aisGirder, false);
        m_lines.push_back(aisGirder);
        
        m_objectMetadata[aisGirder] = partMeta;
        applyMaterial(aisGirder, partMeta);
      }
    }
  }

  fitAll();
}

void OCCTWidget::buildFullBridgeFromBatch(
    const QList<OCCTWidget::AssemblyPart> &parts) {
  qDebug() << "buildFullBridgeFromBatch: Received " << parts.size()
           << " shapes.";

  if (parts.isEmpty() || m_context.IsNull()) {
    qWarning() << "buildFullBridgeFromBatch aborting: Parts empty or "
                  "context null.";
    return;
  }

  for (const auto &part : parts) {
    if (part.shape.IsNull())
      continue;

    Quantity_Color finalColor = Quantity_NOC_GRAY75;
    switch (part.material) {
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
    case Graphic3d_NOM_STONE:
      finalColor = Quantity_NOC_GRAY80;
      break;
    default:
      finalColor = Quantity_NOC_GRAY75;
      break;
    }

    displayShape(part.shape, part.material, finalColor, false, part.metadata);
  }

  fitAll();
}
void OCCTWidget::setUsePbr(bool enabled) {
    m_usePbr = enabled;
    if (m_view.IsNull()) return;

    Graphic3d_RenderingParams& aParams = m_view->ChangeRenderingParams();
    if (enabled) {
        m_view->SetShadingModel(Graphic3d_TOSM_PBR);
        aParams.ToneMappingMethod = Graphic3d_ToneMappingMethod_Filmic;
        aParams.Exposure = -0.5f;   // Raise exposure to avoid being too dark
        aParams.WhitePoint = 1.5f;  // Adjust white point for natural transition
    } else {
        m_view->SetShadingModel(Graphic3d_TOSM_FRAGMENT);
        aParams.ToneMappingMethod = Graphic3d_ToneMappingMethod_Disabled;
        aParams.Exposure = 0.0f;
        aParams.WhitePoint = 1.0f;
    }

    // Regulate light source intensities for PBR workflow
    if (!m_viewer.IsNull()) {
        for (NCollection_List<Handle(Graphic3d_CLight)>::Iterator it(m_viewer->ActiveLights()); it.More(); it.Next()) {
            Handle(Graphic3d_CLight) aLight = it.Value();
            if (aLight.IsNull()) continue;
            if (enabled) {
                if (aLight->Type() == Graphic3d_TypeOfLightSource_Ambient) {
                    aLight->SetIntensity(0.25f); // Set to reasonable ambient light level
                } else if (aLight->Type() == Graphic3d_TypeOfLightSource_Directional) {
                    aLight->SetIntensity(1.0f);  // Retain directional light strength
                }
            } else {
                // Restore defaults for Phong shading
                if (aLight->Type() == Graphic3d_TypeOfLightSource_Ambient || 
                    aLight->Type() == Graphic3d_TypeOfLightSource_Directional) {
                    aLight->SetIntensity(1.0f);
                }
            }
        }
        m_viewer->UpdateLights();
    }
    
    if (m_context.IsNull()) return;
    
    AIS_ListOfInteractive aList;
    m_context->DisplayedObjects(aList);
    for (AIS_ListIteratorOfListOfInteractive it(aList); it.More(); it.Next()) {
        Handle(AIS_InteractiveObject) aisObj = it.Value();
        if (!aisObj.IsNull()) {
            applyMaterial(aisObj, m_objectMetadata.value(aisObj));
        }
    }
    m_context->UpdateCurrentViewer();
}

void OCCTWidget::applyMaterial(const Handle(AIS_InteractiveObject)& aisObj, const QVariantMap& metadata) {
    if (aisObj.IsNull()) return;

    Graphic3d_MaterialAspect material(Graphic3d_NOM_PLASTIC);

    Quantity_Color defaultColor(Quantity_NOC_GRAY70);
    if (aisObj->HasColor()) {
        aisObj->Color(defaultColor);
    }

    if (m_usePbr) {
        // Crucial: Clear custom LDR color override so that PBR material (including BaseColor/Roughness) takes effect!
        m_context->UnsetColor(aisObj, false);

        material.SetMaterialType(Graphic3d_MATERIAL_PHYSIC);
        Graphic3d_PBRMaterial pbrMat;
        
        // Use object's original color (or gray fallback) and reasonable matte PBR defaults
        pbrMat.SetColor(defaultColor);
        pbrMat.SetMetallic(0.0f);
        pbrMat.SetRoughness(0.8f);

        if (metadata.contains("Pset_MaterialPBR")) {
            QVariantMap pbr = metadata["Pset_MaterialPBR"].toMap();
            if (pbr.contains("BaseColor")) {
                QVariantList colorList = pbr["BaseColor"].toList();
                if (colorList.size() >= 3) {
                    pbrMat.SetColor(Quantity_Color(colorList[0].toDouble(), 
                                                 colorList[1].toDouble(), 
                                                 colorList[2].toDouble(), 
                                                 Quantity_TOC_RGB));
                }
            }
            if (pbr.contains("Metallic")) pbrMat.SetMetallic((float)pbr["Metallic"].toDouble());
            if (pbr.contains("Roughness")) pbrMat.SetRoughness((float)pbr["Roughness"].toDouble());
        }
        material.SetPBRMaterial(pbrMat);
    } else {
        material.SetMaterialType(Graphic3d_MATERIAL_ASPECT);
        material.SetColor(defaultColor);

        // Restore custom LDR color for Phong shading mode
        m_context->SetColor(aisObj, defaultColor, false);
    }

    m_context->SetMaterial(aisObj, material, false);
}
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>

void OCCTWidget::selectAndCenterObject(const QString& key, const QVariant& value) {
    if (m_context.IsNull() || m_view.IsNull()) return;

    m_context->ClearSelected(false);
    Handle(AIS_InteractiveObject) targetObj;

    for (auto it = m_objectMetadata.begin(); it != m_objectMetadata.end(); ++it) {
        if (it.value().contains(key) && it.value()[key] == value) {
            targetObj = it.key();
            break;
        }
    }

    if (!targetObj.IsNull()) {
        m_context->SetSelected(targetObj, true);
        
        // Calculate bounding box
        Bnd_Box bbox;
        Handle(AIS_Shape) aisShape = Handle(AIS_Shape)::DownCast(targetObj);
        if (!aisShape.IsNull()) {
            BRepBndLib::Add(aisShape->Shape(), bbox);
        }
        
        // 选中几何时不再执行适应屏幕 (FitAll) 操作
        // if (!bbox.IsVoid()) {
        //     m_view->FitAll(bbox, 0.1, Standard_True);
        // }
    }
    m_view->Redraw();
}

void OCCTWidget::loadXcafDocument(const Handle(TDocStd_Document)& doc) {
  if (m_context.IsNull() || doc.IsNull()) {
    qDebug() << "[OCCTWidget] loadXcafDocument: context or doc is NULL!";
    return;
  }

  qDebug() << "[OCCTWidget] loadXcafDocument: Starting...";

  if (!XCAFDoc_DocumentTool::IsXCAFDocument(doc)) {
    qDebug() << "[OCCTWidget] loadXcafDocument: ERROR: Document is NOT a valid XCAF assembly!";
    return;
  }

  // 1. 清除视口当前显示的普通对象和元数据映射
  m_context->EraseAll(Standard_True);
  m_objectMetadata.clear();

  // 2. 获取 ShapeTool
  Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
  if (shapeTool.IsNull()) {
      qDebug() << "[OCCTWidget] loadXcafDocument: ERROR: ShapeTool is NULL!";
      return;
  }

  // 3. 获取所有 Free Shapes
  TDF_LabelSequence freeShapes;
  shapeTool->GetFreeShapes(freeShapes);
  qDebug() << "[OCCTWidget] loadXcafDocument: Free shapes count:" << freeShapes.Length();

  // 4. 遍历并显示每个 Free Shape 作为 AIS_Shape
  Handle(XCAFDoc_ColorTool) colorTool = XCAFDoc_DocumentTool::ColorTool(doc->Main());
  for (Standard_Integer i = 1; i <= freeShapes.Length(); ++i) {
      TDF_Label label = freeShapes.Value(i);
      
      // 调试：打印 Label 上所挂的名字
      Handle(TDataStd_Name) nameAttr;
      std::string labelName = "Unknown";
      if (label.FindAttribute(TDataStd_Name::GetID(), nameAttr)) {
          labelName = QString::fromUtf16((const char16_t*)nameAttr->Get().ToExtString()).toStdString();
      }
      qDebug() << "[OCCTWidget] loadXcafDocument: Free shape index" << i << "Name =" << QString::fromStdString(labelName);
      
      TopoDS_Shape shape;
      if (XCAFDoc_ShapeTool::GetShape(label, shape) && !shape.IsNull()) {
          Handle(AIS_Shape) aisShape = new AIS_Shape(shape);
          
          Quantity_Color color;
          if (!colorTool.IsNull() && colorTool->GetColor(label, XCAFDoc_ColorCurv, color)) {
              aisShape->SetColor(color);
          } else {
              aisShape->SetColor(Quantity_NOC_BLACK);
          }
          
          m_context->Display(aisShape, Standard_False);
          
          // 读取绑定的 3D nodeId 属性，建立 2D 选择与编辑联动
          Handle(TDataStd_AsciiString) adNodeIdAttr;
          if (label.FindAttribute(TDataStd_AsciiString::GetID(), adNodeIdAttr)) {
              QVariantMap meta;
              meta["_adNodeId"] = QString(adNodeIdAttr->Get().ToCString());
              m_objectMetadata[aisShape] = meta;
              qDebug() << "[OCCTWidget] loadXcafDocument: Bound 2D shape with 3D nodeId:" << meta["_adNodeId"].toString();
          } else {
              qDebug() << "[OCCTWidget] loadXcafDocument: WARNING: Free shape has NO AsciiString nodeId attribute!";
          }
          
          qDebug() << "[OCCTWidget] loadXcafDocument: Displayed free shape index:" << i;
      }
  }

  // 5. 刷新视口并进行 FitAll 适配画面
  if (!m_view.IsNull()) {
      m_view->Redraw();
  }
  fitAll();
  qDebug() << "[OCCTWidget] loadXcafDocument: Done.";
}

void OCCTWidget::setAs2DView() {
  m_showViewCube = false;
  m_enableRotation = false;
  if (!m_viewCube.IsNull() && !m_context.IsNull()) {
    m_context->Erase(m_viewCube, Standard_False);
    m_viewCube.Nullify();
  }
  if (!m_view.IsNull()) {
    m_view->Camera()->SetProjectionType(Graphic3d_Camera::Projection_Orthographic);
    m_view->SetProj(V3d_Zpos);
    m_view->Redraw();
  }
}

void OCCTWidget::showLengthHandle(const gp_Pnt& startPos, const gp_Pnt& endPos, double currentLength, const QString& nodeId, const gp_Trsf& trsf, double height, double slopeRatio) {
  if (m_context.IsNull()) return;
  hideLengthHandle();

  m_dragNodeId = nodeId;
  m_dragOriginalLength = currentLength;
  m_dragCurrentLength = currentLength;
  m_dragTrsf = trsf;
  m_dragStartPos = startPos;
  m_dragEndPos = endPos;
  m_dragHeight = height;
  m_dragSlopeRatio = slopeRatio;

  // 使用专业的三维红色金属拉伸箭头替代原本简陋的小球手柄
  try {
    double arrowScale = (std::max(1000.0, currentLength * 0.05)) / 2.0;
    
    double rShaft = arrowScale * 0.25;  // 箭杆半径
    double hShaft = arrowScale * 1.5;   // 箭杆长度
    double rCone = arrowScale * 0.5;    // 锥体底半径
    double hCone = arrowScale * 1.0;    // 锥体高度

    BRep_Builder builder;

    // 1. 构建起点端手柄（指向负 X 方向拉伸，位于 0 处）
    gp_Ax2 shaftAxStart(gp_Pnt(0, 0, 0), gp_Dir(-1, 0, 0));
    TopoDS_Shape shaftStart = BRepPrimAPI_MakeCylinder(shaftAxStart, rShaft, hShaft).Shape();
    
    gp_Ax2 coneAxStart(gp_Pnt(-hShaft, 0, 0), gp_Dir(-1, 0, 0));
    TopoDS_Shape coneStart = BRepPrimAPI_MakeCone(coneAxStart, rCone, 0.0, hCone).Shape();
    
    TopoDS_Compound startArrow;
    builder.MakeCompound(startArrow);
    builder.Add(startArrow, shaftStart);
    builder.Add(startArrow, coneStart);
    
    m_startHandle = new AIS_Shape(startArrow);
    m_startHandle->SetLocalTransformation(trsf);
    m_context->SetColor(m_startHandle, Quantity_Color(Quantity_NOC_RED), false);
    m_context->SetMaterial(m_startHandle, Graphic3d_NOM_PLASTIC, false);
    m_context->Display(m_startHandle, false);

    // 2. 构建终点端手柄（指向正 X 方向拉伸，位于 currentLength 处）
    gp_Ax2 shaftAxEnd(gp_Pnt(0, 0, 0), gp_Dir(1, 0, 0));
    TopoDS_Shape shaftEnd = BRepPrimAPI_MakeCylinder(shaftAxEnd, rShaft, hShaft).Shape();
    
    gp_Ax2 coneAxEnd(gp_Pnt(hShaft, 0, 0), gp_Dir(1, 0, 0));
    TopoDS_Shape coneEnd = BRepPrimAPI_MakeCone(coneAxEnd, rCone, 0.0, hCone).Shape();
    
    TopoDS_Compound endArrow;
    builder.MakeCompound(endArrow);
    builder.Add(endArrow, shaftEnd);
    builder.Add(endArrow, coneEnd);
    
    gp_Trsf transEnd;
    transEnd.SetTranslation(gp_Vec(currentLength, 0, 0));
    gp_Trsf endGlobalTrsf = trsf * transEnd;
    
    m_lengthHandle = new AIS_Shape(endArrow);
    m_lengthHandle->SetLocalTransformation(endGlobalTrsf);
    m_context->SetColor(m_lengthHandle, Quantity_Color(Quantity_NOC_RED), false);
    m_context->SetMaterial(m_lengthHandle, Graphic3d_NOM_PLASTIC, false);
    m_context->Display(m_lengthHandle, false);
    
    m_context->UpdateCurrentViewer();
    this->update();
    
    qDebug() << "[OCCTWidget] showLengthHandle: Created professional stretch arrows. scale =" << arrowScale << ", nodeId =" << nodeId;
  } catch (...) {
    qDebug() << "[OCCTWidget] showLengthHandle: ERROR: Failed to create professional arrow handles!";
  }
}

void OCCTWidget::hideLengthHandle() {
  if (m_context.IsNull()) return;
  m_isDraggingLength = false;
  
  if (!m_startHandle.IsNull()) {
    m_context->Remove(m_startHandle, false);
    m_startHandle.Nullify();
  }
  if (!m_lengthHandle.IsNull()) {
    m_context->Remove(m_lengthHandle, false);
    m_lengthHandle.Nullify();
  }
  if (!m_dynamicPreview.IsNull()) {
    m_context->Remove(m_dynamicPreview, false);
    m_dynamicPreview.Nullify();
  }
  m_context->UpdateCurrentViewer();
}

void OCCTWidget::cancelLengthDragging() {
  if (!m_isDraggingLength) return;
  m_isDraggingLength = false;
  
  if (!m_dynamicPreview.IsNull()) {
    m_context->Remove(m_dynamicPreview, false);
    m_dynamicPreview.Nullify();
  }
  
  // 恢复显示两端操作小球
  if (!m_startHandle.IsNull()) {
    m_context->Display(m_startHandle, false);
  }
  if (!m_lengthHandle.IsNull()) {
    m_context->Display(m_lengthHandle, false);
  }
  
  m_context->UpdateCurrentViewer();
  this->update();
  qDebug() << "[OCCTWidget] cancelLengthDragging: Dragging canceled. Reverted handles.";
}

void OCCTWidget::keyPressEvent(QKeyEvent *event) {
  if (m_isDraggingLength && event->key() == Qt::Key_Escape) {
    cancelLengthDragging();
    event->accept();
    return;
  }
  QWidget::keyPressEvent(event);
}

QVariantMap OCCTWidget::findMetadataByNodeId(const QString& nodeId) const {
  for (auto it = m_objectMetadata.begin(); it != m_objectMetadata.end(); ++it) {
    if (it.value().contains("_adNodeId") && it.value()["_adNodeId"].toString() == nodeId) {
      return it.value();
    }
  }
  return QVariantMap();
}

