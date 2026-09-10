/**
 * Regression coverage for Material Mapping input validation.
 */

#include "MaterialMappingInputValidation.h"

#include <mitkDataNode.h>
#include <mitkImage.h>
#include <mitkLabelSetImage.h>
#include <mitkPixelType.h>
#include <mitkSurface.h>
#include <mitkUnstructuredGrid.h>

#include <vtkCellType.h>
#include <vtkPoints.h>
#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
  void Require(bool condition, const std::string& message)
  {
    if (!condition)
    {
      throw std::runtime_error(message);
    }
  }

  mitk::Image::Pointer CreateScalarImage()
  {
    constexpr unsigned int dimensions[] = {2, 2, 2};
    auto image = mitk::Image::New();
    image->Initialize(mitk::MakeScalarPixelType<short>(), 3, dimensions);
    return image;
  }

  mitk::Image::Pointer CreateTwoDimensionalScalarImage()
  {
    constexpr unsigned int dimensions[] = {2, 2};
    auto image = mitk::Image::New();
    image->Initialize(mitk::MakeScalarPixelType<short>(), 2, dimensions);
    return image;
  }

  mitk::Image::Pointer CreateTwoComponentImage()
  {
    constexpr unsigned int dimensions[] = {2, 2, 2};
    auto image = mitk::Image::New();
    image->Initialize(mitk::MakePixelType<short, short>(2), 3, dimensions);
    return image;
  }

  mitk::UnstructuredGrid::Pointer CreateGrid(int cellType, vtkIdType numberOfCellPoints)
  {
    auto points = vtkSmartPointer<vtkPoints>::New();
    points->InsertNextPoint(0.0, 0.0, 0.0);
    points->InsertNextPoint(1.0, 0.0, 0.0);
    points->InsertNextPoint(0.0, 1.0, 0.0);
    points->InsertNextPoint(0.0, 0.0, 1.0);

    vtkIdType pointIds[] = {0, 1, 2, 3};
    auto vtkGrid = vtkSmartPointer<vtkUnstructuredGrid>::New();
    vtkGrid->SetPoints(points);
    vtkGrid->InsertNextCell(cellType, numberOfCellPoints, pointIds);

    auto grid = mitk::UnstructuredGrid::New();
    grid->SetVtkUnstructuredGrid(vtkGrid);
    return grid;
  }

  mitk::DataNode::Pointer MakeNode(mitk::BaseData* data)
  {
    auto node = mitk::DataNode::New();
    node->SetData(data);
    return node;
  }
}

int main()
{
  try
  {
    std::string error;

    auto scalarImageNode = MakeNode(CreateScalarImage());
    // A real greyscale CT node is marked non-binary. The synthetic image's
    // all-zero voxel buffer would otherwise be correctly classified as a
    // binary image by MITK's default-property heuristic.
    scalarImageNode->SetBoolProperty("binary", false);
    Require(MaterialMappingInputValidation::ValidateIntensityImageNode(scalarImageNode, error), error);

    auto binaryMaskNode = MakeNode(CreateScalarImage());
    binaryMaskNode->SetBoolProperty("binary", true);
    Require(!MaterialMappingInputValidation::ValidateIntensityImageNode(binaryMaskNode, error),
            "A legacy binary segmentation must not be accepted as a greyscale CT image.");

    auto twoDimensionalImageNode = MakeNode(CreateTwoDimensionalScalarImage());
    twoDimensionalImageNode->SetBoolProperty("binary", false);
    Require(!MaterialMappingInputValidation::ValidateIntensityImageNode(twoDimensionalImageNode, error),
            "A two-dimensional image must not be accepted as a greyscale CT image.");

    auto twoComponentImageNode = MakeNode(CreateTwoComponentImage());
    twoComponentImageNode->SetBoolProperty("binary", false);
    Require(!MaterialMappingInputValidation::ValidateIntensityImageNode(twoComponentImageNode, error),
            "A multi-component image must not be accepted as a greyscale CT image.");

    auto multiLabelSegmentationNode = MakeNode(mitk::MultiLabelSegmentation::New());
    Require(!MaterialMappingInputValidation::ValidateIntensityImageNode(multiLabelSegmentationNode, error),
            "A multi-label segmentation must not be accepted as a greyscale CT image.");

    auto surfaceNode = MakeNode(mitk::Surface::New());
    Require(!MaterialMappingInputValidation::ValidateIntensityImageNode(surfaceNode, error),
            "A surface must not be accepted as a greyscale CT image.");

    auto volumeMesh = CreateGrid(VTK_TETRA, 4);
    auto volumeMeshNode = MakeNode(volumeMesh);
    Require(MaterialMappingInputValidation::ValidateVolumeMeshNode(volumeMeshNode, error), error);
    Require(!MaterialMappingInputValidation::ValidateIntensityImageNode(volumeMeshNode, error),
            "A volume mesh must not be accepted as a greyscale CT image.");

    auto surfaceOnlyGridNode = MakeNode(CreateGrid(VTK_TRIANGLE, 3));
    Require(!MaterialMappingInputValidation::ValidateVolumeMeshNode(surfaceOnlyGridNode, error),
            "A two-dimensional unstructured grid must not be accepted as a volume mesh.");

    Require(!MaterialMappingInputValidation::ValidateVolumeMeshNode(scalarImageNode, error),
            "A greyscale image must not be accepted as a volume mesh.");

    std::cout << "Material-mapping input validation regression test passed." << std::endl;
    return EXIT_SUCCESS;
  }
  catch (const std::exception& exception)
  {
    std::cerr << "Material-mapping input validation regression test failed: " << exception.what() << std::endl;
  }
  catch (...)
  {
    std::cerr << "Material-mapping input validation regression test failed with an unknown error." << std::endl;
  }

  return EXIT_FAILURE;
}
