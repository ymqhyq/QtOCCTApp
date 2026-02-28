cmake_minimum_required(VERSION 3.16)
project(ShxParser VERSION 1.0.0 LANGUAGES CXX)

# C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 选项
option(SHX_BUILD_EXAMPLES "Build example programs" ON)
option(SHX_BUILD_TESTS "Build test programs" ON)

# 库源文件
set(SHX_SOURCES
    src/ShxParser.cpp
)

set(SHX_HEADERS
    include/ShxParser.h
)

# 创建库
add_library(shxparser ${SHX_SOURCES} ${SHX_HEADERS})

# 包含目录
target_include_directories(shxparser PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# 设置库属性
set_target_properties(shxparser PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER include/ShxParser.h
    DEBUG_POSTFIX ""
)

# Windows 特定设置
if(WIN32)
    target_compile_definitions(shxparser PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# 示例程序
if(SHX_BUILD_EXAMPLES)
    add_executable(shx_example examples/example.cpp)
    target_link_libraries(shx_example PRIVATE shxparser)
    
    add_executable(bigfont_test examples/bigfont_test.cpp)
    target_link_libraries(bigfont_test PRIVATE shxparser)
    
    add_executable(chinese_test examples/chinese_test.cpp)
    target_link_libraries(chinese_test PRIVATE shxparser)
    
    add_executable(mixed_test examples/mixed_test.cpp)
    target_link_libraries(mixed_test PRIVATE shxparser)
    
    add_executable(dual_font_test examples/dual_font_test.cpp)
    target_link_libraries(dual_font_test PRIVATE shxparser)
    
    add_executable(ascii_diag examples/ascii_diag.cpp)
    target_link_libraries(ascii_diag PRIVATE shxparser)
endif()

# 测试程序
if(SHX_BUILD_TESTS)
    add_executable(shx_test test/test_shx.cpp)
    target_link_libraries(shx_test PRIVATE shxparser)
endif()

# 安装目标
install(TARGETS shxparser
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include
)
