#include "mitkGraphcutSegmentationToSurfaceFilter.h"

#include <vtkSmartPointer.h>
#include <vtkImageConstantPad.h>
#include <vtkImageData.h>
#include <vtkImageShiftScale.h>
#include <vtkImageThreshold.h>

#include "mitkProgressBar.h"

namespace
{
  vtkSmartPointer<vtkImageData> AddExteriorBackground(vtkImageData* image)
  {
    if (image == nullptr)
    {
      return nullptr;
    }

    int extent[6];
    image->GetExtent(extent);

    auto paddingFilter = vtkSmartPointer<vtkImageConstantPad>::New();
    paddingFilter->SetInputData(image);
    paddingFilter->SetOutputWholeExtent(extent[0] - 1,
                                        extent[1] + 1,
                                        extent[2] - 1,
                                        extent[3] + 1,
                                        extent[4] - 1,
                                        extent[5] + 1);
    paddingFilter->SetConstant(0.0);
    paddingFilter->Update();

    // Detach from the filter pipeline while retaining the padded scalar data.
    auto paddedImage = vtkSmartPointer<vtkImageData>::New();
    paddedImage->ShallowCopy(paddingFilter->GetOutput());
    return paddedImage;
  }
}

mitk::GraphcutSegmentationToSurfaceFilter::GraphcutSegmentationToSurfaceFilter()
        : m_UseMedian(false),
          m_UseThresholding(true),
          m_MedianKernelSizeX(3),
          m_MedianKernelSizeY(3),
          m_MedianKernelSizeZ(3),
          m_UseGaussianSmoothing(true),
          m_GaussianStandardDeviation(2.2),
          m_GaussianRadius(0.49) {
};

void mitk::GraphcutSegmentationToSurfaceFilter::GenerateData() {
    mitk::Surface *surface = this->GetOutput();
    mitk::Image *image = (mitk::Image *) GetInput();
    mitk::Image::RegionType outputRegion = image->GetRequestedRegion();

    int tstart = outputRegion.GetIndex(3);
    int tmax = tstart + outputRegion.GetSize(3);

    ScalarType thresholdExpanded = this->m_Threshold;

    if ((tmax - tstart) > 0) {
        ProgressBar::GetInstance()->AddStepsToDo(6 * (tmax - tstart));
    }
    else {
        ProgressBar::GetInstance()->AddStepsToDo(6);
    }


    for (int t = tstart; t < tmax; ++t) {
        vtkSmartPointer <vtkImageData> vtkimage = image->GetVtkImageData(t);

        if (m_UseThresholding) {
            auto threshold = vtkImageThreshold::New();
            threshold->SetInputData(vtkimage);
            threshold->SetOutputScalarTypeToUnsignedChar();
            threshold->ThresholdByLower(0.99); // range is inclusive but always in double. Precision issue.
            threshold->SetOutValue(255);
            threshold->ReleaseDataFlagOn();
            threshold->UpdateInformation();
            threshold->Update();
            vtkimage = threshold->GetOutput();
            threshold->Delete();
        }

        if (m_UseMedian) {
            vtkImageMedian3D *median = vtkImageMedian3D::New();
            median->SetInputData(vtkimage);
            median->SetKernelSize(m_MedianKernelSizeX, m_MedianKernelSizeY, m_MedianKernelSizeZ);
            median->ReleaseDataFlagOn();
            median->UpdateInformation();
            median->Update();
            vtkimage = median->GetOutput();
            median->Delete();
        }
        ProgressBar::GetInstance()->Progress();

        if (m_UseGaussianSmoothing) {
            vtkImageGaussianSmooth *gaussian = vtkImageGaussianSmooth::New();
            gaussian->SetInputData(vtkimage);
            gaussian->SetDimensionality(3);
            gaussian->SetRadiusFactor(m_GaussianRadius);
            gaussian->SetStandardDeviation(m_GaussianStandardDeviation);
            gaussian->ReleaseDataFlagOn();
            gaussian->UpdateInformation();
            gaussian->Update();
            vtkimage = gaussian->GetOutput();
            gaussian->Delete();
        }
        ProgressBar::GetInstance()->Progress();

        // Marching cubes can only create a contour where samples exist on
        // both sides of the threshold. Add a zero-valued exterior voxel shell
        // after all mask processing so foreground touching an image boundary
        // is closed just outside the acquired volume. The input segmentation
        // itself remains unchanged.
        auto paddedImage = AddExteriorBackground(vtkimage);
        if (paddedImage == nullptr)
        {
            mitkThrow() << "Could not add an exterior background border before surface extraction.";
        }

        CreateSurface(t, paddedImage, surface, thresholdExpanded);
        ProgressBar::GetInstance()->Progress();
    }

    MITK_INFO << "Updating Time Geometry to ensure right timely displaying";
    // Fixing wrong time geometry
    TimeGeometry *surfaceTG = surface->GetTimeGeometry();
    ProportionalTimeGeometry *surfacePTG = dynamic_cast<ProportionalTimeGeometry *>(surfaceTG);
    TimeGeometry *imageTG = image->GetTimeGeometry();
    ProportionalTimeGeometry *imagePTG = dynamic_cast<ProportionalTimeGeometry *>(imageTG);
    // Requires ProportionalTimeGeometries to work. May not be available for all steps.
    assert(surfacePTG != NULL);
    assert(imagePTG != NULL);
    if ((surfacePTG != NULL) && (imagePTG != NULL)) {
        TimePointType firstTime = imagePTG->GetFirstTimePoint();
        TimePointType duration = imagePTG->GetStepDuration();
        surfacePTG->SetFirstTimePoint(firstTime);
        surfacePTG->SetStepDuration(duration);
        MITK_INFO << "First Time Point: " << firstTime << "  Duration: " << duration;
    }
};
