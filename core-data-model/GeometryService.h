#ifndef BR_EXPORT
#ifdef BR_EXPORT_EXPORTS
  #define BR_EXPORT __declspec(dllexport)
#else
  #define BR_EXPORT __declspec(dllimport)
#endif
#endif

#ifndef GeometryService_HeaderFile
#define GeometryService_HeaderFile

#include <string>
#include <TCollection_AsciiString.hxx>
#include <TCollection_ExtendedString.hxx>
#include <TopoDS_Shape.hxx>
#include <Standard_Handle.hxx>
#include <nlohmann/json.hpp>
#include <TDF_Label.hxx>
#include <unordered_map>

// MDA Generated Classes
#include "generated/DataModel.h"
#include "generated/BrNode_adObject.h"
#include "generated/BrNode_adGeometricDef.h"
#include "generated/BrNode_adGeometry.h"
#include "generated/BrNode_adPropertySet.h"
#include "generated/BrNode_adProperty.h"

using json = nlohmann::json;

/**
 * @brief 几何建模服务类
 *
 * 职责:
 *   1. 从 adObject 的几何属性集中提取建模参数
 *   2. 生成参数指纹 (ParamGeoID = MD5)，实现几何缓存
 *   3. 调用 scripts-service 建模服务获取 BREP 几何
 *   4. 将结果保存为 adGeometricDef (几何分区)
 *   5. 将 scripts-service 返回的 inout/out 参数回写到属性集
 *
 * 设计原则:
 *   - 通过 HTTP REST 调用 scripts-service，便于以后迁移为独立微服务
 *   - 所有建模参数和返回参数以 JSON 键值对存储在 GeoParameter 中
 *   - 相同 ParamGeoID 的几何不重复建模，直接返回缓存
 */
class BR_EXPORT GeometryService
{
public:
    /**
     * @brief 构造函数
     * @param model      数据模型（用于访问几何分区和创建节点）
     * @param serviceUrl scripts-service 的基础 URL (默认本地)
     */
    GeometryService(const Handle(DataModel)& model,
                    const std::string& serviceUrl = "http://127.0.0.1:8000");

    /**
     * @brief 为指定 adObject 创建/获取几何
     *
     * 流程:
     *   1. 从 adObject 的几何属性集 (Pset_*Geometry) 中提取参数
     *   2. 计算 ParamGeoID = MD5(ObjectType + 参数JSON)
     *   3. 在几何分区中查找已有的 adGeometricDef
     *   4. 若命中缓存，直接返回
     *   5. 若未命中，调用 scripts-service 建模
     *   6. 保存 BREP 到 adGeometricDef.Shape
     *   7. 回写 inout/out 参数到属性集
     *
     * @param adObj  目标业务对象
     * @return 几何定义节点（含 Shape），失败返回 Null Handle
     */
    TDF_Label BuildGeometry(const Handle(BrNode_adObject)& adObj);

    /**
     * @brief 用显式 JSON 参数调用建模服务
     *
     * 适用于不通过 adObject 而直接指定参数的场景。
     *
     * @param modelType  模型类型名 (如 "Girder", "PierBody")
     * @param params     建模参数 (JSON 键值对)
     * @return { "shape": TopoDS_Shape, "params": json, "paramGeoId": string }
     */
    struct BuildResult {
        TopoDS_Shape shape;
        json         allParams;   // input + inout + out 全量参数
        std::string  paramGeoId;  // MD5 指纹
        bool         fromCache;
    };
    BuildResult BuildGeometryFromParams(const std::string& modelType,
                                        const json& inputParams);

private:
    /**
     * @brief 从 adObject 的属性集中提取几何参数 JSON
     *
     * 查找名称含 "Geometry" 的 Pset，提取其所有 Property 为 JSON。
     * 同时提取 ModelNumber 作为型号标识。
     */
    struct ExtractedParams {
        std::string modelType;    // ObjectType (如 "Girder")
        std::string modelNumber;  // 型号 (如 "GIRD-32")
        json        params;       // { "Length": 31500.0, ... }
        Handle(BrNode_adPropertySet) geoPset; // 几何属性集引用（用于回写）
    };
    ExtractedParams ExtractGeoParams(const Handle(BrNode_adObject)& adObj);

    /**
     * @brief 计算参数指纹
     * @param modelType 模型类型
     * @param params    参数 JSON
     * @return MD5 hex string
     */
    static std::string ComputeParamGeoID(const std::string& modelType,
                                          const json& params);

    /**
     * @brief 调用 scripts-service REST API 进行建模
     * @param modelType  模型类型
     * @param params     建模参数
     * @return { allParams, brepData }
     */
    struct ServiceResult {
        json        allParams;  // scripts-service 返回的完整参数
        std::string brepData;   // BREP 格式的几何数据
        bool        success;
        std::string error;
    };
    ServiceResult CallModelingService(const std::string& modelType,
                                      const json& params);

    /**
     * @brief 从 BREP 字符串解析 TopoDS_Shape
     */
    static TopoDS_Shape ParseBREP(const std::string& brepData);

    /**
     * @brief 将 inout/out 参数回写到属性集
     */
    void WriteBackParams(const Handle(BrNode_adPropertySet)& geoPset,
                         const json& returnedParams);

    /**
     * @brief [重构] 为业务对象初始化必要的 ActiveData 结构 (Psets)
     * @param adObj 目标对象
     * @param type  模型类型名
     */
    static void InitializeObject(const Handle(BrNode_adObject)& adObj, const std::string& type);

private:
    Handle(DataModel) m_model;
    Handle(TDocStd_Document) m_xcafDoc;
    std::string       m_serviceUrl;
    
    std::unordered_map<std::string, TDF_Label> m_cacheMap;
    
    Handle(BrNode_adGeometricDef) FindCachedGeoDef(const std::string& paramGeoId);
    Handle(BrNode_adGeometricDef) CreateGeoDef(const std::string& paramGeoId,
                                                const json& allParams,
                                                const TopoDS_Shape& shape);
    
    void InitializeCacheMap();
    TDF_Label ImportAndMergeCbf(const std::string& cbfByteStream, const std::string& paramGeoId);
};

#endif
