cmake_minimum_required(VERSION 3.16)

project(QtOCCTApp VERSION 1.0.0)

# Export compile commands for IDE IntelliSense support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set output directory for Debug builds
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug")

# Set output directory for Release builds
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set Qt6 root directory
set(CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/3rpart/QT6.7.0/msvc2019_64")

# Find required packages
find_package(Qt6 COMPONENTS Core Widgets Gui OpenGL Svg QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 COMPONENTS Core Widgets Gui OpenGL Svg REQUIRED)
    set(QT_VERSION Qt5)
else()
    set(QT_VERSION Qt6)
endif()

# Set OpenCASCADE directory
set(OpenCASCADE_DIR "${CMAKE_SOURCE_DIR}/3rpart/opencascade-8.0.0-vc14-64-pch-with-debug-combined/opencascade-8.0.0-vc14-64/cmake")
find_package(OpenCASCADE REQUIRED)

# Set VTK directory
set(VTK_DIR "${CMAKE_SOURCE_DIR}/3rpart/opencascade-8.0.0-vc14-64-pch-with-debug-combined/3rdparty-vc14-64/vtk-9.4.1-x64/lib/cmake/vtk-9.4")
find_package(VTK REQUIRED)

# Set TBB directory
set(TBB_DIR "${CMAKE_SOURCE_DIR}/3rpart/opencascade-8.0.0-vc14-64-pch-with-debug-combined/3rdparty-vc14-64/tbb-2021.13.0-x64/lib/cmake/TBB")
find_package(TBB CONFIG REQUIRED)

# Set IfcOpenShell directory
set(IFCOPENSHELL_DIR "${CMAKE_SOURCE_DIR}/3rpart/ifcopenshell-vs2026-x64-v142")
set(BOOST_ROOT "${CMAKE_SOURCE_DIR}/3rpart/boost_1_86_0")

include_directories(${IFCOPENSHELL_DIR}/include)
include_directories(${BOOST_ROOT})

# Define IfcOpenShell schemas
add_definitions(-DHAS_SCHEMA_4x3_add2)

# Find IfcOpenShell Debug and Release libraries individually for robust multi-config builds
find_library(IFC_PARSE_RELEASE NAMES IfcParse PATHS "${IFCOPENSHELL_DIR}/lib" NO_DEFAULT_PATH)
find_library(IFC_PARSE_DEBUG   NAMES IfcParse_d PATHS "${IFCOPENSHELL_DIR}/libd" NO_DEFAULT_PATH)

find_library(IFC_GEOM_RELEASE  NAMES IfcGeom PATHS "${IFCOPENSHELL_DIR}/lib" NO_DEFAULT_PATH)
find_library(IFC_GEOM_DEBUG    NAMES IfcGeom_d PATHS "${IFCOPENSHELL_DIR}/libd" NO_DEFAULT_PATH)

find_library(SERIALIZERS_RELEASE NAMES Serializers PATHS "${IFCOPENSHELL_DIR}/lib" NO_DEFAULT_PATH)
find_library(SERIALIZERS_DEBUG   NAMES Serializers_d PATHS "${IFCOPENSHELL_DIR}/libd" NO_DEFAULT_PATH)

# Enable automatic MOC, UIC and RCC processing for Qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Create executable
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/MainWindow.cpp
    include/MainWindow.h
    src/OCCTWidget.cpp
    include/OCCTWidget.h
    src/AspectWindow.cpp
    include/AspectWindow.h
    src/Line.cpp
    include/ShxTextGenerator.h
    src/ShxTextGenerator.cpp
    include/PythonSyntaxHighlighter.h
    src/PythonSyntaxHighlighter.cpp
    include/ModelExplorerPanel.h
    src/ModelExplorerPanel.cpp
    resources.qrc
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${OpenCASCADE_INCLUDE_DIRS}
    ${VTK_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/libs/shxparser/include
    ${CMAKE_SOURCE_DIR}/core-data-model
    ${CMAKE_SOURCE_DIR}/core-data-model/generated
    ${CMAKE_SOURCE_DIR}/3rpart
    ${CMAKE_SOURCE_DIR}/3rpart/asiActiveData
    ${CMAKE_SOURCE_DIR}/3rpart/asiActiveData/Kernel
    ${CMAKE_SOURCE_DIR}/3rpart/asiActiveData/API
    ${CMAKE_SOURCE_DIR}/3rpart/asiActiveData/Mesh
    ${CMAKE_SOURCE_DIR}/3rpart/asiActiveData/Mesh/DS
    ${CMAKE_SOURCE_DIR}/3rpart/asiActiveData/Tools
)

# Add asiActiveData
add_subdirectory(3rpart/asiActiveData)

# Add core-data-model
add_subdirectory(core-data-model)

# ActiveData needs Standard_Boolean.hxx forced include on MSVC
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /FIStandard_Boolean.hxx /utf-8)
endif()


# Add shxparser library
add_subdirectory(libs/shxparser)
target_link_libraries(${PROJECT_NAME} PRIVATE shxparser)

# Add SARibbonBar library from local 3rpart pre-compiled package
set(SARibbonBar_DIR "${CMAKE_SOURCE_DIR}/3rpart/SARibbon_qt6.7.0_MSVC_x64_v142/lib/cmake/SARibbonBar")
find_package(SARibbonBar REQUIRED)

# Add Qt OpenGL module if needed
if(QT_VERSION STREQUAL "Qt6")
    find_package(Qt6 REQUIRED COMPONENTS OpenGLWidgets Network)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::OpenGLWidgets)
else()
    find_package(Qt5 REQUIRED COMPONENTS OpenGL Network)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::OpenGL)
endif()

# Link libraries based on Qt version
if(QT_VERSION STREQUAL "Qt6")
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Gui
        Qt6::OpenGL
        Qt6::Svg
        Qt6::Network
        core_data_model
        asiActiveData
    )
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt5::Core
        Qt5::Widgets
        Qt5::Gui
        Qt5::OpenGL
        Qt5::Svg
        Qt5::Network
        core_data_model
        asiActiveData
    )
endif()

# Link the OCCT, VTK, TBB, IfcOpenShell, and SARibbonBar libraries
target_link_libraries(${PROJECT_NAME} PRIVATE 
    ${OpenCASCADE_LIBRARIES}
    ${VTK_LIBRARIES}
    TBB::tbb
    debug ${IFC_PARSE_DEBUG}   optimized ${IFC_PARSE_RELEASE}
    debug ${IFC_GEOM_DEBUG}    optimized ${IFC_GEOM_RELEASE}
    debug ${SERIALIZERS_DEBUG} optimized ${SERIALIZERS_RELEASE}
    SARibbonBar::SARibbonBar
)

# Set C++ properties for OCCT
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# For Windows
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE FALSE
    )
endif()

# Copy cq_script and run_cq.py to the output directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/cq_script
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/cq_script
)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/run_cq.py
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/run_cq.py
)

# Add test directory
add_subdirectory(test)
