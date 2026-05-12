#ifndef MODELEXPLORERPANEL_H
#define MODELEXPLORERPANEL_H

#include <QDockWidget>
#include <QTreeView>
#include <QStandardItemModel>
#include "DataModel.h"
#include "BrNode_adObject.h"

class ModelExplorerPanel : public QDockWidget {
    Q_OBJECT
public:
    explicit ModelExplorerPanel(QWidget* parent = nullptr);
    void setModel(Handle(DataModel) model);

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
