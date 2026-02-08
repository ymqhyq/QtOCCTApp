/*
 * ShxParser Test - Unit tests
 */

#include "ShxParser.h"
#include <iostream>
#include <cassert>
#include <cmath>
#include <cstring>

// Simple test framework
#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "Running " #name "... "; \
    try { \
        test_##name(); \
        std::cout << "PASSED\n"; \
        passed++; \
    } catch (const std::exception& e) { \
        std::cout << "FAILED: " << e.what() << "\n"; \
        failed++; \
    } catch (...) { \
        std::cout << "FAILED: Unknown exception\n"; \
        failed++; \
    } \
} while(0)

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) throw std::runtime_error("Assertion failed: " #cond); \
} while(0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_NE(a, b) ASSERT_TRUE((a) != (b))

static int passed = 0;
static int failed = 0;

//=============================================================================
// Test Cases
//=============================================================================

TEST(version) {
    const char* ver = shx::getVersion();
    ASSERT_TRUE(ver != nullptr);
    ASSERT_TRUE(strlen(ver) > 0);
    std::cout << "(v" << ver << ") ";
}

TEST(point2d_operations) {
    shx::Point2D p1(1.0, 2.0);
    shx::Point2D p2(3.0, 4.0);
    
    auto sum = p1 + p2;
    ASSERT_TRUE(std::abs(sum.x - 4.0) < 0.001);
    ASSERT_TRUE(std::abs(sum.y - 6.0) < 0.001);
    
    auto scaled = p1 * 2.0;
    ASSERT_TRUE(std::abs(scaled.x - 2.0) < 0.001);
    ASSERT_TRUE(std::abs(scaled.y - 4.0) < 0.001);
}

TEST(draw_command_factory) {
    auto move = shx::DrawCommand::moveTo(10.0, 20.0);
    ASSERT_EQ(move.type, shx::CommandType::MoveTo);
    ASSERT_TRUE(std::abs(move.endPoint.x - 10.0) < 0.001);
    
    auto line = shx::DrawCommand::lineTo(30.0, 40.0);
    ASSERT_EQ(line.type, shx::CommandType::LineTo);
    
    auto arc = shx::DrawCommand::arcTo(50.0, 60.0, 45.0, 55.0);
    ASSERT_EQ(arc.type, shx::CommandType::ArcTo);
    ASSERT_TRUE(std::abs(arc.controlPoint.x - 45.0) < 0.001);
}

TEST(path_collector) {
    shx::PathCollector collector;
    
    collector.moveTo(0, 0);
    collector.lineTo(10, 10);
    collector.arcTo(20, 0, 15, 5);
    
    ASSERT_EQ(collector.commands.size(), 3u);
    ASSERT_EQ(collector.commands[0].type, shx::CommandType::MoveTo);
    ASSERT_EQ(collector.commands[1].type, shx::CommandType::LineTo);
    ASSERT_EQ(collector.commands[2].type, shx::CommandType::ArcTo);
    
    collector.clear();
    ASSERT_EQ(collector.commands.size(), 0u);
}

TEST(glyph_empty) {
    shx::Glyph glyph;
    ASSERT_TRUE(glyph.isEmpty());
    
    glyph.commands.push_back(shx::DrawCommand::lineTo(1, 1));
    ASSERT_FALSE(glyph.isEmpty());
}

TEST(shx_font_default) {
    shx::ShxFont font;
    ASSERT_FALSE(font.isValid());
    ASSERT_EQ(font.getGlyphCount(), 0u);
    ASSERT_EQ(font.getGlyph(65), nullptr);
}

TEST(shx_font_invalid_file) {
    shx::ShxFont font;
    bool result = font.load("nonexistent_file.shx");
    ASSERT_FALSE(result);
    ASSERT_FALSE(font.isValid());
    ASSERT_FALSE(font.getLastError().empty());
}

TEST(is_valid_shx_file) {
    ASSERT_FALSE(shx::isValidShxFile("nonexistent.shx"));
}

TEST(shx_font_move) {
    shx::ShxFont font1;
    shx::ShxFont font2 = std::move(font1);
    
    ASSERT_FALSE(font2.isValid());
}

//=============================================================================
// Integration Tests (require actual SHX file)
//=============================================================================

void runIntegrationTests(const char* shxFile) {
    std::cout << "\n=== Integration Tests with: " << shxFile << " ===\n";
    
    std::cout << "Checking file validity... ";
    if (!shx::isValidShxFile(shxFile)) {
        std::cout << "SKIPPED (invalid file)\n";
        return;
    }
    std::cout << "OK\n";
    
    std::cout << "Loading font... ";
    shx::ShxFont font;
    if (!font.load(shxFile)) {
        std::cout << "FAILED: " << font.getLastError() << "\n";
        failed++;
        return;
    }
    std::cout << "OK\n";
    passed++;
    
    std::cout << "Validating font properties... ";
    ASSERT_TRUE(font.isValid());
    ASSERT_TRUE(font.getGlyphCount() > 0);
    std::cout << "OK (glyphs: " << font.getGlyphCount() << ")\n";
    passed++;
    
    std::cout << "Testing glyph access... ";
    bool foundGlyph = false;
    for (uint32_t c = 32; c < 128; ++c) {
        if (font.hasGlyph(c)) {
            const shx::Glyph* g = font.getGlyph(c);
            ASSERT_NE(g, nullptr);
            foundGlyph = true;
            break;
        }
    }
    if (foundGlyph) {
        std::cout << "OK\n";
        passed++;
    } else {
        std::cout << "SKIPPED (no ASCII glyphs)\n";
    }
    
    std::cout << "Testing text rendering... ";
    shx::PathCollector collector;
    font.render(collector, "Test", 10.0);
    std::cout << "OK (commands: " << collector.commands.size() << ")\n";
    passed++;
    
    std::cout << "Testing text measurement... ";
    double width = font.measureText("Test", 10.0);
    std::cout << "OK (width: " << width << ")\n";
    passed++;
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char* argv[]) {
    std::cout << "=== SHX Parser Unit Tests ===\n\n";
    
    RUN_TEST(version);
    RUN_TEST(point2d_operations);
    RUN_TEST(draw_command_factory);
    RUN_TEST(path_collector);
    RUN_TEST(glyph_empty);
    RUN_TEST(shx_font_default);
    RUN_TEST(shx_font_invalid_file);
    RUN_TEST(is_valid_shx_file);
    RUN_TEST(shx_font_move);
    
    if (argc > 1) {
        runIntegrationTests(argv[1]);
    } else {
        std::cout << "\nTip: Provide SHX file path to run integration tests\n";
        std::cout << "Usage: " << argv[0] << " [shx_file]\n";
    }
    
    std::cout << "\n=== Test Results ===\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";
    
    return failed > 0 ? 1 : 0;
}
