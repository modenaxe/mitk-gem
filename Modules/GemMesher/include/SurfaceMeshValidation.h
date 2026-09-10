/**
 *  MITK-GEM: Mesh validation utilities
 *
 *  Copyright (c) 2026
 *
 *  Licensed under GNU General Public License 3.0 or later.
 */

#pragma once

#include <GemMesherExports.h>

#include <string>

class vtkPolyData;

namespace gem
{
  /**
   * Validates the topological preconditions shared by all volume-meshing
   * backends. The surface must be a closed two-manifold with polygonal faces.
   */
  GemMesher_EXPORT bool ValidateSurfaceForVolumeMeshing(vtkPolyData* surface, std::string& error);
}
