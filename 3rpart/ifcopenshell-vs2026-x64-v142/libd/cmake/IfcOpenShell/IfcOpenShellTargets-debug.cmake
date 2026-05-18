#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "IfcOpenShell::IfcParse" for configuration "Debug"
set_property(TARGET IfcOpenShell::IfcParse APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(IfcOpenShell::IfcParse PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/lib/IfcParse_d.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/bin/IfcParse_d.dll"
  )

list(APPEND _cmake_import_check_targets IfcOpenShell::IfcParse )
list(APPEND _cmake_import_check_files_for_IfcOpenShell::IfcParse "${_IMPORT_PREFIX}/lib/IfcParse_d.lib" "${_IMPORT_PREFIX}/bin/IfcParse_d.dll" )

# Import target "IfcOpenShell::geometry_kernel_opencascade" for configuration "Debug"
set_property(TARGET IfcOpenShell::geometry_kernel_opencascade APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(IfcOpenShell::geometry_kernel_opencascade PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/lib/geometry_kernel_opencascade_d.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/bin/geometry_kernel_opencascade_d.dll"
  )

list(APPEND _cmake_import_check_targets IfcOpenShell::geometry_kernel_opencascade )
list(APPEND _cmake_import_check_files_for_IfcOpenShell::geometry_kernel_opencascade "${_IMPORT_PREFIX}/lib/geometry_kernel_opencascade_d.lib" "${_IMPORT_PREFIX}/bin/geometry_kernel_opencascade_d.dll" )

# Import target "IfcOpenShell::geometry_serializer" for configuration "Debug"
set_property(TARGET IfcOpenShell::geometry_serializer APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(IfcOpenShell::geometry_serializer PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/lib/geometry_serializer_d.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/bin/geometry_serializer_d.dll"
  )

list(APPEND _cmake_import_check_targets IfcOpenShell::geometry_serializer )
list(APPEND _cmake_import_check_files_for_IfcOpenShell::geometry_serializer "${_IMPORT_PREFIX}/lib/geometry_serializer_d.lib" "${_IMPORT_PREFIX}/bin/geometry_serializer_d.dll" )

# Import target "IfcOpenShell::IfcGeom" for configuration "Debug"
set_property(TARGET IfcOpenShell::IfcGeom APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(IfcOpenShell::IfcGeom PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/lib/IfcGeom_d.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/bin/IfcGeom_d.dll"
  )

list(APPEND _cmake_import_check_targets IfcOpenShell::IfcGeom )
list(APPEND _cmake_import_check_files_for_IfcOpenShell::IfcGeom "${_IMPORT_PREFIX}/lib/IfcGeom_d.lib" "${_IMPORT_PREFIX}/bin/IfcGeom_d.dll" )

# Import target "IfcOpenShell::IfcConvert" for configuration "Debug"
set_property(TARGET IfcOpenShell::IfcConvert APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(IfcOpenShell::IfcConvert PROPERTIES
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/bin/IfcConvert.exe"
  )

list(APPEND _cmake_import_check_targets IfcOpenShell::IfcConvert )
list(APPEND _cmake_import_check_files_for_IfcOpenShell::IfcConvert "${_IMPORT_PREFIX}/bin/IfcConvert.exe" )

# Import target "IfcOpenShell::IfcGeomServer" for configuration "Debug"
set_property(TARGET IfcOpenShell::IfcGeomServer APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(IfcOpenShell::IfcGeomServer PROPERTIES
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/bin/IfcGeomServer.exe"
  )

list(APPEND _cmake_import_check_targets IfcOpenShell::IfcGeomServer )
list(APPEND _cmake_import_check_files_for_IfcOpenShell::IfcGeomServer "${_IMPORT_PREFIX}/bin/IfcGeomServer.exe" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
