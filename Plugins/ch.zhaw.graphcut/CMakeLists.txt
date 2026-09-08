INCLUDE (CheckIncludeFiles)

project(ch_zhaw_graphcut)

mitk_create_plugin(
  EXPORT_DIRECTIVE GRAPHCUT_EXPORT
  EXPORTED_INCLUDE_SUFFIXES src
  MODULE_DEPENDS MitkQtWidgetsExt
)

# GridCut is header-only, but its licence prohibits redistribution under the
# free licence.  Keep it outside the repository and allow each licensed build
# environment to point CMake at its local distribution.
option(MITK_GEM_ENABLE_GRIDCUT "Enable GridCut acceleration when its headers are available" ON)
set(MITK_GEM_GRIDCUT_DIR "${CMAKE_SOURCE_DIR}/__GridCut-1.3" CACHE PATH
  "Path to a licensed GridCut source distribution")

set(GRIDCUT_LIBRARY_AVAILABLE 0)
set(_gridcut_include_dir "${MITK_GEM_GRIDCUT_DIR}/include")
set(_gridcut_examples_include_dir "${MITK_GEM_GRIDCUT_DIR}/examples/include")

# The generated feature flag is needed both by the GridCut implementation and
# by the portable fallback, so make its directory available unconditionally.
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

if(MITK_GEM_ENABLE_GRIDCUT
   AND EXISTS "${_gridcut_include_dir}/GridCut/GridGraph_3D_6C_MT.h"
   AND EXISTS "${_gridcut_examples_include_dir}/AlphaExpansion/AlphaExpansion_3D_6C_MT.h")
  message(STATUS "GridCut enabled from: ${MITK_GEM_GRIDCUT_DIR}")
  set(GRIDCUT_LIBRARY_AVAILABLE 1)
  target_include_directories(${PROJECT_NAME} PRIVATE
    "${_gridcut_include_dir}"
    "${_gridcut_examples_include_dir}")
else()
  message(STATUS "GridCut disabled. Set MITK_GEM_GRIDCUT_DIR to a licensed GridCut 1.3 source distribution to enable it.")
endif()

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/lib/GraphCut3D/lib/gridcut/config.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/gridcut-config.h")
