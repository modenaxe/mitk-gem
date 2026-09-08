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
string(CONCAT mitk_patched
  "set(ep_common_args\n"
  "  # Required by pinned dependencies with pre-3.5 minimum CMake versions.\n"
  "  -DCMAKE_POLICY_VERSION_MINIMUM:STRING=3.5\n"
  "  -DCMAKE_POLICY_DEFAULT_CMP0091:STRING=OLD")

string(FIND "${mitk_normalized_contents}" "${mitk_patched}" mitk_already_patched)
if(mitk_already_patched EQUAL -1)
  string(FIND "${mitk_normalized_contents}" "${mitk_original}" mitk_patch_location)
  if(mitk_patch_location EQUAL -1)
    message(FATAL_ERROR
      "The expected MITK v2025.12.2 SuperBuild.cmake context was not found; refusing to patch an unknown source revision")
  endif()

  string(REPLACE "${mitk_original}" "${mitk_patched}" mitk_normalized_contents "${mitk_normalized_contents}")
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
