# Install script for directory: D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "D:/GitHub_Ymqhyq/SARibbon/bin_qt6.10.1_MSVC_x64")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "headers" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/SARibbonBar" TYPE FILE FILES
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonBarVersionInfo.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonGlobal.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonQt5Compat.hpp"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonUtil.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SAFramelessHelper.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonActionsManager.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonBarLayout.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonBar.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonCustomizeData.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonCustomizeDialog.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonCustomizeWidget.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonMainWindow.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonWidget.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonSystemButtonBar.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonApplicationButton.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonTabBar.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonCategory.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonContextCategory.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonPanel.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonPanelLayout.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonPanelItem.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonPanelOptionButton.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonToolButton.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonMenu.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonSeparatorWidget.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonCategoryLayout.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonGallery.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonGalleryGroup.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonGalleryItem.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonElementFactory.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonElementManager.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonButtonGroupWidget.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonStackedWidget.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonQuickAccessBar.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonCtrlContainer.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonLineWidgetContainer.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonColorToolButton.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonApplicationWidget.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/SARibbonTitleIconWidget.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "headers" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/SARibbonBar/colorWidgets" TYPE FILE FILES
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/colorWidgets/SAColorWidgetsGlobal.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/colorWidgets/SAColorToolButton.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/colorWidgets/SAColorGridWidget.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/colorWidgets/SAColorPaletteGridWidget.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/colorWidgets/SAColorMenu.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/SARibbonBar_amalgamate" TYPE FILE FILES
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/../SARibbon.h"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/../SARibbon.cpp"
    "D:/GitHub_Ymqhyq/SARibbon/src/SARibbonBar/../SARibbon.pri"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY OPTIONAL FILES "D:/QtOCCTApp/build2/Debug/SARibbonBard.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY OPTIONAL FILES "D:/QtOCCTApp/build2/lib/Release/SARibbonBar.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY OPTIONAL FILES "D:/QtOCCTApp/build2/lib/MinSizeRel/SARibbonBar.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY OPTIONAL FILES "D:/QtOCCTApp/build2/lib/RelWithDebInfo/SARibbonBar.lib")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE SHARED_LIBRARY FILES "D:/QtOCCTApp/build2/Debug/SARibbonBard.dll")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE SHARED_LIBRARY FILES "D:/QtOCCTApp/build2/bin/Release/SARibbonBar.dll")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE SHARED_LIBRARY FILES "D:/QtOCCTApp/build2/bin/MinSizeRel/SARibbonBar.dll")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE SHARED_LIBRARY FILES "D:/QtOCCTApp/build2/bin/RelWithDebInfo/SARibbonBar.dll")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/SARibbonBar/SARibbonBarTargets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/SARibbonBar/SARibbonBarTargets.cmake"
         "D:/QtOCCTApp/build2/SARibbon/src/SARibbonBar/CMakeFiles/Export/c3971ada802acd003cb4dad3345cdc1f/SARibbonBarTargets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/SARibbonBar/SARibbonBarTargets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/SARibbonBar/SARibbonBarTargets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/SARibbonBar" TYPE FILE FILES "D:/QtOCCTApp/build2/SARibbon/src/SARibbonBar/CMakeFiles/Export/c3971ada802acd003cb4dad3345cdc1f/SARibbonBarTargets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/SARibbonBar" TYPE FILE FILES "D:/QtOCCTApp/build2/SARibbon/src/SARibbonBar/CMakeFiles/Export/c3971ada802acd003cb4dad3345cdc1f/SARibbonBarTargets-debug.cmake")
  endif()
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/SARibbonBar" TYPE FILE FILES "D:/QtOCCTApp/build2/SARibbon/src/SARibbonBar/CMakeFiles/Export/c3971ada802acd003cb4dad3345cdc1f/SARibbonBarTargets-minsizerel.cmake")
  endif()
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/SARibbonBar" TYPE FILE FILES "D:/QtOCCTApp/build2/SARibbon/src/SARibbonBar/CMakeFiles/Export/c3971ada802acd003cb4dad3345cdc1f/SARibbonBarTargets-relwithdebinfo.cmake")
  endif()
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/SARibbonBar" TYPE FILE FILES "D:/QtOCCTApp/build2/SARibbon/src/SARibbonBar/CMakeFiles/Export/c3971ada802acd003cb4dad3345cdc1f/SARibbonBarTargets-release.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/SARibbonBar" TYPE FILE FILES
    "D:/QtOCCTApp/build2/SARibbon/src/SARibbonBar/SARibbonBarConfig.cmake"
    "D:/QtOCCTApp/build2/SARibbon/src/SARibbonBar/SARibbonBarConfigVersion.cmake"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "D:/QtOCCTApp/build2/SARibbon/src/SARibbonBar/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
