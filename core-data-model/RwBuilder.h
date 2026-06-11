#ifndef RwBuilder_HeaderFile
#define RwBuilder_HeaderFile

#ifndef BR_EXPORT
#ifdef BR_EXPORT_EXPORTS
  #define BR_EXPORT __declspec(dllexport)
#else
  #define BR_EXPORT __declspec(dllimport)
#endif
#endif

#include <Standard_Transient.hxx>
#include <TDocStd_Document.hxx>
#include <TopoDS_Shape.hxx>

/**
 * @brief Base class for railway geometry and annotation builders
 */
class RwBuilder : public Standard_Transient
{
public:
    BR_EXPORT RwBuilder();
    BR_EXPORT virtual ~RwBuilder();

    enum RepresentationType {
        Rep_3D_Solid,
        Rep_2D_Plan,
        Rep_2D_Profile
    };

    /**
     * @brief Build geometric shape
     */
    BR_EXPORT virtual TopoDS_Shape Build(RepresentationType repType) = 0;

    /**
     * @brief Save generated geometry to sub-document XCAF layers
     */
    BR_EXPORT virtual Standard_Boolean SaveToXDE(Handle(TDocStd_Document)& doc, RepresentationType repType, const TopoDS_Shape& shape) = 0;

    // OCCT RTTI
    DEFINE_STANDARD_RTTIEXT(RwBuilder, Standard_Transient)
};

#endif
