# This module defines
# SARibbonBar_FOUND, if false, do not try to link to SARibbonBar
# SARibbonBar_INCLUDE_DIR, where to find the headers
# SARibbonBar_LIBRARIES, where to find the libs

####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was SARibbonBarConfig.cmake.in                            ########

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

# find_dependency(Qt) 会重新 include QtConfig.cmake，把 PACKAGE_PREFIX_DIR 覆盖为 Qt 的安装前缀，
# 因此必须先保存 SARibbonBar 自己的安装前缀
set(_SARIBBON_PACKAGE_PREFIX_DIR "${PACKAGE_PREFIX_DIR}")

include(CMakeFindDependencyMacro)
find_dependency(Qt6 COMPONENTS Core Gui Widgets Svg)

set (PackageName SARibbonBar)
set (SARibbonBar_VERSION 2.9.5)

include ( ${CMAKE_CURRENT_LIST_DIR}/${PackageName}Targets.cmake )
set_and_check ( ${PackageName}_INCLUDE_DIR ${_SARIBBON_PACKAGE_PREFIX_DIR}/include/SARibbonBar )
set ( ${PackageName}_LIBRARIES)
list ( APPEND ${PackageName}_LIBRARIES ${PackageName})

check_required_components(${PackageName})
