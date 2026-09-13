mitk_create_module(GemCore
  DEPENDS PUBLIC MitkCore
  PACKAGE_DEPENDS PUBLIC VTK|CommonCore+CommonDataModel
)

if(TARGET ${MODULE_TARGET} AND (BUILD_TESTING OR MITK_GEM_BUILD_FEM_EXPORT_TESTS))
  add_subdirectory(test)
endif()

add_subdirectory(autoload/IO)
