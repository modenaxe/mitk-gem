/**
 *  MITK-GEM: Voxel-to-Mesh Plugin
 *
 *  Copyright (c) 2026
 *
 *  Licensed under GNU General Public License 3.0 or later.
 */

#ifndef Voxel2MeshSegmentationUtils_h
#define Voxel2MeshSegmentationUtils_h

#include <mitkBaseData.h>
#include <mitkImage.h>

#include <string>

namespace Voxel2MeshSegmentationUtils
{
  /**
   * Resolve meshable image data from a legacy image or a modern MITK
   * MultiLabelSegmentation. Modern segmentations are converted to an 8-bit
   * binary mask for their active painted label (or their first painted label
   * when no active painted label is available).
   */
  bool CreateMeshingImage(mitk::BaseData* data, mitk::Image::Pointer& image, std::string& error);
}

#endif // Voxel2MeshSegmentationUtils_h
