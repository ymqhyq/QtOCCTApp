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
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
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

# =============================================================
# 组织 IDE 中的项目结构（Visual Studio 解决方案文件夹/项目筛选器）
# =============================================================
# 启用解决方案文件夹功能
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# 将主库 shxparser 放入 "ShxParser" 文件夹下
set_target_properties(shxparser PROPERTIES FOLDER "ShxParser")

# 将所有示例可执行程序放入 "ShxParser/Examples" 文件夹下
if(SHX_BUILD_EXAMPLES)
    set(SHX_EXAMPLES shx_example bigfont_test chinese_test mixed_test dual_font_test ascii_diag)
    foreach(target IN LISTS SHX_EXAMPLES)
        if(TARGET ${target})
            set_target_properties(${target} PROPERTIES FOLDER "ShxParser/Examples")
        endif()
    endforeach()
endif()

# 将测试程序放入 "ShxParser/Tests" 文件夹下
if(SHX_BUILD_TESTS)
    if(TARGET shx_test)
        set_target_properties(shx_test PROPERTIES FOLDER "ShxParser/Tests")
    endif()
endif()

