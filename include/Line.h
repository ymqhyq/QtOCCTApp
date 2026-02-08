#ifndef LINE_H
#define LINE_H

#include <Geom_Line.hxx>
#include <AIS_Shape.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <TopoDS_Edge.hxx>

class Line
{
public:
    Line(const gp_Pnt& start, const gp_Pnt& end);
    
    const Handle(AIS_Shape)& getAisShape() const { return m_aisShape; }
    void setColor(const Quantity_Color& color);
    
private:
    gp_Pnt m_start;
    gp_Pnt m_end;
    Handle(AIS_Shape) m_aisShape;
};

#endif // LINE_H