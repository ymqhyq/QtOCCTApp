/*
 * ShxParser Example - Demonstrate SHX parser library usage
 */

#include "ShxParser.h"
#include <iostream>
#include <iomanip>
#include <sstream>

/**
 * @brief Console path renderer - prints drawing commands
 */
class ConsoleRenderer : public shx::IPathRenderer {
public:
    void moveTo(double x, double y) override {
        std::cout << "  MoveTo(" << std::fixed << std::setprecision(2) 
                  << x << ", " << y << ")\n";
    }
    
    void lineTo(double x, double y) override {
        std::cout << "  LineTo(" << std::fixed << std::setprecision(2) 
                  << x << ", " << y << ")\n";
    }
    
    void arcTo(double x, double y, double cx, double cy) override {
        std::cout << "  ArcTo(" << std::fixed << std::setprecision(2) 
                  << x << ", " << y << ") ctrl=(" << cx << ", " << cy << ")\n";
    }
};

/**
 * @brief SVG path renderer - generates SVG path data
 */
class SvgRenderer : public shx::IPathRenderer {
public:
    std::string pathData;
    
    void moveTo(double x, double y) override {
        std::ostringstream oss;
        oss << "M " << x << " " << (-y) << " ";
        pathData += oss.str();
    }
    
    void lineTo(double x, double y) override {
        std::ostringstream oss;
        oss << "L " << x << " " << (-y) << " ";
        pathData += oss.str();
    }
    
    void arcTo(double x, double y, double cx, double cy) override {
        std::ostringstream oss;
        oss << "Q " << cx << " " << (-cy) << " " << x << " " << (-y) << " ";
        pathData += oss.str();
    }
    
    void clear() { pathData.clear(); }
    
    std::string toSvg(double width, double height) const {
        std::ostringstream svg;
        svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" ";
        svg << "width=\"" << width << "\" ";
        svg << "height=\"" << height << "\" ";
        svg << "viewBox=\"0 0 " << width << " " << height << "\">\n";
        svg << "  <g transform=\"translate(10, " << (height - 10) << ")\">\n";
        svg << "    <path d=\"" << pathData << "\" ";
        svg << "fill=\"none\" stroke=\"black\" stroke-width=\"1\"/>\n";
        svg << "  </g>\n";
        svg << "</svg>";
        return svg.str();
    }
};

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " <shx_file> [text]\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << programName << " hztxt.shx\n";
    std::cout << "  " << programName << " txt.shx \"Hello World\"\n";
}

int main(int argc, char* argv[]) {
    std::cout << "=== SHX Parser Example ===\n";
    std::cout << "Version: " << shx::getVersion() << "\n\n";
    
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    const char* filename = argv[1];
    const char* text = (argc > 2) ? argv[2] : "ABC";
    
    if (!shx::isValidShxFile(filename)) {
        std::cerr << "Error: '" << filename << "' is not a valid SHX file\n";
        return 1;
    }
    
    shx::ShxFont font;
    if (!font.load(filename)) {
        std::cerr << "Error: Cannot load SHX file\n";
        std::cerr << "Details: " << font.getLastError() << "\n";
        return 1;
    }
    
    std::cout << "File: " << filename << "\n";
    std::cout << "Font Type: ";
    switch (font.getFontType()) {
        case shx::ShxFontType::Shapes:  std::cout << "Shapes"; break;
        case shx::ShxFontType::BigFont: std::cout << "BigFont"; break;
        case shx::ShxFontType::UniFont: std::cout << "UniFont"; break;
        default: std::cout << "Unknown"; break;
    }
    std::cout << "\n";
    std::cout << "Glyph Count: " << font.getGlyphCount() << "\n";
    std::cout << "Base Height: " << font.getBaseHeight() << "\n";
    std::cout << "Descender: " << font.getDescender() << "\n\n";
    
    std::cout << "=== Glyph Examples ===\n";
    for (char c = 'A'; c <= 'C'; ++c) {
        const shx::Glyph* glyph = font.getGlyph(static_cast<uint32_t>(c));
        if (glyph) {
            std::cout << "Char '" << c << "' (code=" << glyph->code 
                      << ", cmds=" << glyph->commands.size() 
                      << ", width=" << glyph->width << "):\n";
            
            int cmdCount = 0;
            for (const auto& cmd : glyph->commands) {
                if (++cmdCount > 5) {
                    std::cout << "  ... (" << (glyph->commands.size() - 5) << " more commands)\n";
                    break;
                }
                
                switch (cmd.type) {
                    case shx::CommandType::MoveTo:
                        std::cout << "  MoveTo(" << cmd.endPoint.x << ", " << cmd.endPoint.y << ")\n";
                        break;
                    case shx::CommandType::LineTo:
                        std::cout << "  LineTo(" << cmd.endPoint.x << ", " << cmd.endPoint.y << ")\n";
                        break;
                    case shx::CommandType::ArcTo:
                        std::cout << "  ArcTo(" << cmd.endPoint.x << ", " << cmd.endPoint.y 
                                  << ") ctrl=(" << cmd.controlPoint.x << ", " << cmd.controlPoint.y << ")\n";
                        break;
                }
            }
            std::cout << "\n";
        } else {
            std::cout << "Char '" << c << "' not found\n";
        }
    }
    
    std::cout << "=== Render Text ===\n";
    std::cout << "Text: \"" << text << "\"\n";
    std::cout << "Font Size: 20\n\n";
    
    ConsoleRenderer console;
    font.render(console, text, 20.0, 0.0, 0.0);
    
    double textWidth = font.measureText(text, 20.0);
    std::cout << "\nText Width: " << textWidth << "\n";
    
    std::cout << "\n=== SVG Output Example ===\n";
    SvgRenderer svg;
    font.render(svg, text, 50.0, 0.0, 0.0);
    
    double svgWidth = font.measureText(text, 50.0) + 20;
    double svgHeight = font.getBaseHeight() * 50.0 / (font.getBaseHeight() > 0 ? font.getBaseHeight() : 1) + 20;
    
    std::cout << "SVG Path Data (first 200 chars):\n";
    if (svg.pathData.length() > 200) {
        std::cout << svg.pathData.substr(0, 200) << "...";
    } else {
        std::cout << svg.pathData;
    }
    std::cout << "\n";
    
    std::cout << "\nDone!\n";
    
    return 0;
}
