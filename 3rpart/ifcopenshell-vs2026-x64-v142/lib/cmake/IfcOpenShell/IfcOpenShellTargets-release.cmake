#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "IfcOpenShell::IfcParse" for configuration "Release"
set_property(TARGET IfcOpenShell::IfcParse APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(IfcOpenShell::IfcParse PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/IfcParse.lib"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/IfcParse.dll"
  )

list(APPEND _cmake_import_check_targets IfcOpenShell::IfcParse )
list(APPEND _cmake_import_check_files_for_IfcOpenShell::IfcParse "${_IMPORT_PREFIX}/lib/IfcParse.lib" "${_IMPORT_PREFIX}/bin/IfcParse.dll" )

# Import target "IfcOpenShell::geometry_kernel_opencascade" for configuration "Release"
set_property(TARGET IfcOpenShell::geometry_kernel_opencascade APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(IfcOpenShell::geometry_kernel_opencascade PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/geometry_kernel_opencascade.lib"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/geometry_kernel_opencascade.dll"
  )

list(APPEND _cmake_import_check_targets IfcOpenShell::geometry_kernel_opencascade )
list(APPEND _cmake_import_check_files_for_IfcOpenShell::geometry_kernel_opencascade "${_IMPORT_PREFIX}/lib/geometry_kernel_opencascade.lib" "${_IMPORT_PREFIX}/bin/geometry_kernel_opencascade.dll" )

# Import target "IfcOpenShell::geometry_serializer" for configuration "Release"
set_property(TARGET IfcOpenShell::geometry_serializer APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(IfcOpenShell::geometry_serializer PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/geometry_serializer.lib"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/geometry_serializer.dll"
  )

list(APPEND _cmake_import_check_targets IfcOpenShell::geometry_serializer )
list(APPEND _cmake_import_check_files_for_IfcOpenShell::geometry_serializer "${_IMPORT_PREFIX}/lib/geometry_serializer.lib" "${_IMPORT_PREFIX}/bin/geometry_serializer.dll" )

# Import target "IfcOpenShell::IfcGeom" for configuration "Release"
set_property(TARGET IfcOpenShell::IfcGeom APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(IfcOpenShell::IfcGeom PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/IfcGeom.lib"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/IfcGeom.dll"
  )

list(APPEND _cmake_import_check_targets IfcOpenShell::IfcGeom )
list(APPEND _cmake_import_check_files_for_IfcOpenShell::IfcGeom "${_IMPORT_PREFIX}/lib/IfcGeom.lib" "${_IMPORT_PREFIX}/bin/IfcGeom.dll" )

# Import target "IfcOpenShell::IfcConvert" for configuration "Release"
set_property(TARGET IfcOpenShell::IfcConvert APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(IfcOpenShell::IfcConvert PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/IfcConvert.exe"
  )

list(APPEND _cmake_import_check_targets IfcOpenShell::IfcConvert )
list(APPEND _cmake_import_check_files_for_IfcOpenShell::IfcConvert "${_IMPORT_PREFIX}/bin/IfcConvert.exe" )

# Import target "IfcOpenShell::IfcGeomServer" for configuration "Release"
set_property(TARGET IfcOpenShell::IfcGeomServer APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(IfcOpenShell::IfcGeomServer PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/IfcGeomServer.exe"
  )

list(APPEND _cmake_import_check_targets IfcOpenShell::IfcGeomServer )
list(APPEND _cmake_import_check_files_for_IfcOpenShell::IfcGeomServer "${_IMPORT_PREFIX}/bin/IfcGeomServer.exe" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
