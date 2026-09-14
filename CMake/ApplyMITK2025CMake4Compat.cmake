# Apply the CMake 4 and current-MSVC compatibility adjustments needed by the
# MITK v2025.12.2 superbuild. The patch is intentionally idempotent because
# ExternalProject may run its patch step again after build reconfiguration.

if(NOT DEFINED MITK_SOURCE_DIR)
  message(FATAL_ERROR "MITK_SOURCE_DIR was not provided")
endif()

set(mitk_superbuild_file "${MITK_SOURCE_DIR}/SuperBuild.cmake")
if(NOT EXISTS "${mitk_superbuild_file}")
  message(FATAL_ERROR "MITK SuperBuild.cmake was not found: ${mitk_superbuild_file}")
endif()

file(READ "${mitk_superbuild_file}" mitk_superbuild_contents)
string(FIND "${mitk_superbuild_contents}" "\r\n" mitk_uses_crlf)
string(REPLACE "\r\n" "\n" mitk_normalized_contents "${mitk_superbuild_contents}")

string(CONCAT mitk_original
  "set(ep_common_args\n"
  "  -DCMAKE_POLICY_DEFAULT_CMP0091:STRING=OLD")
string(CONCAT mitk_legacy_patched
  "set(ep_common_args\n"
  "  # Required by pinned dependencies with pre-3.5 minimum CMake versions.\n"
  "  -DCMAKE_POLICY_VERSION_MINIMUM:STRING=3.5\n"
  "  -DCMAKE_POLICY_DEFAULT_CMP0091:STRING=OLD")
string(CONCAT mitk_patched
  "set(ep_common_args\n"
  "  # Required by pinned dependencies with pre-3.5 minimum CMake versions.\n"
  "  -DCMAKE_POLICY_VERSION_MINIMUM:STRING=3.5\n"
  "  -DCMAKE_POLICY_DEFAULT_CMP0091:STRING=NEW")

string(FIND "${mitk_normalized_contents}" "${mitk_patched}" mitk_already_patched)
if(mitk_already_patched EQUAL -1)
  set(mitk_patch_location -1)
  string(FIND "${mitk_normalized_contents}" "${mitk_legacy_patched}" mitk_legacy_patch_location)
  if(NOT mitk_legacy_patch_location EQUAL -1)
    string(REPLACE "${mitk_legacy_patched}" "${mitk_patched}" mitk_normalized_contents "${mitk_normalized_contents}")
  else()
    string(FIND "${mitk_normalized_contents}" "${mitk_original}" mitk_patch_location)
  endif()
  if(mitk_legacy_patch_location EQUAL -1 AND mitk_patch_location EQUAL -1)
    message(FATAL_ERROR
      "The expected MITK v2025.12.2 SuperBuild.cmake context was not found; refusing to patch an unknown source revision")
  endif()

  if(mitk_legacy_patch_location EQUAL -1)
    string(REPLACE "${mitk_original}" "${mitk_patched}" mitk_normalized_contents "${mitk_normalized_contents}")
  endif()
  if(NOT mitk_uses_crlf EQUAL -1)
    string(REPLACE "\n" "\r\n" mitk_superbuild_contents "${mitk_normalized_contents}")
  else()
    set(mitk_superbuild_contents "${mitk_normalized_contents}")
  endif()
  file(WRITE "${mitk_superbuild_file}" "${mitk_superbuild_contents}")
  message(STATUS "Applied MITK v2025.12.2 CMake 4 compatibility patch")
else()
  message(STATUS "MITK v2025.12.2 CMake 4 compatibility patch is already applied")
endif()

# The pinned MITK source keeps CMP0091 in its legacy mode, which lets CMake
# select a static runtime for some external projects. Enable CMake's modern
# runtime abstraction before MITK's first project() call instead.
set(mitk_cmakelists_file "${MITK_SOURCE_DIR}/CMakeLists.txt")
if(NOT EXISTS "${mitk_cmakelists_file}")
  message(FATAL_ERROR "MITK CMakeLists.txt was not found: ${mitk_cmakelists_file}")
endif()

file(READ "${mitk_cmakelists_file}" mitk_cmakelists_contents)
string(FIND "${mitk_cmakelists_contents}" "\r\n" mitk_cmakelists_uses_crlf)
string(REPLACE "\r\n" "\n" mitk_cmakelists_normalized_contents "${mitk_cmakelists_contents}")
set(mitk_cmp0091_original "cmake_policy(SET CMP0091 OLD)")
set(mitk_cmp0091_patched "cmake_policy(SET CMP0091 NEW)")
string(CONCAT mitk_cmp0091_comment_original
  "    We pass CMP0091 to all external projects as command-line argument:\n"
  "      -DCMAKE_POLICY_DEFAULT_CMP0091:STRING=OLD")
string(CONCAT mitk_cmp0091_comment_patched
  "    We pass CMP0091 and CMAKE_MSVC_RUNTIME_LIBRARY to all external projects:\n"
  "      -DCMAKE_POLICY_DEFAULT_CMP0091:STRING=NEW")
string(FIND "${mitk_cmakelists_normalized_contents}" "${mitk_cmp0091_patched}" mitk_cmp0091_already_patched)
if(mitk_cmp0091_already_patched EQUAL -1)
  string(FIND "${mitk_cmakelists_normalized_contents}" "${mitk_cmp0091_original}" mitk_cmp0091_patch_location)
  if(mitk_cmp0091_patch_location EQUAL -1)
    message(FATAL_ERROR
      "The expected MITK v2025.12.2 CMP0091 context was not found; refusing to patch an unknown source revision")
  endif()

  string(REPLACE "${mitk_cmp0091_original}" "${mitk_cmp0091_patched}" mitk_cmakelists_normalized_contents "${mitk_cmakelists_normalized_contents}")
  if(NOT mitk_cmakelists_uses_crlf EQUAL -1)
    string(REPLACE "\n" "\r\n" mitk_cmakelists_contents "${mitk_cmakelists_normalized_contents}")
  else()
    set(mitk_cmakelists_contents "${mitk_cmakelists_normalized_contents}")
  endif()
  file(WRITE "${mitk_cmakelists_file}" "${mitk_cmakelists_contents}")
  message(STATUS "Applied MITK v2025.12.2 dynamic MSVC-runtime policy patch")
else()
  message(STATUS "MITK v2025.12.2 dynamic MSVC-runtime policy patch is already applied")
endif()

string(FIND "${mitk_cmakelists_normalized_contents}" "${mitk_cmp0091_comment_patched}" mitk_cmp0091_comment_already_patched)
if(mitk_cmp0091_comment_already_patched EQUAL -1)
  string(FIND "${mitk_cmakelists_normalized_contents}" "${mitk_cmp0091_comment_original}" mitk_cmp0091_comment_patch_location)
  if(mitk_cmp0091_comment_patch_location EQUAL -1)
    message(FATAL_ERROR
      "The expected MITK v2025.12.2 CMP0091 comment context was not found; refusing to patch an unknown source revision")
  endif()

  string(REPLACE "${mitk_cmp0091_comment_original}" "${mitk_cmp0091_comment_patched}" mitk_cmakelists_normalized_contents "${mitk_cmakelists_normalized_contents}")
  if(NOT mitk_cmakelists_uses_crlf EQUAL -1)
    string(REPLACE "\n" "\r\n" mitk_cmakelists_contents "${mitk_cmakelists_normalized_contents}")
  else()
    set(mitk_cmakelists_contents "${mitk_cmakelists_normalized_contents}")
  endif()
  file(WRITE "${mitk_cmakelists_file}" "${mitk_cmakelists_contents}")
  message(STATUS "Updated MITK v2025.12.2 CMP0091 runtime-policy documentation")
endif()

# Forward CMAKE_MSVC_RUNTIME_LIBRARY to every MITK external project. In
# particular, this makes static GDCM objects and ITK use /MD (or /MDd)
# consistently, eliminating the ITKIOGDCM link-time CRT conflict.
string(CONCAT mitk_runtime_original
  "if(MSVC)\n"
  "  list(APPEND ep_common_args\n"
  "    -DCMAKE_DEBUG_POSTFIX:STRING=d")
string(CONCAT mitk_runtime_patched
  "if(MSVC)\n"
  "  list(APPEND ep_common_args\n"
  "    \"-DCMAKE_MSVC_RUNTIME_LIBRARY:STRING=\${CMAKE_MSVC_RUNTIME_LIBRARY}\"\n"
  "    -DCMAKE_DEBUG_POSTFIX:STRING=d")
string(FIND "${mitk_normalized_contents}" "${mitk_runtime_patched}" mitk_runtime_already_patched)
if(mitk_runtime_already_patched EQUAL -1)
  string(FIND "${mitk_normalized_contents}" "${mitk_runtime_original}" mitk_runtime_patch_location)
  if(mitk_runtime_patch_location EQUAL -1)
    message(FATAL_ERROR
      "The expected MITK v2025.12.2 MSVC external-project context was not found; refusing to patch an unknown source revision")
  endif()

  string(REPLACE "${mitk_runtime_original}" "${mitk_runtime_patched}" mitk_normalized_contents "${mitk_normalized_contents}")
  if(NOT mitk_uses_crlf EQUAL -1)
    string(REPLACE "\n" "\r\n" mitk_superbuild_contents "${mitk_normalized_contents}")
  else()
    set(mitk_superbuild_contents "${mitk_normalized_contents}")
  endif()
  file(WRITE "${mitk_superbuild_file}" "${mitk_superbuild_contents}")
  message(STATUS "Applied MITK v2025.12.2 dynamic MSVC-runtime propagation patch")
else()
  message(STATUS "MITK v2025.12.2 dynamic MSVC-runtime propagation patch is already applied")
endif()

# A few MITK dependencies have project-specific switches which override the
# common CMake runtime setting. Pin their dynamic-runtime modes explicitly so
# a changed upstream default or a reused cache cannot reintroduce /MT.
function(mitk_gem_pin_dynamic_msvc_runtime recipe_name argument_list anchor runtime_argument)
  set(recipe_file "${MITK_SOURCE_DIR}/CMakeExternals/${recipe_name}")
  if(NOT EXISTS "${recipe_file}")
    message(FATAL_ERROR "MITK external-project recipe was not found: ${recipe_file}")
  endif()

  file(READ "${recipe_file}" recipe_contents)
  string(FIND "${recipe_contents}" "\r\n" recipe_uses_crlf)
  string(REPLACE "\r\n" "\n" recipe_normalized_contents "${recipe_contents}")
  string(CONCAT runtime_block
    "${anchor}\n\n"
    "    if(MSVC)\n"
    "      list(APPEND ${argument_list}\n"
    "        ${runtime_argument}\n"
    "      )\n"
    "    endif()")

  string(FIND "${recipe_normalized_contents}" "${runtime_block}" runtime_already_pinned)
  if(runtime_already_pinned EQUAL -1)
    string(FIND "${recipe_normalized_contents}" "${anchor}" runtime_anchor_location)
    if(runtime_anchor_location EQUAL -1)
      message(FATAL_ERROR
        "The expected MITK v2025.12.2 ${recipe_name} context was not found; refusing to patch an unknown source revision")
    endif()

    string(REPLACE "${anchor}" "${runtime_block}" recipe_normalized_contents "${recipe_normalized_contents}")
    if(NOT recipe_uses_crlf EQUAL -1)
      string(REPLACE "\n" "\r\n" recipe_contents "${recipe_normalized_contents}")
    else()
      set(recipe_contents "${recipe_normalized_contents}")
    endif()
    file(WRITE "${recipe_file}" "${recipe_contents}")
    message(STATUS "Pinned the dynamic MSVC runtime in MITK's ${recipe_name} recipe")
  else()
    message(STATUS "MITK's ${recipe_name} dynamic MSVC runtime is already pinned")
  endif()
endfunction()

mitk_gem_pin_dynamic_msvc_runtime(
  "ITK.cmake"
  "additional_cmake_args"
  "  set(additional_cmake_args -DUSE_WRAP_ITK:BOOL=OFF)"
  "-DITK_MSVC_STATIC_RUNTIME_LIBRARY:BOOL=OFF")
mitk_gem_pin_dynamic_msvc_runtime(
  "HDF5.cmake"
  "additional_args"
  "    set(additional_args )"
  "-DBUILD_STATIC_CRT_LIBS:BOOL=OFF")
mitk_gem_pin_dynamic_msvc_runtime(
  "Poco.cmake"
  "additional_cmake_args"
  "    set(additional_cmake_args )"
  "-DPOCO_MT:BOOL=OFF")
mitk_gem_pin_dynamic_msvc_runtime(
  "DCMTK.cmake"
  "additional_args"
  "    set(additional_args )"
  "-DDCMTK_COMPILE_WIN32_MULTITHREADED_DLL:BOOL=ON")

# GDCM 3.0.14's embedded socket++ library exports C++ standard-library
# members when built as a DLL with current MSVC. Building GDCM's internals
# statically avoids the resulting duplicate-symbol linker errors.
set(mitk_gdcm_file "${MITK_SOURCE_DIR}/CMakeExternals/GDCM.cmake")
if(NOT EXISTS "${mitk_gdcm_file}")
  message(FATAL_ERROR "MITK GDCM build recipe was not found: ${mitk_gdcm_file}")
endif()

file(READ "${mitk_gdcm_file}" mitk_gdcm_contents)
string(FIND "${mitk_gdcm_contents}" "\r\n" mitk_gdcm_uses_crlf)
string(REPLACE "\r\n" "\n" mitk_gdcm_normalized_contents "${mitk_gdcm_contents}")
set(mitk_gdcm_original "-DGDCM_BUILD_SHARED_LIBS:BOOL=ON")
set(mitk_gdcm_patched "-DGDCM_BUILD_SHARED_LIBS:BOOL=OFF")
string(FIND "${mitk_gdcm_normalized_contents}" "${mitk_gdcm_patched}" mitk_gdcm_already_patched)
if(mitk_gdcm_already_patched EQUAL -1)
  string(FIND "${mitk_gdcm_normalized_contents}" "${mitk_gdcm_original}" mitk_gdcm_patch_location)
  if(mitk_gdcm_patch_location EQUAL -1)
    message(FATAL_ERROR
      "The expected MITK v2025.12.2 GDCM context was not found; refusing to patch an unknown source revision")
  endif()

  string(REPLACE "${mitk_gdcm_original}" "${mitk_gdcm_patched}" mitk_gdcm_normalized_contents "${mitk_gdcm_normalized_contents}")
  if(NOT mitk_gdcm_uses_crlf EQUAL -1)
    string(REPLACE "\n" "\r\n" mitk_gdcm_contents "${mitk_gdcm_normalized_contents}")
  else()
    set(mitk_gdcm_contents "${mitk_gdcm_normalized_contents}")
  endif()
  file(WRITE "${mitk_gdcm_file}" "${mitk_gdcm_contents}")
  message(STATUS "Applied MITK v2025.12.2 static GDCM compatibility patch")
else()
  message(STATUS "MITK v2025.12.2 static GDCM compatibility patch is already applied")
endif()

# CMAKE_CACHE_ARGS writes values into an initial-cache file without escaping
# Windows backslashes. Convert the user-provided OpenSSL location before the
# httplib recipe serializes it, otherwise paths such as C:\\Program Files fail
# to parse as CMake string escapes.
set(mitk_httplib_file "${MITK_SOURCE_DIR}/CMakeExternals/httplib.cmake")
if(NOT EXISTS "${mitk_httplib_file}")
  message(FATAL_ERROR "MITK httplib build recipe was not found: ${mitk_httplib_file}")
endif()

file(READ "${mitk_httplib_file}" mitk_httplib_contents)
string(FIND "${mitk_httplib_contents}" "\r\n" mitk_httplib_uses_crlf)
string(REPLACE "\r\n" "\n" mitk_httplib_normalized_contents "${mitk_httplib_contents}")
string(CONCAT mitk_httplib_original
  "    if(OPENSSL_ROOT_DIR)\n"
  "      list(APPEND cmake_cache_args\n"
  "        -DOPENSSL_ROOT_DIR:PATH=\${OPENSSL_ROOT_DIR}\n"
  "      )\n"
  "    endif()")
string(CONCAT mitk_httplib_patched
  "    if(OPENSSL_ROOT_DIR)\n"
  "      file(TO_CMAKE_PATH \"\${OPENSSL_ROOT_DIR}\" openssl_root_dir)\n"
  "      list(APPEND cmake_cache_args\n"
  "        -DOPENSSL_ROOT_DIR:PATH=\${openssl_root_dir}\n"
  "      )\n"
  "    endif()")
string(FIND "${mitk_httplib_normalized_contents}" "${mitk_httplib_patched}" mitk_httplib_already_patched)
if(mitk_httplib_already_patched EQUAL -1)
  string(FIND "${mitk_httplib_normalized_contents}" "${mitk_httplib_original}" mitk_httplib_patch_location)
  if(mitk_httplib_patch_location EQUAL -1)
    message(FATAL_ERROR
      "The expected MITK v2025.12.2 httplib context was not found; refusing to patch an unknown source revision")
  endif()

  string(REPLACE "${mitk_httplib_original}" "${mitk_httplib_patched}" mitk_httplib_normalized_contents "${mitk_httplib_normalized_contents}")
  if(NOT mitk_httplib_uses_crlf EQUAL -1)
    string(REPLACE "\n" "\r\n" mitk_httplib_contents "${mitk_httplib_normalized_contents}")
  else()
    set(mitk_httplib_contents "${mitk_httplib_normalized_contents}")
  endif()
  file(WRITE "${mitk_httplib_file}" "${mitk_httplib_contents}")
  message(STATUS "Applied MITK v2025.12.2 httplib Windows-path compatibility patch")
else()
  message(STATUS "MITK v2025.12.2 httplib Windows-path compatibility patch is already applied")
endif()

# tinyxml2 8.0.0 explicitly requests CMP0063's OLD behavior, which CMake 4
# has removed. Teach the MITK external-project recipe to apply one narrowly
# scoped, idempotent source patch after tinyxml2 is cloned.
set(mitk_tinyxml2_patch_script "${MITK_SOURCE_DIR}/CMakeExternals/ApplyTinyXML2CMake4Compat.cmake")
set(mitk_tinyxml2_patch_script_contents [=[# Apply the CMake 4 compatibility adjustment needed by tinyxml2 8.0.0.
if(NOT DEFINED TINYXML2_SOURCE_DIR)
  message(FATAL_ERROR "TINYXML2_SOURCE_DIR was not provided")
endif()

set(tinyxml2_cmake_file "${TINYXML2_SOURCE_DIR}/CMakeLists.txt")
if(NOT EXISTS "${tinyxml2_cmake_file}")
  message(FATAL_ERROR "tinyxml2 CMakeLists.txt was not found: ${tinyxml2_cmake_file}")
endif()

file(READ "${tinyxml2_cmake_file}" tinyxml2_contents)
string(FIND "${tinyxml2_contents}" "\r\n" tinyxml2_uses_crlf)
string(REPLACE "\r\n" "\n" tinyxml2_normalized_contents "${tinyxml2_contents}")
string(CONCAT tinyxml2_original
  "if(POLICY CMP0063)\n"
  "\tcmake_policy(SET CMP0063 OLD)\n"
  "endif()")
string(CONCAT tinyxml2_patched
  "if(POLICY CMP0063)\n"
  "\tcmake_policy(SET CMP0063 NEW)\n"
  "endif()")

string(FIND "${tinyxml2_normalized_contents}" "${tinyxml2_patched}" tinyxml2_already_patched)
if(tinyxml2_already_patched EQUAL -1)
  string(FIND "${tinyxml2_normalized_contents}" "${tinyxml2_original}" tinyxml2_patch_location)
  if(tinyxml2_patch_location EQUAL -1)
    message(FATAL_ERROR
      "The expected tinyxml2 8.0.0 CMP0063 context was not found; refusing to patch an unknown source revision")
  endif()

  string(REPLACE "${tinyxml2_original}" "${tinyxml2_patched}" tinyxml2_normalized_contents "${tinyxml2_normalized_contents}")
  if(NOT tinyxml2_uses_crlf EQUAL -1)
    string(REPLACE "\n" "\r\n" tinyxml2_contents "${tinyxml2_normalized_contents}")
  else()
    set(tinyxml2_contents "${tinyxml2_normalized_contents}")
  endif()
  file(WRITE "${tinyxml2_cmake_file}" "${tinyxml2_contents}")
  message(STATUS "Applied tinyxml2 8.0.0 CMake 4 compatibility patch")
else()
  message(STATUS "tinyxml2 8.0.0 CMake 4 compatibility patch is already applied")
endif()
]=])

if(EXISTS "${mitk_tinyxml2_patch_script}")
  file(READ "${mitk_tinyxml2_patch_script}" mitk_existing_tinyxml2_patch_script)
  if(NOT mitk_existing_tinyxml2_patch_script STREQUAL mitk_tinyxml2_patch_script_contents)
    message(FATAL_ERROR
      "An unexpected tinyxml2 compatibility patch script already exists: ${mitk_tinyxml2_patch_script}")
  endif()
else()
  file(WRITE "${mitk_tinyxml2_patch_script}" "${mitk_tinyxml2_patch_script_contents}")
  message(STATUS "Added MITK v2025.12.2 tinyxml2 CMake 4 compatibility helper")
endif()

set(mitk_tinyxml2_recipe "${MITK_SOURCE_DIR}/CMakeExternals/tinyxml2.cmake")
if(NOT EXISTS "${mitk_tinyxml2_recipe}")
  message(FATAL_ERROR "MITK tinyxml2 build recipe was not found: ${mitk_tinyxml2_recipe}")
endif()

file(READ "${mitk_tinyxml2_recipe}" mitk_tinyxml2_recipe_contents)
string(FIND "${mitk_tinyxml2_recipe_contents}" "\r\n" mitk_tinyxml2_recipe_uses_crlf)
string(REPLACE "\r\n" "\n" mitk_tinyxml2_recipe_normalized_contents "${mitk_tinyxml2_recipe_contents}")
string(CONCAT mitk_tinyxml2_recipe_original
  "     GIT_TAG 8.0.0\n"
  "     CMAKE_GENERATOR \${gen}")
string(CONCAT mitk_tinyxml2_recipe_patched
  "     GIT_TAG 8.0.0\n"
  "     PATCH_COMMAND \"\${CMAKE_COMMAND}\" \"-DTINYXML2_SOURCE_DIR=<SOURCE_DIR>\" -P \"\${CMAKE_CURRENT_LIST_DIR}/ApplyTinyXML2CMake4Compat.cmake\"\n"
  "     CMAKE_GENERATOR \${gen}")
string(FIND "${mitk_tinyxml2_recipe_normalized_contents}" "${mitk_tinyxml2_recipe_patched}" mitk_tinyxml2_recipe_already_patched)
if(mitk_tinyxml2_recipe_already_patched EQUAL -1)
  string(FIND "${mitk_tinyxml2_recipe_normalized_contents}" "${mitk_tinyxml2_recipe_original}" mitk_tinyxml2_recipe_patch_location)
  if(mitk_tinyxml2_recipe_patch_location EQUAL -1)
    message(FATAL_ERROR
      "The expected MITK v2025.12.2 tinyxml2 recipe context was not found; refusing to patch an unknown source revision")
  endif()

  string(REPLACE "${mitk_tinyxml2_recipe_original}" "${mitk_tinyxml2_recipe_patched}" mitk_tinyxml2_recipe_normalized_contents "${mitk_tinyxml2_recipe_normalized_contents}")
  if(NOT mitk_tinyxml2_recipe_uses_crlf EQUAL -1)
    string(REPLACE "\n" "\r\n" mitk_tinyxml2_recipe_contents "${mitk_tinyxml2_recipe_normalized_contents}")
  else()
    set(mitk_tinyxml2_recipe_contents "${mitk_tinyxml2_recipe_normalized_contents}")
  endif()
  file(WRITE "${mitk_tinyxml2_recipe}" "${mitk_tinyxml2_recipe_contents}")
  message(STATUS "Applied MITK v2025.12.2 tinyxml2 CMake 4 recipe patch")
else()
  message(STATUS "MITK v2025.12.2 tinyxml2 CMake 4 recipe patch is already applied")
endif()

# MITK's legacy UnstructuredGridMapper2D distinguishes only the classic
# point/cell scalar modes. MITK-GEM selects named field-data arrays in the
# UGrid view, so the 3D VTK actor could show a map while the 2D section mapper
# either used an unrelated active cell array or drew no filled section at all.
# Make the mapper honour both associations and interpolate active point scalars
# across section polygons. This is intentionally kept as a source patch: the
# mapper is part of the pinned MITK build, not MITK-GEM's plug-in binary.
set(mitk_ugrid_mapper2d_file
  "${MITK_SOURCE_DIR}/Modules/MapperExt/src/mitkUnstructuredGridMapper2D.cpp")
if(NOT EXISTS "${mitk_ugrid_mapper2d_file}")
  message(FATAL_ERROR "MITK UnstructuredGridMapper2D source was not found: ${mitk_ugrid_mapper2d_file}")
endif()

file(READ "${mitk_ugrid_mapper2d_file}" mitk_ugrid_mapper2d_contents)
string(FIND "${mitk_ugrid_mapper2d_contents}" "\r\n" mitk_ugrid_mapper2d_uses_crlf)
string(REPLACE "\r\n" "\n" mitk_ugrid_mapper2d_normalized_contents "${mitk_ugrid_mapper2d_contents}")

set(mitk_ugrid_section_patch_marker
  "// MITK-GEM patch: correctly colour UGrid sections from active point or cell scalars.")
string(FIND "${mitk_ugrid_mapper2d_normalized_contents}"
  "${mitk_ugrid_section_patch_marker}" mitk_ugrid_section_patch_already_applied)

if(mitk_ugrid_section_patch_already_applied EQUAL -1)
  set(mitk_ugrid_section_original [=[  const bool useCellData = m_ScalarMode->GetVtkScalarMode() == VTK_SCALAR_MODE_DEFAULT ||
                           m_ScalarMode->GetVtkScalarMode() == VTK_SCALAR_MODE_USE_CELL_DATA;
  const bool usePointData = m_ScalarMode->GetVtkScalarMode() == VTK_SCALAR_MODE_USE_POINT_DATA;

  Point3D p;
  Point2D p2d;

  vlines->InitTraversal();
  vpolys->InitTraversal();

  mitk::Color outlineColor = m_Color->GetColor();

  glLineWidth((float)m_LineWidth->GetValue());

  for (int i = 0; i < numberOfLines; ++i)
  {
    const vtkIdType *cell(nullptr);
    vtkIdType cellSize(0);

    vlines->GetNextCell(cellSize, cell);

    float rgba[4] = {outlineColor[0], outlineColor[1], outlineColor[2], 1.0f};
    if (m_ScalarVisibility->GetValue() && vcellscalars)
    {
      if (useCellData)
      { // color each cell according to cell data
        double scalar = vcellscalars->GetComponent(i, 0);
        double rgb[3] = {1.0f, 1.0f, 1.0f};
        m_ScalarsToColors->GetColor(scalar, rgb);
        rgba[0] = (float)rgb[0];
        rgba[1] = (float)rgb[1];
        rgba[2] = (float)rgb[2];
        rgba[3] = (float)m_ScalarsToOpacity->GetValue(scalar);
      }
      else if (usePointData)
      {
        double scalar = vscalars->GetComponent(i, 0);
        double rgb[3] = {1.0f, 1.0f, 1.0f};
        m_ScalarsToColors->GetColor(scalar, rgb);
        rgba[0] = (float)rgb[0];
        rgba[1] = (float)rgb[1];
        rgba[2] = (float)rgb[2];
        rgba[3] = (float)m_ScalarsToOpacity->GetValue(scalar);
      }
    }

    glColor4fv(rgba);

    glBegin(GL_LINE_LOOP);
    for (int j = 0; j < cellSize; ++j)
    {
      vpoints->GetPoint(cell[j], vp);
      // take transformation via vtktransform into account
      vtktransform->TransformPoint(vp, vp);

      vtk2itk(vp, p);

      // convert 3D point (in mm) to display coordinates (units )
      renderer->WorldToDisplay(p, p2d);

      // convert display coordinates ( (0,0) is top-left ) in GL coordinates ( (0,0) is bottom-left )
      // p2d[1]=toGL-p2d[1];

      // add the current vertex to the line
      glVertex2f(p2d[0], p2d[1]);
    }
    glEnd();
  }

  bool polyOutline = m_Outline->GetValue();
  bool scalarVisibility = m_ScalarVisibility->GetValue();

  // cache the transformed points
  // a fixed size array is way faster than 'new'
  // slices through 3d cells usually do not generated
  // polygons with more than 6 vertices
  const int maxPolySize = 10;
  auto *cachedPoints = new Point2D[maxPolySize * numberOfPolys];

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // only draw polygons if there are cell scalars
  // or the outline property is set to true
  if (scalarVisibility && vcellscalars)
  {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    for (int i = 0; i < numberOfPolys; ++i)
    {
      const vtkIdType *cell(nullptr);
      vtkIdType cellSize(0);

      vpolys->GetNextCell(cellSize, cell);

      float rgba[4] = {1.0f, 1.0f, 1.0f, 0};
      if (scalarVisibility && vcellscalars)
      {
        if (useCellData)
        { // color each cell according to cell data
          double scalar = vcellscalars->GetComponent(i + numberOfLines, 0);
          double rgb[3] = {1.0f, 1.0f, 1.0f};
          m_ScalarsToColors->GetColor(scalar, rgb);
          rgba[0] = (float)rgb[0];
          rgba[1] = (float)rgb[1];
          rgba[2] = (float)rgb[2];
          rgba[3] = (float)m_ScalarsToOpacity->GetValue(scalar);
        }
        else if (usePointData)
        {
          double scalar = vscalars->GetComponent(i, 0);
          double rgb[3] = {1.0f, 1.0f, 1.0f};
          m_ScalarsToColors->GetColor(scalar, rgb);
          rgba[0] = (float)rgb[0];
          rgba[1] = (float)rgb[1];
          rgba[2] = (float)rgb[2];
          rgba[3] = (float)m_ScalarsToOpacity->GetValue(scalar);
        }
      }
      glColor4fv(rgba);

      glBegin(GL_POLYGON);
      for (int j = 0; j < cellSize; ++j)
      {
        vpoints->GetPoint(cell[j], vp);
        // take transformation via vtktransform into account
        vtktransform->TransformPoint(vp, vp);

        vtk2itk(vp, p);

        // convert 3D point (in mm) to display coordinates (units )
        renderer->WorldToDisplay(p, p2d);

        // convert display coordinates ( (0,0) is top-left ) in GL coordinates ( (0,0) is bottom-left )
        // p2d[1]=toGL-p2d[1];

        cachedPoints[i * 10 + j][0] = p2d[0];
        cachedPoints[i * 10 + j][1] = p2d[1];

        // add the current vertex to the line
        glVertex2f(p2d[0], p2d[1]);
      }
      glEnd();
    }

    if (polyOutline)
    {
      vpolys->InitTraversal();

      glColor4f(outlineColor[0], outlineColor[1], outlineColor[2], 1.0f);
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      for (int i = 0; i < numberOfPolys; ++i)
      {
        const vtkIdType *cell(nullptr);
        vtkIdType cellSize(0);

        vpolys->GetNextCell(cellSize, cell);

        glBegin(GL_POLYGON);
        // glPolygonOffset(1.0, 1.0);
        for (int j = 0; j < cellSize; ++j)
        {
          // add the current vertex to the line
          glVertex2f(cachedPoints[i * 10 + j][0], cachedPoints[i * 10 + j][1]);
        }
        glEnd();
      }
    }
  }
  glDisable(GL_BLEND);
  delete[] cachedPoints;
]=])

  set(mitk_ugrid_section_patched [=[  const int scalarMode = m_ScalarMode->GetVtkScalarMode();
  // MITK-GEM patch: correctly colour UGrid sections from active point or cell scalars.
  const bool useCellData = scalarMode == VTK_SCALAR_MODE_DEFAULT ||
                           scalarMode == VTK_SCALAR_MODE_USE_CELL_DATA ||
                           scalarMode == VTK_SCALAR_MODE_USE_CELL_FIELD_DATA;
  const bool usePointData = scalarMode == VTK_SCALAR_MODE_USE_POINT_DATA ||
                            scalarMode == VTK_SCALAR_MODE_USE_POINT_FIELD_DATA;
  const bool scalarVisibility = m_ScalarVisibility->GetValue() &&
                                m_ScalarsToColors != nullptr && m_ScalarsToOpacity != nullptr;
  const bool colorCells = scalarVisibility && useCellData && vcellscalars != nullptr;
  const bool colorPoints = scalarVisibility && usePointData && vscalars != nullptr;

  Point3D p;
  Point2D p2d;

  vlines->InitTraversal();
  vpolys->InitTraversal();

  mitk::Color outlineColor = m_Color->GetColor();

  const auto setScalarColor = [this](double scalar)
  {
    double rgb[3] = {1.0, 1.0, 1.0};
    m_ScalarsToColors->GetColor(scalar, rgb);
    glColor4f(static_cast<float>(rgb[0]),
              static_cast<float>(rgb[1]),
              static_cast<float>(rgb[2]),
              static_cast<float>(m_ScalarsToOpacity->GetValue(scalar)));
  };

  glLineWidth(static_cast<float>(m_LineWidth->GetValue()));

  for (int i = 0; i < numberOfLines; ++i)
  {
    const vtkIdType *cell(nullptr);
    vtkIdType cellSize(0);

    vlines->GetNextCell(cellSize, cell);

    if (colorCells)
      setScalarColor(vcellscalars->GetComponent(i, 0));
    else
      glColor4f(outlineColor[0], outlineColor[1], outlineColor[2], 1.0f);

    glBegin(GL_LINE_LOOP);
    for (int j = 0; j < cellSize; ++j)
    {
      if (colorPoints)
        setScalarColor(vscalars->GetComponent(cell[j], 0));

      vpoints->GetPoint(cell[j], vp);
      vtktransform->TransformPoint(vp, vp);
      vtk2itk(vp, p);
      renderer->WorldToDisplay(p, p2d);
      glVertex2f(p2d[0], p2d[1]);
    }
    glEnd();
  }

  const bool polyOutline = m_Outline->GetValue();

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  if (colorCells || colorPoints)
  {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    for (int i = 0; i < numberOfPolys; ++i)
    {
      const vtkIdType *cell(nullptr);
      vtkIdType cellSize(0);

      vpolys->GetNextCell(cellSize, cell);

      if (colorCells)
        setScalarColor(vcellscalars->GetComponent(i + numberOfLines, 0));

      glBegin(GL_POLYGON);
      for (int j = 0; j < cellSize; ++j)
      {
        if (colorPoints)
          setScalarColor(vscalars->GetComponent(cell[j], 0));

        vpoints->GetPoint(cell[j], vp);
        vtktransform->TransformPoint(vp, vp);
        vtk2itk(vp, p);
        renderer->WorldToDisplay(p, p2d);
        glVertex2f(p2d[0], p2d[1]);
      }
      glEnd();
    }
  }

  if (polyOutline)
  {
    vpolys->InitTraversal();

    glColor4f(outlineColor[0], outlineColor[1], outlineColor[2], 1.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    for (int i = 0; i < numberOfPolys; ++i)
    {
      const vtkIdType *cell(nullptr);
      vtkIdType cellSize(0);

      vpolys->GetNextCell(cellSize, cell);

      glBegin(GL_POLYGON);
      for (int j = 0; j < cellSize; ++j)
      {
        vpoints->GetPoint(cell[j], vp);
        vtktransform->TransformPoint(vp, vp);
        vtk2itk(vp, p);
        renderer->WorldToDisplay(p, p2d);
        glVertex2f(p2d[0], p2d[1]);
      }
      glEnd();
    }
  }
  glDisable(GL_BLEND);
]=])

  string(FIND "${mitk_ugrid_mapper2d_normalized_contents}"
    "${mitk_ugrid_section_original}" mitk_ugrid_section_patch_location)
  if(mitk_ugrid_section_patch_location EQUAL -1)
    message(FATAL_ERROR
      "The expected MITK v2025.12.2 UnstructuredGridMapper2D context was not found; refusing to patch an unknown source revision")
  endif()

  string(REPLACE "${mitk_ugrid_section_original}" "${mitk_ugrid_section_patched}"
    mitk_ugrid_mapper2d_normalized_contents "${mitk_ugrid_mapper2d_normalized_contents}")
  if(NOT mitk_ugrid_mapper2d_uses_crlf EQUAL -1)
    string(REPLACE "\n" "\r\n" mitk_ugrid_mapper2d_contents "${mitk_ugrid_mapper2d_normalized_contents}")
  else()
    set(mitk_ugrid_mapper2d_contents "${mitk_ugrid_mapper2d_normalized_contents}")
  endif()
  file(WRITE "${mitk_ugrid_mapper2d_file}" "${mitk_ugrid_mapper2d_contents}")
  message(STATUS "Applied MITK v2025.12.2 UGrid 2D section-rendering patch")
else()
  message(STATUS "MITK v2025.12.2 UGrid 2D section-rendering patch is already applied")
endif()
