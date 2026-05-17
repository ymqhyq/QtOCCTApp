# -----------------------------------------------------------------------------
# Find Dependencies (already found in root, but we can re-find or just use)
# -----------------------------------------------------------------------------

# -----------------------------------------------------------------------------
# Source Files
# -----------------------------------------------------------------------------
file(GLOB_RECURSE SOURCES "generated/*.cpp")
file(GLOB_RECURSE HEADERS "generated/*.h")

# GeometryService & IfcExportService (manual classes)
list(APPEND SOURCES "GeometryService.cpp" "IfcExportService.cpp")
list(APPEND HEADERS "GeometryService.h" "IfcExportService.h")

# -----------------------------------------------------------------------------
# Target Definition
# -----------------------------------------------------------------------------
add_library(core_data_model SHARED ${SOURCES} ${HEADERS})

set_target_properties(core_data_model PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

# -----------------------------------------------------------------------------
# Compile Definitions & Options
# -----------------------------------------------------------------------------
target_compile_definitions(core_data_model PRIVATE 
    BR_EXPORT_EXPORTS
    HAS_SCHEMA_4x3_add2
    BOOST_LIB_TOOLSET=vc142
)

if(MSVC)
    target_compile_options(core_data_model PRIVATE /FIStandard_Boolean.hxx /utf-8)
endif()

# -----------------------------------------------------------------------------
# Include Directories
# -----------------------------------------------------------------------------
target_include_directories(core_data_model PUBLIC 
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/generated"
    ${OpenCASCADE_INCLUDE_DIRS}
    ${VTK_INCLUDE_DIRS}
    "${CMAKE_SOURCE_DIR}/3rpart"
    "${CMAKE_SOURCE_DIR}/3rpart/asiActiveData"
    "${CMAKE_SOURCE_DIR}/3rpart/asiActiveData/API"
    "${CMAKE_SOURCE_DIR}/3rpart/asiActiveData/Kernel"
    "${CMAKE_SOURCE_DIR}/3rpart/asiActiveData/Mesh"
    "${CMAKE_SOURCE_DIR}/3rpart/asiActiveData/Mesh/DS"
    "${CMAKE_SOURCE_DIR}/3rpart/asiActiveData/Tools"
    "${CMAKE_SOURCE_DIR}/3rpart/ifcopenshell-vs2026-x64-v142/include"
    "D:/IfcOpenShell/_deps/boost_1_86_0"
    "D:/IfcOpenShell/_deps-vs2019-x64-installed/Eigen"
)

# -----------------------------------------------------------------------------
# Link Libraries
# -----------------------------------------------------------------------------
target_link_directories(core_data_model PUBLIC "D:/IfcOpenShell/_deps/boost_1_86_0/stage/vs2019-x64/lib")
target_link_libraries(core_data_model PUBLIC 
    ${OpenCASCADE_LIBRARIES}
    asiActiveData
    TBB::tbb
    IfcParse
    IfcGeom
    Serializers
    geometry_serializer
    geometry_kernel_opencascade
)

if(MSVC)
    target_link_options(core_data_model PUBLIC "/WHOLEARCHIVE:IfcParse")
endif()

# -----------------------------------------------------------------------------
# Test Programs
# -----------------------------------------------------------------------------
add_executable(test_load_bridge test_load_bridge.cpp)
target_link_libraries(test_load_bridge PRIVATE 
    core_data_model 
    ${OpenCASCADE_LIBRARIES} 
    asiActiveData
)

add_executable(test_minimal test_minimal.cpp)
target_link_libraries(test_minimal PRIVATE core_data_model)

# -----------------------------------------------------------------------------
# Output Paths
# -----------------------------------------------------------------------------
set_target_properties(core_data_model PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(test_load_bridge PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(test_minimal PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
