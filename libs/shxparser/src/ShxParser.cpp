/*
 * ShxParser - C++ AutoCAD SHX Font File Parser
 *
 * Implementation file for SHX font parsing.
 *
 * Author: Generated for QwenCodeWS project
 * License: MIT
 */

#define _USE_MATH_DEFINES
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <stack>
#include <string>
#include <vector>

#include "../include/ShxParser.h"

// M_PI definition for MSVC
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace shx {

//=============================================================================
// Constants
//=============================================================================

static const char *SHX_VERSION = "1.0.0";

// SHX file magic numbers
static const char MAGIC_SHAPES[] = "AutoCAD-86 shapes 1.0";
static const char MAGIC_BIGFONT[] = "AutoCAD-86 bigfont 1.0";
static const char MAGIC_UNIFONT[] = "AutoCAD-86 unifont 1.0";

// Octant direction vector table (0-15 directions)
static const double DIRECTION_X[16] = {1.0,  1.0,  0.5,  0.0,  -0.5, -1.0,
                                       -1.0, -1.0, -1.0, -1.0, -0.5, 0.0,
                                       0.5,  1.0,  1.0,  1.0};

static const double DIRECTION_Y[16] = {0.0,  0.5,  1.0,  1.0,  1.0,  0.5,
                                       0.0,  -0.5, -1.0, -1.0, -1.0, -1.0,
                                       -1.0, -0.5, 0.0,  0.5};

// Normalized direction vectors (for precise calculation)
static const double DIR_VECTORS[16][2] = {
    {1.0, 0.0},   // 0: East
    {1.0, 0.5},   // 1: ENE (1, 0.5)
    {1.0, 1.0},   // 2: NE  (1, 1)
    {0.5, 1.0},   // 3: NNE (0.5, 1)
    {0.0, 1.0},   // 4: North
    {-0.5, 1.0},  // 5: NNW (-0.5, 1)
    {-1.0, 1.0},  // 6: NW  (-1, 1)
    {-1.0, 0.5},  // 7: WNW (-1, 0.5)
    {-1.0, 0.0},  // 8: West
    {-1.0, -0.5}, // 9: WSW (-1, -0.5)
    {-1.0, -1.0}, // 10: SW (-1, -1)
    {-0.5, -1.0}, // 11: SSW (-0.5, -1)
    {0.0, -1.0},  // 12: South
    {0.5, -1.0},  // 13: SSE (0.5, -1)
    {1.0, -1.0},  // 14: SE  (1, -1)
    {1.0, -0.5}   // 15: ESE (1, -0.5)
};

// Special command codes
enum SpecialCommand : uint8_t {
  CMD_END_OF_SHAPE = 0x00,
  CMD_PEN_DOWN = 0x01,
  CMD_PEN_UP = 0x02,
  CMD_DIVIDE_VECTOR = 0x03,
  CMD_MULTIPLY_VECTOR = 0x04,
  CMD_PUSH_STACK = 0x05,
  CMD_POP_STACK = 0x06,
  CMD_DRAW_SUBSHAPE = 0x07,
  CMD_XY_DISPLACEMENT = 0x08,
  CMD_POLY_XY_DISPLACEMENT = 0x09,
  CMD_OCTANT_ARC = 0x0A,
  CMD_FRACTIONAL_ARC = 0x0B,
  CMD_BULGE_ARC = 0x0C,
  CMD_POLY_BULGE_ARC = 0x0D,
  CMD_COND_MODE_2 = 0x0E
};

//=============================================================================
// Helper Functions
//=============================================================================

static inline int8_t readSignedByte(const uint8_t *data, size_t &pos) {
  return static_cast<int8_t>(data[pos++]);
}

static inline uint8_t readByte(const uint8_t *data, size_t &pos) {
  return data[pos++];
}

static inline uint16_t readUInt16LE(const uint8_t *data, size_t &pos) {
  uint16_t result = data[pos] | (data[pos + 1] << 8);
  pos += 2;
  return result;
}

static inline int16_t readInt16LE(const uint8_t *data, size_t &pos) {
  return static_cast<int16_t>(readUInt16LE(data, pos));
}

static inline uint32_t readUInt32LE(const uint8_t *data, size_t &pos) {
  uint32_t result = data[pos] | (data[pos + 1] << 8) | (data[pos + 2] << 16) |
                    (data[pos + 3] << 24);
  pos += 4;
  return result;
}

static std::vector<uint32_t> utf8ToCodePoints(const std::string &str) {
  std::vector<uint32_t> result;
  size_t i = 0;
  while (i < str.size()) {
    uint32_t cp = 0;
    uint8_t c = static_cast<uint8_t>(str[i]);

    if ((c & 0x80) == 0) {
      cp = c;
      i += 1;
    } else if ((c & 0xE0) == 0xC0) {
      cp = (c & 0x1F) << 6;
      if (i + 1 < str.size()) {
        cp |= (static_cast<uint8_t>(str[i + 1]) & 0x3F);
      }
      i += 2;
    } else if ((c & 0xF0) == 0xE0) {
      cp = (c & 0x0F) << 12;
      if (i + 1 < str.size()) {
        cp |= (static_cast<uint8_t>(str[i + 1]) & 0x3F) << 6;
      }
      if (i + 2 < str.size()) {
        cp |= (static_cast<uint8_t>(str[i + 2]) & 0x3F);
      }
      i += 3;
    } else if ((c & 0xF8) == 0xF0) {
      cp = (c & 0x07) << 18;
      if (i + 1 < str.size()) {
        cp |= (static_cast<uint8_t>(str[i + 1]) & 0x3F) << 12;
      }
      if (i + 2 < str.size()) {
        cp |= (static_cast<uint8_t>(str[i + 2]) & 0x3F) << 6;
      }
      if (i + 3 < str.size()) {
        cp |= (static_cast<uint8_t>(str[i + 3]) & 0x3F);
      }
      i += 4;
    } else {
      i += 1;
      continue;
    }

    result.push_back(cp);
  }
  return result;
}

//=============================================================================
// ShxFont::Impl Implementation
//=============================================================================

class ShxFont::Impl {
public:
  Impl() = default;
  ~Impl() = default;

  bool load(const std::string &filename);
  bool loadFromMemory(const uint8_t *data, size_t size);

  const Glyph *getGlyph(uint32_t code) const;
  bool hasGlyph(uint32_t code) const;

  void render(IPathRenderer &renderer, const std::string &text, double fontSize,
              double x, double y) const;

  void renderGlyph(IPathRenderer &renderer, const Glyph *glyph, double scale,
                   double originX, double originY) const;

  double measureText(const std::string &text, double fontSize) const;

  // Properties
  ShxFontType fontType = ShxFontType::Unknown;
  std::string fontName;
  double baseHeight = 0.0;
  double descender = 0.0;
  double defWidth = 0.0;
  std::string lastError;
  bool valid = false;

  // Glyph data
  std::map<uint32_t, Glyph> glyphs;

private:
  bool parseHeader(const uint8_t *data, size_t size, size_t &pos);
  bool parseShapes(const uint8_t *data, size_t size, size_t pos);
  bool parseBigFont(const uint8_t *data, size_t size, size_t pos);
  bool parseUniFont(const uint8_t *data, size_t size, size_t pos);

  void parseGlyphData(const uint8_t *data, size_t dataSize, Glyph &glyph);

  void compileAll();
  void compileGlyph(const std::vector<uint8_t> &sourceData,
                    std::vector<DrawCommand> &targetCommands,
                    std::stack<Point2D> &posStack, bool &penDown, double scale,
                    double x, double y, int depth);
};

bool ShxFont::Impl::load(const std::string &filename) {
  std::ifstream file(filename, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    lastError = "Cannot open file: " + filename;
    std::cout << "ShxParser: " << lastError << std::endl;
    return false;
  }

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> buffer(static_cast<size_t>(size));
  if (!file.read(reinterpret_cast<char *>(buffer.data()), size)) {
    lastError = "Failed to read file: " + filename;
    return false;
  }

  return loadFromMemory(buffer.data(), buffer.size());
}

bool ShxFont::Impl::loadFromMemory(const uint8_t *data, size_t size) {
  if (size < 32) {
    lastError = "File too small to be a valid SHX file";
    return false;
  }

  size_t pos = 0;

  if (!parseHeader(data, size, pos)) {
    return false;
  }

  bool result = false;
  switch (fontType) {
  case ShxFontType::Shapes:
    result = parseShapes(data, size, pos);
    break;
  case ShxFontType::BigFont:
    result = parseBigFont(data, size, pos);
    break;
  case ShxFontType::UniFont:
    result = parseUniFont(data, size, pos);
    break;
  default:
    lastError = "Unknown font type";
    return false;
  }

  if (result) {
    compileAll();
  }

  valid = result;
  return result;
}

bool ShxFont::Impl::parseHeader(const uint8_t *data, size_t size, size_t &pos) {
  size_t headerEnd = 0;
  for (size_t i = 0; i < std::min(size, size_t(128)); ++i) {
    if (data[i] == 0x1A) {
      headerEnd = i;
      break;
    }
  }

  if (headerEnd == 0) {
    lastError = "Invalid SHX file header - no 0x1A found in first 128 bytes";
    return false;
  }

  std::string header(reinterpret_cast<const char *>(data), headerEnd);

  if (header.find("shapes") != std::string::npos ||
      header.find("Shapes") != std::string::npos) {
    fontType = ShxFontType::Shapes;
  } else if (header.find("bigfont") != std::string::npos ||
             header.find("Bigfont") != std::string::npos) {
    fontType = ShxFontType::BigFont;
  } else if (header.find("unifont") != std::string::npos ||
             header.find("Unifont") != std::string::npos) {
    fontType = ShxFontType::UniFont;
  } else {
    fontType = ShxFontType::Shapes;
  }

  // After the header text (terminated by \r or \n), the Python shxparser
  // does: read_string(f) which stops at \r, then f.read(2) which skips
  // the \n and 0x1A bytes. Our headerEnd points to 0x1A.
  // We need to position right after 0x1A, but the header text ends with
  // \r\n before the 0x1A. The data starts immediately after 0x1A.
  // Skip exactly 1 byte after 0x1A (padding null) to align properly.
  pos = headerEnd + 1;

  // For shapes 1.0: data starts IMMEDIATELY after 0x1A + any minimal padding
  // Don't skip 0x00 bytes - they may be part of the first u16 (start=0)

  return true;
}

static inline uint16_t swap16(uint16_t val) { return (val << 8) | (val >> 8); }

bool ShxFont::Impl::parseShapes(const uint8_t *data, size_t size, size_t pos) {
  if (pos + 6 > size) {
    lastError = "Unexpected end of file in shapes header";
    return false;
  }

  // Shapes 1.0 format (from tatarize/shxparser):
  // u16 start - starting shape number (usually 0)
  // u16 end   - ending shape number
  // u16 count - number of index table entries
  // Index table: count * (u16 shapeNum, u16 length)
  // Data section: sequential glyph data, read in index order

  uint16_t startVal = readUInt16LE(data, pos);
  uint16_t endVal = readUInt16LE(data, pos);
  uint16_t count = readUInt16LE(data, pos);
  (void)startVal;
  (void)endVal;

  if (count == 0)
    return false;

  // Phase 1: Read the index table
  struct ShapeEntry {
    uint16_t shapeNum;
    uint16_t length;
  };

  std::vector<ShapeEntry> indexTable;
  indexTable.reserve(count);

  for (uint16_t i = 0; i < count; ++i) {
    if (pos + 4 > size)
      break;
    ShapeEntry entry;
    entry.shapeNum = readUInt16LE(data, pos);
    entry.length = readUInt16LE(data, pos);
    indexTable.push_back(entry);
  }

  // Phase 2: Read glyph data sequentially
  for (const auto &entry : indexTable) {
    if (entry.shapeNum == 0) {
      // Shape 0: font metrics (stream-read, NOT length-based)
      // Format: name (null-terminated string), above (u8), below (u8), modes
      // (u8)

      // Skip name string until null terminator
      while (pos < size && data[pos] != 0) {
        pos++;
      }
      pos++; // Skip null terminator

      if (pos < size) {
        baseHeight = data[pos++];
      }
      if (pos < size) {
        descender = static_cast<int8_t>(data[pos++]);
      }
      if (pos < size) {
        pos++; // modes byte (skip)
      }
    } else {
      if (pos + entry.length > size)
        break;

      Glyph glyph;
      glyph.code = entry.shapeNum;

      parseGlyphData(data + pos, entry.length, glyph);

      glyphs[entry.shapeNum] = std::move(glyph);

      pos += entry.length;
    }
  }

  return !glyphs.empty();
}

bool ShxFont::Impl::parseBigFont(const uint8_t *data, size_t size, size_t pos) {
  if (pos + 6 > size) {
    lastError = "Unexpected end of file in bigfont header";
    return false;
  }

  // BigFont header format (from Python shxparser):
  // - count (2 bytes): number of initial glyph definitions
  // - length (2 bytes): number of ADDITIONAL glyph definitions
  // - changeCount (2 bytes): number of character ranges
  // - ranges (changeCount * 4 bytes): start-end pairs for double-byte ranges
  // - glyph_ref ((count + length) * 8 bytes): index table
  //   - index (2 bytes): character code
  //   - length (2 bytes): data length
  //   - offset (4 bytes): absolute file offset

  uint16_t count = readUInt16LE(data, pos);
  uint16_t lengthField = readUInt16LE(data, pos);
  uint16_t changeCount = readUInt16LE(data, pos);

  // Total glyph count = count + lengthField
  uint32_t totalGlyphs =
      static_cast<uint32_t>(count) + static_cast<uint32_t>(lengthField);

  // Read ranges (for double-byte character detection)
  std::vector<std::pair<uint16_t, uint16_t>> ranges;
  for (uint16_t i = 0; i < changeCount && pos + 4 <= size; ++i) {
    uint16_t start = readUInt16LE(data, pos);
    uint16_t end = readUInt16LE(data, pos);
    ranges.emplace_back(start, end);
  }

  // Read glyph index table
  struct GlyphRef {
    uint16_t index;
    uint16_t length;
    uint32_t offset;
  };

  std::vector<GlyphRef> glyphRefs;
  for (uint32_t i = 0; i < totalGlyphs && pos + 8 <= size; ++i) {
    GlyphRef ref;
    ref.index = readUInt16LE(data, pos);
    ref.length = readUInt16LE(data, pos);
    ref.offset = readUInt32LE(data, pos);
    glyphRefs.push_back(ref);
  }

  // Parse each glyph using the offset table
  for (const auto &ref : glyphRefs) {
    if (ref.offset + ref.length > size) {
      continue;
    }

    if (ref.index == 0) {
      // Shape 0 contains font info
      // Format: name (null-terminated), above, below, modes
      size_t fontInfoPos = ref.offset;

      // Skip name string
      while (fontInfoPos < ref.offset + ref.length && data[fontInfoPos] != 0) {
        fontInfoPos++;
      }
      fontInfoPos++; // Skip null terminator

      if (fontInfoPos < ref.offset + ref.length) {
        baseHeight = data[fontInfoPos++];
      }
      if (fontInfoPos < ref.offset + ref.length) {
        descender = static_cast<int8_t>(data[fontInfoPos++]);
      }
      // modes byte follows but we don't need it
    } else {
      Glyph glyph;
      glyph.code = ref.index;

      // BigFont glyph data starts with width info:
      // - Double-byte chars (index >= 256): 2 bytes (width + padding)
      // - Single-byte chars: 1 byte (width)
      // Skip these bytes so they aren't misinterpreted as drawing commands.
      size_t skipBytes = (ref.index >= 256) ? 2 : 1;
      if (ref.length > skipBytes) {
        // First byte is the character width (horizontal advance)
        glyph.width = data[ref.offset];
        parseGlyphData(data + ref.offset + skipBytes, ref.length - skipBytes,
                       glyph);
      }

      glyphs[ref.index] = std::move(glyph);
    }
  }

  return !glyphs.empty();
}

bool ShxFont::Impl::parseUniFont(const uint8_t *data, size_t size, size_t pos) {
  if (pos + 6 > size) {
    lastError = "Unexpected end of file in unifont header";
    return false;
  }

  // UniFont header:
  // - count (4 bytes): number of glyphs
  // - length (2 bytes): length of font description
  // - description (length bytes)
  // - glyphs (sequential)...

  uint32_t count = readUInt32LE(data, pos);
  uint16_t descLen = readUInt16LE(data, pos);

  // Skip font description
  if (pos + descLen > size) {
    lastError = "Unexpected end of file in unifont description";
    return false;
  }
  pos += descLen;

  for (uint32_t i = 0; i < count; ++i) {
    if (pos + 4 > size)
      break;

    uint16_t index = readUInt16LE(data, pos);
    uint16_t cmdLen = readUInt16LE(data, pos);

    if (pos + cmdLen > size)
      break;

    Glyph glyph;
    glyph.code = index;

    parseGlyphData(data + pos, cmdLen, glyph);

    glyphs[index] = std::move(glyph);

    pos += cmdLen;
  }

  return !glyphs.empty();
}

void ShxFont::Impl::parseGlyphData(const uint8_t *data, size_t dataSize,
                                   Glyph &glyph) {
  if (dataSize == 0)
    return;

  // Store raw data for deferred compilation
  glyph.rawData.assign(data, data + dataSize);
}

void ShxFont::Impl::compileAll() {
  for (auto &pair : glyphs) {
    Glyph &glyph = pair.second;
    if (glyph.rawData.empty())
      continue;

    // Initialize parsing state for this glyph
    std::stack<Point2D> posStack;
    bool penDown = true;
    double scale = 1.0;
    double x = 0.0, y = 0.0;

    // Start compilation from scratch
    glyph.commands.clear();
    compileGlyph(glyph.rawData, glyph.commands, posStack, penDown, scale, x, y,
                 0);

    // Calculate width/height after compilation
    if (!glyph.commands.empty()) {
      double minX = 0, maxX = 0, minY = 0, maxY = 0;
      for (const auto &cmd : glyph.commands) {
        minX = std::min(minX, cmd.endPoint.x);
        maxX = std::max(maxX, cmd.endPoint.x);
        minY = std::min(minY, cmd.endPoint.y);
        maxY = std::max(maxY, cmd.endPoint.y);
      }
      glyph.width = maxX - minX;
      glyph.height = maxY - minY;
    }
  }
}

void ShxFont::Impl::compileGlyph(const std::vector<uint8_t> &sourceData,
                                 std::vector<DrawCommand> &targetCommands,
                                 std::stack<Point2D> &posStack, bool &penDown,
                                 double scale, double x, double y, int depth) {
  if (depth > 20)
    return; // Recursion guard

  if (sourceData.empty())
    return;

  size_t pos = 0;
  size_t dataSize = sourceData.size();

  while (pos < dataSize) {
    uint8_t byte = sourceData[pos++];

    uint8_t length = (byte >> 4) & 0x0F;
    uint8_t direction = byte & 0x0F;

    if (length == 0) {
      switch (direction) {
      // ... (skip unchanged cases) ...
      case CMD_END_OF_SHAPE:
        // Treat 0 as a no-op
        break;

      case CMD_PEN_DOWN:
        penDown = true;
        break;

      case CMD_PEN_UP:
        penDown = false;
        break;

      case CMD_DIVIDE_VECTOR:
        if (pos < dataSize) {
          uint8_t divisor = sourceData[pos++];
          if (divisor > 0)
            scale /= divisor;
        }
        break;

      case CMD_MULTIPLY_VECTOR:
        if (pos < dataSize) {
          uint8_t multiplier = sourceData[pos++];
          scale *= multiplier;
        }
        break;

      case CMD_PUSH_STACK:
        posStack.push(Point2D(x, y));
        break;

      case CMD_POP_STACK:
        if (!posStack.empty()) {
          Point2D pt = posStack.top();
          posStack.pop();
          x = pt.x;
          y = pt.y;
          targetCommands.push_back(DrawCommand::moveTo(x, y));
        }
        break;

      case CMD_DRAW_SUBSHAPE: {
        // Shape ID byte count depends on font type:
        // - Shapes and BigFont: 1 byte (shape numbers 0-255)
        // - UniFont: 2 bytes (shape numbers 0-65535)
        uint16_t subShapeId = 0;
        if (fontType == ShxFontType::UniFont) {
          if (pos + 2 <= dataSize) {
            subShapeId = static_cast<uint16_t>(sourceData[pos]) |
                         (static_cast<uint16_t>(sourceData[pos + 1]) << 8);
            pos += 2;
          } else {
            break;
          }
        } else {
          if (pos < dataSize) {
            subShapeId = sourceData[pos++];
          } else {
            break;
          }
        }

        auto it = glyphs.find(subShapeId);
        if (it != glyphs.end()) {
          compileGlyph(it->second.rawData, targetCommands, posStack, penDown,
                       scale, x, y, depth + 1);
          targetCommands.push_back(DrawCommand::moveTo(x, y));
        }
      } break;

      case CMD_XY_DISPLACEMENT: {
        if (pos + 2 <= dataSize) {
          int8_t dx = static_cast<int8_t>(sourceData[pos++]);
          int8_t dy = static_cast<int8_t>(sourceData[pos++]);

          double newX = x + dx * scale;
          double newY = y + dy * scale;

          if (penDown) {
            targetCommands.push_back(DrawCommand::lineTo(newX, newY));
          } else {
            targetCommands.push_back(DrawCommand::moveTo(newX, newY));
          }

          x = newX;
          y = newY;
        }
        break;
      }

      case CMD_POLY_XY_DISPLACEMENT: {
        while (pos + 2 <= dataSize) {
          int8_t dx = static_cast<int8_t>(sourceData[pos++]);
          int8_t dy = static_cast<int8_t>(sourceData[pos++]);

          if (dx == 0 && dy == 0)
            break;

          double newX = x + dx * scale;
          double newY = y + dy * scale;

          if (penDown) {
            targetCommands.push_back(DrawCommand::lineTo(newX, newY));
          } else {
            targetCommands.push_back(DrawCommand::moveTo(newX, newY));
          }

          x = newX;
          y = newY;
        }
        break;
      }

      case CMD_OCTANT_ARC: {
        if (pos + 2 <= dataSize) {
          uint8_t radius = sourceData[pos++];
          int8_t octantSpec = static_cast<int8_t>(sourceData[pos++]);

          int startOct = (octantSpec >> 4) & 0x07;
          int spanOct = octantSpec & 0x07;
          bool ccw = (octantSpec & 0x80) != 0;

          if (spanOct == 0)
            spanOct = 8;

          double r = radius * scale;

          // Split into single octant segments (45 degrees max) to ensure
          // stability
          int steps = spanOct;
          double stepAngle = M_PI / 4.0;
          if (!ccw)
            stepAngle = -stepAngle;

          double currentTheta = startOct * (M_PI / 4.0);

          // Center is calculated relative to current pen at start of arc
          double cx = x - r * cos(currentTheta);
          double cy = y - r * sin(currentTheta);

          for (int i = 0; i < steps; ++i) {
            double nextTheta = currentTheta + stepAngle;

            double segEndX = cx + r * cos(nextTheta);
            double segEndY = cy + r * sin(nextTheta);

            double midTheta = (currentTheta + nextTheta) / 2.0;
            double ctrlX = cx + r * cos(midTheta);
            double ctrlY = cy + r * sin(midTheta);

            if (penDown) {
              targetCommands.push_back(
                  DrawCommand::arcTo(segEndX, segEndY, ctrlX, ctrlY));
            } else {
              targetCommands.push_back(DrawCommand::moveTo(segEndX, segEndY));
            }

            currentTheta = nextTheta;
            x = segEndX;
            y = segEndY;
          }
        }
        break;
      }

      case CMD_FRACTIONAL_ARC: {
        if (pos + 5 <= dataSize) {
          uint8_t startOffset = sourceData[pos++];
          uint8_t endOffset = sourceData[pos++];
          uint8_t highRadius = sourceData[pos++];
          uint8_t radius = sourceData[pos++];
          int8_t octantSpec = static_cast<int8_t>(sourceData[pos++]);

          int startOct = (octantSpec >> 4) & 0x07;
          int spanOct = octantSpec & 0x07;
          bool ccw = (octantSpec & 0x80) != 0;

          if (spanOct == 0)
            spanOct = 8;

          double r = (highRadius * 256.0 + radius) * scale;

          double startFrac = (startOffset / 256.0) * (M_PI / 4.0);
          double endFrac = (endOffset / 256.0) * (M_PI / 4.0);

          double startAngleGlobal = startOct * (M_PI / 4.0) + startFrac;
          double endAngleGlobal = (startOct + spanOct) * (M_PI / 4.0) + endFrac;

          // Correct end angle wrapping/span logic for Fractional
          // Simplistic approach: calculate total span angle
          double rawStart = startOct * 45.0 + (startOffset / 256.0) * 45.0;
          double rawEnd =
              (startOct + spanOct) * 45.0 + (endOffset / 256.0) * 45.0;

          // Adjust rawEnd if it looks wrong?
          // Actually, standard SHX logic:
          // Start is StartOct + Offset.
          // End is StartOct + SpanOct + Offset.
          // So Span is just SpanOct*45 + (EndOff - StartOff).

          double totalSpanDeg =
              spanOct * 45.0 + (endOffset - startOffset) * (45.0 / 256.0);
          if (!ccw)
            totalSpanDeg = -totalSpanDeg;

          double totalSpanRad = totalSpanDeg * M_PI / 180.0;

          // Center calc
          double cx = x - r * cos(startAngleGlobal);
          double cy = y - r * sin(startAngleGlobal);

          // Split if large. Let's split every 45 degrees (approx)
          int steps =
              static_cast<int>(std::ceil(std::abs(totalSpanDeg) / 45.0));
          if (steps < 1)
            steps = 1;

          double stepRad = totalSpanRad / steps;
          double currentTheta = startAngleGlobal;

          for (int i = 0; i < steps; ++i) {
            double nextTheta = currentTheta + stepRad;

            double segEndX = cx + r * cos(nextTheta);
            double segEndY = cy + r * sin(nextTheta);

            double midTheta = (currentTheta + nextTheta) / 2.0;
            double ctrlX = cx + r * cos(midTheta);
            double ctrlY = cy + r * sin(midTheta);

            if (penDown) {
              targetCommands.push_back(
                  DrawCommand::arcTo(segEndX, segEndY, ctrlX, ctrlY));
            } else {
              targetCommands.push_back(DrawCommand::moveTo(segEndX, segEndY));
            }

            currentTheta = nextTheta;
            x = segEndX;
            y = segEndY;
          }
        }
        break;
      }

      case CMD_BULGE_ARC: {
        if (pos + 3 <= dataSize) {
          int8_t dx = static_cast<int8_t>(sourceData[pos++]);
          int8_t dy = static_cast<int8_t>(sourceData[pos++]);
          int8_t bulgeInt = static_cast<int8_t>(sourceData[pos++]);

          double endX = x + dx * scale;
          double endY = y + dy * scale;

          if (bulgeInt == 0) {
            if (penDown) {
              targetCommands.push_back(DrawCommand::lineTo(endX, endY));
            } else {
              targetCommands.push_back(DrawCommand::moveTo(endX, endY));
            }
          } else {
            // Robust Bulge -> Center/Radius Conversion
            double b = bulgeInt / 127.0;
            double rawLen = sqrt(double(dx) * dx + double(dy) * dy);

            if (rawLen < 1e-9) {
              // Degenerate chord, treat as point/line
              if (penDown)
                targetCommands.push_back(DrawCommand::lineTo(endX, endY));
              else
                targetCommands.push_back(DrawCommand::moveTo(endX, endY));
            } else {
              double halfChord = (rawLen * scale) / 2.0;
              // Distance from chord midpoint to center
              // c = (L/2) * (1/b - b)
              double c_dist = halfChord * (1.0 / b - b);

              double midX = (x + endX) / 2.0;
              double midY = (y + endY) / 2.0;

              // Normal vector (-dy, dx) normalized
              double nx = -dy / rawLen;
              double ny = dx / rawLen;

              double cx = midX + nx * c_dist;
              double cy = midY + ny * c_dist;

              double radius = sqrt(pow(x - cx, 2) + pow(y - cy, 2));

              double startAngle = atan2(y - cy, x - cx);
              double endAngle = atan2(endY - cy, endX - cx);

              // Handle angle wrapping
              double span = endAngle - startAngle;
              if (b > 0) { // CCW
                if (span <= -1e-9)
                  span += 2 * M_PI;
              } else { // CW
                if (span >= 1e-9)
                  span -= 2 * M_PI;
              }

              // Split into segments (max 45 deg)
              int steps =
                  static_cast<int>(std::ceil(std::abs(span) / (M_PI / 4.0)));
              if (steps < 1)
                steps = 1;
              double stepAngle = span / steps;

              double currentTheta = startAngle;
              double currentX = x; // Start from exact current pos to avoid gaps
              double currentY = y;

              for (int i = 0; i < steps; ++i) {
                double nextTheta = currentTheta + stepAngle;

                // Calculate target to effectively normalize radius precision
                double segEndX = cx + radius * cos(nextTheta);
                double segEndY = cy + radius * sin(nextTheta);

                // Force final point to match exact end point on last step
                if (i == steps - 1) {
                  segEndX = endX;
                  segEndY = endY;
                }

                double midTheta = (currentTheta + nextTheta) / 2.0;
                double ctrlX = cx + radius * cos(midTheta);
                double ctrlY = cy + radius * sin(midTheta);

                if (penDown) {
                  targetCommands.push_back(
                      DrawCommand::arcTo(segEndX, segEndY, ctrlX, ctrlY));
                } else {
                  targetCommands.push_back(
                      DrawCommand::moveTo(segEndX, segEndY));
                }

                currentTheta = nextTheta;
                currentX = segEndX;
                currentY = segEndY;
              }
            }
          }

          x = endX;
          y = endY;
        }
        break;
      }

      case CMD_POLY_BULGE_ARC: {
        while (pos + 2 <= dataSize) {
          int8_t dx = static_cast<int8_t>(sourceData[pos++]);
          int8_t dy = static_cast<int8_t>(sourceData[pos++]);

          if (dx == 0 && dy == 0)
            break;

          int8_t bulgeInt = 0;
          if (pos < dataSize) {
            bulgeInt = static_cast<int8_t>(sourceData[pos++]);
          }

          double endX = x + dx * scale;
          double endY = y + dy * scale;

          if (bulgeInt == 0) {
            if (penDown) {
              targetCommands.push_back(DrawCommand::lineTo(endX, endY));
            } else {
              targetCommands.push_back(DrawCommand::moveTo(endX, endY));
            }
          } else {
            // Robust Bulge -> Center/Radius Conversion
            double b = bulgeInt / 127.0;
            double rawLen = sqrt(double(dx) * dx + double(dy) * dy);

            if (rawLen < 1e-9) {
              if (penDown)
                targetCommands.push_back(DrawCommand::lineTo(endX, endY));
              else
                targetCommands.push_back(DrawCommand::moveTo(endX, endY));
            } else {
              double halfChord = (rawLen * scale) / 2.0;
              // Distance from chord midpoint to center
              double c_dist = halfChord * (1.0 / b - b);

              double midX = (x + endX) / 2.0;
              double midY = (y + endY) / 2.0;

              double nx = -dy / rawLen;
              double ny = dx / rawLen;

              double cx = midX + nx * c_dist;
              double cy = midY + ny * c_dist;

              double radius = sqrt(pow(x - cx, 2) + pow(y - cy, 2));

              double startAngle = atan2(y - cy, x - cx);
              double endAngle = atan2(endY - cy, endX - cx);

              double span = endAngle - startAngle;
              if (b > 0) { // CCW
                if (span <= -1e-9)
                  span += 2 * M_PI;
              } else { // CW
                if (span >= 1e-9)
                  span -= 2 * M_PI;
              }

              int steps =
                  static_cast<int>(std::ceil(std::abs(span) / (M_PI / 4.0)));
              if (steps < 1)
                steps = 1;
              double stepAngle = span / steps;

              double currentTheta = startAngle;

              for (int i = 0; i < steps; ++i) {
                double nextTheta = currentTheta + stepAngle;

                double segEndX = cx + radius * cos(nextTheta);
                double segEndY = cy + radius * sin(nextTheta);

                if (i == steps - 1) {
                  segEndX = endX;
                  segEndY = endY;
                }

                double midTheta = (currentTheta + nextTheta) / 2.0;
                double ctrlX = cx + radius * cos(midTheta);
                double ctrlY = cy + radius * sin(midTheta);

                if (penDown) {
                  targetCommands.push_back(
                      DrawCommand::arcTo(segEndX, segEndY, ctrlX, ctrlY));
                } else {
                  targetCommands.push_back(
                      DrawCommand::moveTo(segEndX, segEndY));
                }

                currentTheta = nextTheta;
              }
            }
          }

          x = endX;
          y = endY;
        }
        break;
      }

      case CMD_COND_MODE_2:
        if (pos < dataSize) {
          pos++;
        }
        break;

      default:
        break;
      }
    } else {
      // Standard length + direction
      int dir = direction;
      double len = length * scale;
      double dx = DIR_VECTORS[dir][0] * len;
      double dy = DIR_VECTORS[dir][1] * len;

      double endX = x + dx;
      double endY = y + dy;

      if (penDown) {
        targetCommands.push_back(DrawCommand::lineTo(endX, endY));
      } else {
        targetCommands.push_back(DrawCommand::moveTo(endX, endY));
      }

      x = endX;
      y = endY;
    }
  }
}

const Glyph *ShxFont::Impl::getGlyph(uint32_t code) const {
  auto it = glyphs.find(code);
  if (it != glyphs.end()) {
    return &it->second;
  }
  return nullptr;
}

bool ShxFont::Impl::hasGlyph(uint32_t code) const {
  return glyphs.find(code) != glyphs.end();
}

void ShxFont::Impl::renderGlyph(IPathRenderer &renderer, const Glyph *glyph,
                                double scale, double originX,
                                double originY) const {
  if (!glyph)
    return;

  for (const auto &cmd : glyph->commands) {
    double px = originX + cmd.endPoint.x * scale;
    double py = originY + cmd.endPoint.y * scale;

    switch (cmd.type) {
    case CommandType::MoveTo:
      renderer.moveTo(px, py);
      break;
    case CommandType::LineTo:
      renderer.lineTo(px, py);
      break;
    case CommandType::ArcTo: {
      double cx = originX + cmd.controlPoint.x * scale;
      double cy = originY + cmd.controlPoint.y * scale;
      renderer.arcTo(px, py, cx, cy);
      break;
    }
    // SubShapes are now inlined, so no need to handle CommandType::SubShape
    default:
      break;
    }
  }
}

void ShxFont::Impl::render(IPathRenderer &renderer, const std::string &text,
                           double fontSize, double startX,
                           double startY) const {
  if (!valid)
    return;

  double scale = (baseHeight > 0) ? (fontSize / baseHeight) : fontSize;
  double x = startX;
  double y = startY;

  auto codePoints = utf8ToCodePoints(text);

  for (uint32_t code : codePoints) {
    const Glyph *glyph = getGlyph(code);
    if (glyph) {
      renderGlyph(renderer, glyph, scale, x, y);
      x += glyph->width * scale;
    } else {
      x += defWidth * scale;
    }
  }
}

double ShxFont::Impl::measureText(const std::string &text,
                                  double fontSize) const {
  if (!valid)
    return 0.0;

  double scale = (baseHeight > 0) ? (fontSize / baseHeight) : fontSize;
  double width = 0.0;

  auto codePoints = utf8ToCodePoints(text);

  for (uint32_t code : codePoints) {
    const Glyph *glyph = getGlyph(code);
    if (glyph) {
      width += glyph->width * scale;
    } else {
      width += defWidth * scale;
    }
  }

  return width;
}

//=============================================================================
// ShxFont Public Interface
//=============================================================================

ShxFont::ShxFont() : pImpl(std::make_unique<Impl>()) {}

ShxFont::~ShxFont() = default;

ShxFont::ShxFont(ShxFont &&) noexcept = default;
ShxFont &ShxFont::operator=(ShxFont &&) noexcept = default;

bool ShxFont::load(const std::string &filename) {
  return pImpl->load(filename);
}

bool ShxFont::loadFromMemory(const uint8_t *data, size_t size) {
  return pImpl->loadFromMemory(data, size);
}

const Glyph *ShxFont::getGlyph(uint32_t code) const {
  return pImpl->getGlyph(code);
}

bool ShxFont::hasGlyph(uint32_t code) const { return pImpl->hasGlyph(code); }

const Glyph *ShxFont::getGlyphByShape(uint16_t shapeId) const {
  return pImpl->getGlyph(static_cast<uint32_t>(shapeId));
}

void ShxFont::render(IPathRenderer &renderer, const std::string &text,
                     double fontSize, double x, double y) const {
  pImpl->render(renderer, text, fontSize, x, y);
}

double ShxFont::measureText(const std::string &text, double fontSize) const {
  return pImpl->measureText(text, fontSize);
}

ShxFontType ShxFont::getFontType() const { return pImpl->fontType; }

const std::string &ShxFont::getFontName() const { return pImpl->fontName; }

double ShxFont::getBaseHeight() const { return pImpl->baseHeight; }

double ShxFont::getDescender() const { return pImpl->descender; }

size_t ShxFont::getGlyphCount() const { return pImpl->glyphs.size(); }

const std::string &ShxFont::getLastError() const { return pImpl->lastError; }

bool ShxFont::isValid() const { return pImpl->valid; }

//=============================================================================
// Utility Functions
//=============================================================================

const char *getVersion() { return SHX_VERSION; }

bool isValidShxFile(const std::string &filename) {
  std::ifstream file(filename, std::ios::binary);
  if (!file.is_open()) {
    return false;
  }

  char header[32];
  file.read(header, sizeof(header));

  if (file.gcount() < 16) {
    return false;
  }

  std::string headerStr(header, file.gcount());
  return headerStr.find("AutoCAD") != std::string::npos;
}

} // namespace shx
