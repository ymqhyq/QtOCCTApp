#include "RwSlopeGeometryBuilder.h"

#include <BRepAdaptor_CompCurve.hxx>
#include <GCPnts_UniformAbscissa.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS.hxx>
#include <gp_Vec.hxx>
#include <gp_Dir.hxx>
#include <gp_Lin.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <TopExp.hxx>
#include <BRep_Tool.hxx>
#include <TopoDS_Vertex.hxx>

#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFDoc_LayerTool.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <Quantity_Color.hxx>
#include <TDataStd_Name.hxx>

IMPLEMENT_STANDARD_RTTIEXT(RwSlopeGeometryBuilder, RwBuilder)

RwSlopeGeometryBuilder::RwSlopeGeometryBuilder(const TopoDS_Wire& shoulder,
                                                   const TopoDS_Wire& toe,
                                                   Standard_Real spacing,
                                                   Standard_Real longRatio,
                                                   Standard_Real shortRatio)
    : m_shoulderWire(shoulder),
      m_toeWire(toe),
      m_spacing(spacing),
      m_longRatio(longRatio),
      m_shortRatio(shortRatio)
{
}

RwSlopeGeometryBuilder::~RwSlopeGeometryBuilder()
{
}

TopoDS_Shape RwSlopeGeometryBuilder::Build(RepresentationType repType)
{
    switch (repType) {
        case Rep_2D_Plan:
            return Build2DPlanIndication();
        case Rep_3D_Solid:
            return Build3DSolidWedge();
        case Rep_2D_Profile:
            return TopoDS_Shape();
        default:
            return TopoDS_Shape();
    }
}

TopoDS_Shape RwSlopeGeometryBuilder::Build3DSolidWedge()
{
    std::cout << "[DEBUG] Build3DSolidWedge - Started" << std::endl;
    if (m_shoulderWire.IsNull() || m_toeWire.IsNull()) {
        std::cout << "[DEBUG] Build3DSolidWedge - Wires are null" << std::endl;
        return TopoDS_Shape();
    }

    try {
        TopoDS_Vertex vS1, vS2, vT1, vT2;
        TopExp::Vertices(m_shoulderWire, vS1, vS2);
        TopExp::Vertices(m_toeWire, vT1, vT2);

        if (vS1.IsNull() || vS2.IsNull() || vT1.IsNull() || vT2.IsNull()) {
            std::cout << "[DEBUG] Build3DSolidWedge - Extracted vertices are null" << std::endl;
            return TopoDS_Shape();
        }

        gp_Pnt pS1 = BRep_Tool::Pnt(vS1);
        gp_Pnt pS2 = BRep_Tool::Pnt(vS2);
        gp_Pnt pT1 = BRep_Tool::Pnt(vT1);
        gp_Pnt pT2 = BRep_Tool::Pnt(vT2);

        if (pS1.Distance(pT2) < pS1.Distance(pT1)) {
            pT1 = pT2;
        }

        gp_Pnt pBase(pS1.X(), pS1.Y(), pT1.Z()); 

        BRepBuilderAPI_MakePolygon poly;
        poly.Add(pS1);
        poly.Add(pBase);
        poly.Add(pT1);
        poly.Close();

        TopoDS_Face profileFace = BRepBuilderAPI_MakeFace(poly.Wire());

        gp_Vec extrudeVec(pS1, pS2);
        if (extrudeVec.Magnitude() < 1e-6) {
            std::cout << "[DEBUG] Build3DSolidWedge - Extrude vector is too short" << std::endl;
            return TopoDS_Shape();
        }

        TopoDS_Shape wedgeSolid = BRepPrimAPI_MakePrism(profileFace, extrudeVec);
        std::cout << "[DEBUG] Build3DSolidWedge - Finished successfully" << std::endl;
        return wedgeSolid;
    } catch (...) {
        std::cout << "[DEBUG] Build3DSolidWedge - Exception caught" << std::endl;
        return TopoDS_Shape();
    }
}

TopoDS_Shape RwSlopeGeometryBuilder::Build2DPlanIndication()
{
    BRep_Builder builder;
    TopoDS_Compound totalCompound;
    builder.MakeCompound(totalCompound);

    if (!m_shoulderWire.IsNull()) {
        builder.Add(totalCompound, m_shoulderWire);
    }

    TopoDS_Compound teethComp;
    builder.MakeCompound(teethComp);

    if (!m_shoulderWire.IsNull() && !m_toeWire.IsNull())
    {
        try {
            BRepAdaptor_CompCurve adaptor(m_shoulderWire);
            Standard_Real first = adaptor.FirstParameter();
            Standard_Real last = adaptor.LastParameter();

            GCPnts_UniformAbscissa spacingPoints(adaptor, m_spacing);
            if (spacingPoints.IsDone() && spacingPoints.NbPoints() >= 1)
            {
                for (Standard_Integer i = 1; i <= spacingPoints.NbPoints(); ++i)
                {
                    Standard_Real t = spacingPoints.Parameter(i);
                    gp_Pnt pShoulder;
                    gp_Vec tangent;
                    adaptor.D1(t, pShoulder, tangent);
                    if (tangent.SquareMagnitude() < 1e-6) {
                        tangent = gp_Vec(1.0, 0.0, 0.0);
                    }

                    gp_Dir normalDir(-tangent.Y(), tangent.X(), 0.0);

                    gp_Pnt pProjEnd = pShoulder.Translated(gp_Vec(normalDir) * 1000000.0);
                    TopoDS_Edge normalEdge = BRepBuilderAPI_MakeEdge(pShoulder, pProjEnd).Edge();

                    BRepExtrema_DistShapeShape extrema(normalEdge, m_toeWire);
                    gp_Pnt pProjected = pShoulder;
                    bool hasIntersection = false;

                    if (extrema.IsDone() && extrema.NbSolution() > 0)
                    {
                        pProjected = extrema.PointOnShape2(1);
                        hasIntersection = true;
                    }

                    if (hasIntersection)
                    {
                        gp_Vec vectorSP(pShoulder, pProjected);
                        double dist = vectorSP.Magnitude();
                        if (dist > 10.0) 
                        {
                            double ratio = (i % 2 == 1) ? m_longRatio : m_shortRatio;
                            gp_Pnt pEnd = pShoulder.Translated(vectorSP.Normalized() * (dist * ratio));

                            TopoDS_Edge teethEdge = BRepBuilderAPI_MakeEdge(pShoulder, pEnd).Edge();
                            builder.Add(teethComp, teethEdge);
                        }
                    }
                }
            }
        }
        catch (...) {
            // catch error
        }
    }

    builder.Add(totalCompound, teethComp);

    if (!m_toeWire.IsNull()) {
        builder.Add(totalCompound, m_toeWire);
    }

    return totalCompound;
}

Standard_Boolean RwSlopeGeometryBuilder::SaveToXDE(Handle(TDocStd_Document)& doc, RepresentationType repType, const TopoDS_Shape& shape)
{
    std::cout << "[DEBUG] SaveToXDE - Started" << std::endl;
    if (doc.IsNull() || shape.IsNull()) {
        std::cout << "[DEBUG] SaveToXDE - Document or shape is null" << std::endl;
        return Standard_False;
    }

    try {
        TDF_Label mainLabel = doc->Main();
        Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(mainLabel);
        Handle(XCAFDoc_LayerTool) layerTool = XCAFDoc_DocumentTool::LayerTool(mainLabel);
        Handle(XCAFDoc_ColorTool) colorTool = XCAFDoc_DocumentTool::ColorTool(mainLabel);

        if (shapeTool.IsNull() || layerTool.IsNull() || colorTool.IsNull()) {
            std::cout << "[DEBUG] SaveToXDE - Could not retrieve XCAF tools!" << std::endl;
            return Standard_False;
        }

        TDF_Label shapeLabel = shapeTool->AddShape(shape, Standard_False);
        if (shapeLabel.IsNull()) {
            std::cout << "[DEBUG] SaveToXDE - Failed to AddShape" << std::endl;
            return Standard_False;
        }

        if (repType == Rep_2D_Plan) {
            TDataStd_Name::Set(shapeLabel, TCollection_ExtendedString("Slope2DPlan"));
            layerTool->SetLayer(shapeLabel, TCollection_ExtendedString("Layer_Slope2D"));
            colorTool->SetColor(shapeLabel, Quantity_Color(0.2, 0.2, 0.2, Quantity_TOC_RGB), XCAFDoc_ColorGen);
        } 
        else if (repType == Rep_3D_Solid) {
            TDataStd_Name::Set(shapeLabel, TCollection_ExtendedString("Slope3DSolid"));
            layerTool->SetLayer(shapeLabel, TCollection_ExtendedString("Layer_Slope3D"));
            colorTool->SetColor(shapeLabel, Quantity_Color(0.6, 0.6, 0.5, Quantity_TOC_RGB), XCAFDoc_ColorSurf);
        }

        std::cout << "[DEBUG] SaveToXDE - Finished successfully" << std::endl;
        return Standard_True;
    } catch (...) {
        std::cout << "[DEBUG] SaveToXDE - Exception caught" << std::endl;
        return Standard_False;
    }
}
