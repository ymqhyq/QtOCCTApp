#include "../include/ModelExplorerPanel.h"
#include "../include/TextEncoding.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <TCollection_AsciiString.hxx>
#include <NCollection_Sequence.hxx>
#include <vector>

ModelExplorerPanel::ModelExplorerPanel(QWidget* parent) : QDockWidget("Model Explorer", parent) {
    setObjectName("ModelExplorerPanel");
    m_treeView = new QTreeView(this);
    m_itemModel = new QStandardItemModel(this);
    m_itemModel->setHorizontalHeaderLabels({"Model Tree"});
    
    m_treeView->setModel(m_itemModel);
    m_treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_treeView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeView->header()->setStretchLastSection(true);

    connect(m_treeView, &QTreeView::clicked, this, &ModelExplorerPanel::onItemClicked);

    setWidget(m_treeView);
}

void ModelExplorerPanel::setUiPalette(const UiPalette& palette) {
    setStyleSheet(dockWidgetStyle(palette));
    if (m_treeView) {
        m_treeView->setStyleSheet(modelTreeStyle(palette));
    }
}

bool ModelExplorerPanel::selectNodeById(const QString& nodeId) {
    if (nodeId.isEmpty() || m_treeView == nullptr || m_itemModel == nullptr) {
        return false;
    }

    for (auto it = m_nodeMap.constBegin(); it != m_nodeMap.constEnd(); ++it) {
        Handle(BrNode_adObject) node = it.value();
        if (node.IsNull()) continue;

        // 视口对象的元数据里存的是 GlobalID；这里同时兼容自增 ID
        const QString globalId = TextEncoding::FromExtendedString(node->GetGlobalID());
        const QString innerId = QString(node->GetId().ToCString());
        if (globalId != nodeId && innerId != nodeId) continue;

        QStandardItem* item = it.key();
        if (item == nullptr) return false;

        const QModelIndex index = item->index();
        // 展开所有父级，否则目标项可能不可见
        for (QModelIndex parent = index.parent(); parent.isValid();
             parent = parent.parent()) {
            m_treeView->expand(parent);
        }
        m_treeView->setCurrentIndex(index);
        m_treeView->scrollTo(index, QAbstractItemView::PositionAtCenter);
        return true;
    }
    return false;
}

void ModelExplorerPanel::setModel(Handle(DataModel) model) {
    m_dataModel = model;
    m_itemModel->clear();
    m_itemModel->setHorizontalHeaderLabels({"Model Tree"});
    m_nodeMap.clear();

    if (m_dataModel.IsNull()) return;

    Handle(ActAPI_INode) rootBase = m_dataModel->GetRootNode();
    if (rootBase.IsNull()) return;

    Handle(ActAPI_IChildIterator) it = rootBase->GetChildIterator();
    for (; it->More(); it->Next()) {
        Handle(BrNode_adObject) obj = Handle(BrNode_adObject)::DownCast(it->Value());
        if (!obj.IsNull()) {
            populateModel(obj, m_itemModel->invisibleRootItem());
        }
    }
    m_treeView->expandAll();
}

void ModelExplorerPanel::onItemClicked(const QModelIndex& index) {
    QStandardItem* item = m_itemModel->itemFromIndex(index);
    if (item && m_nodeMap.contains(item)) {
        emit nodeSelected(m_nodeMap[item]);
    }
}

void ModelExplorerPanel::populateModel(Handle(BrNode_adObject) node, QStandardItem* parentItem) {
    if (node.IsNull()) return;

    // 统一走 TextEncoding::FromExtendedString，可顺带修复历史数据里
    // “UTF-8 字节被按 Latin-1 写入”造成的乱码（桥梁 → æ¡¥æ¢）
    QString qName = TextEncoding::FromExtendedString(node->GetName());
    QString qType = TextEncoding::FromExtendedString(node->GetObjectType());

    // Format: name(type)
    QString displayText = QString("%1(%2)").arg(qName).arg(qType);

    QStandardItem* nameItem = new QStandardItem(displayText);
    parentItem->appendRow(nameItem);
    m_nodeMap[nameItem] = node;

    NCollection_Sequence<Handle(BrNode_adObject)> children = node->GetSubObjectsList();
    for (int i = 1; i <= children.Length(); ++i) {
        populateModel(children.Value(i), nameItem);
    }
}
