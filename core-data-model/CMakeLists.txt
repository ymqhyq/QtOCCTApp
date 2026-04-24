cmake_minimum_required(VERSION 3.10)
project(CoreDataModel)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenCASCADE (Required by Active Data)
# In Docker, these would be installed via apt-get or a conda env
find_package(OpenCASCADE REQUIRED)

# Find Active Data (Assuming it's installed or available as a subdirectory)
# find_package(ActiveData REQUIRED)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_VALUE}
    ${OpenCASCADE_INCLUDE_DIR}
    # ${ActiveData_INCLUDE_DIR}
)

# Source files
file(GLOB_RECURSE SOURCES "generated/*.cpp")
file(GLOB_RECURSE HEADERS "generated/*.h")

# Build shared library
add_library(core_data_model SHARED ${SOURCES} ${HEADERS})

# Link libraries (No Qt!)
target_link_libraries(core_data_model
    PRIVATE
    ${OpenCASCADE_LIBRARIES}
    # ${ActiveData_LIBRARIES}
)

# Export information for microservices
install(TARGETS core_data_model DESTINATION lib)
install(DIRECTORY generated/ DESTINATION include/core_data_model FILES_MATCHING PATTERN "*.h")
