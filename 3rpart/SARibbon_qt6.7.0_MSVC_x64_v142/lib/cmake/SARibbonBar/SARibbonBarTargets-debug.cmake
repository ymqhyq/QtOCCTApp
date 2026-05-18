#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "SARibbonBar::SARibbonBar" for configuration "Debug"
set_property(TARGET SARibbonBar::SARibbonBar APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(SARibbonBar::SARibbonBar PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/lib/SARibbonBard.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/bin/SARibbonBard.dll"
  )

list(APPEND _cmake_import_check_targets SARibbonBar::SARibbonBar )
list(APPEND _cmake_import_check_files_for_SARibbonBar::SARibbonBar "${_IMPORT_PREFIX}/lib/SARibbonBard.lib" "${_IMPORT_PREFIX}/bin/SARibbonBard.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
