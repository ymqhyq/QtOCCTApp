/**
 * @file test_project_package.cpp
 * @brief RDE 工程包（ZIP 容器 + manifest.json）读写测试
 */

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "ProjectPackage.h"

namespace {

bool writeBytes(const QString &path, const QByteArray &data) {
  if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
    return false;
  }
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return false;
  }
  return file.write(data) == data.size();
}

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

} // namespace

class ProjectPackageTest : public QObject {
  Q_OBJECT

private slots:
  void roundTripPreservesFilesAndManifest();
  void legacyZipWithoutManifestIsAccepted();
  void realProjectArchivesAndReopens();
  void rejectsTruncatedArchive();
  void failsWhenMasterIsMissing();
  void sanitizesRelativePaths();
};

/// 打包 → 解包 → 逐文件哈希比对，并校验清单内容
void ProjectPackageTest::roundTripPreservesFilesAndManifest() {
  QTemporaryDir sourceDir;
  QTemporaryDir outputDir;
  QTemporaryDir extractDir;
  QVERIFY(sourceDir.isValid() && outputDir.isValid() && extractDir.isValid());

  QVERIFY(writeBytes(sourceDir.filePath(QStringLiteral("master.cbf")),
                     QByteArray(4096, 'M')));
  QVERIFY(writeBytes(sourceDir.filePath(QStringLiteral("models/subgrade_3d.cbf")),
                     QByteArray(10000, 'S')));
  QVERIFY(writeBytes(sourceDir.filePath(QStringLiteral("drawings/plan_view.cbf")),
                     QByteArray(3000, 'P')));

  const QString archive = outputDir.filePath(QStringLiteral("project.rde"));
  QList<ProjectPackage::SubDocEntry> subDocs;
  subDocs.append({QStringLiteral("models/subgrade_3d.cbf"), QStringLiteral("3DModel")});
  subDocs.append({QStringLiteral("drawings/plan_view.cbf"), QStringLiteral("2DDrawing")});

  QString error;
  QVERIFY2(ProjectPackage::createArchive(archive, sourceDir.path(), subDocs,
                                         QStringLiteral("unit-test-project"), &error),
           qPrintable(error));

  // 容器必须是 ZIP
  QVERIFY(ProjectPackage::isZipArchive(archive));
  {
    QFile file(archive);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.read(2), QByteArray("PK"));
  }

  ProjectPackage::Manifest manifest;
  error.clear();
  QVERIFY2(ProjectPackage::extractArchive(archive, extractDir.path(), &manifest, &error),
           qPrintable(error));

  QCOMPARE(manifest.formatVersion, ProjectPackage::kFormatVersion);
  QCOMPARE(manifest.projectName, QStringLiteral("unit-test-project"));
  QCOMPARE(manifest.layout, QStringLiteral("indexed"));
  QCOMPARE(manifest.master, QStringLiteral("master.cbf"));
  QCOMPARE(manifest.files.size(), 3);
  QCOMPARE(manifest.subDocuments.size(), 2);

  const QStringList expectedFiles = {
      QStringLiteral("master.cbf"),
      QStringLiteral("models/subgrade_3d.cbf"),
      QStringLiteral("drawings/plan_view.cbf"),
  };
  for (const QString &relPath : expectedFiles) {
    QCOMPARE(ProjectPackage::sha256OfFile(extractDir.filePath(relPath)),
             ProjectPackage::sha256OfFile(sourceDir.filePath(relPath)));
  }
}

/// 旧格式 ZIP（无 manifest.json）必须仍可解包
void ProjectPackageTest::legacyZipWithoutManifestIsAccepted() {
  const QString legacy = findRepoFile(QStringLiteral("slope.rde"));
  if (legacy.isEmpty()) {
    QSKIP("仓库里没有 slope.rde，跳过旧格式兼容测试");
  }
  QVERIFY2(ProjectPackage::isZipArchive(legacy), "slope.rde 应当是 ZIP 容器");

  QTemporaryDir extractDir;
  QVERIFY(extractDir.isValid());
  ProjectPackage::Manifest manifest;
  QString error;
  QVERIFY2(ProjectPackage::extractArchive(legacy, extractDir.path(), &manifest, &error),
           qPrintable(error));

  QCOMPARE(manifest.formatVersion, 0); // 旧包
  QVERIFY(QFileInfo::exists(extractDir.filePath(QStringLiteral("master.cbf"))));
  QVERIFY(QFileInfo::exists(
      extractDir.filePath(QStringLiteral("models/subgrade_3d.cbf"))));
}

/// 截断的包必须报错而不是产生部分结果
void ProjectPackageTest::realProjectArchivesAndReopens() {
  const QString master =
      findRepoFile(QStringLiteral("temp_debug/master.cbf"));
  if (master.isEmpty()) {
    QSKIP("仓库里没有 temp_debug 边坡工程，跳过真实工程打包测试");
  }
  const QDir projectDir = QFileInfo(master).absoluteDir();
  const QString sub3D =
      projectDir.absoluteFilePath(QStringLiteral("models/subgrade_3d.cbf"));
  if (!QFileInfo::exists(sub3D)) {
    QSKIP("边坡工程缺少 3D 子文档，跳过");
  }

  QTemporaryDir outputDir;
  QTemporaryDir extractDir;
  QVERIFY(outputDir.isValid() && extractDir.isValid());
  const QString archive = outputDir.filePath(QStringLiteral("slope_v1.rde"));

  QList<ProjectPackage::SubDocEntry> subDocs;
  subDocs.append({QStringLiteral("models/subgrade_3d.cbf"), QStringLiteral("3DModel")});
  subDocs.append({QStringLiteral("drawings/plan_view.cbf"), QStringLiteral("2DDrawing")});

  QString error;
  QVERIFY2(ProjectPackage::createArchive(archive, projectDir.absolutePath(),
                                         subDocs, QStringLiteral("slope"), &error),
           qPrintable(error));

  ProjectPackage::Manifest manifest;
  error.clear();
  QVERIFY2(ProjectPackage::extractArchive(archive, extractDir.path(), &manifest, &error),
           qPrintable(error));

  QCOMPARE(manifest.formatVersion, ProjectPackage::kFormatVersion);
  QCOMPARE(manifest.layout, QStringLiteral("indexed"));
  QVERIFY2(manifest.subDocuments.size() >= 2, "清单应登记 3D/2D 两个子文档");
  QCOMPARE(ProjectPackage::sha256OfFile(extractDir.filePath(QStringLiteral("master.cbf"))),
           ProjectPackage::sha256OfFile(master));
  QCOMPARE(ProjectPackage::sha256OfFile(
               extractDir.filePath(QStringLiteral("models/subgrade_3d.cbf"))),
           ProjectPackage::sha256OfFile(sub3D));
}

/// 截断的包必须报错而不是产生部分结果
void ProjectPackageTest::rejectsTruncatedArchive() {
  QTemporaryDir sourceDir;
  QTemporaryDir outputDir;
  QTemporaryDir extractDir;
  QVERIFY(sourceDir.isValid() && outputDir.isValid() && extractDir.isValid());
  QVERIFY(writeBytes(sourceDir.filePath(QStringLiteral("master.cbf")),
                     QByteArray(2048, 'X')));

  const QString archive = outputDir.filePath(QStringLiteral("bad.rde"));
  QString error;
  QVERIFY(ProjectPackage::createArchive(archive, sourceDir.path(), {},
                                        QStringLiteral("truncated"), &error));

  QFile original(archive);
  QVERIFY(original.open(QIODevice::ReadOnly));
  const QByteArray bytes = original.readAll();
  original.close();
  QVERIFY(bytes.size() > 40);

  const QString truncated = outputDir.filePath(QStringLiteral("truncated.rde"));
  QVERIFY(writeBytes(truncated, bytes.left(bytes.size() / 2)));

  ProjectPackage::Manifest manifest;
  error.clear();
  QVERIFY2(!ProjectPackage::extractArchive(truncated, extractDir.path(), &manifest, &error),
           "截断的包不应解包成功");
  QVERIFY(!error.isEmpty());
}

/// 缺少 master.cbf 时打包必须失败并给出明确原因
void ProjectPackageTest::failsWhenMasterIsMissing() {
  QTemporaryDir sourceDir;
  QTemporaryDir outputDir;
  QVERIFY(sourceDir.isValid() && outputDir.isValid());
  QVERIFY(writeBytes(sourceDir.filePath(QStringLiteral("models/only.cbf")),
                     QByteArray(128, 'Y')));

  QString error;
  QVERIFY2(!ProjectPackage::createArchive(outputDir.filePath(QStringLiteral("x.rde")),
                                          sourceDir.path(), {},
                                          QStringLiteral("no-master"), &error),
           "缺少 master.cbf 时不应打包成功");
  QVERIFY(error.contains(QStringLiteral("master.cbf")));
}

/// 包内路径安全校验（防目录穿越）
void ProjectPackageTest::sanitizesRelativePaths() {
  QVERIFY(ProjectPackage::isSafeRelativePath(QStringLiteral("master.cbf")));
  QVERIFY(ProjectPackage::isSafeRelativePath(QStringLiteral("models/a.cbf")));
  QVERIFY(!ProjectPackage::isSafeRelativePath(QStringLiteral("")));
  QVERIFY(!ProjectPackage::isSafeRelativePath(QStringLiteral("../evil.txt")));
  QVERIFY(!ProjectPackage::isSafeRelativePath(QStringLiteral("a/../../evil.txt")));
  QVERIFY(!ProjectPackage::isSafeRelativePath(QStringLiteral("C:/Windows/evil.txt")));
  QVERIFY(!ProjectPackage::isSafeRelativePath(QStringLiteral("/etc/passwd")));
}

QTEST_MAIN(ProjectPackageTest)
#include "test_project_package.moc"
