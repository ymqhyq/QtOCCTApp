cmake_minimum_required(VERSION 3.16)
project(brep_to_ifc)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Match main project settings via relative paths in local 3rpart folder
set(IFCOPENSHELL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../3rpart/ifcopenshell-vs2026-x64-v142")
set(BOOST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../3rpart/boost_1_86_0")
set(OCCT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../3rpart/opencascade-8.0.0-vc14-64-pch-with-debug-combined/opencascade-8.0.0-vc14-64")

include_directories(${IFCOPENSHELL_DIR}/include)
include_directories(${BOOST_ROOT})
include_directories(${OCCT_DIR}/inc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../3rpart)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../core-data-model)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../core-data-model/generated)

add_definitions(-DHAS_SCHEMA_4x3_add2)
add_definitions(-DWNT)
add_definitions(-DBOOST_LIB_TOOLSET=vc142)
add_definitions(-DIFC_SHARED_BUILD)
add_definitions(-DBOOST_ALL_NO_LIB)
add_definitions(-D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)
add_definitions(-D_UNICODE)
add_definitions(-DUNICODE)

add_executable(brep_to_ifc brep_to_ifc.cpp)
add_executable(recipe_to_ifc recipe_to_ifc.cpp)
add_executable(asicbf_to_ifc asicbf_to_ifc.cpp)

if(MSVC)
    target_compile_options(brep_to_ifc PRIVATE /FIStandard_Boolean.hxx /utf-8 /permissive)
    target_compile_options(recipe_to_ifc PRIVATE /FIStandard_Boolean.hxx /utf-8 /permissive)
    target_compile_options(asicbf_to_ifc PRIVATE /FIStandard_Boolean.hxx /utf-8 /permissive)
endif()

# Link brep_to_ifc using robust multi-config precompiled Debug/Release IfcOpenShell targets
target_link_libraries(brep_to_ifc PRIVATE
    debug ${IFC_PARSE_DEBUG}   optimized ${IFC_PARSE_RELEASE}
    debug ${IFC_GEOM_DEBUG}    optimized ${IFC_GEOM_RELEASE}
    debug ${SERIALIZERS_DEBUG} optimized ${SERIALIZERS_RELEASE}
    debug ${GEOM_SER_DEBUG}    optimized ${GEOM_SER_RELEASE}
    debug ${GEOM_KER_DEBUG}    optimized ${GEOM_KER_RELEASE}
    TKernel
    TKMath
    TKBRep
    TKTopAlgo
)

target_link_libraries(recipe_to_ifc PRIVATE
    debug ${IFC_PARSE_DEBUG}   optimized ${IFC_PARSE_RELEASE}
    debug ${IFC_GEOM_DEBUG}    optimized ${IFC_GEOM_RELEASE}
    debug ${SERIALIZERS_DEBUG} optimized ${SERIALIZERS_RELEASE}
    debug ${GEOM_SER_DEBUG}    optimized ${GEOM_SER_RELEASE}
    debug ${GEOM_KER_DEBUG}    optimized ${GEOM_KER_RELEASE}
    TKernel
    TKMath
    TKBRep
    TKTopAlgo
)

if(MSVC)
    target_link_options(brep_to_ifc PRIVATE "$<IF:$<CONFIG:DEBUG>,/WHOLEARCHIVE:IfcParse_d,/WHOLEARCHIVE:IfcParse>")
    target_link_options(recipe_to_ifc PRIVATE "$<IF:$<CONFIG:DEBUG>,/WHOLEARCHIVE:IfcParse_d,/WHOLEARCHIVE:IfcParse>")
endif()

target_link_libraries(asicbf_to_ifc PRIVATE
    core_data_model
    debug ${IFC_PARSE_DEBUG}   optimized ${IFC_PARSE_RELEASE}
    debug ${IFC_GEOM_DEBUG}    optimized ${IFC_GEOM_RELEASE}
    debug ${SERIALIZERS_DEBUG} optimized ${SERIALIZERS_RELEASE}
    debug ${GEOM_SER_DEBUG}    optimized ${GEOM_SER_RELEASE}
    debug ${GEOM_KER_DEBUG}    optimized ${GEOM_KER_RELEASE}
    ${OpenCASCADE_LIBRARIES}
    TBB::tbb
)

if(MSVC)
    target_link_options(asicbf_to_ifc PRIVATE "$<IF:$<CONFIG:DEBUG>,/WHOLEARCHIVE:IfcParse_d,/WHOLEARCHIVE:IfcParse>")
endif()

# -----------------------------------------------------------------------------
# 进程内 UI 联动测试 (QtTest)
#
# 用 QTest::mouseClick 在应用进程内直接投递鼠标事件，不经过操作系统输入层，
# 因此不依赖外部自动化工具，可直接用于回归验证：
#   几何元数据 _adNodeId -> 模型树定位 -> 视口点选回传联动 ID
# 用法：test_selection_linkage [工程文件.cbf]（不传则自动挑选内置工程）
# -----------------------------------------------------------------------------
find_package(Qt6 QUIET COMPONENTS Test Widgets Svg OpenGLWidgets)
if(TARGET Qt6::Test)
    add_executable(test_selection_linkage
        test_selection_linkage.cpp
        # 头文件必须列入源文件，AUTOMOC 才会为其生成 moc（否则 Q_OBJECT 符号缺失）
        ${CMAKE_CURRENT_SOURCE_DIR}/../include/OCCTWidget.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../include/ModelExplorerPanel.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../include/AspectWindow.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../include/Line.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../include/UiTheme.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/ModelExplorerPanel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/OCCTWidget.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/AspectWindow.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/Line.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/UiTheme.cpp
    )

    target_include_directories(test_selection_linkage PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../core-data-model
        ${CMAKE_CURRENT_SOURCE_DIR}/../core-data-model/generated
        ${CMAKE_CURRENT_SOURCE_DIR}/../3rpart/asiActiveData
        ${CMAKE_CURRENT_SOURCE_DIR}/../3rpart/asiActiveData/API
        ${CMAKE_CURRENT_SOURCE_DIR}/../3rpart/asiActiveData/Kernel
        ${CMAKE_CURRENT_SOURCE_DIR}/../3rpart/SARibbon_qt6.7.0_MSVC_x64_v142/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../3rpart/SARibbon_qt6.7.0_MSVC_x64_v142/include/SARibbonBar
    )

    target_link_libraries(test_selection_linkage PRIVATE
        core_data_model
        Qt6::Test
        Qt6::Widgets
        Qt6::Svg
        Qt6::OpenGLWidgets
        ${OpenCASCADE_LIBRARIES}
        TBB::tbb
    )

    if(MSVC)
        target_compile_options(test_selection_linkage PRIVATE /FIStandard_Boolean.hxx /utf-8)
    endif()

    add_test(NAME selection_linkage COMMAND test_selection_linkage)
endif()
