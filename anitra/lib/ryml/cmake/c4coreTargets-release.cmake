#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "c4core::c4core" for configuration "Release"
set_property(TARGET c4core::c4core APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(c4core::c4core PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/c4core.lib"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/c4core.dll"
  )

list(APPEND _cmake_import_check_targets c4core::c4core )
list(APPEND _cmake_import_check_files_for_c4core::c4core "${_IMPORT_PREFIX}/lib/c4core.lib" "${_IMPORT_PREFIX}/bin/c4core.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
