#ifndef ProjectManager_HeaderFile
#define ProjectManager_HeaderFile

#ifndef BR_EXPORT
#ifdef BR_EXPORT_EXPORTS
  #define BR_EXPORT __declspec(dllexport)
#else
  #define BR_EXPORT __declspec(dllimport)
#endif
#endif

#include <TDocStd_Application.hxx>
#include <TDocStd_Document.hxx>
#include <string>
#include <map>
#include <vector>

// 数据模型相关
#include "generated/DataModel.h"
#include "generated/BrNode_adDrawing2D.h"
#include "generated/BrNode_adSlopeIndication.h"
#include "generated/BrNode_adObject.h"

/**
 * @brief 多文档工程管理器类
 *
 * 职责:
 *   1. 管理主控文件 (*.cbf) 的加载与统一保存。
 *   2. 管理子文件 (3D模型、2D图纸) 的延迟加载 (Lazy Loading) 与物理隔离存盘。
 *   3. 实现 3D 模型与 2D 图纸之间的延迟同步机制 (Lazy Sync)。
 */
class BR_EXPORT ProjectManager
{
public:
    ProjectManager();
    ~ProjectManager();

    /**
     * @brief 打开主控工程文档
     * @param filepath 主控工程文件的绝对路径
     */
    Standard_Boolean OpenMasterProject(const std::string& filepath);

    /**
     * @brief 统一保存所有已打开的子文档以及主控文档
     */
    Standard_Boolean SaveAll();

    /**
     * @brief 延迟加载子文档
     * @param relativePath 相对于主控文档所在目录的子文档路径
     */
    Handle(TDocStd_Document) GetOrLoadSubDocument(const std::string& relativePath);

    /**
     * @brief 触发特定图纸的 2D 几何与 3D 物理模型的延迟同步检查
     * @param drawingNode 图纸业务节点
     */
    Standard_Boolean Sync2DDrawing(const Handle(BrNode_adDrawing2D)& drawingNode);

    /**
     * @brief 跨文档查找节点 (返回所有匹配该 ID 的节点，解决 OCAF Label 冲突)
     */
    std::vector<std::pair<Handle(ActAPI_INode), Handle(DataModel)>> FindNodesAcrossModels(const std::string& nodeId) const;

    /**
     * @brief 获取主控文档
     */
    Handle(TDocStd_Document) GetMasterDoc() const { return m_masterDoc; }

    /**
     * @brief 获取主控文档业务包装 DataModel
     */
    Handle(DataModel) GetMasterModel() const { return m_masterModel; }

    /**
     * @brief 获取子文档的业务包装 DataModel (已缓存)
     */
    Handle(DataModel) GetSubModel(const std::string& relativePath) const {
        auto it = m_loadedSubModels.find(relativePath);
        if (it != m_loadedSubModels.end()) return it->second;
        return nullptr;
    }

private:
    std::string ResolveAbsolutePath(const std::string& relativePath);

private:
    Handle(TDocStd_Application)                     m_app;
    Handle(TDocStd_Document)                        m_masterDoc;
    Handle(DataModel)                               m_masterModel;
    std::string                                     m_masterPath;

    // 内存中缓存已加载的子文档，Key 为相对路径
    std::map<std::string, Handle(TDocStd_Document)> m_loadedSubDocs;
    std::map<std::string, Handle(DataModel)>        m_loadedSubModels;
};

#endif
