#include "../include/ModelExplorerPanel.h"
#include "../include/TextEncoding.h"
#include <QFileInfo>
#include <QVBoxLayout>
#include <QHeaderView>
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
    m_subDocRefItems.clear();

    if (m_dataModel.IsNull()) return;

    Handle(ActAPI_INode) rootBase = m_dataModel->GetRootNode();
    if (rootBase.IsNull()) return;

    // 根节点的子级里除了普通业务对象（adObject），还有子文档引用（adSubDocRef）等
    // 非 adObject 节点。此前一律 DownCast 成 adObject 并丢弃失败者，导致
    // “主控 + 子文档”形态的工程模型树完全为空。
    QStandardItem* rootItem = m_itemModel->invisibleRootItem();
    Handle(ActAPI_IChildIterator) it = rootBase->GetChildIterator();
    for (; it->More(); it->Next()) {
        Handle(ActAPI_INode) child = it->Value();
        if (child.IsNull()) continue;

        Handle(BrNode_adObject) obj = Handle(BrNode_adObject)::DownCast(child);
        if (!obj.IsNull()) {
            populateModel(obj, rootItem);
            continue;
        }

        Handle(BrNode_adSubDocRef) subRef =
            Handle(BrNode_adSubDocRef)::DownCast(child);
        if (!subRef.IsNull()) {
            const QString docPath =
                TextEncoding::FromExtendedString(subRef->GetDocPath());
            const QString docType =
                TextEncoding::FromExtendedString(subRef->GetDocType());
            const QString fileName = QFileInfo(docPath).fileName();
            QString label = fileName.isEmpty() ? QStringLiteral("子文档") : fileName;
            if (!docType.isEmpty()) {
                label += QStringLiteral(" [%1]").arg(docType);
            }
            QStandardItem* item = new QStandardItem(label);
            item->setEditable(false);
            rootItem->appendRow(item);
            m_subDocRefItems.insert(item, subRef);
            continue;
        }

        // 其它类型的节点（如 adDrawing2D）也按名称显示，保证结构可见
        const QString name = TextEncoding::FromExtendedString(child->GetName());
        // GetObjectType() 只定义在 adObject 上，这里用 OCC RTTI 取类型名
        const QString type = QString::fromLatin1(child->DynamicType()->Name());
        QStandardItem* item =
            new QStandardItem(QStringLiteral("%1(%2)").arg(name, type));
        item->setEditable(false);
        rootItem->appendRow(item);
    }
    m_treeView->expandAll();
}

int ModelExplorerPanel::populateRootChildren(Handle(DataModel) model,
                                             QStandardItem* parentItem) {
    if (model.IsNull() || parentItem == nullptr) return 0;

    Handle(ActAPI_INode) rootBase = model->GetRootNode();
    if (rootBase.IsNull()) return 0;

    int added = 0;
    Handle(ActAPI_IChildIterator) it = rootBase->GetChildIterator();
    for (; it->More(); it->Next()) {
        Handle(BrNode_adObject) obj = Handle(BrNode_adObject)::DownCast(it->Value());
        if (obj.IsNull()) continue;
        populateModel(obj, parentItem);
        ++added;
    }
    return added;
}

int ModelExplorerPanel::populateTopologyPartition(Handle(DataModel) model,
                                                  QStandardItem* parentItem) {
    if (model.IsNull() || parentItem == nullptr) return 0;

    int added = 0;
    try {
        // ID 2 = Topology 分区
        Handle(ActAPI_IPartition) topology = model->Partition(2);
        if (topology.IsNull()) return 0;

        for (ActData_BasePartition::Iterator it(topology); it.More(); it.Next()) {
            Handle(BrNode_adObject) obj = Handle(BrNode_adObject)::DownCast(it.Value());
            if (obj.IsNull()) continue;
            populateModel(obj, parentItem);
            ++added;
        }
    } catch (const Standard_Failure& failure) {
        qWarning() << "[ModelExplorerPanel] 遍历 Topology 分区失败:"
                   << failure.GetMessageString();
    } catch (...) {
        qWarning() << "[ModelExplorerPanel] 遍历 Topology 分区时发生未知异常";
    }
    return added;
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

    // 2. 先按对象层级填充；层级为空时退回 Topology 分区
    int added = populateRootChildren(subModel, parentItem);
    if (added == 0) {
        added = populateTopologyPartition(subModel, parentItem);
    }
    if (added == 0) {
        parentItem->appendRow(new QStandardItem(QStringLiteral("(无对象)")));
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
