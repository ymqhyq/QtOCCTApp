#include "../include/ProjectPackage.h"

#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <zlib.h>

#include <cstring>

namespace {

// ---------------------------------------------------------------------------
// 基础读写
// ---------------------------------------------------------------------------

bool writeAll(QIODevice *device, const QByteArray &data) {
  qint64 written = 0;
  while (written < data.size()) {
    const qint64 n =
        device->write(data.constData() + written, data.size() - written);
    if (n <= 0) {
      return false;
    }
    written += n;
  }
  return true;
}

void put16(QByteArray &out, quint16 value) {
  out.append(static_cast<char>(value & 0xFF));
  out.append(static_cast<char>((value >> 8) & 0xFF));
}

void put32(QByteArray &out, quint32 value) {
  out.append(static_cast<char>(value & 0xFF));
  out.append(static_cast<char>((value >> 8) & 0xFF));
  out.append(static_cast<char>((value >> 16) & 0xFF));
  out.append(static_cast<char>((value >> 24) & 0xFF));
}

quint16 get16(const uchar *data) {
  return static_cast<quint16>(data[0]) |
         static_cast<quint16>(static_cast<quint16>(data[1]) << 8);
}

quint32 get32(const uchar *data) {
  return static_cast<quint32>(data[0]) |
         (static_cast<quint32>(data[1]) << 8) |
         (static_cast<quint32>(data[2]) << 16) |
         (static_cast<quint32>(data[3]) << 24);
}

void dosDateTime(quint16 *dosTime, quint16 *dosDate) {
  const QDateTime now = QDateTime::currentDateTime();
  const QTime time = now.time();
  const QDate date = now.date();
  *dosTime = static_cast<quint16>((time.hour() << 11) | (time.minute() << 5) |
                                  (time.second() / 2));
  *dosDate = static_cast<quint16>(((date.year() - 1980) << 9) |
                                  (date.month() << 5) | date.day());
}

// 原始 deflate（ZIP 使用的无 zlib 头的 deflate）
bool deflateStream(const QByteArray &input, QIODevice *out, quint32 *crcOut,
                   quint32 *compressedOut) {
  z_stream zs{};
  if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8,
                   Z_DEFAULT_STRATEGY) != Z_OK) {
    return false;
  }
  uLong crc = crc32(0L, Z_NULL, 0);
  quint32 totalOut = 0;
  QByteArray outBuf(64 * 1024, '\0');
  bool ok = true;

  zs.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(input.constData()));
  zs.avail_in = static_cast<uInt>(input.size());
  if (!input.isEmpty()) {
    crc = crc32(crc, reinterpret_cast<const Bytef *>(input.constData()),
                static_cast<uInt>(input.size()));
  }

  int rc = Z_OK;
  do {
    zs.next_out = reinterpret_cast<Bytef *>(outBuf.data());
    zs.avail_out = static_cast<uInt>(outBuf.size());
    rc = deflate(&zs, Z_FINISH);
    if (rc != Z_OK && rc != Z_STREAM_END) {
      ok = false;
      break;
    }
    const qint64 produced =
        static_cast<qint64>(outBuf.size()) - static_cast<qint64>(zs.avail_out);
    if (produced > 0) {
      totalOut += static_cast<quint32>(produced);
      if (out != nullptr &&
          !writeAll(out, QByteArray::fromRawData(outBuf.constData(),
                                                 static_cast<int>(produced)))) {
        ok = false;
        break;
      }
    }
  } while (rc != Z_STREAM_END);

  deflateEnd(&zs);
  if (!ok) {
    return false;
  }
  if (crcOut != nullptr) *crcOut = static_cast<quint32>(crc);
  if (compressedOut != nullptr) *compressedOut = totalOut;
  return true;
}

struct DeflateStats {
  quint32 crc = 0;
  quint32 compressed = 0;
  quint32 uncompressed = 0;
};

// 以 64KB 分块单遍流式压缩文件，并上报总体进度
bool deflateFileStreaming(const QString &path, QIODevice *out,
                          const ProjectPackage::ProgressCallback &progress,
                          qint64 *processedBytes, qint64 totalBytes,
                          DeflateStats *stats, bool *canceled) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }

  z_stream zs{};
  if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8,
                   Z_DEFAULT_STRATEGY) != Z_OK) {
    return false;
  }

  uLong crc = crc32(0L, Z_NULL, 0);
  quint32 totalIn = 0;
  quint32 totalOut = 0;
  QByteArray inBuf(64 * 1024, '\0');
  QByteArray outBuf(64 * 1024, '\0');
  bool ok = true;

  for (;;) {
    const qint64 got = file.read(inBuf.data(), inBuf.size());
    if (got < 0) {
      ok = false;
      break;
    }
    const bool finished = (got == 0);
    if (got > 0) {
      crc = crc32(crc, reinterpret_cast<const Bytef *>(inBuf.constData()),
                  static_cast<uInt>(got));
      totalIn += static_cast<quint32>(got);
      if (processedBytes != nullptr) {
        *processedBytes += got;
        if (progress && !progress(*processedBytes, totalBytes, path)) {
          if (canceled != nullptr) *canceled = true;
          ok = false;
          break;
        }
      }
    }

    zs.next_in = reinterpret_cast<Bytef *>(inBuf.data());
    zs.avail_in = static_cast<uInt>(got);
    const int flush = finished ? Z_FINISH : Z_NO_FLUSH;
    int rc = Z_OK;
    do {
      zs.next_out = reinterpret_cast<Bytef *>(outBuf.data());
      zs.avail_out = static_cast<uInt>(outBuf.size());
      rc = deflate(&zs, flush);
      if (rc != Z_OK && rc != Z_STREAM_END) {
        ok = false;
        break;
      }
      const qint64 produced = static_cast<qint64>(outBuf.size()) -
                              static_cast<qint64>(zs.avail_out);
      if (produced > 0) {
        totalOut += static_cast<quint32>(produced);
        if (out != nullptr &&
            !writeAll(out, QByteArray::fromRawData(
                               outBuf.constData(), static_cast<int>(produced)))) {
          ok = false;
          break;
        }
      }
    } while (zs.avail_out == 0);
    if (!ok) {
      break;
    }
    if (finished) {
      break;
    }
  }

  deflateEnd(&zs);
  if (!ok) {
    return false;
  }
  if (stats != nullptr) {
    stats->crc = static_cast<quint32>(crc);
    stats->compressed = totalOut;
    stats->uncompressed = totalIn;
  }
  return true;
}

bool inflateRaw(const QByteArray &input, quint32 expectedSize,
                QByteArray *output) {
  output->clear();
  if (expectedSize == 0) {
    return input.isEmpty();
  }
  output->resize(static_cast<int>(expectedSize));

  z_stream zs{};
  if (inflateInit2(&zs, -15) != Z_OK) {
    return false;
  }
  zs.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(input.constData()));
  zs.avail_in = static_cast<uInt>(input.size());
  zs.next_out = reinterpret_cast<Bytef *>(output->data());
  zs.avail_out = static_cast<uInt>(output->size());

  const int rc = inflate(&zs, Z_FINISH);
  const uInt remaining = zs.avail_out;
  inflateEnd(&zs);
  return rc == Z_STREAM_END && remaining == 0;
}

// ---------------------------------------------------------------------------
// ZIP 结构
// ---------------------------------------------------------------------------

constexpr quint32 kLocalHeaderSig = 0x04034b50;
constexpr quint32 kCentralHeaderSig = 0x02014b50;
constexpr quint32 kEndOfCentralDirSig = 0x06054b50;
constexpr quint16 kFlagUtf8 = 0x0800;
constexpr quint16 kMethodDeflate = 8;

struct ZipItem {
  QString name;
  quint32 crc = 0;
  quint32 compressedSize = 0;
  quint32 uncompressedSize = 0;
  quint32 localOffset = 0;
};

struct ZipEntry {
  QString name;
  quint16 method = 0;
  quint32 crc = 0;
  quint32 compressedSize = 0;
  quint32 uncompressedSize = 0;
  quint32 localOffset = 0;
};

constexpr quint16 kFlagDataDescriptor = 0x0008;

// 数据描述符模式：本地头中的 CRC/尺寸先写 0，真实值紧随数据之后写入，
// 因此可以单遍流式压缩（无需为计算尺寸而先压缩一遍）。
bool writeLocalHeader(QIODevice *out, const QString &name, quint16 dosTime,
                      quint16 dosDate, quint32 *offsetOut) {
  const QByteArray nameBytes = name.toUtf8();
  if (nameBytes.size() > 0xFFFF) {
    return false;
  }
  if (offsetOut != nullptr) {
    *offsetOut = static_cast<quint32>(out->pos());
  }

  QByteArray header;
  header.reserve(30 + nameBytes.size());
  put32(header, kLocalHeaderSig);
  put16(header, 20); // version needed
  put16(header, static_cast<quint16>(kFlagUtf8 | kFlagDataDescriptor));
  put16(header, kMethodDeflate);
  put16(header, dosTime);
  put16(header, dosDate);
  put32(header, 0); // CRC（见数据描述符）
  put32(header, 0); // compressed size
  put32(header, 0); // uncompressed size
  put16(header, static_cast<quint16>(nameBytes.size()));
  put16(header, 0); // extra length
  header.append(nameBytes);
  return writeAll(out, header);
}

bool writeDataDescriptor(QIODevice *out, quint32 crc, quint32 compressedSize,
                         quint32 uncompressedSize) {
  QByteArray descriptor;
  descriptor.reserve(16);
  put32(descriptor, 0x08074b50);
  put32(descriptor, crc);
  put32(descriptor, compressedSize);
  put32(descriptor, uncompressedSize);
  return writeAll(out, descriptor);
}

bool addFileToZip(QIODevice *out, const QString &sourcePath, const QString &zipName,
                  QList<ZipItem> *items,
                  const ProjectPackage::ProgressCallback &progress,
                  qint64 *processedBytes, qint64 totalBytes, QString *error,
                  bool *canceled) {
  quint16 dosTime = 0;
  quint16 dosDate = 0;
  dosDateTime(&dosTime, &dosDate);

  ZipItem item;
  item.name = zipName;

  if (!writeLocalHeader(out, zipName, dosTime, dosDate, &item.localOffset)) {
    if (error != nullptr) *error = QStringLiteral("写入 ZIP 头失败");
    return false;
  }

  DeflateStats stats;
  if (!deflateFileStreaming(sourcePath, out, progress, processedBytes,
                            totalBytes, &stats, canceled)) {
    if (error != nullptr) *error = QStringLiteral("写入 ZIP 数据失败: %1").arg(sourcePath);
    return false;
  }
  if (!writeDataDescriptor(out, stats.crc, stats.compressed, stats.uncompressed)) {
    if (error != nullptr) *error = QStringLiteral("写入 ZIP 描述符失败");
    return false;
  }

  item.crc = stats.crc;
  item.compressedSize = stats.compressed;
  item.uncompressedSize = stats.uncompressed;
  if (items != nullptr) items->append(item);
  return true;
}

bool addBytesToZip(QIODevice *out, const QString &zipName, const QByteArray &data,
                   QList<ZipItem> *items, QString *error) {
  quint16 dosTime = 0;
  quint16 dosDate = 0;
  dosDateTime(&dosTime, &dosDate);

  ZipItem item;
  item.name = zipName;
  if (!writeLocalHeader(out, zipName, dosTime, dosDate, &item.localOffset)) {
    if (error != nullptr) *error = QStringLiteral("写入 ZIP 头失败");
    return false;
  }

  quint32 crc = 0;
  quint32 compressedSize = 0;
  if (!deflateStream(data, out, &crc, &compressedSize)) {
    if (error != nullptr) *error = QStringLiteral("写入清单数据失败");
    return false;
  }
  if (!writeDataDescriptor(out, crc, compressedSize,
                           static_cast<quint32>(data.size()))) {
    if (error != nullptr) *error = QStringLiteral("写入 ZIP 描述符失败");
    return false;
  }

  item.crc = crc;
  item.compressedSize = compressedSize;
  item.uncompressedSize = static_cast<quint32>(data.size());
  if (items != nullptr) items->append(item);
  return true;
}

bool writeCentralDirectory(QIODevice *out, const QList<ZipItem> &items) {
  quint16 dosTime = 0;
  quint16 dosDate = 0;
  dosDateTime(&dosTime, &dosDate);

  const qint64 centralOffset = out->pos();
  for (const ZipItem &item : items) {
    const QByteArray nameBytes = item.name.toUtf8();
    QByteArray header;
    header.reserve(46 + nameBytes.size());
    put32(header, kCentralHeaderSig);
    put16(header, 20); // version made by
    put16(header, 20); // version needed
    put16(header, kFlagUtf8);
    put16(header, kMethodDeflate);
    put16(header, dosTime);
    put16(header, dosDate);
    put32(header, item.crc);
    put32(header, item.compressedSize);
    put32(header, item.uncompressedSize);
    put16(header, static_cast<quint16>(nameBytes.size()));
    put16(header, 0);  // extra
    put16(header, 0);  // comment
    put16(header, 0);  // disk start
    put16(header, 0);  // internal attrs
    put32(header, 0);  // external attrs
    put32(header, item.localOffset);
    header.append(nameBytes);
    if (!writeAll(out, header)) {
      return false;
    }
  }
  const qint64 centralEnd = out->pos();
  const quint32 centralSize =
      static_cast<quint32>(centralEnd - centralOffset);

  QByteArray end;
  put32(end, kEndOfCentralDirSig);
  put16(end, 0);
  put16(end, 0);
  put16(end, static_cast<quint16>(items.size()));
  put16(end, static_cast<quint16>(items.size()));
  put32(end, centralSize);
  put32(end, static_cast<quint32>(centralOffset));
  put16(end, 0);
  return writeAll(out, end);
}

bool readZipIndex(QFile &file, QList<ZipEntry> *entries, QString *error) {
  const qint64 fileSize = file.size();
  if (fileSize < 22) {
    if (error != nullptr) *error = QStringLiteral("文件过小，不是 ZIP 包");
    return false;
  }

  const qint64 tailSize = qMin<qint64>(fileSize, 65557);
  if (!file.seek(fileSize - tailSize)) {
    return false;
  }
  const QByteArray tail = file.read(tailSize);
  if (tail.size() != static_cast<int>(tailSize)) {
    return false;
  }

  int eocdPos = -1;
  for (int i = tail.size() - 22; i >= 0; --i) {
    if (get32(reinterpret_cast<const uchar *>(tail.constData() + i)) ==
        kEndOfCentralDirSig) {
      eocdPos = i;
      break;
    }
  }
  if (eocdPos < 0) {
    if (error != nullptr) *error = QStringLiteral("未找到 ZIP 目录结构");
    return false;
  }

  const uchar *eocd = reinterpret_cast<const uchar *>(tail.constData() + eocdPos);
  const quint16 count = get16(eocd + 10);
  const quint32 centralSize = get32(eocd + 12);
  const quint32 centralOffset = get32(eocd + 16);

  if (!file.seek(centralOffset)) {
    return false;
  }
  const QByteArray central = file.read(centralSize);
  if (central.size() != static_cast<int>(centralSize)) {
    if (error != nullptr) *error = QStringLiteral("ZIP 目录读取失败");
    return false;
  }

  const uchar *p = reinterpret_cast<const uchar *>(central.constData());
  const uchar *end = p + central.size();
  entries->clear();
  for (quint16 i = 0; i < count; ++i) {
    if (p + 46 > end || get32(p) != kCentralHeaderSig) {
      if (error != nullptr) *error = QStringLiteral("ZIP 目录项损坏");
      return false;
    }
    ZipEntry entry;
    entry.method = get16(p + 10);
    entry.crc = get32(p + 16);
    entry.compressedSize = get32(p + 20);
    entry.uncompressedSize = get32(p + 24);
    const quint16 nameLen = get16(p + 28);
    const quint16 extraLen = get16(p + 30);
    const quint16 commentLen = get16(p + 32);
    entry.localOffset = get32(p + 42);
    if (p + 46 + nameLen > end) {
      if (error != nullptr) *error = QStringLiteral("ZIP 文件名损坏");
      return false;
    }
    entry.name = QString::fromUtf8(reinterpret_cast<const char *>(p + 46), nameLen);
    entries->append(entry);
    p += 46 + nameLen + extraLen + commentLen;
  }
  return true;
}

bool readEntryData(QFile &file, const ZipEntry &entry, QByteArray *data,
                   QString *error) {
  if (!file.seek(entry.localOffset)) {
    return false;
  }
  const QByteArray header = file.read(30);
  if (header.size() != 30 ||
      get32(reinterpret_cast<const uchar *>(header.constData())) != kLocalHeaderSig) {
    if (error != nullptr) *error = QStringLiteral("ZIP 本地头损坏: %1").arg(entry.name);
    return false;
  }
  const uchar *h = reinterpret_cast<const uchar *>(header.constData());
  const quint16 nameLen = get16(h + 26);
  const quint16 extraLen = get16(h + 28);
  if (!file.seek(entry.localOffset + 30 + nameLen + extraLen)) {
    return false;
  }
  const QByteArray compressed = file.read(entry.compressedSize);
  if (compressed.size() != static_cast<int>(entry.compressedSize)) {
    if (error != nullptr) *error = QStringLiteral("ZIP 数据不完整: %1").arg(entry.name);
    return false;
  }

  QByteArray output;
  if (entry.method == 0) {
    output = compressed;
  } else if (entry.method == kMethodDeflate) {
    if (!inflateRaw(compressed, entry.uncompressedSize, &output)) {
      if (error != nullptr) *error = QStringLiteral("ZIP 解压失败: %1").arg(entry.name);
      return false;
    }
  } else {
    if (error != nullptr) *error = QStringLiteral("不支持的 ZIP 压缩方式: %1").arg(entry.name);
    return false;
  }

  if (static_cast<quint32>(output.size()) != entry.uncompressedSize) {
    if (error != nullptr) *error = QStringLiteral("ZIP 尺寸校验失败: %1").arg(entry.name);
    return false;
  }
  const quint32 crc =
      output.isEmpty()
          ? 0
          : static_cast<quint32>(crc32(0L,
                                       reinterpret_cast<const Bytef *>(output.constData()),
                                       static_cast<uInt>(output.size())));
  if (crc != entry.crc) {
    if (error != nullptr) *error = QStringLiteral("ZIP CRC 校验失败: %1").arg(entry.name);
    return false;
  }
  *data = output;
  return true;
}

QString sha256OfBytes(const QByteArray &data) {
  return QString::fromLatin1(
      QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

bool parseManifest(const QByteArray &data, ProjectPackage::Manifest *out,
                   QString *error) {
  QJsonParseError parseError{};
  const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
  if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
    if (error != nullptr) {
      *error = QStringLiteral("manifest.json 解析失败: %1").arg(parseError.errorString());
    }
    return false;
  }

  ProjectPackage::Manifest manifest;
  const QJsonObject obj = doc.object();
  manifest.formatVersion = obj.value(QStringLiteral("formatVersion")).toInt();
  if (manifest.formatVersion <= 0 ||
      manifest.formatVersion > ProjectPackage::kFormatVersion) {
    if (error != nullptr) {
      *error = QStringLiteral("不支持的包格式版本: %1（当前程序支持 %2）")
                   .arg(manifest.formatVersion)
                   .arg(ProjectPackage::kFormatVersion);
    }
    return false;
  }
  manifest.appVersion = obj.value(QStringLiteral("appVersion")).toString();
  manifest.projectName = obj.value(QStringLiteral("projectName")).toString();
  manifest.createdAt = obj.value(QStringLiteral("createdAt")).toString();
  manifest.layout = obj.value(QStringLiteral("layout")).toString();
  manifest.master =
      obj.value(QStringLiteral("master")).toString(QStringLiteral("master.cbf"));

  for (const QJsonValue &value : obj.value(QStringLiteral("files")).toArray()) {
    const QJsonObject fileObj = value.toObject();
    ProjectPackage::FileEntry entry;
    entry.path = fileObj.value(QStringLiteral("path")).toString();
    entry.size = static_cast<qint64>(fileObj.value(QStringLiteral("size")).toDouble());
    entry.sha256 = fileObj.value(QStringLiteral("sha256")).toString().toLower();
    if (!entry.path.isEmpty()) {
      manifest.files.append(entry);
    }
  }
  for (const QJsonValue &value : obj.value(QStringLiteral("subDocuments")).toArray()) {
    const QJsonObject subObj = value.toObject();
    ProjectPackage::SubDocEntry entry;
    entry.path = subObj.value(QStringLiteral("path")).toString();
    entry.docType = subObj.value(QStringLiteral("docType")).toString();
    if (!entry.path.isEmpty()) {
      manifest.subDocuments.append(entry);
    }
  }

  if (out != nullptr) {
    *out = manifest;
  }
  return true;
}

} // namespace

// ---------------------------------------------------------------------------
// 公共 API
// ---------------------------------------------------------------------------

QString ProjectPackage::sha256OfFile(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return QString();
  }
  QCryptographicHash hash(QCryptographicHash::Sha256);
  if (!hash.addData(&file)) {
    return QString();
  }
  return QString::fromLatin1(hash.result().toHex());
}

bool ProjectPackage::isSafeRelativePath(const QString &path) {
  if (path.isEmpty() || QDir::isAbsolutePath(path)) {
    return false;
  }
  if (path.size() >= 2 && path.at(1) == QLatin1Char(':')) {
    return false; // 盘符
  }
  const QString normalized = QDir::cleanPath(path);
  if (normalized == QStringLiteral("..") || normalized.startsWith(QStringLiteral("../"))) {
    return false;
  }
  const QStringList parts = normalized.split(QLatin1Char('/'), Qt::SkipEmptyParts);
  for (const QString &part : parts) {
    if (part == QStringLiteral("..")) {
      return false;
    }
  }
  return true;
}

bool ProjectPackage::isZipArchive(const QString &archivePath) {
  QFile file(archivePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }
  const QByteArray magic = file.read(4);
  if (magic.size() < 4) {
    return false;
  }
  const uchar *m = reinterpret_cast<const uchar *>(magic.constData());
  return (m[0] == 'P' && m[1] == 'K') &&
         ((m[2] == 0x03 && m[3] == 0x04) || (m[2] == 0x05 && m[3] == 0x06));
}

bool ProjectPackage::readManifest(const QString &archivePath, Manifest *manifest,
                                  QString *error) {
  QFile file(archivePath);
  if (!file.open(QIODevice::ReadOnly)) {
    if (error != nullptr) *error = QStringLiteral("无法读取工程包: %1").arg(archivePath);
    return false;
  }
  QList<ZipEntry> entries;
  if (!readZipIndex(file, &entries, error)) {
    return false;
  }
  for (const ZipEntry &entry : entries) {
    if (entry.name != QStringLiteral("manifest.json")) {
      continue;
    }
    QByteArray data;
    if (!readEntryData(file, entry, &data, error)) {
      return false;
    }
    return parseManifest(data, manifest, error);
  }
  if (error != nullptr) *error = QStringLiteral("包内没有 manifest.json（旧格式）");
  return false;
}

bool ProjectPackage::createArchive(const QString &archivePath,
                                   const QString &sourceDir,
                                   const QList<SubDocEntry> &subDocuments,
                                   const QString &projectName,
                                   const ProgressCallback &progress,
                                   QString *error) {
  const QDir root(sourceDir);
  if (!root.exists()) {
    if (error != nullptr) *error = QStringLiteral("工程目录不存在: %1").arg(sourceDir);
    return false;
  }

  const QString masterRel = QStringLiteral("master.cbf");
  const QString masterAbs = root.absoluteFilePath(masterRel);
  if (!QFileInfo::exists(masterAbs)) {
    if (error != nullptr) *error = QStringLiteral("工程目录缺少 master.cbf: %1").arg(sourceDir);
    return false;
  }

  // 1. 收集文件并计算校验值
  QList<FileEntry> files;
  const QString archiveAbs = QFileInfo(archivePath).absoluteFilePath();
  QDirIterator it(sourceDir, QDir::Files | QDir::NoSymLinks,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    const QString absPath = it.next();
    // 目标包若位于工程目录内，绝不能把旧包再打进去（会导致包中包、体积翻倍）
    if (QFileInfo(absPath).absoluteFilePath() == archiveAbs) {
      continue;
    }
    const QString relPath = QDir::fromNativeSeparators(root.relativeFilePath(absPath));
    if (!isSafeRelativePath(relPath)) {
      continue;
    }
    FileEntry entry;
    entry.path = relPath;
    entry.size = QFileInfo(absPath).size();
    entry.sha256 = sha256OfFile(absPath);
    if (entry.sha256.isEmpty()) {
      if (error != nullptr) *error = QStringLiteral("计算校验值失败: %1").arg(relPath);
      return false;
    }
    files.append(entry);
  }

  // 2. 生成清单
  QJsonObject manifest;
  manifest[QStringLiteral("formatVersion")] = kFormatVersion;
  manifest[QStringLiteral("appVersion")] =
      QCoreApplication::applicationVersion().isEmpty()
          ? QStringLiteral("1.0.0")
          : QCoreApplication::applicationVersion();
  manifest[QStringLiteral("projectName")] =
      projectName.isEmpty() ? root.dirName() : projectName;
  manifest[QStringLiteral("createdAt")] =
      QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
  manifest[QStringLiteral("layout")] =
      subDocuments.isEmpty() ? QStringLiteral("self-contained")
                             : QStringLiteral("indexed");
  manifest[QStringLiteral("master")] = masterRel;

  QJsonArray fileArray;
  for (const FileEntry &entry : files) {
    QJsonObject obj;
    obj[QStringLiteral("path")] = entry.path;
    obj[QStringLiteral("size")] = static_cast<double>(entry.size);
    obj[QStringLiteral("sha256")] = entry.sha256;
    fileArray.append(obj);
  }
  manifest[QStringLiteral("files")] = fileArray;

  QJsonArray subDocArray;
  for (const SubDocEntry &entry : subDocuments) {
    if (!isSafeRelativePath(entry.path)) {
      continue;
    }
    QJsonObject obj;
    obj[QStringLiteral("path")] = entry.path;
    obj[QStringLiteral("docType")] = entry.docType;
    subDocArray.append(obj);
  }
  manifest[QStringLiteral("subDocuments")] = subDocArray;

  const QByteArray manifestJson =
      QJsonDocument(manifest).toJson(QJsonDocument::Indented);

  // 3. 写入 ZIP（QSaveFile 保证原子替换）
  QSaveFile out(archivePath);
  if (!out.open(QIODevice::WriteOnly)) {
    if (error != nullptr) *error = QStringLiteral("无法写入目标文件: %1").arg(out.errorString());
    return false;
  }

  qint64 totalBytes = manifestJson.size();
  for (const FileEntry &entry : files) {
    totalBytes += entry.size;
  }
  qint64 processedBytes = 0;
  bool canceled = false;

  QList<ZipItem> items;
  if (!addFileToZip(&out, masterAbs, masterRel, &items, progress, &processedBytes,
                    totalBytes, error, &canceled)) {
    out.cancelWriting();
    if (canceled && error != nullptr) *error = QStringLiteral("打包已取消");
    return false;
  }
  for (const FileEntry &entry : files) {
    if (entry.path == masterRel) {
      continue;
    }
    if (!addFileToZip(&out, root.absoluteFilePath(entry.path), entry.path, &items,
                      progress, &processedBytes, totalBytes, error, &canceled)) {
      out.cancelWriting();
      if (canceled && error != nullptr) *error = QStringLiteral("打包已取消");
      return false;
    }
  }
  if (!addBytesToZip(&out, QStringLiteral("manifest.json"), manifestJson, &items,
                     error)) {
    out.cancelWriting();
    return false;
  }
  processedBytes += manifestJson.size();
  if (progress && !progress(processedBytes, totalBytes, QStringLiteral("manifest.json"))) {
    out.cancelWriting();
    if (error != nullptr) *error = QStringLiteral("打包已取消");
    return false;
  }
  if (!writeCentralDirectory(&out, items)) {
    if (error != nullptr) *error = QStringLiteral("写入 ZIP 目录失败");
    out.cancelWriting();
    return false;
  }
  if (!out.commit()) {
    if (error != nullptr) *error = QStringLiteral("保存工程包失败: %1").arg(out.errorString());
    return false;
  }
  return true;
}

bool ProjectPackage::extractArchive(const QString &archivePath,
                                    const QString &destDir, Manifest *manifestOut,
                                    QString *error) {
  QFile file(archivePath);
  if (!file.open(QIODevice::ReadOnly)) {
    if (error != nullptr) *error = QStringLiteral("无法读取工程包: %1").arg(archivePath);
    return false;
  }

  QList<ZipEntry> entries;
  if (!readZipIndex(file, &entries, error)) {
    return false; // 非 ZIP（调用方可退回旧式 TAR）
  }

  QHash<QString, int> indexByName;
  for (int i = 0; i < entries.size(); ++i) {
    indexByName.insert(entries.at(i).name, i);
  }

  // 1. 读取并校验清单
  Manifest manifest;
  const auto manifestIt = indexByName.constFind(QStringLiteral("manifest.json"));
  if (manifestIt != indexByName.constEnd()) {
    QByteArray manifestData;
    if (!readEntryData(file, entries.at(manifestIt.value()), &manifestData, error)) {
      return false;
    }
    if (!parseManifest(manifestData, &manifest, error)) {
      return false;
    }
  }

  // 2. 解包（有清单时逐项校验哈希；旧包则全量解出）
  QDir dest(destDir);
  if (!dest.exists() && !QDir().mkpath(destDir)) {
    if (error != nullptr) *error = QStringLiteral("无法创建解包目录: %1").arg(destDir);
    return false;
  }

  auto extractEntryTo = [&](const QString &name, const QString &expectedSha) -> bool {
    const auto it = indexByName.constFind(name);
    if (it == indexByName.constEnd()) {
      if (error != nullptr) *error = QStringLiteral("包内缺少文件: %1").arg(name);
      return false;
    }
    if (!isSafeRelativePath(name)) {
      if (error != nullptr) *error = QStringLiteral("包内路径非法: %1").arg(name);
      return false;
    }
    QByteArray data;
    if (!readEntryData(file, entries.at(it.value()), &data, error)) {
      return false;
    }
    if (!expectedSha.isEmpty() && sha256OfBytes(data) != expectedSha) {
      if (error != nullptr) *error = QStringLiteral("文件校验失败（内容被修改）: %1").arg(name);
      return false;
    }
    const QString target = dest.absoluteFilePath(name);
    const QFileInfo targetInfo(target);
    if (!QDir().mkpath(targetInfo.absolutePath())) {
      if (error != nullptr) *error = QStringLiteral("无法创建目录: %1").arg(targetInfo.absolutePath());
      return false;
    }
    QFile out(target);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      if (error != nullptr) *error = QStringLiteral("无法写入文件: %1").arg(target);
      return false;
    }
    if (!writeAll(&out, data)) {
      if (error != nullptr) *error = QStringLiteral("写入文件失败: %1").arg(target);
      return false;
    }
    return true;
  };

  if (manifest.formatVersion > 0) {
    for (const FileEntry &entry : manifest.files) {
      if (!extractEntryTo(entry.path, entry.sha256)) {
        return false;
      }
    }
  } else {
    for (const ZipEntry &entry : entries) {
      if (entry.name.endsWith(QLatin1Char('/'))) {
        continue;
      }
      if (!isSafeRelativePath(entry.name)) {
        if (error != nullptr) *error = QStringLiteral("包内路径非法: %1").arg(entry.name);
        return false;
      }
      QByteArray data;
      if (!readEntryData(file, entry, &data, error)) {
        return false;
      }
      const QString target = dest.absoluteFilePath(entry.name);
      if (!QDir().mkpath(QFileInfo(target).absolutePath())) {
        return false;
      }
      QFile out(target);
      if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
          !writeAll(&out, data)) {
        if (error != nullptr) *error = QStringLiteral("写入文件失败: %1").arg(target);
        return false;
      }
    }
  }

  // 3. 主控文件必须存在
  if (!QFileInfo::exists(dest.absoluteFilePath(manifest.master))) {
    if (error != nullptr) *error = QStringLiteral("工程包内缺少主控文档: %1").arg(manifest.master);
    return false;
  }

  if (manifestOut != nullptr) {
    *manifestOut = manifest;
  }
  return true;
}
