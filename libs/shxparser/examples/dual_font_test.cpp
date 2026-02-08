/*
 * Dual Font Test - Use txt.shx (ASCII) + hztxt.shx (Chinese BigFont)
 * Test string: "这个对HZTXT.shx字型文件的测试，Hellow SHX"
 */

#include "ShxParser.h"
#include <iostream>
#include <iomanip>
#include <vector>

int main(int argc, char* argv[]) {
    std::cout << "=== Dual Font Mixed Text Test ===\n\n";
    
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <ascii_shx> <bigfont_shx>\n";
        std::cout << "Example: " << argv[0] << " txt.shx hztxt.shx\n";
        return 1;
    }
    
    // Load ASCII font
    shx::ShxFont asciiFont;
    if (!asciiFont.load(argv[1])) {
        std::cerr << "Error loading ASCII font: " << asciiFont.getLastError() << "\n";
        return 1;
    }
    std::cout << "ASCII Font: " << argv[1] << "\n";
    std::cout << "  Type: " << (asciiFont.getFontType() == shx::ShxFontType::Shapes ? "Shapes" : 
                                asciiFont.getFontType() == shx::ShxFontType::BigFont ? "BigFont" : "UniFont") << "\n";
    std::cout << "  Glyphs: " << asciiFont.getGlyphCount() << "\n";
    std::cout << "  Base Height: " << asciiFont.getBaseHeight() << "\n\n";
    
    // Load BigFont
    shx::ShxFont bigFont;
    if (!bigFont.load(argv[2])) {
        std::cerr << "Error loading BigFont: " << bigFont.getLastError() << "\n";
        return 1;
    }
    std::cout << "BigFont: " << argv[2] << "\n";
    std::cout << "  Type: " << (bigFont.getFontType() == shx::ShxFontType::Shapes ? "Shapes" : 
                               bigFont.getFontType() == shx::ShxFontType::BigFont ? "BigFont" : "UniFont") << "\n";
    std::cout << "  Glyphs: " << bigFont.getGlyphCount() << "\n";
    std::cout << "  Base Height: " << bigFont.getBaseHeight() << "\n\n";
    
    // Test string character codes
    // "这个对HZTXT.shx字型文件的测试，Hellow SHX"
    struct CharInfo {
        uint32_t code;
        const char* display;
        bool isChinese;
    };
    
    CharInfo chars[] = {
        { 0xD5E2, "zhe", true },   // 这
        { 0xB8F6, "ge", true },    // 个
        { 0xB6D4, "dui", true },   // 对
        { 'H', "H", false },
        { 'Z', "Z", false },
        { 'T', "T", false },
        { 'X', "X", false },
        { 'T', "T", false },
        { '.', ".", false },
        { 's', "s", false },
        { 'h', "h", false },
        { 'x', "x", false },
        { 0xD7D6, "zi", true },    // 字
        { 0xD0CD, "xing", true },  // 型
        { 0xCEC4, "wen", true },   // 文
        { 0xBCFE, "jian", true },  // 件
        { 0xB5C4, "de", true },    // 的
        { 0xB2E2, "ce", true },    // 测
        { 0xCAD4, "shi", true },   // 试
        { 0xA3AC, ",", true },     // ，
        { 'H', "H", false },
        { 'e', "e", false },
        { 'l', "l", false },
        { 'l', "l", false },
        { 'o', "o", false },
        { 'w', "w", false },
        { ' ', "_", false },
        { 'S', "S", false },
        { 'H', "H", false },
        { 'X', "X", false },
    };
    
    std::cout << "=== Testing: \"这个对HZTXT.shx字型文件的测试，Hellow SHX\" ===\n\n";
    
    std::cout << "Char | Code   | Font     | Status | Commands\n";
    std::cout << "-----|--------|----------|--------|----------\n";
    
    int totalFound = 0;
    int totalMissed = 0;
    int totalCommands = 0;
    
    for (const auto& ch : chars) {
        const shx::Glyph* g = nullptr;
        const char* fontUsed = nullptr;
        
        if (ch.isChinese) {
            // Try BigFont for Chinese characters
            g = bigFont.getGlyph(ch.code);
            fontUsed = "BigFont";
        } else {
            // Try ASCII font for English characters
            g = asciiFont.getGlyph(ch.code);
            fontUsed = "ASCII";
        }
        
        std::cout << std::setw(4) << std::left << ch.display << " | ";
        
        if (ch.isChinese) {
            std::cout << "0x" << std::hex << std::setw(4) << std::setfill('0') << ch.code 
                      << std::dec << std::setfill(' ') << " | ";
        } else {
            std::cout << "0x" << std::hex << std::setw(4) << std::setfill('0') << ch.code 
                      << std::dec << std::setfill(' ') << " | ";
        }
        
        std::cout << std::setw(8) << fontUsed << " | ";
        
        if (g) {
            std::cout << "FOUND  | " << g->commands.size() << "\n";
            totalFound++;
            totalCommands += static_cast<int>(g->commands.size());
        } else {
            std::cout << "MISS   | -\n";
            totalMissed++;
        }
    }
    
    std::cout << "\n=== Summary ===\n";
    std::cout << "Found:          " << totalFound << "/" << (totalFound + totalMissed) << "\n";
    std::cout << "Missed:         " << totalMissed << "/" << (totalFound + totalMissed) << "\n";
    std::cout << "Total commands: " << totalCommands << "\n";
    
    // Render a sample of the full text
    std::cout << "\n=== Sample Character Rendering ===\n";
    
    // Show rendering for both fonts
    std::cout << "\nASCII 'H' from txt.shx:\n";
    const shx::Glyph* gH = asciiFont.getGlyph('H');
    if (gH) {
        for (size_t i = 0; i < std::min(gH->commands.size(), size_t(8)); ++i) {
            const auto& cmd = gH->commands[i];
            switch (cmd.type) {
                case shx::CommandType::MoveTo:
                    std::cout << "  MoveTo(" << cmd.endPoint.x << ", " << cmd.endPoint.y << ")\n";
                    break;
                case shx::CommandType::LineTo:
                    std::cout << "  LineTo(" << cmd.endPoint.x << ", " << cmd.endPoint.y << ")\n";
                    break;
                case shx::CommandType::ArcTo:
                    std::cout << "  ArcTo(" << cmd.endPoint.x << ", " << cmd.endPoint.y << ")\n";
                    break;
            }
        }
        if (gH->commands.size() > 8) {
            std::cout << "  ... and " << (gH->commands.size() - 8) << " more\n";
        }
    }
    
    std::cout << "\nChinese '这' (0xD5E2) from hztxt.shx:\n";
    const shx::Glyph* gZhe = bigFont.getGlyph(0xD5E2);
    if (gZhe) {
        for (size_t i = 0; i < std::min(gZhe->commands.size(), size_t(8)); ++i) {
            const auto& cmd = gZhe->commands[i];
            switch (cmd.type) {
                case shx::CommandType::MoveTo:
                    std::cout << "  MoveTo(" << cmd.endPoint.x << ", " << cmd.endPoint.y << ")\n";
                    break;
                case shx::CommandType::LineTo:
                    std::cout << "  LineTo(" << cmd.endPoint.x << ", " << cmd.endPoint.y << ")\n";
                    break;
                case shx::CommandType::ArcTo:
                    std::cout << "  ArcTo(" << cmd.endPoint.x << ", " << cmd.endPoint.y << ")\n";
                    break;
            }
        }
        if (gZhe->commands.size() > 8) {
            std::cout << "  ... and " << (gZhe->commands.size() - 8) << " more\n";
        }
    }
    
    std::cout << "\n=== Test Complete ===\n";
    return 0;
}
