/**
 * @file test_selection_linkage.cpp
 * @brief 视口 ↔ 模型树 联动的进程内测试
 *
 * 背景：此前"在视口里点选构件，模型树不跟着选中"的问题，根因是几何元数据里的
 *      _adNodeId 为空（工程数据没有设置 GlobalID），导致树查找与业务节点查找
 *      都无法定位。这类问题靠人工点击回归代价很高。
 *
 * 本测试用 QtTest 在应用进程内直接投递鼠标事件（QTest::mouseClick），
 * 不经过操作系统输入层，因此不依赖任何外部自动化工具，可直接纳入回归。
 *
 * 用法：
 *   test_selection_linkage [工程文件.cbf]
 *   不传参数时依次尝试若干内置工程文件，取第一个能提取出几何的。
 */

#include <QtTest>
#include <QSignalSpy>
#include <QTreeView>
#include <QDir>
#include <QFileInfo>

#include <BRepPrimAPI_MakeBox.hxx>
#include <Graphic3d_NameOfMaterial.hxx>
#include <Quantity_Color.hxx>
#include <TopoDS_Shape.hxx>

#include <ActAPI_IPartition.h>
#include <ActData_BasePartition.h>

#include <gp_Pnt.hxx>
#include <Standard_Failure.hxx>
#include <vector>

#include "ModelExplorerPanel.h"
#include "OCCTWidget.h"
#include "TextEncoding.h"
#include "SceneDataExtractor.h"
#include "generated/DataModel.h"
#include "generated/BrNode_adObject.h"

class SelectionLinkageTest : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void metadataCarriesUsableNodeId();
  void treeSelectionFollowsLinkageId();
  void treeSelectionRejectsUnknownId();
  void viewportPickReportsLinkedNode();

private:
  /// 从工程文件加载模型并提取几何；返回提取到的几何数量
  int extractShapes(std::vector<SceneDataExtractor::VisualShape> &outShapes) const;

  Handle(DataModel) m_model;
  QStringList m_linkIds;              ///< 几何元数据里出现过的联动 ID
  QMap<QString, QString> m_idToName;  ///< 联动 ID -> 节点名（用于校验树选中项）
  QString m_sourceFile;
};

int SelectionLinkageTest::extractShapes(
    std::vector<SceneDataExtractor::VisualShape> &outShapes) const {
  outShapes.clear();
  if (m_model.IsNull()) {
    return 0;
  }
  SceneDataExtractor::Extract(m_model, outShapes);
  return static_cast<int>(outShapes.size());
}

void SelectionLinkageTest::initTestCase() {
  // 1. 确定要用哪个工程文件：命令行优先，否则在工程根目录下挑一个能出几何的
  const QStringList args = QCoreApplication::arguments();
  QString explicitPath;
  for (int i = 1; i < args.size(); ++i) {
    if (!args.at(i).startsWith(QLatin1Char('-'))) {
      explicitPath = args.at(i);
      break;
    }
  }

  const QStringList candidates = {
      QStringLiteral("temp_debug/models/subgrade_3d.cbf"),
      QStringLiteral("temp_debug/master.cbf"),
      QStringLiteral("scratch/check_rde/models/subgrade_3d.cbf"),
      QStringLiteral("scratch/check_rde/master.cbf"),
      QStringLiteral("bridge.asi.cbf"),
  };

  // 可执行文件位于 build_v142/<config>/ 下，向上若干级找工程根目录
  QDir rootDir(QCoreApplication::applicationDirPath());
  QStringList searchDirs;
  for (int up = 0; up < 5; ++up) {
    searchDirs << rootDir.absolutePath();
    if (!rootDir.cdUp()) {
      break;
    }
  }

  std::vector<SceneDataExtractor::VisualShape> shapes;
  QStringList tried;

  auto tryOpen = [&](const QString &path) -> bool {
    if (path.isEmpty() || !QFileInfo::exists(path)) {
      return false;
    }
    tried << path;
    try {
      Handle(DataModel) model = new DataModel();
      // 与 MainWindow 保持一致：路径按 UTF-8 传入
      if (!model->Open(path.toStdString().c_str())) {
        qWarning() << "工程文件打开失败:" << path;
        return false;
      }
      Handle(DataModel) previous = m_model;
      m_model = model;
      if (extractShapes(shapes) > 0) {
        m_sourceFile = path;
        return true;
      }
      m_model = previous;
    } catch (const Standard_Failure &failure) {
      qWarning() << "打开工程时 OCCT 异常:" << failure.GetMessageString();
    } catch (const std::exception &e) {
      qWarning() << "打开工程时异常:" << e.what();
    } catch (...) {
      qWarning() << "打开工程时发生未知异常:" << path;
    }
    return false;
  };

  if (!explicitPath.isEmpty()) {
    QVERIFY2(tryOpen(explicitPath),
             qPrintable(QStringLiteral("指定的工程文件无法打开或没有几何: %1")
                            .arg(explicitPath)));
  } else {
    for (const QString &dir : searchDirs) {
      for (const QString &candidate : candidates) {
        if (tryOpen(QDir(dir).absoluteFilePath(candidate))) {
          break;
        }
      }
      if (!m_sourceFile.isEmpty()) {
        break;
      }
    }
  }

  QVERIFY2(!m_model.IsNull(),
           qPrintable(QStringLiteral("未找到可用于测试的工程文件（尝试过: %1）。"
                                     "可通过命令行参数显式指定 .cbf 路径。")
                          .arg(tried.join(QStringLiteral(", ")))));

  qInfo() << "测试工程:" << m_sourceFile << " 几何数量:" << int(shapes.size());

  // 2. 收集几何元数据里的联动 ID，并建立 ID -> 节点名 映射用于校验
  for (const SceneDataExtractor::VisualShape &vs : shapes) {
    if (!vs.metadata.contains("_adNodeId")) {
      continue;
    }
    const QString id =
        QString::fromStdString(vs.metadata["_adNodeId"].get<std::string>());
    if (!id.isEmpty()) {
      m_linkIds << id;
    }
  }
  m_linkIds.removeDuplicates();

  // 遍历各分区，记录每个节点的条目 ID / GlobalID 与名称
  // 注意：未注册的分区编号会让 Partition() 抛异常，这里逐个兜住
  for (int partIndex = 1; partIndex <= 8; ++partIndex) {
    try {
      Handle(ActAPI_IPartition) part = m_model->Partition(partIndex);
      if (part.IsNull()) {
        continue;
      }
      for (ActData_BasePartition::Iterator it(part); it.More(); it.Next()) {
        Handle(BrNode_adObject) obj = Handle(BrNode_adObject)::DownCast(it.Value());
        if (obj.IsNull()) {
          continue;
        }
        const QString name = TextEncoding::FromExtendedString(obj->GetName());
        m_idToName.insert(QString(obj->GetId().ToCString()), name);
        const QString globalId = TextEncoding::FromExtendedString(obj->GetGlobalID());
        if (!globalId.isEmpty()) {
          m_idToName.insert(globalId, name);
        }
      }
    } catch (const Standard_Failure &failure) {
      qWarning() << "遍历分区" << partIndex << "失败:" << failure.GetMessageString();
    } catch (...) {
      qWarning() << "遍历分区" << partIndex << "时发生未知异常";
    }
  }

  qInfo() << "联动 ID 数量:" << m_linkIds.size()
          << " 节点索引数量:" << m_idToName.size();
}

/// 几何元数据必须带上可用的联动 ID。
/// 这是本次问题的回归点：GlobalID 缺失时必须退化为条目 ID，不能是空串。
void SelectionLinkageTest::metadataCarriesUsableNodeId() {
  std::vector<SceneDataExtractor::VisualShape> shapes;
  const int count = extractShapes(shapes);
  QVERIFY(count > 0);

  int checked = 0;
  for (const SceneDataExtractor::VisualShape &vs : shapes) {
    QVERIFY2(vs.metadata.contains("_adNodeId"), "几何元数据缺少 _adNodeId");
    const QString id =
        QString::fromStdString(vs.metadata["_adNodeId"].get<std::string>());
    QVERIFY2(!id.isEmpty(),
             "几何元数据的 _adNodeId 为空：GlobalID 缺失时应退化为条目 ID");
    ++checked;
  }
  QVERIFY(checked > 0);
}

/// 视口点选拿到的 ID 必须能在模型树里定位到节点
void SelectionLinkageTest::treeSelectionFollowsLinkageId() {
  QVERIFY(!m_linkIds.isEmpty());

  ModelExplorerPanel panel;
  panel.resize(320, 480);
  panel.setModel(m_model);
  panel.show();
  QVERIFY(QTest::qWaitForWindowExposed(&panel));

  QTreeView *tree = panel.findChild<QTreeView *>();
  QVERIFY2(tree != nullptr, "模型树内部没有 QTreeView");

  int matched = 0;
  for (const QString &id : m_linkIds) {
    const bool ok = panel.selectNodeById(id);
    QVERIFY2(ok, qPrintable(QStringLiteral("selectNodeById 未能选中联动 ID = %1")
                                .arg(id)));

    const QModelIndex current = tree->currentIndex();
    QVERIFY2(current.isValid(),
             qPrintable(QStringLiteral("选中 ID = %1 后当前项无效").arg(id)));

    const QString text = current.data(Qt::DisplayRole).toString();
    const QString expectedName = m_idToName.value(id);
    if (!expectedName.isEmpty()) {
      QVERIFY2(text.startsWith(expectedName),
               qPrintable(QStringLiteral("ID = %1 选中的树项为 \"%2\"，期望以 \"%3\" 开头")
                              .arg(id, text, expectedName)));
    }
    ++matched;
  }
  qInfo() << "已在模型树中成功定位" << matched << "个联动 ID";
}

/// 空 ID 与不存在的 ID 都不能误选中节点（此前空串会误命中第一个节点）
void SelectionLinkageTest::treeSelectionRejectsUnknownId() {
  ModelExplorerPanel panel;
  panel.setModel(m_model);

  QVERIFY2(!panel.selectNodeById(QString()),
           "空 ID 不应选中任何节点");
  QVERIFY2(!panel.selectNodeById(QStringLiteral("__no_such_node__")),
           "不存在的 ID 不应选中任何节点");
  QVERIFY2(!panel.selectNodeById(QStringLiteral("0:0:0:0")),
           "不存在的条目 ID 不应选中任何节点");
}

/// 在视口里点中构件后，必须发出带联动 ID 的 objectSelected 信号
void SelectionLinkageTest::viewportPickReportsLinkedNode() {
  OCCTWidget widget;
  widget.resize(640, 480);
  widget.show();
  QVERIFY(QTest::qWaitForWindowExposed(&widget));
  QTest::qWait(200);

  const TopoDS_Shape box =
      BRepPrimAPI_MakeBox(gp_Pnt(-50.0, -50.0, -50.0), gp_Pnt(50.0, 50.0, 50.0))
          .Shape();
  QVERIFY(!box.IsNull());

  QVariantMap meta;
  meta[QStringLiteral("_adNodeId")] = QStringLiteral("unit-test-node");
  meta[QStringLiteral("Pset_UnitTest.Marker")] = QStringLiteral("1");
  widget.addShape(box, Quantity_NOC_GRAY75, Graphic3d_NOM_PLASTIC, meta);
  widget.fitAll();
  QTest::qWait(200);

  QSignalSpy spy(&widget, &OCCTWidget::objectSelected);
  QVERIFY2(spy.isValid(), "无法监听 OCCTWidget::objectSelected");

  // 视图已 FitAll，构件位于视口中心，点中心即可命中
  const QPoint center(widget.width() / 2, widget.height() / 2);
  QTest::mouseClick(&widget, Qt::LeftButton, Qt::NoModifier, center);

  QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 3000);

  const QVariantMap emitted = spy.last().at(0).toMap();
  QVERIFY2(!emitted.isEmpty(), "点选后收到的元数据为空，说明拾取未命中");
  QCOMPARE(emitted.value(QStringLiteral("_adNodeId")).toString(),
           QStringLiteral("unit-test-node"));
  QCOMPARE(emitted.value(QStringLiteral("Pset_UnitTest.Marker")).toString(),
           QStringLiteral("1"));
}

QTEST_MAIN(SelectionLinkageTest)
#include "test_selection_linkage.moc"
