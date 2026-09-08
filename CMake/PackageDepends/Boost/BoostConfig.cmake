# MITK's superbuild stages the Boost 1.91 headers but does not install the
# Boost CMake package configuration.  CGAL 6.2 uses config-mode discovery
# with CMake 4.x, so expose the header-only targets required by CGAL.
if(NOT DEFINED Boost_ROOT OR NOT EXISTS "${Boost_ROOT}/include/boost/version.hpp")
  message(FATAL_ERROR
    "MITK-GEM's Boost compatibility package requires Boost_ROOT to reference "
    "the MITK superbuild prefix containing include/boost/version.hpp.")
endif()

set(Boost_FOUND TRUE)
set(Boost_VERSION "1.91.0")
set(Boost_INCLUDE_DIR "${Boost_ROOT}/include")
set(Boost_INCLUDE_DIRS "${Boost_INCLUDE_DIR}")
set(Boost_LIBRARIES "")

if(NOT TARGET Boost::headers)
  add_library(Boost::headers INTERFACE IMPORTED)
  set_target_properties(Boost::headers PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIR}")
endif()

if(NOT TARGET Boost::boost)
  add_library(Boost::boost INTERFACE IMPORTED)
  set_target_properties(Boost::boost PROPERTIES
    INTERFACE_LINK_LIBRARIES Boost::headers)
endif()

# FindBoost exposes these compatibility targets on Windows.  MITK's exported
# targets reference dynamic_linking, so provide the same interface when this
# header-only package is used in config mode.
foreach(_boost_compat_target diagnostic_definitions disable_autolinking dynamic_linking)
  if(NOT TARGET Boost::${_boost_compat_target})
    add_library(Boost::${_boost_compat_target} INTERFACE IMPORTED)
  endif()
endforeach()
set_target_properties(Boost::dynamic_linking PROPERTIES
  INTERFACE_COMPILE_DEFINITIONS BOOST_ALL_DYN_LINK)
