mitk_create_executable(GraphcutModernizationTest
  CPP_FILES
    GraphcutModernizationTest.cpp
    ${PROJECT_SOURCE_DIR}/Plugins/ch.zhaw.graphcut/src/internal/GraphcutSegmentationUtils.cpp
    ${PROJECT_SOURCE_DIR}/Plugins/ch.zhaw.voxel2mesh/src/internal/Voxel2MeshSegmentationUtils.cpp
    ${PROJECT_SOURCE_DIR}/Plugins/ch.zhaw.voxel2mesh/src/internal/mitkGraphcutSegmentationToSurfaceFilter.cpp
    ${PROJECT_SOURCE_DIR}/Plugins/ch.zhaw.graphcut/src/internal/lib/GraphCut3D/lib/kolmogorov-3.04/graph.cpp
    ${PROJECT_SOURCE_DIR}/Plugins/ch.zhaw.graphcut/src/internal/lib/GraphCut3D/lib/kolmogorov-3.04/maxflow.cpp
  INCLUDE_DIRS
    ${PROJECT_SOURCE_DIR}/Plugins/ch.zhaw.graphcut/src/internal
    ${PROJECT_SOURCE_DIR}/Plugins/ch.zhaw.voxel2mesh/src/internal
  DEPENDS
    MitkCore
    MitkMultilabel
  PACKAGE_DEPENDS
    ITK
    VTK|ImagingGeneral+ImagingCore+FiltersCore
  NO_INIT
  NO_INSTALL
  NO_BATCH_FILE
  NO_FEATURE_INFO
)

if(EXECUTABLE_IS_ENABLED)
  set_target_properties(${EXECUTABLE_TARGET} PROPERTIES WIN32_EXECUTABLE FALSE)
  add_test(NAME GraphcutModernizationTest COMMAND $<TARGET_FILE:${EXECUTABLE_TARGET}>)

  # The test is intentionally not packaged. Give CTest the same runtime DLL
  # search path as a MITK application so that MitkMultilabel and its
  # dependencies resolve from the superbuild output directory.
  mitkFunctionGetLibrarySearchPaths(_graphcut_test_runtime_paths release RELEASE)
  list(APPEND _graphcut_test_runtime_paths $ENV{PATH})
  list(REMOVE_DUPLICATES _graphcut_test_runtime_paths)
  string(REGEX REPLACE "\\;" "\\\\;" _graphcut_test_runtime_path "${_graphcut_test_runtime_paths}")
  set_property(TEST GraphcutModernizationTest PROPERTY ENVIRONMENT "PATH=${_graphcut_test_runtime_path}" APPEND)
  # The test links the required MITK modules directly; unrelated application
  # plug-ins are not needed and can introduce optional DICOM runtime warnings.
  set_property(TEST GraphcutModernizationTest PROPERTY ENVIRONMENT "US_DISABLE_AUTOLOADING=1" APPEND)
endif()
