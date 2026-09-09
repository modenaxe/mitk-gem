/**
 *  MITK-GEM: Graphcut Plugin
 *
 *  Copyright (c) 2026
 *
 *  Licensed under GNU General Public License 3.0 or later.
 */

#ifndef GraphcutSegmentationUtils_h
#define GraphcutSegmentationUtils_h

#include <mitkDataNode.h>
#include <mitkDataStorage.h>
#include <mitkImage.h>
#include <mitkLabelSetImage.h>

#include <string>

namespace GraphcutSegmentationUtils
{
  using LabelValueType = mitk::MultiLabelSegmentation::LabelValueType;

  /**
   * A seed image ready to be cast to the unsigned-char ITK mask consumed by
   * GraphCut. For a modern segmentation, image is a temporary binary mask for
   * the selected label. For a legacy binary image, it is the original image.
   */
  struct SeedSelection
  {
    mitk::DataNode* node = nullptr;
    mitk::Image::Pointer image;
    LabelValueType labelValue = mitk::MultiLabelSegmentation::UNLABELED_VALUE;
    bool isMultiLabelSegmentation = false;
  };

  /**
   * Resolve a selected seed node. Modern multi-label segmentations require a
   * label value and are converted to a binary mask. Legacy nodes must retain
   * MITK's binary node property and are accepted unchanged.
   */
  bool CreateSeedSelection(mitk::DataNode* node,
                           LabelValueType labelValue,
                           SeedSelection& selection,
                           std::string& error);

  /**
   * Validate GraphCut's single-volume input contract, seed-pair relationship,
   * dimensions, and complete spatial geometry.
   */
  bool ValidateGraphCutInputs(const mitk::Image* greyscaleImage,
                              const SeedSelection& foreground,
                              const SeedSelection& background,
                              std::string& error);

  /**
   * Convert a GraphCut result image to the modern MITK segmentation data type.
   * The caller owns adding the returned node to the data storage and linking it
   * to the supplied reference node.
   */
  mitk::DataNode::Pointer CreateResultSegmentationNode(const mitk::Image* resultImage,
                                                        const mitk::DataNode* referenceNode,
                                                        const mitk::DataStorage* dataStorage,
                                                        const std::string& nodeName);
}

#endif // GraphcutSegmentationUtils_h
