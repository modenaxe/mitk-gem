/**
 * Regression coverage for GraphCut3D's modern MITK segmentation integration.
 */

#include "GraphcutSegmentationUtils.h"
#include "Voxel2MeshSegmentationUtils.h"
#include "mitkGraphcutSegmentationToSurfaceFilter.h"
#include "lib/GraphCut3D/GraphCut.h"

#include <mitkDataStorage.h>
#include <mitkImageCast.h>
#include <mitkITKImageImport.h>
#include <mitkPixelType.h>
#include <mitkStandaloneDataStorage.h>
#include <mitkSurface.h>

#include <itkImage.h>

#include <vtkPolyData.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
  using LabelValueType = mitk::MultiLabelSegmentation::LabelValueType;

  void Require(bool condition, const std::string& message)
  {
    if (!condition)
    {
      throw std::runtime_error(message);
    }
  }

  std::size_t Offset(unsigned int x, unsigned int y, unsigned int z, unsigned int width, unsigned int height)
  {
    return static_cast<std::size_t>(x) + static_cast<std::size_t>(y) * width
      + static_cast<std::size_t>(z) * width * height;
  }

  mitk::Image::Pointer CreateReferenceImage()
  {
    const unsigned int dimensions[] = { 7, 7, 7 };
    std::vector<short> pixels(dimensions[0] * dimensions[1] * dimensions[2], 0);
    for (unsigned int z = 0; z < dimensions[2]; ++z)
    {
      for (unsigned int y = 0; y < dimensions[1]; ++y)
      {
        for (unsigned int x = 0; x < dimensions[0]; ++x)
        {
          pixels[Offset(x, y, z, dimensions[0], dimensions[1])] = static_cast<short>(x + y + z);
        }
      }
    }

    auto image = mitk::Image::New();
    image->Initialize(mitk::MakeScalarPixelType<short>(), 3, dimensions);
    image->SetVolume(pixels.data());
    return image;
  }

  mitk::Image::Pointer CreateLabelImage(const mitk::Image* reference,
                                        LabelValueType foregroundLabel,
                                        LabelValueType backgroundLabel)
  {
    const auto* dimensions = reference->GetDimensions();
    const auto numberOfPixels = static_cast<std::size_t>(dimensions[0]) * dimensions[1] * dimensions[2];
    std::vector<LabelValueType> pixels(numberOfPixels, mitk::MultiLabelSegmentation::UNLABELED_VALUE);
    pixels[Offset(1, 1, 1, dimensions[0], dimensions[1])] = foregroundLabel;
    pixels[Offset(5, 5, 5, dimensions[0], dimensions[1])] = backgroundLabel;

    auto image = mitk::Image::New();
    image->Initialize(mitk::MakeScalarPixelType<LabelValueType>(), 3, dimensions);
    image->SetGeometry(reference->GetGeometry()->Clone());
    image->SetVolume(pixels.data());
    return image;
  }

  mitk::Image::Pointer CreateLegacyMask(const mitk::Image* reference)
  {
    const auto* dimensions = reference->GetDimensions();
    const auto numberOfPixels = static_cast<std::size_t>(dimensions[0]) * dimensions[1] * dimensions[2];
    std::vector<unsigned char> pixels(numberOfPixels, 0);
    pixels[Offset(5, 5, 5, dimensions[0], dimensions[1])] = 1;

    auto image = mitk::Image::New();
    image->Initialize(mitk::MakeScalarPixelType<unsigned char>(), 3, dimensions);
    image->SetGeometry(reference->GetGeometry()->Clone());
    image->SetVolume(pixels.data());
    return image;
  }
}

int main()
{
  try
  {
    const auto foregroundLabel = static_cast<LabelValueType>(7);
    const auto backgroundLabel = static_cast<LabelValueType>(13);
    auto referenceImage = CreateReferenceImage();
    auto labelImage = CreateLabelImage(referenceImage, foregroundLabel, backgroundLabel);

    auto segmentation = mitk::MultiLabelSegmentation::New();
    segmentation->InitializeByLabeledImage(labelImage);
    auto segmentationNode = mitk::DataNode::New();
    segmentationNode->SetData(segmentation);
    segmentationNode->SetName("modern seed segmentation");

    GraphcutSegmentationUtils::SeedSelection foregroundSeed;
    GraphcutSegmentationUtils::SeedSelection backgroundSeed;
    std::string error;
    Require(GraphcutSegmentationUtils::CreateSeedSelection(segmentationNode, foregroundLabel, foregroundSeed, error), error);
    Require(GraphcutSegmentationUtils::CreateSeedSelection(segmentationNode, backgroundLabel, backgroundSeed, error), error);
    Require(foregroundSeed.isMultiLabelSegmentation && backgroundSeed.isMultiLabelSegmentation,
            "Modern multi-label segmentations must be recognized as seed sources.");
    Require(GraphcutSegmentationUtils::ValidateGraphCutInputs(referenceImage, foregroundSeed, backgroundSeed, error), error);

    GraphcutSegmentationUtils::SeedSelection duplicateForegroundSeed;
    Require(GraphcutSegmentationUtils::CreateSeedSelection(segmentationNode, foregroundLabel, duplicateForegroundSeed, error), error);
    Require(!GraphcutSegmentationUtils::ValidateGraphCutInputs(referenceImage, foregroundSeed, duplicateForegroundSeed, error),
            "The same modern label must not be accepted for both foreground and background.");

    auto mismatchedLegacyMask = CreateLegacyMask(referenceImage);
    auto mismatchGeometry = mismatchedLegacyMask->GetGeometry()->Clone();
    auto mismatchOrigin = mismatchGeometry->GetOrigin();
    mismatchOrigin[0] += 1.0;
    mismatchGeometry->SetOrigin(mismatchOrigin);
    mismatchedLegacyMask->SetGeometry(mismatchGeometry);

    auto mismatchedNode = mitk::DataNode::New();
    mismatchedNode->SetData(mismatchedLegacyMask);
    mismatchedNode->SetBoolProperty("binary", true);
    GraphcutSegmentationUtils::SeedSelection mismatchedBackgroundSeed;
    Require(GraphcutSegmentationUtils::CreateSeedSelection(mismatchedNode,
                                                           mitk::MultiLabelSegmentation::UNLABELED_VALUE,
                                                           mismatchedBackgroundSeed,
                                                           error),
            error);
    Require(!GraphcutSegmentationUtils::ValidateGraphCutInputs(referenceImage, foregroundSeed, mismatchedBackgroundSeed, error),
            "A seed image with a different origin must be rejected.");

    auto spacingMismatchedLegacyMask = CreateLegacyMask(referenceImage);
    auto spacingMismatchGeometry = spacingMismatchedLegacyMask->GetGeometry()->Clone();
    auto mismatchSpacing = spacingMismatchGeometry->GetSpacing();
    mismatchSpacing[1] *= 1.5;
    spacingMismatchGeometry->SetSpacing(mismatchSpacing);
    spacingMismatchedLegacyMask->SetGeometry(spacingMismatchGeometry);

    auto spacingMismatchedNode = mitk::DataNode::New();
    spacingMismatchedNode->SetData(spacingMismatchedLegacyMask);
    spacingMismatchedNode->SetBoolProperty("binary", true);
    GraphcutSegmentationUtils::SeedSelection spacingMismatchedBackgroundSeed;
    Require(GraphcutSegmentationUtils::CreateSeedSelection(spacingMismatchedNode,
                                                           mitk::MultiLabelSegmentation::UNLABELED_VALUE,
                                                           spacingMismatchedBackgroundSeed,
                                                           error),
            error);
    Require(!GraphcutSegmentationUtils::ValidateGraphCutInputs(referenceImage, foregroundSeed, spacingMismatchedBackgroundSeed, error),
            "A seed image with different spacing must be rejected.");

    using InputImageType = itk::Image<short, 3>;
    using MaskImageType = itk::Image<unsigned char, 3>;
    using OutputImageType = itk::Image<unsigned char, 3>;
    InputImageType::Pointer inputImage;
    MaskImageType::Pointer foregroundMask;
    MaskImageType::Pointer backgroundMask;
    mitk::CastToItkImage(referenceImage, inputImage);
    mitk::CastToItkImage(foregroundSeed.image, foregroundMask);
    mitk::CastToItkImage(backgroundSeed.image, backgroundMask);

    itk::Index<3> foregroundIndex = { { 1, 1, 1 } };
    itk::Index<3> backgroundIndex = { { 5, 5, 5 } };
    Require(foregroundMask->GetPixel(foregroundIndex) == 1 && foregroundMask->GetPixel(backgroundIndex) == 0,
            "The selected foreground label must become a binary 8-bit seed mask.");
    Require(backgroundMask->GetPixel(foregroundIndex) == 0 && backgroundMask->GetPixel(backgroundIndex) == 1,
            "The selected background label must become a binary 8-bit seed mask.");

    using FilterType = GraphCut::FilterType<InputImageType, MaskImageType, MaskImageType, OutputImageType>;
    auto filter = FilterType::New();
    filter->SetInputImage(inputImage);
    filter->SetForegroundImage(foregroundMask);
    filter->SetBackgroundImage(backgroundMask);
    filter->SetSigma(50.0);
    filter->SetForegroundPixelValue(1);
    filter->SetBoundaryDirectionTypeToNoDirection();
    filter->Update();

    auto graphCutResult = filter->GetOutput();
    Require(graphCutResult->GetPixel(foregroundIndex) == 1, "GraphCut must preserve the foreground seed.");
    Require(graphCutResult->GetPixel(backgroundIndex) == 0, "GraphCut must preserve the background seed.");

    auto resultImage = mitk::GrabItkImageMemory(graphCutResult, nullptr, nullptr, false);
    auto dataStorage = mitk::StandaloneDataStorage::New();
    auto referenceNode = mitk::DataNode::New();
    referenceNode->SetData(referenceImage);
    referenceNode->SetName("reference image");
    dataStorage->Add(referenceNode);

    auto resultNode = GraphcutSegmentationUtils::CreateResultSegmentationNode(
      resultImage, referenceNode, dataStorage, "GraphCut segmentation");
    Require(resultNode.IsNotNull(), "GraphCut output must be converted to a data node.");
    mitk::DataStorage::Pointer dataStorageBase = dataStorage.GetPointer();
    dataStorageBase->Add(resultNode, referenceNode);
    Require(dataStorageBase->GetSources(resultNode)->Size() == 1,
            "The GraphCut segmentation must be linked to its source image.");
    auto* resultSegmentation = dynamic_cast<mitk::MultiLabelSegmentation*>(resultNode->GetData());
    Require(resultSegmentation != nullptr, "GraphCut output must use MITK's MultiLabelSegmentation data type.");
    Require(resultSegmentation->GetTotalNumberOfLabels() == 1, "The GraphCut result must contain one foreground label.");
    Require(resultSegmentation->GetLabel(resultSegmentation->GetAllLabelValues().front())->GetName() == "GraphCut foreground",
            "The GraphCut output label must be named consistently.");

    bool isLegacyBinary = false;
    Require(!resultNode->GetBoolProperty("binary", isLegacyBinary) || !isLegacyBinary,
            "Modern GraphCut output must not be marked as a legacy binary image.");

    mitk::Image::Pointer meshingMask;
    Require(Voxel2MeshSegmentationUtils::CreateMeshingImage(resultSegmentation, meshingMask, error), error);
    Require(meshingMask.IsNotNull(), "Voxel-to-Mesh must extract a usable image mask from GraphCut's modern segmentation output.");
    MaskImageType::Pointer meshingMaskItk;
    mitk::CastToItkImage(meshingMask, meshingMaskItk);
    Require(meshingMaskItk->GetPixel(foregroundIndex) == 1 && meshingMaskItk->GetPixel(backgroundIndex) == 0,
            "The Voxel-to-Mesh bridge must preserve the GraphCut foreground label as a binary mask.");

    auto surfaceFilter = mitk::GraphcutSegmentationToSurfaceFilter::New();
    surfaceFilter->SetUseMedian(false);
    surfaceFilter->SetUseGaussianSmoothing(false);
    surfaceFilter->SetSmooth(false);
    surfaceFilter->SetThreshold(127.5);
    surfaceFilter->SetInput(meshingMask);
    surfaceFilter->Update();
    auto surface = surfaceFilter->GetOutput();
    Require(surface != nullptr && surface->GetVtkPolyData() != nullptr
              && surface->GetVtkPolyData()->GetNumberOfPoints() > 0,
            "Voxel-to-Mesh must generate a non-empty surface from a modern GraphCut segmentation.");

    mitk::Image::Pointer legacyMeshingImage;
    Require(Voxel2MeshSegmentationUtils::CreateMeshingImage(mismatchedLegacyMask, legacyMeshingImage, error), error);
    Require(legacyMeshingImage.GetPointer() == mismatchedLegacyMask.GetPointer(),
            "Voxel-to-Mesh must preserve legacy image-mask inputs.");

    std::cout << "GraphCut modernization regression test passed." << std::endl;
    return EXIT_SUCCESS;
  }
  catch (const std::exception& exception)
  {
    std::cerr << "GraphCut modernization regression test failed: " << exception.what() << std::endl;
  }
  catch (...)
  {
    std::cerr << "GraphCut modernization regression test failed with an unknown exception." << std::endl;
  }

  return EXIT_FAILURE;
}
