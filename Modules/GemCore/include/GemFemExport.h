#pragma once

#include <GemCoreExports.h>

#include <iosfwd>
#include <string>

class vtkUnstructuredGrid;

namespace gem
{
  namespace io
  {
    enum class MaterialMappingMethod
    {
      MethodA,
      MethodB
    };

    struct ExportOptions
    {
      MaterialMappingMethod materialMappingMethod = MaterialMappingMethod::MethodA;
      unsigned int maxMaterialDefinitions = 500;
      double poissonRatio = 0.3;
    };

    /**
     * Return the VTK cell-data array used by the selected material-mapping method.
     */
    GemCore_EXPORT const char *GetMaterialArrayName(MaterialMappingMethod method);

    /**
     * Check whether a grid is a homogeneous linear or quadratic tetrahedral
     * volume mesh with at least one valid method-A or method-B material array.
     */
    GemCore_EXPORT bool CanExportFemMesh(vtkUnstructuredGrid *grid, std::string *reason = nullptr);

    /**
     * Write a self-contained Abaqus input deck containing nodes, tetrahedra,
     * element sets, solid sections, and isotropic elastic material definitions.
     * Units are inherited from the input mesh and are not converted.
     *
     * Throws std::invalid_argument for invalid mesh/options and
     * std::runtime_error for stream failures.
     */
    GemCore_EXPORT void WriteAbaqus(std::ostream &output, vtkUnstructuredGrid *grid, const ExportOptions &options);

    /**
     * Write a Mechanical APDL command file containing nodes, tetrahedra, and
     * isotropic elastic material definitions. The file is intended to be read
     * with /INPUT. Units are inherited from the input mesh and are not converted.
     *
     * Throws std::invalid_argument for invalid mesh/options and
     * std::runtime_error for stream failures.
     */
    GemCore_EXPORT void WriteAnsys(std::ostream &output, vtkUnstructuredGrid *grid, const ExportOptions &options);
  }
}
