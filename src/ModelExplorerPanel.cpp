#include "../include/ModelExplorerPanel.h"
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

    auto convertToUtf8 = [](const TCollection_ExtendedString& extStr) -> QString {
        std::vector<char> buf(static_cast<size_t>(extStr.Length()) * 4 + 1);
        Standard_PCharacter pBuffer = buf.data();
        const Standard_Integer len = extStr.ToUTF8CString(pBuffer);
        return QString::fromUtf8(pBuffer, len);
    };

    QString qName = convertToUtf8(node->GetName());
    QString qType = convertToUtf8(node->GetObjectType());

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
