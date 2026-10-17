#include "../include/ModelExplorerPanel.h"
#include "../include/TextEncoding.h"
#include <QFileInfo>
#include <QHeaderView>
#include <QSet>
#include <QVBoxLayout>
#include <ActAPI_IPartition.h>
#include <ActData_BasePartition.h>
#include <Standard_Failure.hxx>
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

/**
 * @brief 为任意节点创建树项
 *
 * schema 中 adSubDocRef / adDrawing2D / adRepresentation2D 等都派生自 adRoot
 * 而不是 adObject。这里统一按 adRoot 处理，避免再次出现“只认 adObject、
 * 其余节点被静默丢弃”的问题。
 */
QStandardItem* ModelExplorerPanel::appendNode(Handle(ActAPI_INode) node,
                                              QStandardItem* parentItem) {
    if (node.IsNull() || parentItem == nullptr) {
        return nullptr;
    }

    QStandardItem* item = nullptr;
    Handle(BrNode_adSubDocRef) subRef = Handle(BrNode_adSubDocRef)::DownCast(node);
    if (!subRef.IsNull()) {
        const QString docPath = TextEncoding::FromExtendedString(subRef->GetDocPath());
        const QString docType = TextEncoding::FromExtendedString(subRef->GetDocType());
        const QString fileName = QFileInfo(docPath).fileName();
        QString label = fileName.isEmpty() ? QStringLiteral("子文档") : fileName;
        if (!docType.isEmpty()) {
            label += QStringLiteral(" [%1]").arg(docType);
        }
        item = new QStandardItem(label);
        m_subDocRefItems.insert(item, subRef);
    } else {
        const QString name = TextEncoding::FromExtendedString(node->GetName());
        QString type;
        if (Handle(BrNode_adObject) obj = Handle(BrNode_adObject)::DownCast(node)) {
            // GetObjectType() 定义在 adObject 上，优先使用业务类型名
            type = TextEncoding::FromExtendedString(obj->GetObjectType());
        } else {
            type = QString::fromLatin1(node->DynamicType()->Name());
        }
        item = new QStandardItem(QStringLiteral("%1(%2)").arg(name, type));
    }

    item->setEditable(false);
    parentItem->appendRow(item);
    m_nodeMap.insert(item, node);
    return item;
}

void ModelExplorerPanel::populateNode(Handle(ActAPI_INode) node,
                                      QStandardItem* parentItem,
                                      QSet<QString>* seen) {
    if (node.IsNull() || parentItem == nullptr) {
        return;
    }

    const QString entryId = QString(node->GetId().ToCString());
    if (seen != nullptr) {
        if (seen->contains(entryId)) {
            return; // 层级路径与分区路径可能指向同一节点
        }
        seen->insert(entryId);
    }

    QStandardItem* item = appendNode(node, parentItem);
    if (item == nullptr) {
        return;
    }

    Handle(BrNode_adObject) obj = Handle(BrNode_adObject)::DownCast(node);
    if (obj.IsNull()) {
        return; // 非业务对象节点（子文档引用/图纸/表达）没有 adObject 子级
    }

    const NCollection_Sequence<Handle(BrNode_adObject)> children = obj->GetSubObjectsList();
    for (int i = 1; i <= children.Length(); ++i) {
        populateNode(children.Value(i), item, seen);
    }
}

int ModelExplorerPanel::populateRootChildren(Handle(DataModel) model,
                                             QStandardItem* parentItem,
                                             QSet<QString>* seen) {
    if (model.IsNull() || parentItem == nullptr) return 0;

    Handle(ActAPI_INode) rootBase = model->GetRootNode();
    if (rootBase.IsNull()) return 0;

    int added = 0;
    Handle(ActAPI_IChildIterator) it = rootBase->GetChildIterator();
    for (; it->More(); it->Next()) {
        Handle(ActAPI_INode) child = it->Value();
        if (child.IsNull()) continue;
        const int before = parentItem->rowCount();
        populateNode(child, parentItem, seen);
        if (parentItem->rowCount() > before) ++added;
    }
    return added;
}

int ModelExplorerPanel::populateTopologyPartition(Handle(DataModel) model,
                                                  QStandardItem* parentItem,
                                                  QSet<QString>* seen) {
    if (model.IsNull() || parentItem == nullptr) return 0;

    int added = 0;
    try {
        // ID 2 = Topology 分区。子文档里的图纸/表达节点通常挂在这里，
        // 且它们不是 adObject，必须按 adRoot 处理。
        Handle(ActAPI_IPartition) topology = model->Partition(2);
        if (topology.IsNull()) return 0;

        for (ActData_BasePartition::Iterator it(topology); it.More(); it.Next()) {
            Handle(ActAPI_INode) node = it.Value();
            if (node.IsNull()) continue;
            const int before = parentItem->rowCount();
            populateNode(node, parentItem, seen);
            if (parentItem->rowCount() > before) ++added;
        }
    } catch (const Standard_Failure& failure) {
        qWarning() << "[ModelExplorerPanel] 遍历 Topology 分区失败:"
                   << failure.GetMessageString();
    } catch (...) {
        qWarning() << "[ModelExplorerPanel] 遍历 Topology 分区时发生未知异常";
    }
    return added;
}

bool ModelExplorerPanel::selectNodeById(const QString& nodeId) {
    if (nodeId.isEmpty() || m_treeView == nullptr || m_itemModel == nullptr) {
        return false;
    }

    for (auto it = m_nodeMap.constBegin(); it != m_nodeMap.constEnd(); ++it) {
        Handle(ActAPI_INode) node = it.value();
        if (node.IsNull()) continue;

        // 视口元数据里存的是 GlobalID；同时兼容 OCAF 条目 ID
        Handle(BrNode_adRoot) rootNode = Handle(BrNode_adRoot)::DownCast(node);
        const QString globalId =
            rootNode.IsNull()
                ? QString()
                : TextEncoding::FromExtendedString(rootNode->GetGlobalID());
        const QString entryId = QString(node->GetId().ToCString());
        if (globalId != nodeId && entryId != nodeId) continue;

        QStandardItem* item = it.key();
        if (item == nullptr) return false;

        const QModelIndex index = item->index();
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
    m_subDocRefItems.clear();

    if (m_dataModel.IsNull()) return;

    Handle(ActAPI_INode) rootBase = m_dataModel->GetRootNode();
    if (rootBase.IsNull()) return;

    // 根节点的子级里除了普通业务对象（adObject），还有子文档引用（adSubDocRef）
    // 等非 adObject 节点；统一按 adRoot 处理，避免它们被丢弃。
    QStandardItem* rootItem = m_itemModel->invisibleRootItem();
    QSet<QString> seen;
    Handle(ActAPI_IChildIterator) it = rootBase->GetChildIterator();
    for (; it->More(); it->Next()) {
        populateNode(it->Value(), rootItem, &seen);
    }
    m_treeView->expandAll();
}

void ModelExplorerPanel::addSubDocument(const QString& title,
                                        Handle(DataModel) subModel) {
    if (m_itemModel == nullptr) return;

    // 1. 优先挂到同名子文档引用节点下，找不到就新建一个顶层分组
    QStandardItem* parentItem = nullptr;
    const QString titleFileName = QFileInfo(title).fileName();
    for (auto it = m_subDocRefItems.constBegin(); it != m_subDocRefItems.constEnd();
         ++it) {
        const QString docPath =
            TextEncoding::FromExtendedString(it.value()->GetDocPath());
        const QString docFileName = QFileInfo(docPath).fileName();
        if ((!titleFileName.isEmpty() && docFileName == titleFileName) ||
            (!docPath.isEmpty() && (title == docPath || title.endsWith(docPath)))) {
            parentItem = it.key();
            break;
        }
    }
    if (parentItem == nullptr) {
        parentItem = new QStandardItem(title);
        parentItem->setEditable(false);
        m_itemModel->invisibleRootItem()->appendRow(parentItem);
    }

    if (subModel.IsNull()) {
        parentItem->appendRow(new QStandardItem(QStringLiteral("(未加载)")));
        return;
    }

    // 2. 先按对象层级填充，再补 Topology 分区里尚未挂载的节点
    //    （图纸的 adDrawing2D / adRepresentation2D 只在分区里，不在层级里）
    QSet<QString> seen;
    int added = populateRootChildren(subModel, parentItem, &seen);
    added += populateTopologyPartition(subModel, parentItem, &seen);
    if (added == 0) {
        parentItem->appendRow(new QStandardItem(QStringLiteral("(无对象)")));
    }

    m_treeView->expandAll();
}

void ModelExplorerPanel::onItemClicked(const QModelIndex& index) {
    QStandardItem* item = m_itemModel->itemFromIndex(index);
    if (item == nullptr || !m_nodeMap.contains(item)) {
        return;
    }

    // 属性面板目前只接受业务对象；子文档引用/图纸等节点仅做树内选中
    Handle(BrNode_adObject) obj = Handle(BrNode_adObject)::DownCast(m_nodeMap[item]);
    if (!obj.IsNull()) {
        emit nodeSelected(obj);
    }
}
