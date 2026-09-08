project(ch_zhaw_materialmapping)

mitk_create_plugin(
  EXPORT_DIRECTIVE MATERIALMAPPING_EXPORT
  EXPORTED_INCLUDE_SUFFIXES src
  MODULE_DEPENDS MitkQtWidgetsExt GemCore
  PACKAGE_DEPENDS VTK|ImagingGeneral+ImagingMorphological tinyxml2
)

# GemIO is an autoload module and must not be linked directly by a plug-in.
# This plug-in only consumes its public resource-name constants.
target_include_directories(${PLUGIN_TARGET} PRIVATE
  "${CMAKE_SOURCE_DIR}/Modules/GemCore/autoload/IO")

if(BUILD_TESTING)
  target_compile_definitions(${PLUGIN_TARGET} PRIVATE MITK_GEM_ENABLE_GUI_TESTS)
endif()
