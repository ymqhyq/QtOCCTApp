/**
 * @file test_guid_migration.cpp
 * @brief 「统一 GUID」端到端回归测试（真实 MainWindow 路径）
 *
 * 复现过的崩溃：OCAF 要求属性修改必须处于事务中，早期实现把“引用更新”
 * 放在事务提交之后，抛出 "Attribute ... is changed outside transaction"，
 * 在正式程序中表现为崩溃。这里用真实工程覆盖自包含与“主控 + 子文档”两种形态。
 *
 * 注意：OCAF 应用是进程级单例，同一个进程内反复打开/包装同一文档会触发冲突，
 * 因此两个场景分别作为独立的 CTest 进程运行：
 *   test_guid_migration mainWindowNormalizeGuids
 *   test_guid_migration mainWindowNormalizeGuidsIndexed
 */

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "MainWindow.h"

namespace {

QString findRepoFile(const QString &relativeName) {
  QDir dir(QCoreApplication::applicationDirPath());
  for (int up = 0; up < 6; ++up) {
    const QString candidate = dir.absoluteFilePath(relativeName);
    if (QFileInfo::exists(candidate)) {
      return candidate;
    }
    if (!dir.cdUp()) {
      break;
    }
  }
  return QString();
}

/// 复制边坡工程（主控 + 3D/2D 子文档）到临时目录
bool prepareIndexedProject(const QString &sourceMaster, const QString &workRoot) {
  const QDir sourceDir = QFileInfo(sourceMaster).absoluteDir();
  QDir().mkpath(workRoot + QStringLiteral("/models"));
  QDir().mkpath(workRoot + QStringLiteral("/drawings"));
  if (!QFile::copy(sourceDir.absoluteFilePath(QStringLiteral("master.cbf")),
                   workRoot + QStringLiteral("/master.cbf"))) {
    return false;
  }
  if (!QFile::copy(sourceDir.absoluteFilePath(QStringLiteral("models/subgrade_3d.cbf")),
                   workRoot + QStringLiteral("/models/subgrade_3d.cbf"))) {
    return false;
  }
  const QString drawing =
      sourceDir.absoluteFilePath(QStringLiteral("drawings/plan_view.cbf"));
  if (QFileInfo::exists(drawing)) {
    QFile::copy(drawing, workRoot + QStringLiteral("/drawings/plan_view.cbf"));
  }
  return true;
}

} // namespace

class GuidMigrationTest : public QObject {
  Q_OBJECT

private slots:
  void mainWindowNormalizeGuids();
  void mainWindowNormalizeGuidsIndexed();
};

/// 自包含工程（几何全在主控）：bridge.asi.cbf
void GuidMigrationTest::mainWindowNormalizeGuids() {
  const QString source = findRepoFile(QStringLiteral("bridge.asi.cbf"));
  if (source.isEmpty()) {
    QSKIP("仓库里没有 bridge.asi.cbf，跳过");
  }

  QTemporaryDir workDir;
  QVERIFY(workDir.isValid());
  const QString projectPath = workDir.filePath(QStringLiteral("project.cbf"));
  QVERIFY(QFile::copy(source, projectPath));

  MainWindow window;
  window.resize(1280, 900);
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  QVERIFY2(window.openProjectFile(projectPath), "MainWindow 打开工程失败");
  QTest::qWait(600);

  int changed = 0;
  QString error;
  const bool ok = window.normalizeProjectGuids(&changed, &error);
  qInfo() << "自包含工程: ok=" << ok << " 改动=" << changed << " 错误=" << error;
  QVERIFY2(ok, qPrintable(error));
  QVERIFY2(changed > 0, "应当有需要规范化的节点");

  // 幂等：再执行一次不应再有改动
  int again = -1;
  QVERIFY2(window.normalizeProjectGuids(&again, &error), qPrintable(error));
  QCOMPARE(again, 0);
}

/// 主控 + 3D/2D 子文档（含图纸同步）：
/// 这正是崩溃复现场景——引用更新必须在事务内完成。
void GuidMigrationTest::mainWindowNormalizeGuidsIndexed() {
  const QString master = findRepoFile(QStringLiteral("temp_debug/master.cbf"));
  if (master.isEmpty()) {
    QSKIP("仓库里没有 temp_debug 边坡工程，跳过");
  }

  QTemporaryDir workDir;
  QVERIFY(workDir.isValid());
  QVERIFY(prepareIndexedProject(master, workDir.path()));

  MainWindow window;
  window.resize(1280, 900);
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  QVERIFY2(window.openProjectFile(workDir.filePath(QStringLiteral("master.cbf"))),
           "MainWindow 打开主控工程失败");
  QTest::qWait(800);

  int changed = 0;
  QString error;
  const bool ok = window.normalizeProjectGuids(&changed, &error);
  qInfo() << "索引工程: ok=" << ok << " 改动=" << changed << " 错误=" << error;
  QVERIFY2(ok, qPrintable(error));
  QVERIFY2(changed > 0, "应当有需要规范化的节点");
}

QTEST_MAIN(GuidMigrationTest)
#include "test_guid_migration.moc"
