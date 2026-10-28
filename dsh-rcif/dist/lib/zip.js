/**
 * Minimal, dependency-free ZIP reader for .rcif packages.
 *
 * Supports stored (0) and deflate (8) entries, rejects Zip64, path traversal,
 * duplicate names and oversized archives. RCIF packages are small (KB–MB),
 * so reading the archive into memory keeps the reader simple and bounded.
 */
'use strict';

const fs = require('fs');
const zlib = require('zlib');

const EOCD_SIG = 0x06054b50;
const CEN_SIG = 0x02014b50;
const LOC_SIG = 0x04034b50;
const MAX_ARCHIVE_BYTES = 128 * 1024 * 1024;
const MAX_ENTRIES = 500;
const MAX_FILE_BYTES = 100 * 1024 * 1024;
const MAX_TOTAL_BYTES = 400 * 1024 * 1024;

class ZipError extends Error {
  constructor(message, code = 'ZIP_ERROR') {
    super(message);
    this.code = code;
  }
}

function decodeName(buffer, utf8) {
  return utf8 ? buffer.toString('utf8') : buffer.toString('latin1');
}

function verifyEntryName(name) {
  if (!name || name.includes('\0')) throw new ZipError('ZIP 条目名非法', 'ZIP_BAD_NAME');
  const normalized = name.replace(/\\/g, '/');
  if (normalized.startsWith('/') || /^[A-Za-z]:/.test(normalized)) {
    throw new ZipError(`ZIP 条目使用绝对路径: ${name}`, 'ZIP_ABSOLUTE_PATH');
  }
  if (normalized.split('/').includes('..')) {
    throw new ZipError(`ZIP 条目包含目录穿越: ${name}`, 'ZIP_TRAVERSAL');
  }
  return normalized;
}

/**
 * @param {string} file absolute .rcif path
 * @returns {{ buffer: Buffer, entries: Map<string, object> }}
 */
function readZip(file) {
  const stat = fs.statSync(file);
  if (!stat.isFile()) throw new ZipError(`不是文件: ${file}`, 'NOT_A_FILE');
  if (stat.size > MAX_ARCHIVE_BYTES) throw new ZipError('构件包超过 128MB 上限', 'ZIP_TOO_LARGE');
  const buffer = fs.readFileSync(file);

  let eocd = -1;
  const min = Math.max(0, buffer.length - 22 - 0xffff);
  for (let i = buffer.length - 22; i >= min; i -= 1) {
    if (buffer.readUInt32LE(i) === EOCD_SIG) {
      eocd = i;
      break;
    }
  }
  if (eocd < 0) throw new ZipError('不是合法 ZIP（缺少 EOCD）', 'ZIP_NO_EOCD');

  const count = buffer.readUInt16LE(eocd + 10);
  const centralSize = buffer.readUInt32LE(eocd + 12);
  const centralOffset = buffer.readUInt32LE(eocd + 16);
  if (count === 0xffff || centralOffset === 0xffffffff) {
    throw new ZipError('不支持 Zip64 构件包', 'ZIP64_UNSUPPORTED');
  }
  if (count > MAX_ENTRIES) throw new ZipError(`条目数 ${count} 超过 ${MAX_ENTRIES} 上限`, 'ZIP_TOO_MANY_ENTRIES');
  if (centralOffset + centralSize > buffer.length) throw new ZipError('ZIP 中央目录越界', 'ZIP_BAD_CENTRAL_DIRECTORY');

  const entries = new Map();
  let totalUncompressed = 0;
  let pointer = centralOffset;
  for (let index = 0; index < count; index += 1) {
    if (buffer.readUInt32LE(pointer) !== CEN_SIG) throw new ZipError('ZIP 中央目录项签名错误', 'ZIP_BAD_ENTRY');
    const flags = buffer.readUInt16LE(pointer + 8);
    const method = buffer.readUInt16LE(pointer + 10);
    const compressedSize = buffer.readUInt32LE(pointer + 20);
    const uncompressedSize = buffer.readUInt32LE(pointer + 24);
    const nameLength = buffer.readUInt16LE(pointer + 28);
    const extraLength = buffer.readUInt16LE(pointer + 30);
    const commentLength = buffer.readUInt16LE(pointer + 32);
    const localOffset = buffer.readUInt32LE(pointer + 42);
    if (compressedSize === 0xffffffff || uncompressedSize === 0xffffffff || localOffset === 0xffffffff) {
      throw new ZipError('不支持 Zip64 条目', 'ZIP64_UNSUPPORTED');
    }
    const name = verifyEntryName(decodeName(buffer.subarray(pointer + 46, pointer + 46 + nameLength), (flags & 0x800) !== 0));
    if (entries.has(name)) throw new ZipError(`ZIP 条目重复: ${name}`, 'ZIP_DUPLICATE_ENTRY');
    if (uncompressedSize > MAX_FILE_BYTES) throw new ZipError(`条目 ${name} 超过 100MB`, 'ZIP_ENTRY_TOO_LARGE');
    totalUncompressed += uncompressedSize;
    if (totalUncompressed > MAX_TOTAL_BYTES) throw new ZipError('解压总尺寸超过 400MB', 'ZIP_TOTAL_TOO_LARGE');

    entries.set(name, {
      name,
      method,
      compressedSize,
      uncompressedSize,
      localOffset,
      crc32: buffer.readUInt32LE(pointer + 16),
    });
    pointer += 46 + nameLength + extraLength + commentLength;
  }
  return { buffer, entries };
}

function extractEntry(zip, name) {
  const entry = zip.entries.get(name);
  if (!entry) throw new ZipError(`ZIP 中不存在条目: ${name}`, 'ZIP_ENTRY_NOT_FOUND');
  const local = entry.localOffset;
  if (local + 30 > zip.buffer.length || zip.buffer.readUInt32LE(local) !== LOC_SIG) {
    throw new ZipError(`ZIP 本地头损坏: ${name}`, 'ZIP_BAD_LOCAL_HEADER');
  }
  const nameLength = zip.buffer.readUInt16LE(local + 26);
  const extraLength = zip.buffer.readUInt16LE(local + 28);
  const start = local + 30 + nameLength + extraLength;
  const end = start + entry.compressedSize;
  if (end > zip.buffer.length) throw new ZipError(`ZIP 数据越界: ${name}`, 'ZIP_BAD_DATA');
  const raw = zip.buffer.subarray(start, end);

  let data;
  if (entry.method === 0) data = Buffer.from(raw);
  else if (entry.method === 8) data = zlib.inflateRawSync(raw, { maxOutputLength: MAX_FILE_BYTES });
  else throw new ZipError(`不支持的压缩方法 ${entry.method}: ${name}`, 'ZIP_UNSUPPORTED_METHOD');
  if (data.length > MAX_FILE_BYTES) throw new ZipError(`条目解压超过 100MB: ${name}`, 'ZIP_ENTRY_TOO_LARGE');
  return data;
}

module.exports = {
  ZipError,
  readZip,
  extractEntry,
  MAX_ARCHIVE_BYTES,
  MAX_FILE_BYTES,
};
