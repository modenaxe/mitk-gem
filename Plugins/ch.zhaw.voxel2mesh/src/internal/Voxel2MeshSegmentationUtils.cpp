/**
 *  MITK-GEM: Voxel-to-Mesh Plugin
 *
 *  Copyright (c) 2026
 *
 *  Licensed under GNU General Public License 3.0 or later.
 */

#include "Voxel2MeshSegmentationUtils.h"

#include <mitkException.h>
#include <mitkLabelSetImage.h>
#include <mitkLabelSetImageConverter.h>
#include <mitkLabelSetImageHelper.h>

#include <exception>

namespace
{
  mitk::MultiLabelSegmentation::LabelValueType FindPaintedLabel(const mitk::MultiLabelSegmentation* segmentation)
  {
    const auto activeLabel = segmentation->GetActiveLabel();
    if (activeLabel != nullptr
        && activeLabel->GetValue() != mitk::MultiLabelSegmentation::UNLABELED_VALUE
        && !segmentation->IsEmpty(activeLabel->GetValue()))
    {
      return activeLabel->GetValue();
    }

    for (const auto labelValue : segmentation->GetAllLabelValues())
    {
      if (labelValue != mitk::MultiLabelSegmentation::UNLABELED_VALUE && !segmentation->IsEmpty(labelValue))
      {
        return labelValue;
      }
    }

    return mitk::MultiLabelSegmentation::UNLABELED_VALUE;
  }
}

bool Voxel2MeshSegmentationUtils::CreateMeshingImage(mitk::BaseData* data,
                                                       mitk::Image::Pointer& image,
                                                       std::string& error)
{
  image = nullptr;
  error.clear();

  if (data == nullptr)
  {
    error = "No segmentation data has been selected.";
    return false;
  }

  auto* segmentation = dynamic_cast<mitk::MultiLabelSegmentation*>(data);
  if (segmentation != nullptr)
  {
    const auto labelValue = FindPaintedLabel(segmentation);
    if (labelValue == mitk::MultiLabelSegmentation::UNLABELED_VALUE)
    {
      error = "The selected segmentation contains no painted labels to convert into a surface.";
      return false;
    }

    try
    {
      image = mitk::CreateLabelMask(segmentation, labelValue, true);
    }
    catch (const mitk::Exception& exception)
    {
      error = std::string("Could not extract the selected segmentation label: ") + exception.GetDescription();
      return false;
    }
    catch (const std::exception& exception)
    {
      error = std::string("Could not extract the selected segmentation label: ") + exception.what();
      return false;
    }
    catch (...)
    {
      error = "Could not extract the selected segmentation label due to an unknown error.";
      return false;
    }

    if (image.IsNull())
    {
      error = "Could not extract the selected segmentation label.";
      return false;
    }

    return true;
  }

  auto* legacyImage = dynamic_cast<mitk::Image*>(data);
  if (legacyImage == nullptr)
  {
    error = "Select either a legacy image mask or a modern multi-label segmentation.";
    return false;
  }

  if (!legacyImage->IsInitialized())
  {
    error = "The selected image mask is not initialized.";
    return false;
  }

  image = legacyImage;
  return true;
}
