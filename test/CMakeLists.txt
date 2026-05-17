cmake_minimum_required(VERSION 3.16)
project(brep_to_ifc)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Match main project settings
set(IFCOPENSHELL_DIR "D:/QtOCCTApp/3rpart/ifcopenshell-vs2026-x64-v142")
set(BOOST_ROOT "D:/IfcOpenShell/_deps/boost_1_86_0")
set(OCCT_DIR "D:/QtOCCTApp/3rpart/opencascade-8.0.0-vc14-64-pch-with-debug-combined/opencascade-8.0.0-vc14-64")

include_directories(${IFCOPENSHELL_DIR}/include)
include_directories(${BOOST_ROOT})
include_directories(${OCCT_DIR}/inc)

add_definitions(-DHAS_SCHEMA_4x3_add2)
add_definitions(-DWNT)
add_definitions(-DBOOST_LIB_TOOLSET=vc142)
add_definitions(-DIFC_SHARED_BUILD)
add_definitions(-DBOOST_ALL_NO_LIB)
add_definitions(-D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)
add_definitions(-D_UNICODE)
add_definitions(-DUNICODE)

link_directories(${IFCOPENSHELL_DIR}/lib)
link_directories(${OCCT_DIR}/win64/vc14/lib)

add_executable(brep_to_ifc brep_to_ifc.cpp)
add_executable(test_crash test_crash.cpp)

if(MSVC)
    target_compile_options(brep_to_ifc PRIVATE /FIStandard_Boolean.hxx /utf-8 /permissive)
    target_compile_options(test_crash PRIVATE /FIStandard_Boolean.hxx /utf-8 /permissive)
endif()

target_link_libraries(brep_to_ifc PRIVATE
    IfcParse
    IfcGeom
    Serializers
    geometry_serializer
    geometry_kernel_opencascade
    TKernel
    TKMath
    TKBRep
    TKTopAlgo
)

target_link_libraries(test_crash PRIVATE
    IfcParse
)

if(MSVC)
    target_link_options(brep_to_ifc PRIVATE "/WHOLEARCHIVE:IfcParse")
    target_link_options(test_crash PRIVATE "/WHOLEARCHIVE:IfcParse")
endif()

