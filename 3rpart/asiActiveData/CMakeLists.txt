cmake_minimum_required(VERSION 3.16)

project(asiActiveData)

# -----------------------------------------------------------------------------
# Find Dependencies
# -----------------------------------------------------------------------------

# OpenCASCADE
find_package(OpenCASCADE REQUIRED)

# VTK
find_package(VTK REQUIRED)

# TBB (using CONFIG for modern TBB versions)
find_package(TBB CONFIG REQUIRED)

# -----------------------------------------------------------------------------
# Source Files
# -----------------------------------------------------------------------------
file(GLOB_RECURSE SOURCES 
    "*.cpp" 
    "*.h" 
    "*.hpp"
)

# -----------------------------------------------------------------------------
# Target Definition
# -----------------------------------------------------------------------------
add_library(asiActiveData SHARED ${SOURCES})

set_target_properties(asiActiveData PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# -----------------------------------------------------------------------------
# Compile Definitions
# -----------------------------------------------------------------------------
target_compile_definitions(asiActiveData PRIVATE 
    ASIACTIVEDATA_EXPORTS 
    _WINDOWS 
    _USRDLL
)

# -----------------------------------------------------------------------------
# Include Directories & Linking
# -----------------------------------------------------------------------------
target_include_directories(asiActiveData PUBLIC 
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/API"
    "${CMAKE_CURRENT_SOURCE_DIR}/BinDrivers"
    "${CMAKE_CURRENT_SOURCE_DIR}/Kernel"
    "${CMAKE_CURRENT_SOURCE_DIR}/Mesh"
    "${CMAKE_CURRENT_SOURCE_DIR}/Mesh/DS"
    "${CMAKE_CURRENT_SOURCE_DIR}/Patterns"
    "${CMAKE_CURRENT_SOURCE_DIR}/STD"
    "${CMAKE_CURRENT_SOURCE_DIR}/Tools"
    ${OpenCASCADE_INCLUDE_DIRS}
    ${VTK_INCLUDE_DIRS}
)

target_link_libraries(asiActiveData PRIVATE 
    ${OpenCASCADE_LIBRARIES}
    ${VTK_LIBRARIES}
    TBB::tbb
)
