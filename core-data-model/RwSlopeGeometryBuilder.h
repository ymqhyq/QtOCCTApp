#ifndef RwSlopeGeometryBuilder_HeaderFile
#define RwSlopeGeometryBuilder_HeaderFile

#include "RwBuilder.h"
#include <TopoDS_Wire.hxx>
#include <TopoDS_Shape.hxx>

/**
 * @brief Builder for railway subgrade slope indication line geometry and XDE storage
 */
class RwSlopeGeometryBuilder : public RwBuilder
{
public:
    BR_EXPORT RwSlopeGeometryBuilder(const TopoDS_Wire& shoulder,
                             const TopoDS_Wire& toe,
                             Standard_Real spacing = 2000.0,
                             Standard_Real longRatio = 0.6,
                             Standard_Real shortRatio = 0.3);
    
    BR_EXPORT virtual ~RwSlopeGeometryBuilder();

    BR_EXPORT virtual TopoDS_Shape Build(RepresentationType repType) override;

    BR_EXPORT virtual Standard_Boolean SaveToXDE(Handle(TDocStd_Document)& doc, RepresentationType repType, const TopoDS_Shape& shape) override;

    DEFINE_STANDARD_RTTIEXT(RwSlopeGeometryBuilder, RwBuilder)

private:
    TopoDS_Shape Build2DPlanIndication();
    TopoDS_Shape Build3DSolidWedge();

private:
    TopoDS_Wire   m_shoulderWire;
    TopoDS_Wire   m_toeWire;
    Standard_Real m_spacing;
    Standard_Real m_longRatio;
    Standard_Real m_shortRatio;
};

#endif
