add_executable(GemFemExportTest GemFemExportTest.cpp)
target_link_libraries(GemFemExportTest PRIVATE GemCore MitkDataTypesExt)
set_property(TARGET GemFemExportTest PROPERTY FOLDER "${MITK_ROOT_FOLDER}/Modules/Tests")

add_test(
  NAME GemFemExportTest_LegacyReference
  COMMAND GemFemExportTest
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/legacy-material-mapped-mesh.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/abaqus-method-a.inp"
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/ansys-method-a.cdb"
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/febio-method-a.feb"
    "${CMAKE_CURRENT_BINARY_DIR}/GemFemExport-service.inp"
    "${CMAKE_CURRENT_BINARY_DIR}/GemFemExport-service.cdb"
    "${CMAKE_CURRENT_BINARY_DIR}/GemFemExport-service.feb"
)

if(WIN32)
  mitkFunctionGetLibrarySearchPaths(_gem_test_release_path release RELEASE)
  mitkFunctionGetLibrarySearchPaths(_gem_test_debug_path debug DEBUG)
  set(_gem_test_path ${_gem_test_release_path} ${_gem_test_debug_path} $ENV{PATH})
  list(REMOVE_DUPLICATES _gem_test_path)
  string(REGEX REPLACE "\;" "\\\;" _gem_test_path "${_gem_test_path}")
  set_property(TEST GemFemExportTest_LegacyReference PROPERTY ENVIRONMENT "PATH=${_gem_test_path}")
endif()
