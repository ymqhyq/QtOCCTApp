/*
 * Mixed Chinese-English Text Test
 * Test string: "这个对HZTXT.shx字型文件的测试，Hellow SHX"
 */

#include "ShxParser.h"
#include <iostream>
#include <iomanip>
#include <vector>

int main(int argc, char* argv[]) {
    std::cout << "=== Mixed Text Glyph Test ===\n\n";
    
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <shx_file>\n";
        return 1;
    }
    
    shx::ShxFont font;
    if (!font.load(argv[1])) {
        std::cerr << "Error: " << font.getLastError() << "\n";
        return 1;
    }
    
    std::cout << "Font: " << argv[1] << "\n";
    std::cout << "Glyph count: " << font.getGlyphCount() << "\n\n";
    
    // Test string character codes (GB2312 for Chinese, ASCII for English)
    // "这个对HZTXT.shx字型文件的测试，Hellow SHX"
    struct CharInfo {
        uint32_t code;
        const char* display;
        bool isChinese;
    };
    
    CharInfo chars[] = {
        { 0xD5E2, "zhe (this)", true },
        { 0xB8F6, "ge (this)", true },
        { 0xB6D4, "dui (to)", true },
        { 0x48, "H", false },
        { 0x5A, "Z", false },
        { 0x54, "T", false },
        { 0x58, "X", false },
        { 0x54, "T", false },
        { 0x2E, ".", false },
        { 0x73, "s", false },
        { 0x68, "h", false },
        { 0x78, "x", false },
        { 0xD7D6, "zi (char)", true },
        { 0xD0CD, "xing (type)", true },
        { 0xCEC4, "wen (text)", true },
        { 0xBCFE, "jian (file)", true },
        { 0xB5C4, "de (of)", true },
        { 0xB2E2, "ce (test)", true },
        { 0xCAD4, "shi (test)", true },
        { 0xA3AC, ", (comma)", true },
        { 0x48, "H", false },
        { 0x65, "e", false },
        { 0x6C, "l", false },
        { 0x6C, "l", false },
        { 0x6F, "o", false },
        { 0x77, "w", false },
        { 0x20, " (space)", false },
        { 0x53, "S", false },
        { 0x48, "H", false },
        { 0x58, "X", false },
    };
    
    std::cout << "Testing: \"zhe ge dui HZTXT.shx zi xing wen jian de ce shi, Hellow SHX\"\n";
    std::cout << "(Chinese: \"" << "这个对HZTXT.shx字型文件的测试，Hellow SHX" << "\")\n\n";
    
    int found = 0;
    int notFound = 0;
    
    std::cout << "Char | Code     | Type    | Status | Commands\n";
    std::cout << "-----|----------|---------|--------|----------\n";
    
    for (const auto& ch : chars) {
        const shx::Glyph* g = font.getGlyph(ch.code);
        
        std::cout << std::setw(4) << std::left << ch.display << " | ";
        std::cout << "0x" << std::hex << std::setw(6) << std::setfill('0') << ch.code << std::dec << std::setfill(' ') << " | ";
        std::cout << std::setw(7) << (ch.isChinese ? "Chinese" : "ASCII") << " | ";
        
        if (g) {
            std::cout << "FOUND  | " << g->commands.size() << " cmds\n";
            found++;
        } else {
            std::cout << "MISS   | -\n";
            notFound++;
        }
    }
    
    std::cout << "\n=== Summary ===\n";
    std::cout << "Found:     " << found << "/" << (found + notFound) << "\n";
    std::cout << "Not found: " << notFound << "/" << (found + notFound) << "\n";
    
    // Calculate how many unique codes exist
    std::cout << "\n=== Sample Rendering ===\n";
    std::cout << "First 3 Chinese characters:\n";
    
    uint32_t sampleCodes[] = { 0xD5E2, 0xB8F6, 0xB6D4 }; // 这 个 对
    for (uint32_t code : sampleCodes) {
        const shx::Glyph* g = font.getGlyph(code);
        if (g) {
            std::cout << "\nGlyph 0x" << std::hex << code << std::dec << " (" << g->commands.size() << " commands):\n";
            int count = 0;
            for (const auto& cmd : g->commands) {
                if (++count > 5) {
                    std::cout << "  ... and " << (g->commands.size() - 5) << " more\n";
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
                        std::cout << "  ArcTo(" << cmd.endPoint.x << ", " << cmd.endPoint.y << ")\n";
                        break;
                }
            }
        }
    }
    
    std::cout << "\nDone!\n";
    return 0;
}
