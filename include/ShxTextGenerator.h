#pragma once

#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>
#include <memory>
#include <string>
#include <utility>
#include <vector>


namespace shx {
class ShxFont;
}

class ShxTextGenerator {
public:
  ShxTextGenerator();
  ~ShxTextGenerator();

  bool loadFont(const std::string &fontPath);
  bool loadBigFont(const std::string &fontPath);

  // Generate text as a TopoDS_Shape and independent Bounding Boxes
  // Returns: {TextCompound, ListOfBoxShapes}
  // angle: Rotation angle in degrees (counter-clockwise)
  // widthFactor: Width/Height ratio (1.0 = normal, >1.0 = wide, <1.0 =
  // compressed)
  std::pair<TopoDS_Shape, std::vector<TopoDS_Shape>>
  generateText(const std::string &text, const gp_Pnt &position, double height,
               double angle = 0.0, double widthFactor = 1.0);

private:
  std::unique_ptr<shx::ShxFont> m_font;
  std::unique_ptr<shx::ShxFont> m_bigFont;
};
