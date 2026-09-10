#pragma once

#include <string>

namespace mitk
{
  class DataNode;
  class Image;
  class UnstructuredGrid;
}

namespace MaterialMappingInputValidation
{
  /**
   * Validate a volumetric unstructured grid for material mapping. Every cell
   * must be three-dimensional; surface-only grids are not valid inputs.
   */
  bool ValidateVolumeMesh(const mitk::UnstructuredGrid* mesh, std::string& error);

  /** Validate a DataNode and its contained volumetric unstructured grid. */
  bool ValidateVolumeMeshNode(const mitk::DataNode* node, std::string& error);

  /**
   * Validate a scalar three-dimensional intensity image. Material mapping
   * samples this image at mesh coordinates, so label images and vector images
   * are not appropriate inputs.
   */
  bool ValidateIntensityImage(const mitk::Image* image, std::string& error);

  /**
   * Validate a DataNode and its contained intensity image. In addition to the
   * image checks, this rejects legacy binary segmentation nodes.
   */
  bool ValidateIntensityImageNode(const mitk::DataNode* node, std::string& error);
}
