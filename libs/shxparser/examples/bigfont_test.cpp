/*
 * BigFont Test - Test Chinese characters in BigFont SHX files
 */

#include "ShxParser.h"
#include <iostream>
#include <iomanip>

int main(int argc, char* argv[]) {
    std::cout << "=== BigFont Chinese Character Test ===\n\n";
    
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <shx_file>\n";
        return 1;
    }
    
    const char* filename = argv[1];
    
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
    std::cout << "Glyph Count: " << font.getGlyphCount() << "\n\n";
    
    // Try to find glyphs in different ranges
    std::cout << "=== Scanning for Available Glyphs ===\n\n";
    
    // ASCII range (32-127)
    int asciiCount = 0;
    std::cout << "ASCII range (32-127): ";
    for (uint32_t c = 32; c < 128; ++c) {
        if (font.hasGlyph(c)) {
            asciiCount++;
            if (asciiCount <= 10) {
                std::cout << (char)c << " ";
            }
        }
    }
    if (asciiCount > 10) std::cout << "... ";
    std::cout << "(" << asciiCount << " glyphs)\n";
    
    // Extended ASCII (128-255)
    int extCount = 0;
    std::cout << "Extended ASCII (128-255): ";
    for (uint32_t c = 128; c < 256; ++c) {
        if (font.hasGlyph(c)) {
            extCount++;
            if (extCount <= 5) {
                std::cout << "0x" << std::hex << c << std::dec << " ";
            }
        }
    }
    if (extCount > 5) std::cout << "... ";
    std::cout << "(" << extCount << " glyphs)\n";
    
    // GB2312 range (common Chinese encoding)
    int gbCount = 0;
    std::cout << "GB2312 range (0xA1A1-0xFEFE): ";
    for (uint32_t hi = 0xA1; hi <= 0xFE; ++hi) {
        for (uint32_t lo = 0xA1; lo <= 0xFE; ++lo) {
            uint32_t code = (hi << 8) | lo;
            if (font.hasGlyph(code)) {
                gbCount++;
            }
        }
    }
    std::cout << "(" << gbCount << " glyphs)\n";
    
    // Low range (0-255)
    int lowCount = 0;
    std::cout << "Low range (0-255): ";
    for (uint32_t c = 0; c < 256; ++c) {
        if (font.hasGlyph(c)) {
            lowCount++;
        }
    }
    std::cout << "(" << lowCount << " glyphs)\n";
    
    // Check specific ranges
    int rangeCount = 0;
    std::cout << "Range 0x0000-0xFFFF scan: ";
    for (uint32_t c = 0; c <= 0xFFFF; ++c) {
        if (font.hasGlyph(c)) {
            rangeCount++;
        }
    }
    std::cout << "(" << rangeCount << " glyphs)\n";
    
    // List all found glyphs
    std::cout << "\n=== All Found Glyphs ===\n";
    int count = 0;
    for (uint32_t c = 0; c <= 0xFFFFFF; ++c) {
        if (font.hasGlyph(c)) {
            const shx::Glyph* g = font.getGlyph(c);
            std::cout << "  Code: 0x" << std::hex << std::setw(6) << std::setfill('0') << c 
                      << std::dec << " (" << c << ")";
            if (g) {
                std::cout << " - Commands: " << g->commands.size() 
                          << ", Width: " << g->width;
            }
            std::cout << "\n";
            count++;
            if (count >= 50) {
                std::cout << "  ... (showing first 50 only)\n";
                break;
            }
        }
    }
    
    std::cout << "\nTotal glyphs found: " << font.getGlyphCount() << "\n";
    std::cout << "\nDone!\n";
    
    return 0;
}
