#ifndef TEXT_ENCODING_H
#define TEXT_ENCODING_H

#include <QByteArray>
#include <QString>

#include <TCollection_ExtendedString.hxx>

#include <cstring>
#include <string>
#include <vector>

//! OCAF / ActData 文本解码工具。
//!
//! 历史数据（例如早期 ASI→CBF 转换脚本）曾把 UTF-8 字节按“每个字节一个字符”
//! 写进 TCollection_ExtendedString，读出来就是 桥梁 → æ¡¥æ¢ 这类乱码：
//! 存储的码点其实是 UTF-8 字节本身，只是被逐个塞进了 UTF-16 字符里。
//! 下面的解码会在读取时识别并还原这类数据，正常的 Unicode 文本不受影响。
namespace TextEncoding {

//! 判断“Latin-1 码点序列”其实是不是被错写的 UTF-8 字节流。
//! 只有整体是合法 UTF-8 且含多字节序列（往返编码一致）时才认为需要还原，
//! 这样真正的 Latin-1 文本（例如 Café）不会被误改。
inline bool RepairUtf8FromLatin1Bytes(const std::vector<char> &theBytes, QString &theResult)
{
  if (theBytes.empty()) {
    return false;
  }

  bool hasHighByte = false;
  for (const char aByte : theBytes) {
    if (static_cast<unsigned char>(aByte) >= 0x80) {
      hasHighByte = true;
      break;
    }
  }
  if (!hasHighByte) {
    return false; // 纯 ASCII，无需修复
  }

  const QString aDecoded = QString::fromUtf8(theBytes.data(), static_cast<int>(theBytes.size()));
  if (aDecoded.contains(QChar::ReplacementCharacter)) {
    return false; // 非法 UTF-8 序列，说明确实是 Latin-1 文本
  }

  const QByteArray aReEncoded = aDecoded.toUtf8();
  if (aReEncoded.size() != static_cast<int>(theBytes.size()) ||
      std::memcmp(aReEncoded.constData(), theBytes.data(), theBytes.size()) != 0) {
    return false; // 往返不一致，不能确认是被错写的 UTF-8
  }

  theResult = aDecoded;
  return true;
}

//! TCollection_ExtendedString → QString，必要时修复上述乱码。
inline QString FromExtendedString(const TCollection_ExtendedString &theText)
{
  const Standard_Integer aNbChars = theText.Length();
  if (aNbChars <= 0) {
    return QString();
  }

  const char16_t *aRaw = theText.ToExtString();
  std::vector<char> aLatin1Bytes;
  aLatin1Bytes.reserve(static_cast<size_t>(aNbChars));
  bool hasWideChar = false;
  for (Standard_Integer i = 0; i < aNbChars; ++i) {
    if (aRaw[i] > 0xFF) {
      hasWideChar = true;
      break;
    }
    aLatin1Bytes.push_back(static_cast<char>(aRaw[i] & 0xFF));
  }

  if (hasWideChar) {
    // 正常的宽字符文本：按 UTF-8 输出即可
    std::vector<char> aBuffer(static_cast<size_t>(theText.LengthOfCString()) + 1, '\0');
    Standard_PCharacter aCursor = aBuffer.data();
    const int aLength = theText.ToUTF8CString(aCursor);
    return QString::fromUtf8(aBuffer.data(), aLength > 0 ? aLength : 0);
  }

  QString aRepaired;
  if (RepairUtf8FromLatin1Bytes(aLatin1Bytes, aRepaired)) {
    return aRepaired;
  }
  return QString::fromLatin1(aLatin1Bytes.data(), static_cast<int>(aLatin1Bytes.size()));
}

//! std::string(UTF-8) 版本，供以 std::string 为接口的地方使用。
inline std::string ToStdString(const TCollection_ExtendedString &theText)
{
  return FromExtendedString(theText).toStdString();
}

} // namespace TextEncoding

#endif // TEXT_ENCODING_H
