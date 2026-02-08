#include "../include/ShxTextGenerator.h"
#include "../libs/shxparser/include/ShxParser.h"
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_GTransform.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRep_Builder.hxx>
#include <Bnd_Box.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <QByteArray>
#include <QString>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_GTrsf.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <math.h>

ShxTextGenerator::ShxTextGenerator()
    : m_font(new shx::ShxFont()), m_bigFont(new shx::ShxFont()) {}
ShxTextGenerator::~ShxTextGenerator() = default;

bool ShxTextGenerator::loadFont(const std::string &fontPath) {
  return m_font->load(fontPath);
}

bool ShxTextGenerator::loadBigFont(const std::string &fontPath) {
  return m_bigFont->load(fontPath);
}

// Helper for recursive Bounding Box scan
static void scanGlyphBBox(const shx::ShxFont *font, const shx::Glyph *glyph,
                          double offsetX, double offsetY, double &minX,
                          double &maxX, double &minY, double &maxY,
                          bool &hasGeo, int depth = 0,
                          gp_Pnt startPen = gp_Pnt(0, 0, 0)) {
  if (!glyph || depth > 10)
    return;

  // Local pen tracking relative to offset
  // Should we pass Current Pen from caller?
  // For SubShape, yes? No, SubShape defines its own valid context usually?
  // In SHX, SubShape is just a macro.
  // But usually SubShapes are self-contained or start at their local 0,0?
  // No, SubShape command documentation says it uses current pen?
  // Actually, cmd.subShapeId is just an ID.
  // The parser stores 'x,y' in DrawCommand for SubShape insertion point.
  // So we can treat it as a fresh start at that point?

  // In our parser logic, 'drawGlyphRecursive' passes 'origin' (Start Point) and
  // 'currentPen'. We should probably init 'currentPen' to (offsetX, offsetY)
  // which is the Insertion Point.

  gp_Pnt currentPen(offsetX, offsetY, 0.0);

  // If this is a recursive call for a SubShape that CONTINUES the path, we
  // might need the actual pen? But 'scanGlyphBBox' is called with specific
  // Absolute 'offsetX, offsetY'. In recursion (Step 1082 Line 61):
  // scanGlyphBBox(..., absX, absY, ...) which is the Insertion Point. And
  // 'currentPen' should start there.

  for (const auto &cmd : glyph->commands) {
    // cmd.endPoint is relative to Glyph Origin (0,0 of the glyph definition).
    // So Absolute EndPoint = offsetX + cmd.endPoint.x

    double absX = offsetX + cmd.endPoint.x;
    double absY = offsetY + cmd.endPoint.y;
    gp_Pnt endP(absX, absY, 0.0);

    if (cmd.type == shx::CommandType::MoveTo) {
      currentPen = endP;
    } else if (cmd.type == shx::CommandType::LineTo) {
      // Check Start (currentPen) and End (endP)
      if (currentPen.X() < minX)
        minX = currentPen.X();
      if (currentPen.X() > maxX)
        maxX = currentPen.X();
      if (currentPen.Y() < minY)
        minY = currentPen.Y();
      if (currentPen.Y() > maxY)
        maxY = currentPen.Y();

      if (endP.X() < minX)
        minX = endP.X();
      if (endP.X() > maxX)
        maxX = endP.X();
      if (endP.Y() < minY)
        minY = endP.Y();
      if (endP.Y() > maxY)
        maxY = endP.Y();

      hasGeo = true;
      currentPen = endP;
    } else if (cmd.type == shx::CommandType::ArcTo) {
      // Calculate Arc Bounds
      double ctrlAbsX = offsetX + cmd.controlPoint.x;
      double ctrlAbsY = offsetX + cmd.controlPoint.y; // WRONG: offsetY
      ctrlAbsY = offsetY + cmd.controlPoint.y;

      gp_Pnt ctrlP(ctrlAbsX, ctrlAbsY, 0.0);

      try {
        GC_MakeArcOfCircle mkArc(currentPen, ctrlP, endP);
        if (mkArc.IsDone()) {
          TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(mkArc.Value());
          Bnd_Box box;
          BRepBndLib::Add(edge, box);

          double xmin, ymin, zmin, xmax, ymax, zmax;
          box.Get(xmin, ymin, zmin, xmax, ymax, zmax);

          if (xmin < minX)
            minX = xmin;
          if (xmax > maxX)
            maxX = xmax;
          if (ymin < minY)
            minY = ymin;
          if (ymax > maxY)
            maxY = ymax;
          hasGeo = true;
        }
      } catch (...) {
      }

      currentPen = endP;
    } else if (cmd.type == shx::CommandType::SubShape) {
      const shx::Glyph *sub = font->getGlyphByShape(cmd.subShapeId);
      if (sub) {
        // Use local pen for subshape to isolate state
        gp_Pnt subPen = endP;
        scanGlyphBBox(font, sub, absX, absY, minX, maxX, minY, maxY, hasGeo,
                      depth + 1, subPen);
      }
      // Strict restore for current context
      currentPen = endP;
      continue;
    }
  }
}

// Helper for recursive Drawing
static void drawGlyphRecursive(const shx::ShxFont *font,
                               const shx::Glyph *glyph, gp_Pnt origin,
                               double scale, BRep_Builder &builder,
                               TopoDS_Compound &compound, gp_Pnt &currentPen,
                               int depth = 0) {
  if (!glyph || depth > 10)
    return;

  for (const auto &cmd : glyph->commands) {
    // Calculate absolute position for this command
    // cmd.endPoint is relative to glyph origin.
    // We add it to 'origin' which is the insertion point for this glyph
    // instance.
    gp_Pnt endP;
    endP.SetCoord(origin.X() + cmd.endPoint.x * scale,
                  origin.Y() + cmd.endPoint.y * scale, 0.0);

    if (cmd.type == shx::CommandType::MoveTo) {
      currentPen = endP;
    } else if (cmd.type == shx::CommandType::LineTo) {
      if (!currentPen.IsEqual(endP, 1e-6)) {
        try {
          TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(currentPen, endP);
          builder.Add(compound, edge);
        } catch (...) {
        }
        currentPen = endP;
      } else {
        // Zero-length line -> Point explicitly drawn as a small cross
        // Vertex alone might be invisible or filtered
        double dotSize = 2.0 * scale; // Heuristic size
        gp_Pnt p1(currentPen.X() - dotSize, currentPen.Y(), currentPen.Z());
        gp_Pnt p2(currentPen.X() + dotSize, currentPen.Y(), currentPen.Z());
        gp_Pnt p3(currentPen.X(), currentPen.Y() - dotSize, currentPen.Z());
        gp_Pnt p4(currentPen.X(), currentPen.Y() + dotSize, currentPen.Z());

        try {
          TopoDS_Edge e1 = BRepBuilderAPI_MakeEdge(p1, p2);
          TopoDS_Edge e2 = BRepBuilderAPI_MakeEdge(p3, p4);
          builder.Add(compound, e1);
          builder.Add(compound, e2);
        } catch (...) {
        }
      }
    } else if (cmd.type == shx::CommandType::ArcTo) {
      gp_Pnt ctrlP;
      ctrlP.SetCoord(origin.X() + cmd.controlPoint.x * scale,
                     origin.Y() + cmd.controlPoint.y * scale, 0.0);

      bool built = false;
      try {
        GC_MakeArcOfCircle mkArc(currentPen, ctrlP, endP);
        if (mkArc.IsDone()) {
          TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(mkArc.Value());
          builder.Add(compound, edge);
          built = true;
        }
      } catch (...) {
      }

      if (!built) {
        // Fallback to line
        if (!currentPen.IsEqual(endP, 1e-6)) {
          try {
            TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(currentPen, endP);
            builder.Add(compound, edge);
          } catch (...) {
          }
        } else {
          // Degenerate arc -> Point (small cross)
          double dotSize = 2.0 * scale;
          gp_Pnt p1(currentPen.X() - dotSize, currentPen.Y(), currentPen.Z());
          gp_Pnt p2(currentPen.X() + dotSize, currentPen.Y(), currentPen.Z());
          gp_Pnt p3(currentPen.X(), currentPen.Y() - dotSize, currentPen.Z());
          gp_Pnt p4(currentPen.X(), currentPen.Y() + dotSize, currentPen.Z());

          try {
            TopoDS_Edge e1 = BRepBuilderAPI_MakeEdge(p1, p2);
            TopoDS_Edge e2 = BRepBuilderAPI_MakeEdge(p3, p4);
            builder.Add(compound, e1);
            builder.Add(compound, e2);
          } catch (...) {
          }
        }
      }
      currentPen = endP;
    } else if (cmd.type == shx::CommandType::SubShape) {
      const shx::Glyph *sub = font->getGlyphByShape(cmd.subShapeId);
      if (sub) {
        // Use local pen for subshape to isolate state
        gp_Pnt subPen = endP;
        drawGlyphRecursive(font, sub, endP, scale, builder, compound, subPen,
                           depth + 1);

        // Strict restore
        currentPen = endP;
      }
    }
  }
}

std::pair<TopoDS_Shape, std::vector<TopoDS_Shape>>
ShxTextGenerator::generateText(const std::string &text, const gp_Pnt &position,
                               double height, double angle,
                               double widthFactor) {
  BRep_Builder builder;
  TopoDS_Compound compound;
  builder.MakeCompound(compound);

  std::vector<TopoDS_Shape> boxes;

  if (text.empty())
    return {compound, boxes};

  double scaleFont = 1.0;
  double scaleBigFont = 1.0;

  // Determine scale for standard font
  if (m_font->getGlyphCount() > 0) {
    double baseH = m_font->getBaseHeight();
    if (baseH == 0) {
      const shx::Glyph *g = m_font->getGlyph('A');
      if (g) {
        double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
        bool hasGeo = false;
        scanGlyphBBox(&(*m_font), g, 0, 0, minX, maxX, minY, maxY, hasGeo);
        baseH = (hasGeo) ? (maxY - minY) : 20.0;
        if (baseH <= 0)
          baseH = 20.0;
      } else {
        baseH = 20.0;
      }
    }
    if (baseH > 0)
      scaleFont = height / baseH;
  }

  // Determine scale for BigFont
  if (m_bigFont->getGlyphCount() > 0) {
    double baseH = m_bigFont->getBaseHeight();
    if (baseH <= 1.0)
      baseH = 127.0;
    if (baseH > 0)
      scaleBigFont = height / baseH;
  }

  QByteArray gbData = QString::fromStdString(text).toLocal8Bit();
  gp_Pnt cursorPos(0, 0, 0);

  for (int i = 0; i < gbData.size(); ++i) {
    uint8_t b1 = static_cast<uint8_t>(gbData[i]);
    uint32_t code = 0;
    bool isBig = false;

    if (b1 < 0x80) {
      code = b1;
    } else {
      if (i + 1 < gbData.size()) {
        uint8_t b2 = static_cast<uint8_t>(gbData[i + 1]);
        code = (b1 << 8) | b2;
        isBig = true;
        i++;
      } else {
        continue;
      }
    }

    const shx::ShxFont *activeFont = &(*m_font);
    const shx::Glyph *glyph = nullptr;
    double activeScale = scaleFont;

    if (isBig && m_bigFont->getGlyphCount() > 0) {
      glyph = m_bigFont->getGlyph(code);
      if (glyph) {
        activeFont = &(*m_bigFont);
        activeScale = scaleBigFont;
      }
    } else {
      glyph = m_font->getGlyph(code);
    }

    if (!glyph) {
      if (code == ' ')
        cursorPos.SetX(cursorPos.X() + height * 0.5);
      continue;
    }

    // Explicitly handle Space (32)
    if (code == 32) {
      // Space width: 0.4 * Height (World Units)
      // This decouples space from the BBox/Padding logic which made it too
      // wide.
      double spaceWidth = height * 0.4;
      cursorPos.SetX(cursorPos.X() + spaceWidth);
      continue;
    }

    // 1. Scan BBox (Recursive)
    double minX = 1e9, maxX = -1e9;
    double minY = 1e9, maxY = -1e9;
    bool hasGeo = false;

    scanGlyphBBox(activeFont, glyph, 0, 0, minX, maxX, minY, maxY, hasGeo);

    if (!hasGeo) {
      // Fallback for empty glyphs (other than space)
      minX = 0.0;
      // Use a safe fallback width in Font Units (e.g. 20.0 or 0.5*BaseHeight)
      // Since we don't know BaseH easily here, use scaling inverse.
      // height * 0.5 in World => (height * 0.5) / activeScale in Font.
      if (activeScale > 1e-6)
        maxX = (height * 0.5) / activeScale;
      else
        maxX = 20.0;

      minY = 0.0;
      maxY = 0.0;
    }

    // 2. Normalize and Draw (Recursive)
    // Strict BBox Mode with Centering Logic:
    // We calculate the inked width.
    // We enforce a minimum BBox width of 0.5 * Height.
    // We center the ink within this effectively wider BBox.

    double inkWidthFont = maxX - minX;
    double inkWidthWorld = inkWidthFont * activeScale;

    // Scale fixed padding by height to ensure proportional look
    // 8.0 was for Height 50 (0.16 ratio).
    // Let's use proportional padding: height * 0.15
    double padding = height * 0.15;
    double minWidth = height * 0.15; // Minimum character body width

    double finalBBoxWidthWorld = std::max(inkWidthWorld, minWidth);

    // Recalculate centering shift based on new dynamic parameters
    double finalBBoxWidthFont = finalBBoxWidthWorld / activeScale;
    double centeringPaddingFont = (finalBBoxWidthFont - inkWidthFont) * 0.5;
    double shiftX = -minX + centeringPaddingFont;
    double shiftY = -minY;

    gp_Pnt drawOrigin;
    drawOrigin.SetCoord(cursorPos.X() + shiftX * activeScale,
                        cursorPos.Y() + shiftY * activeScale, 0.0);

    gp_Pnt currentPen = drawOrigin;

    drawGlyphRecursive(activeFont, glyph, drawOrigin, activeScale, builder,
                       compound, currentPen);

    // Create Individual BBox Shape
    // The BBox Visual should represent the "Box" the character lives in.
    // It starts at cursorPos (X=0 relative to start) and extends to
    // finalBBoxWidthWorld. But wait, the BBox code usually surrounds the INK.
    // User said "Character in BBox horizontal centered". This implies the BBox
    // is the container. So we should draw the box of 'finalBBoxWidthWorld'.

    // 3. Advance Logic
    // Advance = BBoxWidth + Padding
    double advance = finalBBoxWidthWorld + padding;

    cursorPos.SetX(cursorPos.X() + advance);
  }

  // --- Apply Transformations ---
  // Sequence: Scale (WidthFactor) -> Rotate (Angle) -> Translate (Position)

  TopoDS_Shape currentShape = compound;

  // 1. Width Factor (Non-Uniform Scaling via GTrsf)
  if (std::abs(widthFactor - 1.0) > 1e-6) {
    gp_GTrsf scaleT;
    // Scale X by widthFactor, Y/Z by 1.0
    gp_Mat scaleMat(widthFactor, 0, 0, 0, 1, 0, 0, 0, 1);
    scaleT.SetVectorialPart(scaleMat);

    BRepBuilderAPI_GTransform gXform(currentShape, scaleT,
                                     true); // Copy=True
    currentShape = gXform.Shape();

    // Also transform boxes if present
    for (auto &box : boxes) {
      BRepBuilderAPI_GTransform gb(box, scaleT, true);
      box = gb.Shape();
    }
  }

  // 2. Rotation & Translation (Rigid Transform via Trsf)
  gp_Trsf finalTrsf;

  // Rotation (around Z axis at local origin)
  if (std::abs(angle) > 1e-6) {
    // Convert degrees to radians
    double rad = angle * M_PI / 180.0;
    gp_Ax1 axis(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1));
    finalTrsf.SetRotation(axis, rad);
  }

  // Translation (to target position)
  gp_Trsf posTrsf;
  posTrsf.SetTranslation(gp_Vec(position.XYZ()));

  // Combine: Final = Pos * Rot (Apply Rot then Pos)
  finalTrsf = posTrsf * finalTrsf;

  BRepBuilderAPI_Transform xform(currentShape, finalTrsf);
  currentShape = xform.Shape();

  // Transform boxes
  std::vector<TopoDS_Shape> finalBoxes;
  for (const auto &box : boxes) {
    BRepBuilderAPI_Transform xb(box, finalTrsf);
    finalBoxes.push_back(xb.Shape());
  }

  return {currentShape, finalBoxes};
}
