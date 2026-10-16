#ifndef MODELEXPLORERPANEL_H
#define MODELEXPLORERPANEL_H

#include <QDockWidget>
#include <QTreeView>
#include <QStandardItemModel>
#include "DataModel.h"
#include "BrNode_adObject.h"
#include "UiTheme.h"

class ModelExplorerPanel : public QDockWidget {
    Q_OBJECT
public:
    explicit ModelExplorerPanel(QWidget* parent = nullptr);
    void setModel(Handle(DataModel) model);
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
    void populateModel(Handle(BrNode_adObject) node, QStandardItem* parentItem);

    QTreeView* m_treeView;
    QStandardItemModel* m_itemModel;
    Handle(DataModel) m_dataModel;
    QMap<QStandardItem*, Handle(BrNode_adObject)> m_nodeMap;
};

#endif
