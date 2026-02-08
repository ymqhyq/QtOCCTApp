#include "../include/AspectWindow.h"
#include <QWidget>

AspectWindow::AspectWindow(QWidget *widget)
    : Aspect_Window(), m_widget(widget), m_is_mapped(false) {
  SetBackground(Quantity_NOC_WHITE);
}

AspectWindow::~AspectWindow() {}

Aspect_Drawable AspectWindow::NativeHandle() const {
  return (Aspect_Drawable)(m_widget->winId());
}

Aspect_Drawable AspectWindow::NativeParentHandle() const {
  WId parentId = 0;
  const QWidget *const parent = m_widget->parentWidget();
  if (parent)
    parentId = parent->winId();
  return (Aspect_Drawable)parentId;
}

Aspect_TypeOfResize AspectWindow::DoResize() {
  // 返回合适的调整类型
  return Aspect_TOR_UNKNOWN;
}

Standard_Boolean AspectWindow::IsMapped() const {
  return m_is_mapped && !(m_widget->isMinimized() || m_widget->isHidden());
}

Standard_Boolean AspectWindow::DoMapping() const { return true; }

void AspectWindow::Map() const {
  m_widget->show();
  m_widget->update();
  m_is_mapped = true;
}

void AspectWindow::Unmap() const {
  m_widget->hide();
  m_widget->update();
  m_is_mapped = false;
}

void AspectWindow::Position(Standard_Integer &theX1, Standard_Integer &theY1,
                            Standard_Integer &theX2,
                            Standard_Integer &theY2) const {
  QRect geometry = m_widget->geometry();
  theX1 = geometry.left();
  theX2 = geometry.right();
  theY1 = geometry.top();
  theY2 = geometry.bottom();
}

Standard_Real AspectWindow::Ratio() const {
  QRectF geometry = m_widget->geometry();
  if (geometry.height() != 0) {
    return static_cast<Standard_Real>((geometry.right() - geometry.left()) /
                                      (geometry.bottom() - geometry.top()));
  } else {
    return 1.0; // 默认宽高比
  }
}

void AspectWindow::Size(Standard_Integer &theWidth,
                        Standard_Integer &theHeight) const {
  qreal pixelRatio = m_widget->devicePixelRatio();
  theWidth = static_cast<Standard_Integer>(m_widget->width() * pixelRatio);
  theHeight = static_cast<Standard_Integer>(m_widget->height() * pixelRatio);
}

Aspect_FBConfig AspectWindow::NativeFBConfig() const { return nullptr; }