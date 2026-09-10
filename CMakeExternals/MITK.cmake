#-----------------------------------------------------------------------------
# MITK
#-----------------------------------------------------------------------------

set(MITK_DEPENDS)
set(proj_DEPENDENCIES)
set(proj MITK)

if(NOT MITK_DIR)

  #-----------------------------------------------------------------------------
  # Create CMake options to customize the MITK build
  #-----------------------------------------------------------------------------

  option(MITK_USE_SUPERBUILD "Use superbuild for MITK" ON)
  option(MITK_USE_BLUEBERRY "Build the BlueBerry platform in MITK" ON)
  option(MITK_BUILD_EXAMPLES "Build the MITK examples" OFF)
  option(MITK_BUILD_ALL_PLUGINS "Build all MITK plugins" OFF)
  option(MITK_BUILD_TESTING "Build the MITK unit tests" OFF)
  option(MITK_GEM_BUILD_MITK_SEGMENTATION
    "Build MITK's standard Segmentation workbench plug-in required by MITK-GEM" ON)
  set(MITK_BUILD_CONFIGURATION "Custom" CACHE STRING "MITK build configuration")
  option(MITK_USE_ACVD "Use Approximated Centroidal Voronoi Diagrams" ON)
  option(MITK_USE_CTK "Use CTK in MITK" ${MITK_USE_BLUEBERRY})
  option(MITK_USE_DCMTK "Use DCMTK in MITK" ON)
  option(MITK_USE_Qt6 "Use Qt 6 in MITK" ON)
  option(MITK_USE_Python3 "Enable Python 3 support in MITK" OFF)

  if(MITK_USE_BLUEBERRY AND NOT MITK_USE_CTK)
    message("Forcing MITK_USE_CTK to ON because of MITK_USE_BLUEBERRY")
    set(MITK_USE_CTK ON CACHE BOOL "Use CTK in MITK" FORCE)
  endif()

  if(MITK_USE_CTK AND NOT MITK_USE_Qt6)
    message("Forcing MITK_USE_Qt6 to ON because of MITK_USE_CTK")
    set(MITK_USE_Qt6 ON CACHE BOOL "Use Qt 6 in MITK" FORCE)
  endif()

  mark_as_advanced(MITK_USE_SUPERBUILD
                   MITK_BUILD_ALL_PLUGINS
                   MITK_BUILD_TESTING
                   MITK_BUILD_CONFIGURATION
                   )

  set(mitk_cmake_boolean_args
    MITK_USE_SUPERBUILD
    MITK_USE_BLUEBERRY
    MITK_BUILD_EXAMPLES
    MITK_BUILD_ALL_PLUGINS
    MITK_USE_ACVD
    MITK_USE_CTK
    MITK_USE_DCMTK
    MITK_USE_Qt6
    MITK_USE_Python3
   )

  if(MITK_USE_Qt6)
    # Look for Qt at the superbuild level, so a missing Qt installation is
    # reported before the MITK external project starts.
    find_package(Qt6 6.10 COMPONENTS
      Concurrent
      Core
      Core5Compat
      Gui
      Help
      LinguistTools
      Network
      OpenGL
      OpenGLWidgets
      Qml
      Sql
      StateMachine
      Svg
      UiTools
      WebEngineCore
      WebEngineWidgets
      Widgets
      Xml
      REQUIRED)

    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake LOCATION)
  endif()

  # Configure the set of default pixel types
  set(MITK_ACCESSBYITK_INTEGRAL_PIXEL_TYPES
      "int, unsigned int, short, unsigned short, char, unsigned char"
      CACHE STRING "List of integral pixel types used in AccessByItk and InstantiateAccessFunction macros")

  set(MITK_ACCESSBYITK_FLOATING_PIXEL_TYPES
      "double, float"
      CACHE STRING "List of floating pixel types used in AccessByItk and InstantiateAccessFunction macros")

  set(MITK_ACCESSBYITK_COMPOSITE_PIXEL_TYPES
      ""
      CACHE STRING "List of composite pixel types used in AccessByItk and InstantiateAccessFunction macros")

  set(MITK_ACCESSBYITK_DIMENSIONS
      "2,3"
      CACHE STRING "List of dimensions used in AccessByItk and InstantiateAccessFunction macros")

  foreach(_arg MITK_ACCESSBYITK_INTEGRAL_PIXEL_TYPES MITK_ACCESSBYITK_FLOATING_PIXEL_TYPES
               MITK_ACCESSBYITK_COMPOSITE_PIXEL_TYPES MITK_ACCESSBYITK_DIMENSIONS)
    mark_as_advanced(${_arg})
    list(APPEND additional_mitk_cmakevars "-D${_arg}:STRING=${${_arg}}")
  endforeach()

  #-----------------------------------------------------------------------------
  # Create options to inject pre-build dependencies
  #-----------------------------------------------------------------------------

  foreach(proj CTK DCMTK GDCM VTK ITK OpenCV CableSwig)
    if(MITK_USE_${proj})
      set(MITK_${proj}_DIR "${${proj}_DIR}" CACHE PATH "Path to ${proj} build directory")
      mark_as_advanced(MITK_${proj}_DIR)
      if(MITK_${proj}_DIR)
        list(APPEND additional_mitk_cmakevars "-D${proj}_DIR:PATH=${MITK_${proj}_DIR}")
      endif()
    endif()
  endforeach()

  set(MITK_BOOST_ROOT "${BOOST_ROOT}" CACHE PATH "Path to Boost directory")
  mark_as_advanced(MITK_BOOST_ROOT)
  if(MITK_BOOST_ROOT)
    list(APPEND additional_mitk_cmakevars "-DBOOST_ROOT:PATH=${MITK_BOOST_ROOT}")
  endif()

  set(_mitk_local_source_dir "${CMAKE_CURRENT_SOURCE_DIR}/__MITK-2025.12.2")
  if(EXISTS "${_mitk_local_source_dir}/CMakeLists.txt")
    set(_mitk_default_source_dir "${_mitk_local_source_dir}")
  endif()
  set(MITK_SOURCE_DIR "${_mitk_default_source_dir}" CACHE PATH "MITK source code location. If empty, MITK will be cloned from MITK_GIT_REPOSITORY")
  unset(_mitk_default_source_dir)
  unset(_mitk_local_source_dir)
  set(MITK_GIT_REPOSITORY "https://github.com/MITK/MITK.git" CACHE STRING "The git repository for cloning MITK")
  set(MITK_GIT_TAG "v2025.12.2" CACHE STRING "The git tag/hash to be used when cloning from MITK_GIT_REPOSITORY")
  set(MITK_WHITELIST "VCLab")
  mark_as_advanced(MITK_SOURCE_DIR MITK_GIT_REPOSITORY MITK_GIT_TAG)

  #-----------------------------------------------------------------------------
  # Create the final variable containing superbuild boolean args
  #-----------------------------------------------------------------------------

  set(mitk_boolean_args)
  foreach(mitk_cmake_arg ${mitk_cmake_boolean_args})
    list(APPEND mitk_boolean_args -D${mitk_cmake_arg}:BOOL=${${mitk_cmake_arg}})
  endforeach()

  #-----------------------------------------------------------------------------
  # pass the whitelist to MITK build
  #-----------------------------------------------------------------------------
  MESSAGE( STATUS "WHITELIST VARIABLES ON EXTERNAL MITK LEVEL" )
  MESSAGE( STATUS "WHITELIST:                          " ${MITK_WHITELIST} )
  MESSAGE( STATUS "MITK_WHITELISTS_EXTERNAL_PATH:      " ${MITK_WHITELISTS_EXTERNAL_PATH} )
  MESSAGE( STATUS "MITK_WHITELISTS_INTERNAL_PATH:      " ${MITK_WHITELISTS_INTERNAL_PATH} )

  get_filename_component(MITK_WHITELISTS_INTERNAL_PATH ${MITK_WHITELISTS_INTERNAL_PATH} ABSOLUTE)
  list(APPEND additional_mitk_cmakevars "-DMITK_WHITELIST:STRING=${MITK_WHITELIST}")
  list(APPEND additional_mitk_cmakevars "-DMITK_BUILD_CONFIGURATION:STRING=${MITK_BUILD_CONFIGURATION}")
  list(APPEND additional_mitk_cmakevars "-DMITK_WHITELISTS_EXTERNAL_PATH:FILEPATH=${MITK_WHITELISTS_EXTERNAL_PATH}")
  list(APPEND additional_mitk_cmakevars "-DMITK_WHITELISTS_INTERNAL_PATH:FILEPATH=${MITK_WHITELISTS_INTERNAL_PATH}")
  list(APPEND additional_mitk_cmakevars "-DMITK_BUILD_ALL_PLUGINS:BOOL=${MITK_BUILD_ALL_PLUGINS}")

  # MITK's whitelist filters the available plug-ins, but it deliberately does
  # not turn on plug-ins which MITK declares OFF by default. MITK-GEM's default
  # perspective embeds the standard Segmentation view, so request it explicitly.
  # A plug-in cache key contains dots, which CMake cannot pass with -D, so use
  # an initial-cache file instead.
  if(MITK_GEM_BUILD_MITK_SEGMENTATION)
    set(_mitk_gem_initial_cache
      "${CMAKE_CURRENT_BINARY_DIR}/MITK-GEM-MITKInitialCache.cmake")
    configure_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/CMake/MITK-GEM-MITKInitialCache.cmake.in"
      "${_mitk_gem_initial_cache}"
      @ONLY)
    list(APPEND additional_mitk_cmakevars "-C${_mitk_gem_initial_cache}")
  endif()

  #-----------------------------------------------------------------------------
  # Additional MITK CMake variables
  #-----------------------------------------------------------------------------

  if(MITK_USE_Qt6 AND Qt6_DIR)
    list(APPEND additional_mitk_cmakevars "-DQt6_DIR:PATH=${Qt6_DIR}")
  endif()

  # Forward the installed OpenSSL location into the external MITK configure
  # step so MITK can discover its HTTP support on Windows.
  set(MITK_OPENSSL_ROOT_DIR "" CACHE PATH "Path to the OpenSSL installation used by MITK")
  mark_as_advanced(MITK_OPENSSL_ROOT_DIR)
  if(MITK_OPENSSL_ROOT_DIR)
    list(APPEND additional_mitk_cmakevars "-DOPENSSL_ROOT_DIR:PATH=${MITK_OPENSSL_ROOT_DIR}")
  endif()

  if(MITK_USE_CTK)
    list(APPEND additional_mitk_cmakevars "-DGIT_EXECUTABLE:FILEPATH=${GIT_EXECUTABLE}")
  endif()

  if(MITK_INITIAL_CACHE_FILE)
    list(APPEND additional_mitk_cmakevars "-DMITK_INITIAL_CACHE_FILE:INTERNAL=${MITK_INITIAL_CACHE_FILE}")
  endif()

  if(MITK_USE_SUPERBUILD)
    set(MITK_BINARY_DIR ${proj}-superbuild)
  else()
    set(MITK_BINARY_DIR ${proj}-build)
  endif()

  set(proj_DEPENDENCIES)
  set(MITK_DEPENDS ${proj})

  # Configure the MITK souce code location

  if(NOT MITK_SOURCE_DIR)
    set(mitk_source_location
        SOURCE_DIR ${CMAKE_BINARY_DIR}/${proj}
        GIT_REPOSITORY ${MITK_GIT_REPOSITORY}
        GIT_TAG ${MITK_GIT_TAG}
        )
  else()
    set(mitk_source_location
        SOURCE_DIR ${MITK_SOURCE_DIR}
       )
  endif()

  # MITK 2025.12.2 pins a few third-party projects whose declared CMake
  # minimum version predates the compatibility floor enforced by CMake 4.
  # Keep the workaround as an explicit, reproducible project patch instead
  # of relying on manual edits in the local MITK checkout.
  set(mitk_patch_command)
  if(MITK_GIT_TAG STREQUAL "v2025.12.2")
    set(mitk_patch_command
      PATCH_COMMAND
        ${CMAKE_COMMAND}
        -DMITK_SOURCE_DIR:PATH=<SOURCE_DIR>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CMake/ApplyMITK2025CMake4Compat.cmake
    )
  endif()

  ExternalProject_Add(${proj}
    ${mitk_source_location}
    BINARY_DIR ${MITK_BINARY_DIR}
    PREFIX ${proj}${ep_suffix}
    INSTALL_COMMAND ""
    ${mitk_patch_command}
    CMAKE_GENERATOR ${gen}
    CMAKE_ARGS
      ${ep_common_args}
      ${mitk_boolean_args}
      ${additional_mitk_cmakevars}
      -DBUILD_SHARED_LIBS:BOOL=ON
      -DBUILD_TESTING:BOOL=${MITK_BUILD_TESTING}
    CMAKE_CACHE_ARGS
      ${ep_common_cache_args}
    CMAKE_CACHE_DEFAULT_ARGS
      ${ep_common_cache_default_args}
    DEPENDS
      ${proj_DEPENDENCIES}
    )

  if(MITK_USE_SUPERBUILD)
    set(MITK_DIR "${CMAKE_CURRENT_BINARY_DIR}/${MITK_BINARY_DIR}/MITK-build")
    # MITK's exported CMake package finds its third-party dependencies (Boost,
    # ITK, VTK, and others) in this staged prefix. Keep it on the consuming
    # project's search path when MITK is built through its superbuild.
    list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_BINARY_DIR}/${MITK_BINARY_DIR}/ep")
  else()
    set(MITK_DIR "${CMAKE_CURRENT_BINARY_DIR}/${MITK_BINARY_DIR}")
  endif()

else()

  # The project is provided using MITK_DIR, nevertheless since other
  # projects may depend on MITK, let's add an 'empty' one
  MacroEmptyExternalProject(${proj} "${proj_DEPENDENCIES}")

  # Further, do some sanity checks in the case of a pre-built MITK
  set(my_itk_dir ${ITK_DIR})
  set(my_vtk_dir ${VTK_DIR})
  set(my_qt6_dir ${Qt6_DIR})

  find_package(MITK 2025.12.2 REQUIRED CONFIG)

  if(my_itk_dir AND ITK_DIR)
    if(NOT my_itk_dir STREQUAL ${ITK_DIR})
      message(FATAL_ERROR "ITK packages do not match:\n   ${MY_PROJECT_NAME}: ${my_itk_dir}\n  MITK: ${ITK_DIR}")
    endif()
  endif()

  if(my_vtk_dir AND VTK_DIR)
    if(NOT my_vtk_dir STREQUAL ${VTK_DIR})
      message(FATAL_ERROR "VTK packages do not match:\n   ${MY_PROJECT_NAME}: ${my_vtk_dir}\n  MITK: ${VTK_DIR}")
    endif()
  endif()

  if(my_qt6_dir AND Qt6_DIR)
    if(NOT my_qt6_dir STREQUAL ${Qt6_DIR})
      message(FATAL_ERROR "Qt 6 packages do not match:\n   ${MY_PROJECT_NAME}: ${my_qt6_dir}\n  MITK: ${Qt6_DIR}")
    endif()
  endif()
endif()
