/**
 *  MITK-GEM: Graphcut Plugin
 *
 *  Copyright (c) 2026
 *
 *  Licensed under GNU General Public License 3.0 or later.
 */

#include "GraphcutSegmentationUtils.h"

#include <mitkBaseGeometry.h>
#include <mitkDataStorage.h>
#include <mitkException.h>
#include <mitkLabelSetImageConverter.h>
#include <mitkLabelSetImageHelper.h>
#include <mitkNodePredicateGeometry.h>

#include <exception>
#include <sstream>

namespace
{
  bool ValidateSingleVolumeImage(const mitk::Image* image, const char* role, std::string& error)
  {
    if (image == nullptr || !image->IsInitialized())
    {
      error = std::string("The ") + role + " image is not initialized.";
      return false;
    }

    if (image->GetDimension() != 3 || image->GetTimeSteps() != 1)
    {
      error = std::string("The ") + role + " image must be a single 3D volume. GraphCut3D does not support time-resolved or non-3D inputs.";
      return false;
    }

    const auto* dimensions = image->GetDimensions();
    for (unsigned int dimension = 0; dimension < 3; ++dimension)
    {
      if (dimensions[dimension] == 0)
      {
        error = std::string("The ") + role + " image has an empty dimension.";
        return false;
      }
    }

    if (image->GetGeometry() == nullptr)
    {
      error = std::string("The ") + role + " image has no spatial geometry.";
      return false;
    }

    return true;
  }

  bool HaveMatchingGeometry(const mitk::Image* reference,
                            const mitk::Image* candidate,
                            const char* candidateRole,
                            std::string& error)
  {
    const auto* referenceDimensions = reference->GetDimensions();
    const auto* candidateDimensions = candidate->GetDimensions();
    for (unsigned int dimension = 0; dimension < 3; ++dimension)
    {
      if (referenceDimensions[dimension] != candidateDimensions[dimension])
      {
        std::ostringstream stream;
        stream << "The " << candidateRole << " image has a different size in dimension " << dimension
               << ". Resample it to the greyscale image before running GraphCut3D.";
        error = stream.str();
        return false;
      }
    }

    if (!mitk::Equal(*reference->GetGeometry(),
                     *candidate->GetGeometry(),
                     mitk::NODE_PREDICATE_GEOMETRY_DEFAULT_CHECK_COORDINATE_PRECISION,
                     mitk::NODE_PREDICATE_GEOMETRY_DEFAULT_CHECK_DIRECTION_PRECISION))
    {
      error = std::string("The ") + candidateRole
        + " image does not share the greyscale image geometry (spacing, origin, or orientation). Resample it before running GraphCut3D.";
      return false;
    }

    return true;
  }
}

bool GraphcutSegmentationUtils::CreateSeedSelection(mitk::DataNode* node,
                                                      LabelValueType labelValue,
                                                      SeedSelection& selection,
                                                      std::string& error)
{
  selection = SeedSelection{};
  error.clear();

  if (node == nullptr)
  {
    error = "No seed image has been selected.";
    return false;
  }

  selection.node = node;

  auto* multiLabelSegmentation = dynamic_cast<mitk::MultiLabelSegmentation*>(node->GetData());
  if (multiLabelSegmentation != nullptr)
  {
    selection.isMultiLabelSegmentation = true;
    selection.labelValue = labelValue;

    if (!multiLabelSegmentation->ExistLabel(labelValue))
    {
      error = "Choose a valid painted label from the selected segmentation.";
      return false;
    }

    if (multiLabelSegmentation->IsEmpty(labelValue))
    {
      error = "The selected segmentation label contains no painted voxels.";
      return false;
    }

    try
    {
      // MAXFLOW consumes a binary image. CreateLabelMask deliberately maps the
      // selected label to 1 and every other label to 0.
      selection.image = mitk::CreateLabelMask(multiLabelSegmentation, labelValue, true);
    }
    catch (const mitk::Exception& exception)
    {
      error = std::string("Could not create a seed mask from the selected segmentation: ") + exception.GetDescription();
      return false;
    }
    catch (const std::exception& exception)
    {
      error = std::string("Could not create a seed mask from the selected segmentation: ") + exception.what();
      return false;
    }
    catch (...)
    {
      error = "Could not create a seed mask from the selected segmentation due to an unknown error.";
      return false;
    }

    if (selection.image.IsNull())
    {
      error = "Could not create a seed mask from the selected segmentation.";
      return false;
    }

    return true;
  }

  auto* legacyMask = dynamic_cast<mitk::Image*>(node->GetData());
  bool isBinary = false;
  if (legacyMask == nullptr || !node->GetBoolProperty("binary", isBinary) || !isBinary)
  {
    error = "Choose either a modern multi-label segmentation or a legacy binary mask image.";
    return false;
  }

  selection.image = legacyMask;
  return true;
}

bool GraphcutSegmentationUtils::ValidateGraphCutInputs(const mitk::Image* greyscaleImage,
                                                         const SeedSelection& foreground,
                                                         const SeedSelection& background,
                                                         std::string& error)
{
  error.clear();

  if (!ValidateSingleVolumeImage(greyscaleImage, "greyscale", error)
      || !ValidateSingleVolumeImage(foreground.image, "foreground seed", error)
      || !ValidateSingleVolumeImage(background.image, "background seed", error))
  {
    return false;
  }

  if (foreground.node == background.node)
  {
    if (!foreground.isMultiLabelSegmentation || !background.isMultiLabelSegmentation
        || foreground.labelValue == background.labelValue)
    {
      error = "Foreground and background must use different seed masks. A single modern segmentation is allowed only when different labels are selected.";
      return false;
    }
  }

  return HaveMatchingGeometry(greyscaleImage, foreground.image, "foreground seed", error)
    && HaveMatchingGeometry(greyscaleImage, background.image, "background seed", error);
}

mitk::DataNode::Pointer GraphcutSegmentationUtils::CreateResultSegmentationNode(const mitk::Image* resultImage,
                                                                                  const mitk::DataNode* referenceNode,
                                                                                  const mitk::DataStorage* dataStorage,
                                                                                  const std::string& nodeName)
{
  if (resultImage == nullptr)
  {
    return nullptr;
  }

  auto resultNode = mitk::LabelSetImageHelper::CreateNewSegmentationNode(referenceNode, resultImage, nodeName, dataStorage);
  auto* resultSegmentation = resultNode.IsNotNull()
    ? dynamic_cast<mitk::MultiLabelSegmentation*>(resultNode->GetData())
    : nullptr;

  if (resultSegmentation == nullptr)
  {
    return nullptr;
  }

  resultSegmentation->InitializeByLabeledImage(resultImage);

  const auto labelValues = resultSegmentation->GetAllLabelValues();
  if (!labelValues.empty())
  {
    mitk::Color foregroundColor;
    foregroundColor.SetRed(1.0f);
    foregroundColor.SetGreen(0.0f);
    foregroundColor.SetBlue(0.0f);
    resultSegmentation->RenameLabel(labelValues.front(), "GraphCut foreground", foregroundColor);
  }

  resultNode->SetOpacity(0.5f);
  resultNode->SetVisibility(true);
  resultNode->SetBoolProperty("volumerendering", true);
  resultNode->SetIntProperty("layer", 1);

  return resultNode;
}
