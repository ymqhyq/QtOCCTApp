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
find_package(Qt6 COMPONENTS Core Widgets Gui OpenGL QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 COMPONENTS Core Widgets Gui OpenGL REQUIRED)
    set(QT_VERSION Qt5)
else()
    set(QT_VERSION Qt6)
endif()

# Set OpenCASCADE include and library directories
set(OpenCASCADE_INCLUDE_DIR "D:/GitHub_Ymqhyq/OCCT/build-vs2022-x64/inc")
set(OpenCASCADE_LIBRARY_DIR "D:/GitHub_Ymqhyq/OCCT/build-vs2022-x64/win64/vc14/lib")

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
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${OpenCASCADE_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/libs/shxparser/include
)

# Add shxparser library
add_subdirectory(libs/shxparser)
target_link_libraries(${PROJECT_NAME} shxparser)

# Add Qt OpenGL module if needed
if(QT_VERSION STREQUAL "Qt6")
    find_package(Qt6 REQUIRED COMPONENTS OpenGLWidgets)
    target_link_libraries(${PROJECT_NAME} Qt6::OpenGLWidgets)
else()
    target_link_libraries(${PROJECT_NAME} Qt5::OpenGL)
endif()

# Link libraries based on Qt version
if(QT_VERSION STREQUAL "Qt6")
    target_link_libraries(${PROJECT_NAME}
        Qt6::Core
        Qt6::Widgets
        Qt6::Gui
        Qt6::OpenGL
    )
else()
    target_link_libraries(${PROJECT_NAME}
        Qt5::Core
        Qt5::Widgets
        Qt5::Gui
        Qt5::OpenGL
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
    "TKernel.lib"
    "TKMath.lib"
    "TKCDF.lib"
    "TKTObj.lib"
    "TKBin.lib"
    "TKXml.lib"
    "TKBool.lib"
    "TKBO.lib"
    "TKCAF.lib"
    "TKXCAF.lib"
    "TKVCAF.lib"
    "TKFeat.lib"
    "TKFillet.lib"
    "TKGeomAlgo.lib"
    "TKGeomBase.lib"
    "TKHLR.lib"
    "TKTopAlgo.lib"
    "TKMesh.lib"
    "TKPrim.lib"
    "TKOffset.lib"
    "TKShHealing.lib"
    "TKG2d.lib"
    "TKG3d.lib"
    "TKOpenGl.lib"
    "TKService.lib"
    "TKV3d.lib"
    "TKGeom.lib"
    "TKBRep.lib"
    "TKXSBase.lib"
    "TKDraw.lib"
    "TKStd.lib"
    "TKStdL.lib"
    "TKXSDRAW.lib"
    "TKXmlXCAF.lib"
    "TKXmlL.lib"
    "TKXmlTObj.lib"
    "TKBinL.lib"
    "TKBinTObj.lib"
    "TKBinXCAF.lib"
    "TKDE.lib"
    "TKDCAF.lib"
    "TKDEIGES.lib"
    "TKDESTEP.lib"
    "TKDESTL.lib"
    "TKXDEIGES.lib"
    "TKXDESTEP.lib"
    "TKXMesh.lib"
    "TKMeshVS.lib"
    "TKRWMesh.lib"
    "TKLCAF.lib"
    "TKXDE.lib"
    "TKXSDRAWDE.lib"
    "TKXSDRAWIGES.lib"
    "TKXSDRAWOBJ.lib"
    "TKXSDRAWPLY.lib"
    "TKXSDRAWSTEP.lib"
    "TKXSDRAWSTL.lib"
    "TKXSDRAWVRML.lib"
)

# Set the library directory for linker
link_directories(${OpenCASCADE_LIBRARY_DIR})

# Add linker flags to ensure the library path is included
if(WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LIBPATH:\"${OpenCASCADE_LIBRARY_DIR}\"")
endif()

# Link the OCCT libraries by name (the linker will find them in the specified directory)
set(OCCT_LIBRARIES_NAMES
    "TKernel"
    "TKMath"
    "TKCDF"
    "TKTObj"
    "TKBin"
    "TKXml"
    "TKBool"
    "TKBO"
    "TKCAF"
    "TKXCAF"
    "TKVCAF"
    "TKFeat"
    "TKFillet"
    "TKGeomAlgo"
    "TKGeomBase"
    "TKHLR"
    "TKTopAlgo"
    "TKMesh"
    "TKPrim"
    "TKOffset"
    "TKShHealing"
    "TKG2d"
    "TKG3d"
    "TKOpenGl"
    "TKService"
    "TKV3d"
    "TKBRep"
    "TKXSBase"
    "TKDraw"
    "TKStd"
    "TKStdL"
    "TKXSDRAW"
    "TKXmlXCAF"
    "TKXmlL"
    "TKXmlTObj"
    "TKBinL"
    "TKBinTObj"
    "TKBinXCAF"
    "TKDE"
    "TKDCAF"
    "TKDEIGES"
    "TKDEOBJ"
    "TKDEPLY"
    "TKDESTEP"
    "TKDESTL"
    "TKDEVRML"
    "TKXMesh"
    "TKMeshVS"
    "TKRWMesh"
    "TKLCAF"
    "TKXSDRAWDE"
    "TKXSDRAWIGES"
    "TKXSDRAWOBJ"
    "TKXSDRAWPLY"
    "TKXSDRAWSTEP"
    "TKXSDRAWSTL"
    "TKXSDRAWVRML"
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
        WIN32_EXECUTABLE TRUE
    )
endif()