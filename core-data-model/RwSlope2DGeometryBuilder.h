#ifndef RwSlope2DGeometryBuilder_HeaderFile
#define RwSlope2DGeometryBuilder_HeaderFile

#include "RwBuilder.h"
#include <TopoDS_Wire.hxx>
#include <TopoDS_Shape.hxx>

/**
 * @brief Builder for railway subgrade slope indication line geometry and XDE storage
 */
class RwSlope2DGeometryBuilder : public RwBuilder
{
public:
    /**
     * @brief Constructor
     * @param shoulder   Shoulder feature wire
     * @param toe        Toe feature wire
     * @param spacing    Spacing of slope teeth indication (mm)
     * @param longRatio  Ratio of long teeth length relative to shoulder-to-toe actual distance (0.0~1.0)
     * @param shortRatio Ratio of short teeth length relative to shoulder-to-toe actual distance (0.0~1.0)
     */
    BR_EXPORT RwSlope2DGeometryBuilder(const TopoDS_Wire& shoulder,
                             const TopoDS_Wire& toe,
                             Standard_Real spacing = 2000.0,
                             Standard_Real longRatio = 0.6,
                             Standard_Real shortRatio = 0.3);
    
    BR_EXPORT virtual ~RwSlope2DGeometryBuilder();

    /**
     * @brief Build geometry of slope indication (includes shoulder, toe, and teeth)
     * @return TopoDS_Compound containing all generated curves
     */
    BR_EXPORT virtual TopoDS_Shape Build() override;

    /**
     * @brief Decompose geometry and map to XCAF layers/colors
     * @param doc Target drawing XCAF document
     * @return Standard_True on success
     */
    BR_EXPORT virtual Standard_Boolean SaveToXDE(Handle(TDocStd_Document)& doc) override;

    /**
     * @brief Get only the generated teeth compound
     */
    const TopoDS_Shape& GetTeethCompound() const { return m_teethCompound; }

    // RTTI registration
    DEFINE_STANDARD_RTTIEXT(RwSlope2DGeometryBuilder, RwBuilder)

private:
    TopoDS_Wire   m_shoulderWire;
    TopoDS_Wire   m_toeWire;
    Standard_Real m_spacing;
    Standard_Real m_longRatio;
    Standard_Real m_shortRatio;

    // Cache of generated teeth compound for SaveToXDE
    TopoDS_Shape  m_teethCompound;
};

#endif
