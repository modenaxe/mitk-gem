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
      MethodB,
      MethodE
    };

    struct ExportOptions
    {
      MaterialMappingMethod materialMappingMethod = MaterialMappingMethod::MethodA;
      unsigned int maxMaterialDefinitions = 500;
      double poissonRatio = 0.3;
    };

    /**
     * FEBio exports preserve the selected scalar element map exactly. Unlike
     * Abaqus and ANSYS output, they do not bin the field into material cards.
     */
    struct FebioExportOptions
    {
      MaterialMappingMethod materialMappingMethod = MaterialMappingMethod::MethodA;
      double poissonRatio = 0.3;
      std::string unitSystem = "mm-N-s";
      double geometryScale = 1.0;
      double youngsModulusScale = 1.0;
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
     * Check whether a grid is a homogeneous, positively oriented linear or
     * quadratic tetrahedral volume mesh with at least one valid FEBio element
     * material map (method A, B, or E).
     */
    GemCore_EXPORT bool CanExportFebioMesh(vtkUnstructuredGrid *grid, std::string *reason = nullptr);

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

    /**
     * Write a FEBio 4.0 XML model containing one solid domain and a continuous
     * element-wise Young's-modulus map. The result can be opened in FEBio
     * Studio; loads, boundary conditions, and analysis steps are intentionally
     * left for the user to define there.
     *
     * Throws std::invalid_argument for invalid mesh/options and
     * std::runtime_error for stream failures.
     */
    GemCore_EXPORT void WriteFebio(std::ostream &output,
                                   vtkUnstructuredGrid *grid,
                                   const FebioExportOptions &options);
  }
}
