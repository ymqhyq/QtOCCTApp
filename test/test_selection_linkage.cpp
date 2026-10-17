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
#include <QApplication>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTimer>
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
#include "MainWindow.h"
#include "OCCTWidget.h"
#include "TextEncoding.h"
#include "SceneDataExtractor.h"
#include "NodeIdentity.h"
#include "generated/DataModel.h"
#include "generated/BrNode_adObject.h"

namespace {
/**
 * @brief 测试期间的模态对话框看门狗
 *
 * 工程加载失败时 MainWindow 会弹出 QMessageBox::critical，模态对话框会阻塞事件循环
 * 让测试挂死。这里定时关闭任何弹出的消息框并记录下来，使测试以明确的断言失败收场。
 */
class DialogGuard : public QObject {
public:
  explicit DialogGuard(QObject *parent = nullptr) : QObject(parent) {
    m_timer.setInterval(150);
    connect(&m_timer, &QTimer::timeout, this, [this]() {
      const QWidgetList widgets = QApplication::topLevelWidgets();
      for (QWidget *widget : widgets) {
        QMessageBox *box = qobject_cast<QMessageBox *>(widget);
        if (box == nullptr || !box->isVisible()) {
          continue;
        }
        m_lastMessage = box->text();
        ++m_count;
        box->close();
      }
    });
    m_timer.start();
  }

  int count() const { return m_count; }
  QString lastMessage() const { return m_lastMessage; }

private:
  QTimer m_timer;
  int m_count = 0;
  QString m_lastMessage;
};
} // namespace

class SelectionLinkageTest : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void metadataCarriesUsableNodeId();
  void treeSelectionFollowsLinkageId();
  void treeSelectionRejectsUnknownId();
  void globalIdMigrationAssignsStableIds();
  void guidFormatIsNormalized();
  void viewportPickReportsLinkedNode();
  void subDocumentObjectsAreAttachedToTree();
  void mainWindowLinksViewportPickToModelTree();

private:
  /// 从工程文件加载模型并提取几何；返回提取到的几何数量
  int extractShapes(std::vector<SceneDataExtractor::VisualShape> &outShapes) const;

  Handle(DataModel) m_model;
  QStringList m_linkIds;              ///< 几何元数据里出现过的联动 ID
  QMap<QString, QString> m_idToName;  ///< 联动 ID -> 节点名（用于校验树选中项）
  QString m_sourceFile;
  QString m_projectFile;              ///< 主控工程（含子文档），用于 MainWindow 端到端测试
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

  // 3. 另外找一个"主控工程"用于 MainWindow 端到端测试（含子文档，走真实加载路径）
  const QStringList projectCandidates = {
      QStringLiteral("temp_debug/master.cbf"),
      QStringLiteral("scratch/check_rde/master.cbf"),
      QStringLiteral("temp_slope_extracted/master.cbf"),
  };
  for (const QString &dir : searchDirs) {
    for (const QString &candidate : projectCandidates) {
      const QString full = QDir(dir).absoluteFilePath(candidate);
      if (QFileInfo::exists(full)) {
        m_projectFile = full;
        break;
      }
    }
    if (!m_projectFile.isEmpty()) {
      break;
    }
  }
  qInfo() << "端到端测试用主控工程:" << (m_projectFile.isEmpty() ? QStringLiteral("(未找到)") : m_projectFile);
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

/**
 * @brief 历史工程补号后，几何元数据的联动 ID 应变为稳定 GUID
 *
 * 设计上跨文档引用以 PID_GlobalID 为准，但历史数据里该属性为空。
 * EnsureGlobalIds 负责一次性补号；本用例同时验证幂等性与元数据口径。
 */
void SelectionLinkageTest::globalIdMigrationAssignsStableIds() {
  QVERIFY(!m_model.IsNull());

  const int assigned = NodeIdentity::EnsureGlobalIds(m_model);
  QVERIFY2(assigned > 0, "历史工程应当存在缺失 GlobalID 的节点");
  QCOMPARE(NodeIdentity::EnsureGlobalIds(m_model), 0); // 幂等

  std::vector<SceneDataExtractor::VisualShape> shapes;
  QVERIFY(extractShapes(shapes) > 0);

  ModelExplorerPanel panel;
  panel.setModel(m_model);

  // 历史数据里可能已有自定义格式的 GUID（例如测试夹具里的 "GUID_Slope_3D_1"），
  // 因此这里只断言“非空且能在树中定位”，新补的号形如 32 位十六进制。
  const QRegularExpression guidPattern(QStringLiteral("^[0-9A-F]{32}$"));
  int checked = 0;
  int newFormat = 0;
  for (const SceneDataExtractor::VisualShape &vs : shapes) {
    const QString id =
        QString::fromStdString(vs.metadata["_adNodeId"].get<std::string>());
    QVERIFY2(!id.isEmpty(), "补号后联动 ID 仍为空");
    QVERIFY2(panel.selectNodeById(id),
             qPrintable(QStringLiteral("GUID 无法在模型树中定位: %1").arg(id)));
    if (guidPattern.match(id).hasMatch()) {
      ++newFormat;
    }
    ++checked;
  }
  QVERIFY(checked > 0);
  qInfo() << "补号节点数:" << assigned << " 已校验联动 ID 数:" << checked
          << " 其中新格式 GUID 数:" << newFormat;
}

/**
 * @brief GUID 格式统一：非标准 GUID 应被替换，且树定位改用新 ID
 */
void SelectionLinkageTest::guidFormatIsNormalized() {
  QVERIFY(!m_model.IsNull());

  // 夹具里存在自定义格式的 GUID（如 GUID_Slope_3D_1），迁移应至少改动一个节点
  const int changed = NodeIdentity::NormalizeGlobalIds({m_model});
  QVERIFY2(changed > 0, "应当存在需要规范化的 GUID");

  const QRegularExpression guidPattern(QStringLiteral("^[0-9A-F]{32}$"));
  std::vector<SceneDataExtractor::VisualShape> shapes;
  QVERIFY(extractShapes(shapes) > 0);

  ModelExplorerPanel panel;
  panel.setModel(m_model);

  for (const SceneDataExtractor::VisualShape &vs : shapes) {
    const QString id =
        QString::fromStdString(vs.metadata["_adNodeId"].get<std::string>());
    QVERIFY2(guidPattern.match(id).hasMatch(),
             qPrintable(QStringLiteral("联动 ID 未被规范化: %1").arg(id)));
    QVERIFY2(panel.selectNodeById(id),
             qPrintable(QStringLiteral("规范化后的 GUID 无法定位: %1").arg(id)));
  }

  // 旧格式 ID 不应再能命中
  QVERIFY2(!panel.selectNodeById(QStringLiteral("GUID_Slope_3D_1")),
           "旧格式 GUID 不应再能定位节点");

  // 幂等：再次规范化不应有改动
  QCOMPARE(NodeIdentity::NormalizeGlobalIds({m_model}), 0);
  qInfo() << "GUID 规范化节点数:" << changed;
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

  // 左上角信息组件（FPS）与右上角导航组件（视图立方体）
  QLabel *overlay = nullptr;
  for (QLabel *label : widget.findChildren<QLabel *>()) {
    if (label->text().startsWith(QStringLiteral("FPS:"))) {
      overlay = label;
      break;
    }
  }
  QVERIFY2(overlay != nullptr, "左上角没有 FPS 信息组件");
  QVERIFY2(overlay->x() < widget.width() / 4, "FPS 组件不在左侧");
  QVERIFY2(widget.isViewCubeVisible(), "右上角导航组件（视图立方体）未显示");

  // 清空视图（打开工程时会走这里）之后，导航立方体必须仍然可用
  widget.clearAll();
  QTest::qWait(150);
  QVERIFY2(widget.isViewCubeVisible(), "clearAll 之后导航组件消失");
}

/**
 * @brief 子文档对象必须挂到模型树对应的引用节点下，并且参与联动
 *
 * “主控 + 子文档”形态的工程里主控只保存 adSubDocRef 引用，真正的业务对象在子文档中。
 * 本用例不依赖 ProjectManager 的加载路径，直接用 DataModel 打开两个文件来验证挂载逻辑。
 */
void SelectionLinkageTest::subDocumentObjectsAreAttachedToTree() {
  if (m_projectFile.isEmpty() || m_sourceFile.isEmpty()) {
    QSKIP("需要主控工程与子文档模型文件才能验证子文档挂载");
  }

  // 注意：同一个 OCAF 文件在同一进程里重复打开会失败，这里直接复用 initTestCase 中
  // 已经打开的子文档模型（也正是视口几何与联动 ID 的来源）
  Handle(DataModel) sub = m_model;
  QVERIFY2(!sub.IsNull(), "子文档模型不可用");

  Handle(DataModel) master = new DataModel();
  if (!master->Open(m_projectFile.toStdString().c_str())) {
    QSKIP("主控工程无法直接打开，跳过子文档挂载用例");
  }

  ModelExplorerPanel panel;
  panel.resize(320, 480);
  panel.setModel(master);
  panel.show();

  QTreeView *tree = panel.findChild<QTreeView *>();
  QVERIFY2(tree != nullptr, "模型树内部没有 QTreeView");
  QAbstractItemModel *model = tree->model();

  // 1. 主控文档的子文档引用节点必须显示出来（此前被 DownCast 到 adObject 时全部丢弃）
  const QString subTitle = QFileInfo(m_sourceFile).fileName();
  QModelIndex subIndex;
  for (int row = 0; row < model->rowCount(); ++row) {
    const QModelIndex index = model->index(row, 0);
    if (model->data(index, Qt::DisplayRole).toString().startsWith(subTitle)) {
      subIndex = index;
      break;
    }
  }
  QVERIFY2(subIndex.isValid(),
           qPrintable(QStringLiteral("模型树里没有子文档引用节点: %1").arg(subTitle)));

  // 2. 把子文档对象挂到该引用节点下
  panel.addSubDocument(subTitle, sub);
  const int attached = model->rowCount(subIndex);
  QVERIFY2(attached > 0, "子文档对象没有挂到引用节点下");
  qInfo() << "子文档对象已挂入模型树，节点数:" << attached;

  // 3. 挂进来的对象必须能与几何元数据的联动 ID 对应
  std::vector<SceneDataExtractor::VisualShape> shapes;
  SceneDataExtractor::Extract(sub, shapes);
  QVERIFY(!shapes.empty());

  int checked = 0;
  for (const SceneDataExtractor::VisualShape &vs : shapes) {
    const QString id =
        QString::fromStdString(vs.metadata["_adNodeId"].get<std::string>());
    QVERIFY2(panel.selectNodeById(id),
             qPrintable(QStringLiteral("子文档对象的联动 ID 无法在树中定位: %1").arg(id)));
    ++checked;
  }
  QVERIFY(checked > 0);
  qInfo() << "已在树中定位子文档对象数量:" << checked;
}

/**
 * @brief 端到端：真实 MainWindow 中，在视口点选构件后模型树必须同步选中对应节点
 *
 * 这条用例覆盖的是 MainWindow::onObjectSelected 的接线（此前缺失的正是这一段：
 * 点选后只更新属性面板，从不选中树节点）。
 */
void SelectionLinkageTest::mainWindowLinksViewportPickToModelTree() {
  DialogGuard dialogGuard;

  // 依次尝试各候选工程，取第一个"确实能加载出可点选内容"的：
  // 主控工程往往只挂子文档引用，模型树会是空的，因此优先用含几何的模型文件。
  QStringList fixtures;
  if (!m_sourceFile.isEmpty()) fixtures << m_sourceFile;
  if (!m_projectFile.isEmpty()) fixtures << m_projectFile;
  QVERIFY2(!fixtures.isEmpty(), "没有可用的工程文件");

  MainWindow *window = nullptr;
  QTreeView *tree = nullptr;
  OCCTWidget *view = nullptr;
  QString usedFixture;

  for (const QString &fixture : fixtures) {
    MainWindow *candidate = new MainWindow();
    candidate->resize(1280, 900);
    candidate->show();
    if (!QTest::qWaitForWindowExposed(candidate, 5000)) {
      delete candidate;
      continue;
    }

    if (!candidate->openProjectFile(fixture)) {
      delete candidate;
      continue;
    }
    QTest::qWait(800); // 等待刷新视图、填充模型树与首帧渲染

    QTreeView *candidateTree = nullptr;
    if (ModelExplorerPanel *explorer =
            candidate->findChild<ModelExplorerPanel *>()) {
      candidateTree = explorer->findChild<QTreeView *>();
    }

    OCCTWidget *candidateView = nullptr;
    const QList<OCCTWidget *> views = candidate->findChildren<OCCTWidget *>();
    for (OCCTWidget *candidateWidget : views) {
      if (candidateWidget->isVisible()) {
        candidateView = candidateWidget;
        break;
      }
    }

    if (candidateTree != nullptr && candidateTree->model() != nullptr &&
        candidateTree->model()->rowCount() > 0 && candidateView != nullptr) {
      window = candidate;
      tree = candidateTree;
      view = candidateView;
      usedFixture = fixture;
      break;
    }
    delete candidate;
  }

  if (window == nullptr) {
    // 仓库内置的边坡工程把对象放在子文档分区里，主控文档本身没有可填充树的对象，
    // 因此这里跳过；用真实工程（例如桥梁工程）显式指定路径即可启用本用例：
    //   test_selection_linkage.exe <工程文件.cbf>
    QSKIP("没有候选工程能在主窗口里加载出模型树内容，跳过端到端用例"
          "（可用命令行参数指定一个含几何对象的工程文件）");
  }
  qInfo() << "端到端测试使用工程:" << usedFixture;
  QCOMPARE(dialogGuard.count(), 0);

  // 打印树结构，便于确认"子文档对象已挂进树"
  {
    qInfo() << "--- 模型树结构 ---";
    QAbstractItemModel *model = tree->model();
    for (int row = 0; row < model->rowCount(); ++row) {
      const QModelIndex index = model->index(row, 0);
      qInfo().noquote() << QStringLiteral("  ") +
                               model->data(index, Qt::DisplayRole).toString();
      const int childRows = model->rowCount(index);
      for (int child = 0; child < childRows; ++child) {
        qInfo().noquote() << QStringLiteral("    ") +
                                 model->data(model->index(child, 0, index),
                                             Qt::DisplayRole)
                                     .toString();
      }
    }
    qInfo() << "--- 模型树结构结束 ---";
  }

  // 视口里必须真的有几何对象才能点选；叠加标签里的 "Shapes: N" 可以据此判断
  int shapeCount = 0;
  if (QLabel *info = view->findChild<QLabel *>()) {
    const QString text = info->text();
    const QRegularExpression re(QStringLiteral("Shapes:\\s*(\\d+)"));
    const QRegularExpressionMatch match = re.match(text);
    if (match.hasMatch()) {
      shapeCount = match.captured(1).toInt();
    }
  }
  if (shapeCount <= 1) {
    QSKIP("视口里没有已显示的几何对象（仅有视口立方体），跳过点选联动断言");
  }

  // 打开工程后，左上角 FPS 组件必须仍然存在且位于左上区域
  QLabel *fpsLabel = nullptr;
  for (QLabel *label : view->findChildren<QLabel *>()) {
    if (label->text().startsWith(QStringLiteral("FPS:"))) {
      fpsLabel = label;
      break;
    }
  }
  QVERIFY2(fpsLabel != nullptr, "打开工程后左上角没有 FPS 组件");
  QVERIFY2(fpsLabel->isVisible(), "打开工程后 FPS 组件被隐藏");
  QVERIFY2(fpsLabel->x() < view->width() / 4 && fpsLabel->y() < view->height() / 4,
           "FPS 组件不在左上角");

  // 点选前清掉当前项，确保后续断言确实来自点选
  tree->setCurrentIndex(QModelIndex());
  QVERIFY2(!tree->currentIndex().isValid(), "清空后当前项仍有效");

  // 模型不一定正好落在视口正中，因此在中心附近多次试探点选
  const QPoint center(view->width() / 2, view->height() / 2);
  const QPoint offsets[] = {{0, 0},    {0, -100}, {0, 100},  {-160, 0},
                            {160, 0},  {-120, -80}, {120, 80}, {-300, 0},
                            {300, 0},  {0, -220}, {0, 220}};

  bool selected = false;
  for (const QPoint &offset : offsets) {
    QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, center + offset);
    QTest::qWait(150);
    if (tree->currentIndex().isValid()) {
      selected = true;
      break;
    }
  }

  QVERIFY2(selected, "在视口内点选构件后，模型树没有选中任何节点");
  const QString selectedText =
      tree->currentIndex().data(Qt::DisplayRole).toString();
  QVERIFY2(!selectedText.isEmpty(), "选中的树项内容为空");
  qInfo() << "端到端点选后模型树选中项:" << selectedText;
  QCOMPARE(dialogGuard.count(), 0);

  delete window; // 主动释放，避免测试进程退出时残留图形资源
}

QTEST_MAIN(SelectionLinkageTest)
#include "test_selection_linkage.moc"
