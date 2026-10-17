#ifndef NodeIdentity_HeaderFile
#define NodeIdentity_HeaderFile

#ifndef BR_EXPORT
#ifdef BR_EXPORT_EXPORTS
  #define BR_EXPORT __declspec(dllexport)
#else
  #define BR_EXPORT __declspec(dllimport)
#endif
#endif

#include <Standard_Handle.hxx>
#include <TCollection_ExtendedString.hxx>

#include <vector>

#include "generated/DataModel.h"

/**
 * @brief 节点稳定标识（GUID）工具
 *
 * 多文档设计里，2D/3D 软链接、XCAF 标签绑定、跨文档查找都以节点 GUID
 * （PID_GlobalID）为准。历史工程数据里该属性为空，这里提供统一的补号能力；
 * 业务侧仍可退化为 OCAF 条目 ID，但 GUID 是首选身份。
 */
namespace NodeIdentity {

/// 生成一个新的 GUID 字符串（32 位十六进制，无连字符）
BR_EXPORT TCollection_ExtendedString NewGuid();

/**
 * @brief 为模型内缺失 GlobalID 的节点补号
 *
 * 只在属性为空时写入，因此可重复调用（幂等）。补号结果随文档保存而持久化。
 * @return 实际补号数量
 */
BR_EXPORT int EnsureGlobalIds(const Handle(DataModel) &model);

/**
 * @brief 统一 GUID 格式：把非 32 位十六进制的 GUID 替换为标准格式
 *
 * 迁移会同步更新引用旧 GUID 的字符串属性（adSlopeIndication::TargetObjectID、
 * 图纸 XCAF 标签上的 AsciiString），因此必须把主控与全部相关子文档一起传入，
 * 否则跨文档引用会失效。
 *
 * @return 发生变化的节点数量
 */
BR_EXPORT int NormalizeGlobalIds(const std::vector<Handle(DataModel)> &models);

} // namespace NodeIdentity

#endif // NodeIdentity_HeaderFile
