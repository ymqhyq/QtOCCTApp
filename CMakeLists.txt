cmake_minimum_required(VERSION 3.16)

project(QtOCCTApp VERSION 1.0.0)

# Export compile commands for IDE IntelliSense support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set output directory for Debug builds
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set Qt6 root directory
set(CMAKE_PREFIX_PATH "D:/Qt/6.10.1/msvc2022_64")

# Find required packages
find_package(Qt6 COMPONENTS Core Widgets Gui OpenGL Svg QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 COMPONENTS Core Widgets Gui OpenGL Svg REQUIRED)
    set(QT_VERSION Qt5)
else()
    set(QT_VERSION Qt6)
endif()

# Set OpenCASCADE include and library directories
set(OpenCASCADE_INCLUDE_DIR "D:/BrCAD/OCCT/OCCT_7_6_0/inc")
set(OpenCASCADE_LIBRARY_DIR "D:/BrCAD/OCCT/OCCT_7_6_0/win64/vc14/lib")

# Manually set the required libraries
set(OpenCASCADE_LIBRARIES
    "TKernel"
    "TKMath"
    "TCollection"
    "gp"
    "Geom"
    "Geom2d"
    "Graphic3d"
    "Prs3d"
    "AIS"
    "V3d"
    "Aspect"
    "StdPrs"
    "StdObj"
    "BRep"
    "BRepPrim"
    "BRepAlgoAPI"
    "BRepBuilderAPI"
    "StlAPI"
    "Draw"
)

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
    resources.qrc
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${OpenCASCADE_INCLUDE_DIR}
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

# Add core-data-model
add_subdirectory(core-data-model)

# ActiveData needs Standard_Boolean.hxx forced include on MSVC
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /FIStandard_Boolean.hxx /utf-8)
endif()


# Add shxparser library
add_subdirectory(libs/shxparser)
target_link_libraries(${PROJECT_NAME} shxparser)

# Add SARibbonBar library
set(SARIBBON_BUILD_EXAMPLES OFF CACHE BOOL "Disable SARibbon examples" FORCE)
add_subdirectory("D:/GitHub_Ymqhyq/SARibbon" "${CMAKE_BINARY_DIR}/SARibbon")
target_link_libraries(${PROJECT_NAME} SARibbonBar)

# Add Qt OpenGL module if needed
if(QT_VERSION STREQUAL "Qt6")
    find_package(Qt6 REQUIRED COMPONENTS OpenGLWidgets Network)
    target_link_libraries(${PROJECT_NAME} Qt6::OpenGLWidgets)
else()
    find_package(Qt5 REQUIRED COMPONENTS OpenGL Network)
    target_link_libraries(${PROJECT_NAME} Qt5::OpenGL)
endif()

# Link libraries based on Qt version
# Link libraries based on Qt version
if(QT_VERSION STREQUAL "Qt6")
    target_link_libraries(${PROJECT_NAME}
        Qt6::Core
        Qt6::Widgets
        Qt6::Gui
        Qt6::OpenGL
        Qt6::Svg
        Qt6::Network
        core_data_model
    )
else()
    target_link_libraries(${PROJECT_NAME}
        Qt5::Core
        Qt5::Widgets
        Qt5::Gui
        Qt5::OpenGL
        Qt5::Svg
        Qt5::Network
        core_data_model
    )
endif()

# Define the actual OCCT library names
set(OCCT_POSSIBLE_LIB_NAMES
    "TKernel"
    "TKMath"
    "TCollection"
    "gp"
    "Geom"
    "Geom2d"
    "Graphic3d"
    "Prs3d"
    "AIS"
    "V3d"
    "Aspect"
    "StdPrs"
    "StdObj"
    "BRep"
    "BRepPrim"
    "BRepAlgoAPI"
    "BRepBuilderAPI"
    "StlAPI"
    "Draw"
)

# Define the actual OCCT library names for Windows/MSVC (based on the actual files in the lib directory)
set(OCCT_LIBRARIES_FULLNAMES
    "TKBin.lib"
    "TKBinL.lib"
    "TKBinTObj.lib"
    "TKBinXCAF.lib"
    "TKBO.lib"
    "TKBool.lib"
    "TKBRep.lib"
    "TKCAF.lib"
    "TKCDF.lib"
    "TKDCAF.lib"
    "TKDraw.lib"
    "TKernel.lib"
    "TKFeat.lib"
    "TKFillet.lib"
    "TKG2d.lib"
    "TKG3d.lib"
    "TKGeomAlgo.lib"
    "TKGeomBase.lib"
    "TKHLR.lib"
    "TKIGES.lib"
    "TKLCAF.lib"
    "TKMath.lib"
    "TKMesh.lib"
    "TKMeshVS.lib"
    "TKOffset.lib"
    "TKOpenGl.lib"
    "TKPrim.lib"
    "TKRWMesh.lib"
    "TKService.lib"
    "TKShHealing.lib"
    "TKStd.lib"
    "TKStdL.lib"
    "TKSTEP.lib"
    "TKSTEP209.lib"
    "TKSTEPAttr.lib"
    "TKSTEPBase.lib"
    "TKSTL.lib"
    "TKTObj.lib"
    "TKTopAlgo.lib"
    "TKV3d.lib"
    "TKVCAF.lib"
    "TKVRML.lib"
    "TKXCAF.lib"
    "TKXDEIGES.lib"
    "TKXDESTEP.lib"
    "TKXMesh.lib"
    "TKXml.lib"
    "TKXmlL.lib"
    "TKXmlTObj.lib"
    "TKXmlXCAF.lib"
    "TKXSBase.lib"
    "TKXSDRAW.lib"
)

# Set the library directory for linker
link_directories(${OpenCASCADE_LIBRARY_DIR})

# Add linker flags to ensure the library path is included
if(WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LIBPATH:\"${OpenCASCADE_LIBRARY_DIR}\"")
endif()

# Link the OCCT libraries by name (the linker will find them in the specified directory)
set(OCCT_LIBRARIES_NAMES
    "TKBin"
    "TKBinL"
    "TKBinTObj"
    "TKBinXCAF"
    "TKBO"
    "TKBool"
    "TKBRep"
    "TKCAF"
    "TKCDF"
    "TKDCAF"
    "TKDraw"
    "TKernel"
    "TKFeat"
    "TKFillet"
    "TKG2d"
    "TKG3d"
    "TKGeomAlgo"
    "TKGeomBase"
    "TKHLR"
    "TKIGES"
    "TKLCAF"
    "TKMath"
    "TKMesh"
    "TKMeshVS"
    "TKOffset"
    "TKOpenGl"
    "TKPrim"
    "TKRWMesh"
    "TKService"
    "TKShHealing"
    "TKStd"
    "TKStdL"
    "TKSTEP"
    "TKSTEP209"
    "TKSTEPAttr"
    "TKSTEPBase"
    "TKSTL"
    "TKTObj"
    "TKTopAlgo"
    "TKV3d"
    "TKVCAF"
    "TKVRML"
    "TKXCAF"
    "TKXDEIGES"
    "TKXDESTEP"
    "TKXMesh"
    "TKXml"
    "TKXmlL"
    "TKXmlTObj"
    "TKXmlXCAF"
    "TKXSBase"
    "TKXSDRAW"
)

foreach(OCCT_LIB_NAME ${OCCT_LIBRARIES_NAMES})
    target_link_libraries(${PROJECT_NAME} ${OCCT_LIB_NAME})
endforeach()

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
