cmake_minimum_required(VERSION 3.10)
project(core_data_model)

# --- 路径配置 ---
set(OCCT_DIR "D:/BrCAD/OCCT/OCCT_7_6_0")
set(ACTDATA_DIR "D:/QtOCCTApp/3rpart/asiActiveData")

set(OCCT_INC_DIR "${OCCT_DIR}/inc")
set(OCCT_LIB_DIR "${OCCT_DIR}/win64/vc14/lib")

# Active Data 头文件分布在多个子目录
set(ACTDATA_INC_DIRS 
    "${ACTDATA_DIR}"
    "${ACTDATA_DIR}/Kernel"
    "${ACTDATA_DIR}/API"
    "${ACTDATA_DIR}/Mesh"
    "${ACTDATA_DIR}/Mesh/DS"
    "${ACTDATA_DIR}/Tools"
)
set(ACTDATA_LIB_DIR "${ACTDATA_DIR}/x64/Release")

# --- 编译选项 ---
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

include_directories("${OCCT_INC_DIR}")
foreach(INC_DIR ${ACTDATA_INC_DIRS})
    include_directories("${INC_DIR}")
endforeach()

# 强制包含标准布尔定义，解决宏冲突
if(MSVC)
    add_compile_options(/FI"Standard_Boolean.hxx")
    add_compile_options(/utf-8)
endif()

# --- 源文件 ---
file(GLOB_RECURSE SOURCES "generated/*.cpp")
file(GLOB_RECURSE HEADERS "generated/*.h")

add_library(core_data_model SHARED ${SOURCES} ${HEADERS})
target_compile_definitions(core_data_model PRIVATE BR_EXPORT_EXPORTS)

# --- 库链接 ---
set(OCCT_LIBS 
    TKernel TKMath TKG3d TKG2d TKBRep TKGeomBase TKGeomAlgo TKTopAlgo 
    TKPrim TKBO TKShHealing TKMesh TKCDF TKLCAF TKBin TKXml 
    TKV3d TKService TKVCAF TKCAF TKXCAF
)
set(ACTDATA_LIBS asiActiveData)

# 包含 3rpart 目录以支持 nlohmann/json
include_directories("D:/QtOCCTApp/3rpart")

target_link_directories(core_data_model PUBLIC "${OCCT_LIB_DIR}" "${ACTDATA_LIB_DIR}")
target_link_libraries(core_data_model PUBLIC ${OCCT_LIBS} ${ACTDATA_LIBS})

# --- 测试程序 ---
add_executable(test_load_bridge test_load_bridge.cpp)
target_link_libraries(test_load_bridge PRIVATE core_data_model ${OCCT_LIBS} ${ACTDATA_LIBS})
add_executable(test_minimal test_minimal.cpp)
target_link_libraries(test_minimal PRIVATE core_data_model)
set_target_properties(test_minimal PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# 输出路径
set_target_properties(core_data_model PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(test_load_bridge PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
