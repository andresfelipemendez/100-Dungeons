#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "ryml::ryml" for configuration "Release"
set_property(TARGET ryml::ryml APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(ryml::ryml PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/ryml.lib"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/ryml.dll"
  )

list(APPEND _cmake_import_check_targets ryml::ryml )
list(APPEND _cmake_import_check_files_for_ryml::ryml "${_IMPORT_PREFIX}/lib/ryml.lib" "${_IMPORT_PREFIX}/bin/ryml.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
