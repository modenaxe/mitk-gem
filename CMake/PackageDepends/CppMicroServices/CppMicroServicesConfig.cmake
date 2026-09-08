# MITK 2026.06 builds CppMicroServices as its internal MitkCppMicroServices
# module, but its exported MITKConfig.cmake still calls find_package for the
# standalone CppMicroServices package.  The exported MitkCppMicroServices
# target supplies the implementation, so this shim satisfies that stale
# package-discovery check for consumers of the in-tree MITK build.
set(CppMicroServices_FOUND TRUE)
