
set(_app_options)
if(${MY_PROJECT_NAME}_SHOW_CONSOLE_WINDOW)
  list(APPEND _app_options SHOW_CONSOLE)
endif()

# Plug-ins listed below will not be
#  - added as a build-time dependency to the executable
#  - listed in the provisioning file for the executable
#  - installed if they are external plug-ins

set(_exclude_plugins

)

mitkFunctionCreateBlueBerryApplication(
  NAME ${MY_APP_NAME}
  DESCRIPTION "MITK - ${MY_APP_NAME} Application"
  EXCLUDE_PLUGINS ${_exclude_plugins}
  ${_app_options}
)
mitk_use_modules(TARGET ${MY_APP_NAME}
  MODULES MitkAppUtil
  PACKAGES
    Qt6|Widgets
    # MultiLabelSegmentation creates vtkGPUVolumeRayCastMapper instances
    # whenever a segmentation becomes visible in a 3D renderer. The VTK
    # implementation is selected through an object-factory registration, so
    # the executable must initialise the OpenGL2 volume-rendering module.
    VTK|RenderingVolumeOpenGL2
)
