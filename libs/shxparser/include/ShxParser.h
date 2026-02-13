/*
 * ShxParser - C++ AutoCAD SHX Font File Parser
 *
 * A pure C++ library for parsing AutoCAD SHX (Shape eXtended) font files.
 * Based on the Python shxparser library by tatarize.
 *
 * Author: Generated for QwenCodeWS project
 * License: MIT
 */

#ifndef SHX_PARSER_H
#define SHX_PARSER_H

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>


namespace shx {

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief SHX font type
 */
enum class ShxFontType {
  Unknown,
  Shapes,  // Shape file
  BigFont, // Big font (Asian fonts)
  UniFont  // Unicode font
};

/**
 * @brief Drawing command type
 */
enum class CommandType {
  MoveTo,  // Move to position (no drawing)
  LineTo,  // Draw line to position
  ArcTo,   // Draw arc to position
  SubShape // Draw sub-shape (recursive)
};

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief 2D point structure
 */
struct Point2D {
  double x = 0.0;
  double y = 0.0;

  Point2D() = default;
  Point2D(double x_, double y_) : x(x_), y(y_) {}

  Point2D operator+(const Point2D &other) const {
    return Point2D(x + other.x, y + other.y);
  }

  Point2D operator*(double scale) const {
    return Point2D(x * scale, y * scale);
  }
};

/**
 * @brief Drawing command
 */
struct DrawCommand {
  CommandType type;
  Point2D endPoint;        // End point
  Point2D controlPoint;    // Control point (arc only)
  uint16_t subShapeId = 0; // Sub-shape ID

  DrawCommand() : type(CommandType::MoveTo) {}

  static DrawCommand moveTo(double x, double y) {
    DrawCommand cmd;
    cmd.type = CommandType::MoveTo;
    cmd.endPoint = Point2D(x, y);
    return cmd;
  }

  static DrawCommand lineTo(double x, double y) {
    DrawCommand cmd;
    cmd.type = CommandType::LineTo;
    cmd.endPoint = Point2D(x, y);
    return cmd;
  }

  static DrawCommand arcTo(double x, double y, double cx, double cy) {
    DrawCommand cmd;
    cmd.type = CommandType::ArcTo;
    cmd.endPoint = Point2D(x, y);
    cmd.controlPoint = Point2D(cx, cy);
    return cmd;
  }

  static DrawCommand subShape(uint16_t id, double x, double y) {
    DrawCommand cmd;
    cmd.type = CommandType::SubShape;
    cmd.subShapeId = id;
    cmd.endPoint = Point2D(x, y); // Store insertion point
    return cmd;
  }
};

/**
 * @brief Glyph data
 */
struct Glyph {
  uint32_t code = 0;                 // Character code
  std::string name;                  // Glyph name
  std::vector<DrawCommand> commands; // Drawing commands
  std::vector<uint8_t> rawData;      // Raw SHX bytes (for delayed compilation)
  double width = 0.0;                // Glyph width
  double height = 0.0;               // Glyph height

  bool isEmpty() const { return commands.empty(); }
};

//=============================================================================
// Path Renderer Interface
//=============================================================================

/**
 * @brief Path renderer interface
 *
 * Users can implement this interface to receive glyph rendering commands
 */
class IPathRenderer {
public:
  virtual ~IPathRenderer() = default;

  virtual void moveTo(double x, double y) = 0;
  virtual void lineTo(double x, double y) = 0;
  virtual void arcTo(double x, double y, double cx, double cy) = 0;
  virtual void closePath() {}
};

/**
 * @brief Simple path collector
 *
 * Collects rendering commands into a DrawCommand list
 */
class PathCollector : public IPathRenderer {
public:
  std::vector<DrawCommand> commands;

  void moveTo(double x, double y) override {
    commands.push_back(DrawCommand::moveTo(x, y));
  }

  void lineTo(double x, double y) override {
    commands.push_back(DrawCommand::lineTo(x, y));
  }

  void arcTo(double x, double y, double cx, double cy) override {
    commands.push_back(DrawCommand::arcTo(x, y, cx, cy));
  }

  void clear() { commands.clear(); }
};

//=============================================================================
// SHX Font Parser
//=============================================================================

/**
 * @brief SHX font file parser
 */
class ShxFont {
public:
  ShxFont();
  ~ShxFont();

  // No copy
  ShxFont(const ShxFont &) = delete;
  ShxFont &operator=(const ShxFont &) = delete;

  // Allow move
  ShxFont(ShxFont &&) noexcept;
  ShxFont &operator=(ShxFont &&) noexcept;

  /**
   * @brief Load SHX font from file
   * @param filename SHX file path
   * @return true on success
   */
  bool load(const std::string &filename);

  /**
   * @brief Load SHX font from memory
   * @param data File data
   * @param size Data size
   * @return true on success
   */
  bool loadFromMemory(const uint8_t *data, size_t size);

  /**
   * @brief Get glyph
   * @param code Character code
   * @return Glyph pointer, nullptr if not found
   */
  const Glyph *getGlyph(uint32_t code) const;

  /**
   * @brief Check if glyph exists
   */
  /**
   * @brief Check if glyph exists
   */
  bool hasGlyph(uint32_t code) const;

  /**
   * @brief Get glyph by raw shape ID (for sub-shapes)
   */
  const Glyph *getGlyphByShape(uint16_t shapeId) const;

  /**
   * @brief Render text
   * @param renderer Renderer
   * @param text Text content (UTF-8 encoded)
   * @param fontSize Font size
   * @param x Starting X coordinate
   * @param y Starting Y coordinate
   */
  void render(IPathRenderer &renderer, const std::string &text,
              double fontSize = 1.0, double x = 0.0, double y = 0.0) const;

  /**
   * @brief Measure text width
   */
  double measureText(const std::string &text, double fontSize = 1.0) const;

  // Property accessors
  ShxFontType getFontType() const;
  const std::string &getFontName() const;
  double getBaseHeight() const;
  double getDescender() const;
  size_t getGlyphCount() const;

  /**
   * @brief Get last error message
   */
  const std::string &getLastError() const;

  /**
   * @brief Check if valid font is loaded
   */
  bool isValid() const;

private:
  class Impl;
  std::unique_ptr<Impl> pImpl;
};

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get library version
 */
const char *getVersion();

/**
 * @brief Check if file is a valid SHX file
 */
bool isValidShxFile(const std::string &filename);

} // namespace shx

#endif // SHX_PARSER_H
