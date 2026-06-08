# -----------------------------------------------------------------------------
# Find Dependencies (already found in root, but we can re-find or just use)
# -----------------------------------------------------------------------------

# -----------------------------------------------------------------------------
# Source Files
# -----------------------------------------------------------------------------
file(GLOB_RECURSE SOURCES "generated/*.cpp")
file(GLOB_RECURSE HEADERS "generated/*.h")

# GeometryService & IfcExportService (manual classes)
list(APPEND SOURCES "GeometryService.cpp" "IfcExportService.cpp" "RwBuilder.cpp" "RwSlope2DGeometryBuilder.cpp" "ProjectManager.cpp")
list(APPEND HEADERS "GeometryService.h" "IfcExportService.h" "RwBuilder.h" "RwSlope2DGeometryBuilder.h" "ProjectManager.h")

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
    "${CMAKE_SOURCE_DIR}/3rpart/boost_1_86_0"
    "${CMAKE_SOURCE_DIR}/3rpart/Eigen"
)

# -----------------------------------------------------------------------------
# Link Libraries
# -----------------------------------------------------------------------------
target_link_directories(core_data_model PUBLIC "${CMAKE_SOURCE_DIR}/3rpart/boost_1_86_0/stage/vs2026-x64-v142/lib")
target_link_libraries(core_data_model PUBLIC 
    ${OpenCASCADE_LIBRARIES}
    TBB::tbb
    debug ${IFC_PARSE_DEBUG}   optimized ${IFC_PARSE_RELEASE}
    debug ${IFC_GEOM_DEBUG}    optimized ${IFC_GEOM_RELEASE}
    debug ${SERIALIZERS_DEBUG} optimized ${SERIALIZERS_RELEASE}
    debug ${GEOM_SER_DEBUG}    optimized ${GEOM_SER_RELEASE}
    debug ${GEOM_KER_DEBUG}    optimized ${GEOM_KER_RELEASE}
    debug ${ASIACTIVEDATA_DEBUG} optimized ${ASIACTIVEDATA_RELEASE}
)

if(MSVC)
    target_link_options(core_data_model PUBLIC "$<IF:$<CONFIG:DEBUG>,/WHOLEARCHIVE:IfcParse_d,/WHOLEARCHIVE:IfcParse>")
endif()

# -----------------------------------------------------------------------------
# Test Programs
# -----------------------------------------------------------------------------
add_executable(test_load_bridge test_load_bridge.cpp)
target_link_libraries(test_load_bridge PRIVATE 
    core_data_model 
    ${OpenCASCADE_LIBRARIES} 
    debug ${ASIACTIVEDATA_DEBUG} optimized ${ASIACTIVEDATA_RELEASE}
)

add_executable(test_master_project test_master_project.cpp)
target_link_libraries(test_master_project PRIVATE 
    core_data_model 
    ${OpenCASCADE_LIBRARIES} 
    debug ${ASIACTIVEDATA_DEBUG} optimized ${ASIACTIVEDATA_RELEASE}
)

# -----------------------------------------------------------------------------
# Output Paths
# -----------------------------------------------------------------------------
set_target_properties(core_data_model PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(test_load_bridge PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(test_master_project PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# -----------------------------------------------------------------------------
# brep_to_cbf CLI tool
# -----------------------------------------------------------------------------
add_executable(brep_to_cbf tools/brep_to_cbf.cpp)
target_link_libraries(brep_to_cbf PRIVATE 
    ${OpenCASCADE_LIBRARIES} 
)
if(MSVC)
    target_compile_options(brep_to_cbf PRIVATE /FIStandard_Boolean.hxx /utf-8)
endif()
target_include_directories(brep_to_cbf PRIVATE 
    ${OpenCASCADE_INCLUDE_DIRS}
)
set_target_properties(brep_to_cbf PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
