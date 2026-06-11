#ifndef SceneDataExtractor_HeaderFile
#define SceneDataExtractor_HeaderFile

#ifndef BR_EXPORT
#ifdef BR_EXPORT_EXPORTS
  #define BR_EXPORT __declspec(dllexport)
#else
  #define BR_EXPORT __declspec(dllimport)
#endif
#endif

#include <vector>
#include <string>
#include <TopoDS_Shape.hxx>
#include <gp_Trsf.hxx>
#include <nlohmann/json.hpp>
#include <Standard_Handle.hxx>

#include "generated/DataModel.h"
#include "generated/BrNode_adObject.h"

using json = nlohmann::json;

/**
 * @brief 场景数据提取器
 *
 * 职责:
 *   1. 遍历 DataModel 中的业务对象树。
 *   2. 从各个对象关联的 BrNode_adGeometry / BrNode_adGeometricDef 中直接读取已持久化的 TopoDS_Shape。
 *   3. 读取对象的 ObjectPlacement 获得局部与全局变换矩阵 (gp_Trsf)。
 *   4. 读取对象的属性集 (Pset) 转换为元数据，供前端 PBR 渲染和属性展示使用。
 *   5. 将结果封装为 VisualShape 输出，供前端界面直接渲染。
 * 
 * 此类仅执行读取逻辑，绝不会调用后台脚本或修改/重构现有的几何。
 */
class BR_EXPORT SceneDataExtractor
{
public:
    struct VisualShape {
        TopoDS_Shape shape;
        std::string  name;
        json         metadata;
        gp_Trsf      transform;
    };

    /**
     * @brief 提取指定模型中的所有顶层对象及其子对象的场景渲染数据
     * @param model 目标数据模型
     * @param outShapes 提取结果数组
     */
    static void Extract(const Handle(DataModel)& model, std::vector<VisualShape>& outShapes);

    /**
     * @brief 递归提取特定对象及其子对象的渲染数据
     * @param rootObj 起始根对象
     * @param outShapes 提取结果数组
     * @param parentTrsf 父级变换矩阵
     */
    static void ExtractFromObject(const Handle(BrNode_adObject)& rootObj,
                                  std::vector<VisualShape>& outShapes,
                                  const gp_Trsf& parentTrsf = gp_Trsf());
};

#endif
