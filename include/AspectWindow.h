/* This file implements the AspectWindow class that bridges Qt widgets
   and Open CASCADE's Aspect_Window */

#ifndef ASPECT_WINDOW_H
#define ASPECT_WINDOW_H

#include <Aspect_Window.hxx>
#include <Quantity_Color.hxx>

class QWidget;

class AspectWindow : public Aspect_Window
{
public:
    AspectWindow(QWidget *widget);
    ~AspectWindow();

    Aspect_Drawable NativeHandle() const final;
    Aspect_Drawable NativeParentHandle() const final;

    Aspect_TypeOfResize DoResize() final;

    Standard_Boolean IsMapped() const final;
    Standard_Boolean DoMapping() const final;
    void Map() const final;
    void Unmap() const final;

    void Position(Standard_Integer &theX1, Standard_Integer &theY1,
                  Standard_Integer &theX2, Standard_Integer &theY2) const final;
    Standard_Real Ratio() const final;
    void Size(Standard_Integer& theWidth, Standard_Integer& theHeight) const final;

    Aspect_FBConfig NativeFBConfig() const final;

private:
    QWidget *m_widget;
    mutable bool m_is_mapped;
};

#endif // ASPECT_WINDOW_H