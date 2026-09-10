#include "MaterialMappingInputValidation.h"

#include <mitkDataNode.h>
#include <mitkImage.h>
#include <mitkUnstructuredGrid.h>

#include <vtkCell.h>
#include <vtkUnstructuredGrid.h>

#include <string>

namespace MaterialMappingInputValidation
{
  bool ValidateVolumeMesh(const mitk::UnstructuredGrid* mesh, std::string& error)
  {
    error.clear();

    if (mesh == nullptr)
    {
      error = "Select a volumetric unstructured grid for material mapping.";
      return false;
    }

    // MITK's UnstructuredGrid accessor is not const-qualified, although it
    // only returns the owned VTK object here.
    auto* vtkMesh = const_cast<mitk::UnstructuredGrid*>(mesh)->GetVtkUnstructuredGrid();
    if (vtkMesh == nullptr || vtkMesh->GetNumberOfPoints() == 0 || vtkMesh->GetNumberOfCells() == 0)
    {
      error = "The selected unstructured grid is empty and cannot be used for material mapping.";
      return false;
    }

    for (vtkIdType cellId = 0; cellId < vtkMesh->GetNumberOfCells(); ++cellId)
    {
      auto* cell = vtkMesh->GetCell(cellId);
      if (cell == nullptr || cell->GetCellDimension() != 3)
      {
        error = "The selected unstructured grid is not a volume mesh: every cell must be three-dimensional.";
        return false;
      }
    }

    return true;
  }

  bool ValidateVolumeMeshNode(const mitk::DataNode* node, std::string& error)
  {
    error.clear();

    if (node == nullptr)
    {
      error = "Select a volumetric unstructured grid for material mapping.";
      return false;
    }

    auto* mesh = dynamic_cast<const mitk::UnstructuredGrid*>(node->GetData());
    if (mesh == nullptr)
    {
      error = "Material mapping requires a volumetric unstructured grid; images, segmentations, and surfaces are not valid mesh inputs.";
      return false;
    }

    return ValidateVolumeMesh(mesh, error);
  }

  bool ValidateIntensityImage(const mitk::Image* image, std::string& error)
  {
    error.clear();

    if (image == nullptr)
    {
      error = "Select a three-dimensional greyscale intensity image for material mapping.";
      return false;
    }

    // NodePredicateDataType("Image") is deliberately exact for the UI.
    // Mirror that rule here so legacy derived-image segmentations and other
    // specialized image classes cannot reach the scalar CT-processing path.
    if (std::string(image->GetNameOfClass()) != "Image")
    {
      error = "Material mapping requires a standard greyscale intensity image, not a segmentation or specialized image type.";
      return false;
    }

    if (!image->IsInitialized())
    {
      error = "The selected greyscale image is not initialized.";
      return false;
    }

    if (image->GetDimension() != 3)
    {
      error = "Material mapping requires a three-dimensional greyscale intensity image.";
      return false;
    }

    if (image->GetPixelType().GetNumberOfComponents() != 1)
    {
      error = "Material mapping requires a single-component greyscale intensity image.";
      return false;
    }

    return true;
  }

  bool ValidateIntensityImageNode(const mitk::DataNode* node, std::string& error)
  {
    error.clear();

    if (node == nullptr)
    {
      error = "Select a three-dimensional greyscale intensity image for material mapping.";
      return false;
    }

    auto* image = dynamic_cast<const mitk::Image*>(node->GetData());
    if (image == nullptr)
    {
      error = "Material mapping requires a greyscale intensity image; segmentations, surfaces, and volume meshes are not valid image inputs.";
      return false;
    }

    bool isBinarySegmentation = false;
    if (node->GetBoolProperty("binary", isBinarySegmentation) && isBinarySegmentation)
    {
      error = "The selected image is marked as a binary segmentation. Select the original greyscale intensity image instead.";
      return false;
    }

    return ValidateIntensityImage(image, error);
  }
}
