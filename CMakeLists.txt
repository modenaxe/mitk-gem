if(WIN32)
  set(MITK_GEM_CMAKE_MINIMUM_VERSION 3.31)
else()
  set(MITK_GEM_CMAKE_MINIMUM_VERSION 3.28)
endif()

cmake_minimum_required(VERSION ${MITK_GEM_CMAKE_MINIMUM_VERSION}...4.3 FATAL_ERROR)

# Change project and application name to your own
set(MY_PROJECT_NAME MITK-GEM)
set(MY_APP_NAME MITK-GEM)

#-----------------------------------------------------------------------------
# Set a default build type if none was specified
#-----------------------------------------------------------------------------

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Debug' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)

  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY
               STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

#-----------------------------------------------------------------------------
# Superbuild Option - Enabled by default
#-----------------------------------------------------------------------------

option(${MY_PROJECT_NAME}_USE_SUPERBUILD "Build ${MY_PROJECT_NAME} and the projects it depends on via SuperBuild.cmake." ON)

if(${MY_PROJECT_NAME}_USE_SUPERBUILD)
  project(${MY_PROJECT_NAME}-superbuild)
  set(${MY_PROJECT_NAME}_SOURCE_DIR ${PROJECT_SOURCE_DIR})
  set(${MY_PROJECT_NAME}_BINARY_DIR ${PROJECT_BINARY_DIR})
else()
  project(${MY_PROJECT_NAME})
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC otherwise reports the obsolete 199711L value for __cplusplus.  ITK uses
# this macro to validate ABI-compatible C++ language standards in consumers.
if(MSVC)
  add_compile_options(/Zc:__cplusplus)
endif()

#-----------------------------------------------------------------------------
# See http://cmake.org/cmake/help/cmake-2-8-docs.html#section_Policies for details
#-----------------------------------------------------------------------------

set(project_policies
  CMP0001 # NEW: CMAKE_BACKWARDS_COMPATIBILITY should no longer be used.
  CMP0002 # NEW: Logical target names must be globally unique.
  CMP0003 # NEW: Libraries linked via full path no longer produce linker search paths.
  CMP0004 # NEW: Libraries linked may NOT have leading or trailing whitespace.
  CMP0005 # NEW: Preprocessor definition values are now escaped automatically.
  CMP0006 # NEW: Installing MACOSX_BUNDLE targets requires a BUNDLE DESTINATION.
  CMP0007 # NEW: List command no longer ignores empty elements.
  CMP0008 # NEW: Libraries linked by full-path must have a valid library file name.
  CMP0009 # NEW: FILE GLOB_RECURSE calls should not follow symlinks by default.
  CMP0010 # NEW: Bad variable reference syntax is an error.
  CMP0011 # NEW: Included scripts do automatic cmake_policy PUSH and POP.
  CMP0012 # NEW: if() recognizes numbers and boolean constants.
  CMP0013 # NEW: Duplicate binary directories are not allowed.
  CMP0014 # NEW: Input directories must have CMakeLists.txt
  )
foreach(policy ${project_policies})
  if(POLICY ${policy})
    cmake_policy(SET ${policy} NEW)
  endif()
endforeach()

#-----------------------------------------------------------------------------
# Update CMake module path
#------------------------------------------------------------------------------

set(CMAKE_MODULE_PATH
  ${${MY_PROJECT_NAME}_SOURCE_DIR}/CMake
  ${CMAKE_MODULE_PATH}
  )

#-----------------------------------------------------------------------------
# CMake Function(s) and Macro(s)
#-----------------------------------------------------------------------------

include(MacroEmptyExternalProject)

#-----------------------------------------------------------------------------
# Output directories.
#-----------------------------------------------------------------------------

foreach(type LIBRARY RUNTIME ARCHIVE)
  set(output_dir ${${MY_PROJECT_NAME}_BINARY_DIR}/bin)
  set(CMAKE_${type}_OUTPUT_DIRECTORY ${output_dir} CACHE INTERNAL "Single output directory for building all libraries.")
  mark_as_advanced(CMAKE_${type}_OUTPUT_DIRECTORY)
endforeach()

#-----------------------------------------------------------------------------
# Additional Settings (also shown during superbuild)
#-----------------------------------------------------------------------------

set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" CACHE PATH "")

#-----------------------------------------------------------------------------
# Additional Options (also shown during superbuild)
#-----------------------------------------------------------------------------

option(BUILD_SHARED_LIBS "Build ${MY_PROJECT_NAME} with shared libraries" ON)
option(WITH_COVERAGE "Enable/Disable coverage" OFF)
option(BUILD_TESTING "Test the project" ON)

option(${MY_PROJECT_NAME}_BUILD_ALL_PLUGINS "Build all ${MY_PROJECT_NAME} plugins" OFF)
option(${MY_PROJECT_NAME}_BUILD_ALL_APPS "Build all ${MY_PROJECT_NAME} applications" OFF)

mark_as_advanced(${MY_PROJECT_NAME}_INSTALL_RPATH_RELATIVE
                 ${MY_PROJECT_NAME}_BUILD_ALL_PLUGINS
                 ${MY_PROJECT_NAME}_BUILD_ALL_APPS
                 )

include(mitkFunctionWhitelists)
mitkFunctionCreateWhitelistPaths(MITK)
mitkFunctionFindWhitelists(MITK)
set(MITK_WHITELIST "VCLab" CACHE STRING "" FORCE)

#-----------------------------------------------------------------------------
# Superbuild script
#-----------------------------------------------------------------------------

if(${MY_PROJECT_NAME}_USE_SUPERBUILD)
  include("${CMAKE_CURRENT_SOURCE_DIR}/SuperBuild.cmake")
  return()
endif()

#*****************************************************************************
#**************************** END OF SUPERBUILD ****************************
#*****************************************************************************

#-----------------------------------------------------------------------------
# Prerequesites
#-----------------------------------------------------------------------------

# MITK 2025.12.2 stages header-only Boost without an installed BoostConfig.cmake
# package. CMake 3.31+ otherwise prefers config-mode Boost discovery, so use
# the bundled FindBoost module while importing this MITK build.
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 OLD)
endif()
if(POLICY CMP0219)
  cmake_policy(SET CMP0219 NEW)
endif()

# CMake 4.x needs a Boost package configuration to import the header-only
# Boost staged by MITK's superbuild.
list(PREPEND CMAKE_PREFIX_PATH
  "${CMAKE_CURRENT_SOURCE_DIR}/CMake/PackageDepends/Boost")

# MITK's exported targets retain these public build-time dependencies.  Find
# them before importing MITK so the imported link interfaces are complete.
find_package(Boost 1.74 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(OpenMP QUIET COMPONENTS CXX)
if(NOT TARGET OpenMP::OpenMP_CXX)
  # The MITK 2025.12.2 build was configured with MSVC's -openmp option but its
  # exported package does not rediscover OpenMP for consumers.  Recreate the
  # equivalent header-only CMake target; MSVC requires no extra link library.
  add_library(OpenMP::OpenMP_CXX INTERFACE IMPORTED)
  set_target_properties(OpenMP::OpenMP_CXX PROPERTIES
    INTERFACE_COMPILE_OPTIONS "-openmp")
endif()

find_package(MITK 2025.12.2 REQUIRED CONFIG)

# MITK uses this legacy variable when generating Windows launch scripts.  Qt 6
# no longer sets it during package discovery, but qmake is installed with the
# selected desktop kit and provides the required runtime-location query.
if(MITK_USE_Qt6 AND NOT QT_QMAKE_EXECUTABLE AND Qt6_DIR)
  get_filename_component(_gem_qt_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
  find_program(QT_QMAKE_EXECUTABLE NAMES qmake qmake.exe
    HINTS "${_gem_qt_prefix}/bin" NO_DEFAULT_PATH)
endif()

if(COMMAND mitkFunctionCheckMitkCompatibility)
  mitkFunctionCheckMitkCompatibility(VERSIONS MITK_VERSION_PLUGIN_SYSTEM 1 REQUIRED)
else()
  message(SEND_ERROR "The selected MITK package does not expose the expected compatibility helper.")
endif()

#-----------------------------------------------------------------------------
# CMake Function(s) and Macro(s)
#-----------------------------------------------------------------------------

set(CMAKE_MODULE_PATH
  ${MITK_SOURCE_DIR}/CMake
  ${CMAKE_MODULE_PATH}
  )

include(mitkFunctionCheckCompilerFlags)
include(mitkFunctionGetVersion)

#-----------------------------------------------------------------------------
# Set project specific options and variables (NOT available during superbuild)
#-----------------------------------------------------------------------------

set(${PROJECT_NAME}_VERSION_MAJOR "2017")
set(${PROJECT_NAME}_VERSION_MINOR "5")
set(${PROJECT_NAME}_VERSION_PATCH "0")
set(${PROJECT_NAME}_VERSION_STRING "${${PROJECT_NAME}_VERSION_MAJOR}.${${PROJECT_NAME}_VERSION_MINOR}.${${PROJECT_NAME}_VERSION_PATCH}")

# Ask the user if a console window should be shown with the applications
option(${PROJECT_NAME}_SHOW_CONSOLE_WINDOW "Use this to enable or disable the console window when starting GUI Applications" ON)
mark_as_advanced(${PROJECT_NAME}_SHOW_CONSOLE_WINDOW)

if(NOT UNIX AND NOT MINGW)
  set(MITK_WIN32_FORCE_STATIC "STATIC")
endif()

set(${PROJECT_NAME}_MODULES_PACKAGE_DEPENDS_DIR "${PROJECT_SOURCE_DIR}/CMake/PackageDepends")
list(APPEND MODULES_PACKAGE_DEPENDS_DIRS ${${PROJECT_NAME}_MODULES_PACKAGE_DEPENDS_DIR})

#-----------------------------------------------------------------------------
# Get project version info
#-----------------------------------------------------------------------------

mitkFunctionGetVersion(${PROJECT_SOURCE_DIR} ${PROJECT_NAME})

#-----------------------------------------------------------------------------
# Installation preparation
#
# These should be set before any MITK install macros are used
#-----------------------------------------------------------------------------

# on Mac OSX all CTK plugins get copied into every
# application bundle (.app directory) specified here
set(MACOSX_BUNDLE_NAMES)
if(APPLE)
  list(APPEND MACOSX_BUNDLE_NAMES ${MY_APP_NAME})
endif(APPLE)

#-----------------------------------------------------------------------------
# Set symbol visibility Flags
#-----------------------------------------------------------------------------

# MinGW does not export all symbols automatically, so no need to set flags
if(CMAKE_COMPILER_IS_GNUCXX AND NOT MINGW)
  # The MITK module build system does not yet support default hidden visibility
  set(VISIBILITY_CXX_FLAGS ) # "-fvisibility=hidden -fvisibility-inlines-hidden")
endif()

#-----------------------------------------------------------------------------
# Set coverage Flags
#-----------------------------------------------------------------------------

if(WITH_COVERAGE)
  if(CMAKE_COMPILER_IS_GNUCXX)
    set(coverage_flags "-g -fprofile-arcs -ftest-coverage -O0 -DNDEBUG")
    set(COVERAGE_CXX_FLAGS ${coverage_flags})
    set(COVERAGE_C_FLAGS ${coverage_flags})
  endif()
endif()

#-----------------------------------------------------------------------------
# Project C/CXX Flags
#-----------------------------------------------------------------------------

set(${PROJECT_NAME}_C_FLAGS "${MITK_C_FLAGS} ${COVERAGE_C_FLAGS}")
set(${PROJECT_NAME}_C_FLAGS_DEBUG ${MITK_C_FLAGS_DEBUG})
set(${PROJECT_NAME}_C_FLAGS_RELEASE ${MITK_C_FLAGS_RELEASE})
set(${PROJECT_NAME}_CXX_FLAGS "${MITK_CXX_FLAGS} ${VISIBILITY_CXX_FLAGS} ${COVERAGE_CXX_FLAGS}")
set(${PROJECT_NAME}_CXX_FLAGS_DEBUG ${MITK_CXX_FLAGS_DEBUG})
set(${PROJECT_NAME}_CXX_FLAGS_RELEASE ${MITK_CXX_FLAGS_RELEASE})

set(${PROJECT_NAME}_EXE_LINKER_FLAGS ${MITK_EXE_LINKER_FLAGS})
set(${PROJECT_NAME}_SHARED_LINKER_FLAGS ${MITK_SHARED_LINKER_FLAGS})
set(${PROJECT_NAME}_MODULE_LINKER_FLAGS ${MITK_MODULE_LINKER_FLAGS})

#-----------------------------------------------------------------------------
# Set C/CXX Flags
#-----------------------------------------------------------------------------

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${${PROJECT_NAME}_C_FLAGS}")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${${PROJECT_NAME}_C_FLAGS_DEBUG}")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${${PROJECT_NAME}_C_FLAGS_RELEASE}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${${PROJECT_NAME}_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${${PROJECT_NAME}_CXX_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${${PROJECT_NAME}_CXX_FLAGS_RELEASE}")

set(CMAKE_EXE_LINKER_FLAGS ${${PROJECT_NAME}_EXE_LINKER_FLAGS})
set(CMAKE_SHARED_LINKER_FLAGS ${${PROJECT_NAME}_SHARED_LINKER_FLAGS})
set(CMAKE_MODULE_LINKER_FLAGS ${${PROJECT_NAME}_MODULE_LINKER_FLAGS})

#-----------------------------------------------------------------------------
# Testing
#-----------------------------------------------------------------------------

if(BUILD_TESTING)
  enable_testing()
  include(CTest)
  mark_as_advanced(TCL_TCLSH DART_ROOT)

  # Setup file for setting custom ctest vars
  configure_file(
    CMake/CTestCustom.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/CTestCustom.cmake
    @ONLY
    )

  # Configuration for the CMake-generated test driver
  set(CMAKE_TESTDRIVER_EXTRA_INCLUDES "#include <stdexcept>")
  set(CMAKE_TESTDRIVER_BEFORE_TESTMAIN "
    try
      {")
  set(CMAKE_TESTDRIVER_AFTER_TESTMAIN " }
      catch( std::exception & excp )
        {
        fprintf(stderr,\"%s\\n\",excp.what());
        return EXIT_FAILURE;
        }
      catch( ... )
        {
        printf(\"Exception caught in the test driver\\n\");
        return EXIT_FAILURE;
        }
      ")
endif()

#-----------------------------------------------------------------------------
# ${MY_PROJECT_NAME}_SUPERBUILD_BINARY_DIR
#-----------------------------------------------------------------------------

# If ${MY_PROJECT_NAME}_SUPERBUILD_BINARY_DIR isn't defined, it means this project is
# *NOT* build using Superbuild. In that specific case, ${MY_PROJECT_NAME}_SUPERBUILD_BINARY_DIR
# should default to PROJECT_BINARY_DIR
if(NOT DEFINED ${PROJECT_NAME}_SUPERBUILD_BINARY_DIR)
  set(${PROJECT_NAME}_SUPERBUILD_BINARY_DIR ${PROJECT_BINARY_DIR})
endif()

#-----------------------------------------------------------------------------
# Qt support
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
# MITK modules
#-----------------------------------------------------------------------------

# This project's directory holding module config files
set(${PROJECT_NAME}_MODULES_CONF_DIR "${PROJECT_BINARY_DIR}/${MODULES_CONF_DIRNAME}")

# Append this projects's module config directory to the global list
# (This is used to get include directories for the <module_name>Exports.h files right)
list(APPEND MODULES_CONF_DIRS ${${PROJECT_NAME}_MODULES_CONF_DIR})

# Clean the modulesConf directory. This ensures that modules are sorted
# according to their dependencies in the Modules/CMakeLists.txt file
file(GLOB _modules_conf_files ${${PROJECT_NAME}_MODULES_CONF_DIR}/*.cmake)
if(_modules_conf_files)
  file(REMOVE ${_modules_conf_files})
endif()

add_subdirectory(Modules)

#-----------------------------------------------------------------------------
# CTK plugins
#-----------------------------------------------------------------------------

# The CMake code in this section *must* be in the top-level CMakeLists.txt file

macro(GetMyTargetLibraries all_target_libraries varname)
  set(re_ctkplugin "^ch_[a-zA-Z0-9_]+$")
  set(_tmp_list)
  list(APPEND _tmp_list ${all_target_libraries})
  ctkMacroListFilter(_tmp_list re_ctkplugin OUTPUT_VARIABLE ${varname})
endmacro()

include(${CMAKE_CURRENT_SOURCE_DIR}/Plugins/Plugins.cmake)
ctkMacroSetupPlugins(${PROJECT_PLUGINS}
                     APPS ${_apps_fullpath}
                     BUILD_OPTION_PREFIX ${PROJECT_NAME}_
                     BUILD_ALL ${${PROJECT_NAME}_BUILD_ALL_PLUGINS})

#-----------------------------------------------------------------------------
# Add subdirectories
#-----------------------------------------------------------------------------

add_subdirectory(Apps/MITK-GEM)

#-----------------------------------------------------------------------------
# Installation
#-----------------------------------------------------------------------------

# set MITK cpack variables
include(mitkSetupCPack)

# Customize CPack variables for this project
include(CPackSetup)

list(APPEND CPACK_CREATE_DESKTOP_LINKS "${MY_APP_NAME}")

configure_file(${MITK_SOURCE_DIR}/MITKCPackOptions.cmake.in
               ${PROJECT_BINARY_DIR}/${PROJECT_NAME}CPackOptions.cmake @ONLY)
set(CPACK_PROJECT_CONFIG_FILE "${PROJECT_BINARY_DIR}/${PROJECT_NAME}CPackOptions.cmake")

# include CPack model once all variables are set
include(CPack)

# Additional installation rules
include(mitkInstallRules)

# CPack's generic Windows dependency scanner cannot resolve all DLLs in a
# modern MITK superbuild when packaging outside a Visual Studio developer
# shell.  Add an explicit deployment pass after MITK has installed the app,
# plug-ins, and Qt resources.
if(WIN32)
  get_filename_component(MITK_GEM_SUPERBUILD_ROOT "${MITK_DIR}" DIRECTORY)
  get_filename_component(_mitk_gem_qt_bin "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
  find_program(MITK_GEM_WINDEPLOYQT_EXECUTABLE
    NAMES windeployqt.exe
    HINTS "${_mitk_gem_qt_bin}"
    NO_DEFAULT_PATH
    REQUIRED)

  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/CMake/InstallWindowsRuntime.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/InstallWindowsRuntime.cmake"
    @ONLY)
  install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/InstallWindowsRuntime.cmake")
endif()

#-----------------------------------------------------------------------------
# Last configuration steps
#-----------------------------------------------------------------------------

# If we are under Windows, create two batch files which correctly
# set up the environment for the application and for Visual Studio
if(WIN32)
  include(mitkFunctionCreateWindowsBatchScript)

  set(VS_SOLUTION_FILE "${PROJECT_BINARY_DIR}/${PROJECT_NAME}.sln")
  foreach(VS_BUILD_TYPE debug release)
    mitkFunctionCreateWindowsBatchScript("${PROJECT_SOURCE_DIR}/CMake/StartVS.bat.in"
      ${PROJECT_BINARY_DIR}/StartVS_${VS_BUILD_TYPE}.bat
      ${VS_BUILD_TYPE})
  endforeach()
endif(WIN32)
