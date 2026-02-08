/*
 * ASCII Font Diagnostic
 * Check why txt.shx glyphs have 0 commands
 */

#include "ShxParser.h"
#include <iostream>
#include <iomanip>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <ascii_shx>\n";
        return 1;
    }
    
    shx::ShxFont asciiFont;
    if (!asciiFont.load(argv[1])) {
        std::cerr << "Error: " << asciiFont.getLastError() << "\n";
        return 1;
    }
    
    std::cout << "Font: " << argv[1] << "\n";
    std::cout << "Glyphs: " << asciiFont.getGlyphCount() << "\n";
    
    // Check 'A' (0x41)
    uint32_t code = 'A';
    const shx::Glyph* g = asciiFont.getGlyph(code);
    
    std::cout << "Glyph 'A' (0x41): ";
    if (!g) {
        std::cout << "NOT FOUND\n";
    } else {
        std::cout << "FOUND, Commands: " << g->commands.size() << "\n";
        // Print raw data if needed (requires access to raw bytes, currently not exposed)
    }
    
    // Check 'H' (0x48)
    code = 'H';
    g = asciiFont.getGlyph(code);
    std::cout << "Glyph 'H' (0x48): ";
    if (!g) {
        std::cout << "NOT FOUND\n";
    } else {
        std::cout << "FOUND, Commands: " << g->commands.size() << "\n";
    }
    
    return 0;
}
