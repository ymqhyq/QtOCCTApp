#include "RwSlope2DGeometryBuilder.h"

// OCCT 几何与拓扑分析
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

// OCCT XCAF / XDE 工具
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFDoc_LayerTool.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <Quantity_Color.hxx>
#include <TDataStd_Name.hxx>

IMPLEMENT_STANDARD_RTTIEXT(RwSlope2DGeometryBuilder, RwBuilder)

RwSlope2DGeometryBuilder::RwSlope2DGeometryBuilder(const TopoDS_Wire& shoulder,
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
    BRep_Builder builder;
    builder.MakeCompound(TopoDS::Compound(m_teethCompound));
}

RwSlope2DGeometryBuilder::~RwSlope2DGeometryBuilder()
{
}

TopoDS_Shape RwSlope2DGeometryBuilder::Build()
{
    BRep_Builder builder;
    TopoDS_Compound totalCompound;
    builder.MakeCompound(totalCompound);

    // 1. 保留路肩特征线作为实线
    builder.Add(totalCompound, m_shoulderWire);

    // 2. 清空并重建示坡齿线 Compound
    TopoDS_Compound teethComp;
    builder.MakeCompound(teethComp);

    // 3. 对路肩线进行等距离散
    if (!m_shoulderWire.IsNull() && !m_toeWire.IsNull())
    {
        try {
            // 使用 BRepAdaptor_CompCurve 处理可能包含多段 Edge 的 Wire
            BRepAdaptor_CompCurve adaptor(m_shoulderWire);
            Standard_Real first = adaptor.FirstParameter();
            Standard_Real last = adaptor.LastParameter();

            // GCPnts_UniformAbscissa 按等弧长参数化离散采样点
            GCPnts_UniformAbscissa spacingPoints(adaptor, m_spacing);
            if (spacingPoints.IsDone() && spacingPoints.NbPoints() >= 1)
            {
                for (Standard_Integer i = 1; i <= spacingPoints.NbPoints(); ++i)
                {
                    Standard_Real t = spacingPoints.Parameter(i);
                    gp_Pnt pShoulder;
                    gp_Vec tangent;
                    adaptor.D1(t, pShoulder, tangent); // 计算坐标与切线

                    if (tangent.SquareMagnitude() < 1e-6) {
                        tangent = gp_Vec(1.0, 0.0, 0.0); // 退化处理
                    }

                    // 4. 2D 垂直方向计算 (在 XY 平面中)
                    gp_Dir normalDir(-tangent.Y(), tangent.X(), 0.0);

                    // 5. 垂线与坡脚线求交 (建立长达 1000m 的射线段)
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
                        if (dist > 10.0) // 间距大于 10mm 时才绘制
                        {
                            // 奇数项为长齿线，偶数项为短齿线
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
            std::cerr << "[RwSlope2DGeometryBuilder] Exception occurred during spacing discretization." << std::endl;
        }
    }

    m_teethCompound = teethComp;
    builder.Add(totalCompound, m_teethCompound);

    // 6. 保留坡脚特征线
    builder.Add(totalCompound, m_toeWire);

    return totalCompound;
}

Standard_Boolean RwSlope2DGeometryBuilder::SaveToXDE(Handle(TDocStd_Document)& doc)
{
    if (doc.IsNull()) return Standard_False;

    // 1. 获取 XCAF Tools
    TDF_Label mainLabel = doc->Main();
    Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(mainLabel);
    Handle(XCAFDoc_LayerTool) layerTool = XCAFDoc_DocumentTool::LayerTool(mainLabel);
    Handle(XCAFDoc_ColorTool) colorTool = XCAFDoc_DocumentTool::ColorTool(mainLabel);

    // 2. 注册并绑定路肩线 (实线，蓝色)
    if (!m_shoulderWire.IsNull()) {
        TDF_Label shoulderLabel = shapeTool->AddShape(m_shoulderWire, Standard_False);
        TDataStd_Name::Set(shoulderLabel, TCollection_ExtendedString("RoadShoulderLine"));
        layerTool->SetLayer(shoulderLabel, TCollection_ExtendedString("Layer_RoadShoulder"));
        colorTool->SetColor(shoulderLabel, Quantity_Color(0.0, 0.4, 0.8, Quantity_TOC_RGB), XCAFDoc_ColorCurv);
    }

    // 3. 注册并绑定坡脚线 (虚线，红色)
    if (!m_toeWire.IsNull()) {
        TDF_Label toeLabel = shapeTool->AddShape(m_toeWire, Standard_False);
        TDataStd_Name::Set(toeLabel, TCollection_ExtendedString("SlopeToeLine"));
        // 前端渲染器或导出器识别 "_Dashed" 后缀以渲染为虚线
        layerTool->SetLayer(toeLabel, TCollection_ExtendedString("Layer_SlopeToe_Dashed"));
        colorTool->SetColor(toeLabel, Quantity_Color(0.8, 0.0, 0.0, Quantity_TOC_RGB), XCAFDoc_ColorCurv);
    }

    // 4. 注册并绑定齿线 (细线，灰色)
    if (!m_teethCompound.IsNull()) {
        TDF_Label teethLabel = shapeTool->AddShape(m_teethCompound, Standard_False);
        TDataStd_Name::Set(teethLabel, TCollection_ExtendedString("SlopeTeeth"));
        layerTool->SetLayer(teethLabel, TCollection_ExtendedString("Layer_SlopeTeeth"));
        colorTool->SetColor(teethLabel, Quantity_Color(0.5, 0.5, 0.5, Quantity_TOC_RGB), XCAFDoc_ColorCurv);
    }

    return Standard_True;
}
