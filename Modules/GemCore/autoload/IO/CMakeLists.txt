MITK_CREATE_MODULE( GemIO
  INCLUDE_DIRS
    PRIVATE src/IO
  DEPENDS PUBLIC GemCore MitkDataTypesExt MitkSceneSerialization
  AUTOLOAD_WITH MitkCore
)
