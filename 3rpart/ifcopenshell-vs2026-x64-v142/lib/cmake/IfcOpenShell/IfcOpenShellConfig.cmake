
####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was IfcOpenShellConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

# Variable to inspect installed schema versions.
set(IFCOPENSHELL_SCHEMA_VERSIONS 2x3;4;4x3_add2)

set(IFCOPENSHELL_WITH_OPENCASCADE ON)
set(IFCOPENSHELL_WITH_CGAL OFF)
set(IFCOPENSHELL_IFCXML ON)
set(IFCOPENSHELL_WITH_ROCKSDB On)

include(CMakeFindDependencyMacro)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_COMPONENTS
    system
    program_options
    regex
    thread
    date_time
    iostreams
)
find_dependency(Boost CONFIG COMPONENTS ${Boost_COMPONENTS})
find_dependency(Eigen3 CONFIG)

if(IFCOPENSHELL_WITH_ROCKSDB)
    find_dependency(zstd CONFIG)

    # Temporaily mess with CMAKE_FIND_PACKAGE_PREFER_CONFIG to help RocksDB
    # find it's zstd dependency on Windows.
    # Only do it on Windows, otherwise it might create problems as
    # findzstd and zstd-config target names do not match.
    # https://github.com/facebook/rocksdb/pull/13975
    if(WIN32)
        set(TEMP CMAKE_FIND_PACKAGE_PREFER_CONFIG)
        set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)
    endif()
    find_dependency(RocksDB CONFIG)
    if(WIN32)
        set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ${TEMP})
    endif()
endif()

if(IFCOPENSHELL_IFCXML)
    find_dependency(LibXml2 CONFIG)
endif()

if(IFCOPENSHELL_WITH_CGAL)
    find_dependency(CGAL CONFIG)
endif()

if(IFCOPENSHELL_WITH_OPENCASCADE)
    find_dependency(OpenCASCADE CONFIG)
    if(OpenCASCADE_VERSION VERSION_LESS "7.7.0")
        # cmake configs < 7.7.0 were not adding include directories to targets automatically.
        set_target_properties(TKernel PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${OpenCASCADE_INCLUDE_DIR}")
    endif()
endif()

include("${CMAKE_CURRENT_LIST_DIR}/IfcOpenShellTargets.cmake")

check_required_components("IfcOpenShell")
