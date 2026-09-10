/**
 * Regression coverage for the common volume-meshing surface preflight.
 */

#include "IMesher.h"
#include "SurfaceMeshValidation.h"
#include "SurfaceToUnstructuredGridFilter.h"

#include <mitkException.h>
#include <mitkImage.h>
#include <mitkSurface.h>

#include <vtkCellArray.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

#include <cstdlib>
#include <iostream>
#include <memory>
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

  vtkSmartPointer<vtkPolyData> CreateTetrahedralSurface(bool leaveOneFaceOpen)
  {
    auto points = vtkSmartPointer<vtkPoints>::New();
    points->InsertNextPoint(0.0, 0.0, 0.0);
    points->InsertNextPoint(1.0, 0.0, 0.0);
    points->InsertNextPoint(0.0, 1.0, 0.0);
    points->InsertNextPoint(0.0, 0.0, 1.0);

    constexpr vtkIdType faces[][3] = {
      {0, 2, 1},
      {0, 1, 3},
      {1, 2, 3},
      {2, 0, 3}
    };

    auto polygons = vtkSmartPointer<vtkCellArray>::New();
    const auto faceCount = leaveOneFaceOpen ? 3 : 4;
    for (int faceIndex = 0; faceIndex < faceCount; ++faceIndex)
    {
      polygons->InsertNextCell(3, faces[faceIndex]);
    }

    auto surface = vtkSmartPointer<vtkPolyData>::New();
    surface->SetPoints(points);
    surface->SetPolys(polygons);
    return surface;
  }

  mitk::Surface::Pointer MakeMitkSurface(vtkPolyData* polyData)
  {
    auto surface = mitk::Surface::New();
    surface->SetVtkPolyData(polyData);
    return surface;
  }

  class CountingMesher final : public gem::IMesher
  {
  public:
    bool computeWasCalled = false;

  protected:
    void compute() override
    {
      computeWasCalled = true;
    }
  };

  class TestableSurfaceToUnstructuredGridFilter final : public SurfaceToUnstructuredGridFilter
  {
  public:
    using Self = TestableSurfaceToUnstructuredGridFilter;
    using Pointer = itk::SmartPointer<Self>;
    itkNewMacro(Self);

    void SetRawInput(itk::DataObject* input)
    {
      this->SetNthInput(0, input);
    }

  protected:
    TestableSurfaceToUnstructuredGridFilter() = default;
  };
}

int main()
{
  try
  {
    auto openSurface = CreateTetrahedralSurface(true);
    std::string validationError;
    Require(!gem::ValidateSurfaceForVolumeMeshing(openSurface, validationError),
            "An open surface must fail volume-meshing validation.");
    Require(validationError.find("open") != std::string::npos,
            "Open-surface validation must explain that boundary edges were found.");

    auto closedSurface = CreateTetrahedralSurface(false);
    Require(gem::ValidateSurfaceForVolumeMeshing(closedSurface, validationError), validationError);

    auto rejectingMesher = std::make_shared<CountingMesher>();
    auto rejectingFilter = SurfaceToUnstructuredGridFilter::New();
    rejectingFilter->SetInput(MakeMitkSurface(openSurface), rejectingMesher);
    bool wasRejected = false;
    try
    {
      rejectingFilter->Update();
    }
    catch (const mitk::Exception&)
    {
      wasRejected = true;
    }
    Require(wasRejected, "The common volume-meshing filter must reject an open surface.");
    Require(!rejectingMesher->computeWasCalled,
            "The common preflight must stop every meshing backend before it receives an open surface.");

    auto acceptingMesher = std::make_shared<CountingMesher>();
    auto acceptingFilter = SurfaceToUnstructuredGridFilter::New();
    acceptingFilter->SetInput(MakeMitkSurface(closedSurface), acceptingMesher);
    acceptingFilter->Update();
    Require(acceptingMesher->computeWasCalled,
            "The common preflight must allow a valid closed surface to reach its meshing backend.");

    // A custom MITK pipeline can bypass the typed Surface overload. The common
    // filter must still reject an image before either backend sees it.
    auto imageInputFilter = TestableSurfaceToUnstructuredGridFilter::New();
    auto image = mitk::Image::New();
    imageInputFilter->SetRawInput(image.GetPointer());
    Require(imageInputFilter->GetInput() == nullptr,
            "A medical image must not be interpreted as a surface mesh.");

    bool imageWasRejected = false;
    try
    {
      imageInputFilter->Update();
    }
    catch (const mitk::Exception&)
    {
      imageWasRejected = true;
    }
    Require(imageWasRejected,
            "The common volume-meshing filter must reject a medical image before meshing starts.");

    std::cout << "Volume meshing surface validation regression test passed." << std::endl;
    return EXIT_SUCCESS;
  }
  catch (const std::exception& exception)
  {
    std::cerr << "Volume meshing surface validation regression test failed: " << exception.what() << std::endl;
  }
  catch (...)
  {
    std::cerr << "Volume meshing surface validation regression test failed with an unknown exception." << std::endl;
  }

  return EXIT_FAILURE;
}
