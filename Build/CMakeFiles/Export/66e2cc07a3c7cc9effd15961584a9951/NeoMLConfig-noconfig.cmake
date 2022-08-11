#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "Neo::NeoML" for configuration ""
set_property(TARGET Neo::NeoML APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(Neo::NeoML PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libNeoML.dylib"
  IMPORTED_SONAME_NOCONFIG "@rpath/libNeoML.dylib"
  )

list(APPEND _cmake_import_check_targets Neo::NeoML )
list(APPEND _cmake_import_check_files_for_Neo::NeoML "${_IMPORT_PREFIX}/lib/libNeoML.dylib" )

# Import target "Neo::NeoMathEngine" for configuration ""
set_property(TARGET Neo::NeoMathEngine APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(Neo::NeoMathEngine PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libNeoMathEngine.dylib"
  IMPORTED_SONAME_NOCONFIG "@rpath/libNeoMathEngine.dylib"
  )

list(APPEND _cmake_import_check_targets Neo::NeoMathEngine )
list(APPEND _cmake_import_check_files_for_Neo::NeoMathEngine "${_IMPORT_PREFIX}/lib/libNeoMathEngine.dylib" )

# Import target "Neo::NeoOnnx" for configuration ""
set_property(TARGET Neo::NeoOnnx APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(Neo::NeoOnnx PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libNeoOnnx.dylib"
  IMPORTED_SONAME_NOCONFIG "@rpath/libNeoOnnx.dylib"
  )

list(APPEND _cmake_import_check_targets Neo::NeoOnnx )
list(APPEND _cmake_import_check_files_for_Neo::NeoOnnx "${_IMPORT_PREFIX}/lib/libNeoOnnx.dylib" )

# Import target "Neo::Onnx2NeoML" for configuration ""
set_property(TARGET Neo::Onnx2NeoML APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(Neo::Onnx2NeoML PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/bin/Onnx2NeoML"
  )

list(APPEND _cmake_import_check_targets Neo::Onnx2NeoML )
list(APPEND _cmake_import_check_files_for_Neo::Onnx2NeoML "${_IMPORT_PREFIX}/bin/Onnx2NeoML" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
