#ifndef MODELEXPLORERPANEL_H
#define MODELEXPLORERPANEL_H

#include <QDockWidget>
#include <QTreeView>
#include <QStandardItemModel>
#include "DataModel.h"
#include "BrNode_adObject.h"
#include "BrNode_adSubDocRef.h"
#include "UiTheme.h"

class ModelExplorerPanel : public QDockWidget {
    Q_OBJECT
public:
    explicit ModelExplorerPanel(QWidget* parent = nullptr);
    void setModel(Handle(DataModel) model);
    /**
     * @brief 把某个子文档里的对象挂进模型树
     *
     * “主控 + 子文档”形态的工程里，主控只保存子文档引用（adSubDocRef），
     * 真正的业务对象在子文档中；本方法把子文档对象挂到对应的引用节点下。
     * @param title    子文档标题（一般为文件名，用于匹配引用节点）
     * @param subModel 已加载的子文档模型
     */
    void addSubDocument(const QString& title, Handle(DataModel) subModel);
    /// 应用界面配色（切换主题时调用）
    void setUiPalette(const UiPalette& palette);
    /**
     * @brief 按节点标识选中树中的对应项（供视口点选时反向联动）
     * @param nodeId 节点的 GlobalID 或自增 ID
     * @return 找到并选中返回 true
     */
    bool selectNodeById(const QString& nodeId);

signals:
    void nodeSelected(Handle(BrNode_adObject) node);

private slots:
    void onItemClicked(const QModelIndex& index);

private:
    /// 为任意节点创建树项（名称/类型由节点自身决定），并登记到 m_nodeMap
    QStandardItem* appendNode(Handle(ActAPI_INode) node, QStandardItem* parentItem);
    /**
     * @brief 递归挂载一个节点及其子对象
     * @param seen 已挂载节点的条目 ID 集合（避免层级与分区两条路径重复添加）
     */
    void populateNode(Handle(ActAPI_INode) node, QStandardItem* parentItem,
                      QSet<QString>* seen);
    /// 遍历某个模型的根节点子级
    int populateRootChildren(Handle(DataModel) model, QStandardItem* parentItem,
                            QSet<QString>* seen);
    /// 遍历 Topology 分区（子文档对象常挂在分区里，而非根节点子级）
    int populateTopologyPartition(Handle(DataModel) model, QStandardItem* parentItem,
                                 QSet<QString>* seen);

    QTreeView* m_treeView;
    QStandardItemModel* m_itemModel;
    Handle(DataModel) m_dataModel;
    QMap<QStandardItem*, Handle(ActAPI_INode)> m_nodeMap;
    QMap<QStandardItem*, Handle(BrNode_adSubDocRef)> m_subDocRefItems;
};

#endif
