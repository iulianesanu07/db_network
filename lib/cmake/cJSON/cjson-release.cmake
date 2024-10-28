#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "cjson" for configuration "Release"
set_property(TARGET cjson APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(cjson PROPERTIES
  IMPORTED_LOCATION_RELEASE "/opt/homebrew/Cellar/cjson/1.7.18/lib/libcjson.1.7.18.dylib"
  IMPORTED_SONAME_RELEASE "@rpath/libcjson.1.dylib"
  )

list(APPEND _cmake_import_check_targets cjson )
list(APPEND _cmake_import_check_files_for_cjson "/opt/homebrew/Cellar/cjson/1.7.18/lib/libcjson.1.7.18.dylib" )

# Import target "cjson-static" for configuration "Release"
set_property(TARGET cjson-static APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(cjson-static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
  IMPORTED_LOCATION_RELEASE "/opt/homebrew/Cellar/cjson/1.7.18/lib/libcjson.a"
  )

list(APPEND _cmake_import_check_targets cjson-static )
list(APPEND _cmake_import_check_files_for_cjson-static "/opt/homebrew/Cellar/cjson/1.7.18/lib/libcjson.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
