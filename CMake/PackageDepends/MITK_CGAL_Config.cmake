# CGAL is a project-specific dependency. MITK no longer ships a CGAL package
# configuration, so resolve the installed CGAL package here and expose its
# modern imported target to mitk_use_modules().

find_package(CGAL REQUIRED)

if(TARGET CGAL::CGAL)
  list(APPEND ALL_LIBRARIES CGAL::CGAL)
else()
  # Compatibility fallback for older CGAL installations that do not export
  # the CGAL::CGAL target.
  list(APPEND ALL_INCLUDE_DIRECTORIES ${CGAL_INCLUDE_DIRS})
  list(APPEND ALL_LIBRARIES ${CGAL_LIBRARIES} ${CGAL_3RD_PARTY_LIBRARIES})
endif()
