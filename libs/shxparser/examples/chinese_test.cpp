/*
 * Chinese BigFont Test - Test Chinese character rendering from BigFont SHX
 */

#include "ShxParser.h"
#include <iostream>
#include <iomanip>

// Convert UTF-8 Chinese text to GB2312 code points
// For BigFont, we need to use GB2312 encoding
std::vector<uint32_t> utf8ToGB2312Codes(const std::string& text) {
    std::vector<uint32_t> codes;
    
    // Simple UTF-8 to GB2312 mapping for common characters
    // In a real application, you'd use a proper conversion library
    
    size_t i = 0;
    while (i < text.size()) {
        uint8_t c = static_cast<uint8_t>(text[i]);
        
        if ((c & 0x80) == 0) {
            // ASCII - use directly
            codes.push_back(c);
            i += 1;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < text.size()) {
            // 2-byte UTF-8
            uint32_t cp = ((c & 0x1F) << 6) | (static_cast<uint8_t>(text[i+1]) & 0x3F);
            codes.push_back(cp);
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < text.size()) {
            // 3-byte UTF-8 - likely Chinese character
            uint32_t cp = ((c & 0x0F) << 12) | 
                          ((static_cast<uint8_t>(text[i+1]) & 0x3F) << 6) |
                          (static_cast<uint8_t>(text[i+2]) & 0x3F);
            
            // Convert Unicode to GB2312 (simplified - only works for some chars)
            // Real implementation would need a lookup table
            // For now, we'll just use the Unicode code point
            codes.push_back(cp);
            i += 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < text.size()) {
            // 4-byte UTF-8
            uint32_t cp = ((c & 0x07) << 18) |
                          ((static_cast<uint8_t>(text[i+1]) & 0x3F) << 12) |
                          ((static_cast<uint8_t>(text[i+2]) & 0x3F) << 6) |
                          (static_cast<uint8_t>(text[i+3]) & 0x3F);
            codes.push_back(cp);
            i += 4;
        } else {
            i += 1;
        }
    }
    return codes;
}

int main(int argc, char* argv[]) {
    std::cout << "=== Chinese BigFont Character Test ===\n\n";
    
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <shx_file>\n";
        return 1;
    }
    
    shx::ShxFont font;
    if (!font.load(argv[1])) {
        std::cerr << "Error: " << font.getLastError() << "\n";
        return 1;
    }
    
    std::cout << "Font loaded: " << argv[1] << "\n";
    std::cout << "Glyph count: " << font.getGlyphCount() << "\n";
    std::cout << "Base height: " << font.getBaseHeight() << "\n\n";
    
    // Test specific GB2312 codes for common Chinese characters
    // These are the GB2312 codes for the test string
    struct TestChar {
        uint32_t gb2312;
        const char* desc;
    };
    
    TestChar testChars[] = {
        { 0xB2E2, "ce (test)" },
        { 0xCAD4, "shi (test)" },
        { 0xBABA, "han (Chinese)" },
        { 0xD7D6, "zi (character)" },
        { 0xD0CD, "xing (type)" },
        { 0xCEC4, "wen (text)" },
        { 0xBCFE, "jian (file)" },
        { 0xA1A1, "space (GB2312)" },
        { 0xA1A2, "comma (GB2312)" },
    };
    
    std::cout << "=== Testing GB2312 Character Glyphs ===\n";
    for (const auto& tc : testChars) {
        const shx::Glyph* g = font.getGlyph(tc.gb2312);
        std::cout << "  0x" << std::hex << std::setw(4) << std::setfill('0') << tc.gb2312 
                  << std::dec << " (" << tc.desc << "): ";
        if (g) {
            std::cout << "FOUND - " << g->commands.size() << " commands\n";
        } else {
            std::cout << "NOT FOUND\n";
        }
    }
    
    // Try to render a simple character
    std::cout << "\n=== Rendering Sample Character (0xB2E2) ===\n";
    const shx::Glyph* testGlyph = font.getGlyph(0xB2E2);
    if (testGlyph) {
        std::cout << "Character has " << testGlyph->commands.size() << " draw commands:\n";
        int count = 0;
        for (const auto& cmd : testGlyph->commands) {
            if (++count > 20) {
                std::cout << "  ... (" << (testGlyph->commands.size() - 20) << " more)\n";
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
    }
    
    std::cout << "\nDone!\n";
    return 0;
}
