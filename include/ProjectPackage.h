#ifndef PROJECTPACKAGE_H
#define PROJECTPACKAGE_H

#include <QList>
#include <QString>

/**
 * @brief RDE 工程包（ZIP 容器）的读写
 *
 * 设计约定：
 *  - 容器固定为 ZIP（自实现，基于 zlib，不依赖系统 tar.exe 等外部程序）；
 *  - 包内固定包含 master.cbf，可选 models/、drawings/ 等子文档目录；
 *  - 包内固定包含 manifest.json，记录格式版本、布局类型、文件清单与 SHA-256；
 *  - 读取时兼容没有 manifest.json 的旧 ZIP（formatVersion = 0）。
 */
namespace ProjectPackage {

/// 当前包格式版本
constexpr int kFormatVersion = 1;

struct FileEntry {
  QString path;        ///< 包内相对路径（'/' 分隔）
  qint64 size = 0;     ///< 字节数
  QString sha256;      ///< 小写十六进制 SHA-256
};

struct SubDocEntry {
  QString path;        ///< 相对主控文件的路径，例如 models/subgrade_3d.cbf
  QString docType;     ///< 3DModel / 2DDrawing / ...
};

struct Manifest {
  int formatVersion = 0;   ///< 0 表示旧包（无 manifest.json）
  QString appVersion;
  QString projectName;
  QString createdAt;       ///< ISO 8601
  QString layout;          ///< indexed | self-contained
  QString master = QStringLiteral("master.cbf");
  QList<FileEntry> files;
  QList<SubDocEntry> subDocuments;
};

/// 文件 SHA-256（小写十六进制）；失败返回空串
QString sha256OfFile(const QString &path);

/// 相对路径是否安全（拒绝绝对路径、盘符、`..` 片段）
bool isSafeRelativePath(const QString &path);

/// 是否为 ZIP 容器（用于区分旧式 TAR 包）
bool isZipArchive(const QString &archivePath);

/**
 * @brief 把工程目录打包为 ZIP 格式的 .rde
 *
 * 会递归收集 sourceDir 下的所有文件，写入 master.cbf、子文档与 manifest.json。
 * 采用“先写临时文件、成功后原子替换”的方式，避免中途失败留下半成品。
 *
 * @param subDocuments 需要登记到清单里的子文档（路径相对 sourceDir）
 * @param error 失败原因（可为空）
 */
bool createArchive(const QString &archivePath, const QString &sourceDir,
                   const QList<SubDocEntry> &subDocuments,
                   const QString &projectName, QString *error = nullptr);

/**
 * @brief 解包到 destDir，并校验清单/哈希
 *
 * 兼容没有 manifest.json 的旧 ZIP（返回的 formatVersion 为 0）。
 * 不是 ZIP 容器时返回 false（调用方可退回旧式 TAR 解析）。
 */
bool extractArchive(const QString &archivePath, const QString &destDir,
                    Manifest *manifest = nullptr, QString *error = nullptr);

} // namespace ProjectPackage

#endif // PROJECTPACKAGE_H
