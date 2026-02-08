#include "../include/Line.h"
#include <AIS_Line.hxx>
#include <Prs3d_LineAspect.hxx>
#include <Graphic3d_AspectLine3d.hxx>
#include <Geom_CartesianPoint.hxx>

Line::Line(const gp_Pnt& start, const gp_Pnt& end)
    : m_start(start)
    , m_end(end)
{
    // Create edge from the two points
    TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(start, end);

    // Create AIS_Shape for visualization
    m_aisShape = new AIS_Shape(edge);
}

void Line::setColor(const Quantity_Color& color)
{
    // For now, changing color is handled by the AIS context that contains the shape
    // In practice, you would have a reference to the context that displays this shape
}