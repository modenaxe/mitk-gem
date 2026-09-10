/**
 *  MITK-GEM: Mesh validation utilities
 *
 *  Copyright (c) 2026
 *
 *  Licensed under GNU General Public License 3.0 or later.
 */

#include "SurfaceMeshValidation.h"

#include <vtkFeatureEdges.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

#include <string>

namespace
{
  vtkIdType CountTopologyEdges(vtkPolyData* surface, bool boundaryEdges)
  {
    auto edgeFilter = vtkSmartPointer<vtkFeatureEdges>::New();
    edgeFilter->SetInputData(surface);
    edgeFilter->BoundaryEdgesOff();
    edgeFilter->FeatureEdgesOff();
    edgeFilter->ManifoldEdgesOff();
    edgeFilter->NonManifoldEdgesOff();

    if (boundaryEdges)
    {
      edgeFilter->BoundaryEdgesOn();
    }
    else
    {
      edgeFilter->NonManifoldEdgesOn();
    }

    edgeFilter->Update();
    return edgeFilter->GetOutput()->GetNumberOfCells();
  }
}

bool gem::ValidateSurfaceForVolumeMeshing(vtkPolyData* surface, std::string& error)
{
  error.clear();

  if (surface == nullptr)
  {
    error = "No surface data is available for volume meshing.";
    return false;
  }

  if (surface->GetNumberOfPoints() < 3 || surface->GetNumberOfPolys() == 0)
  {
    error = "The selected surface has no usable polygonal faces for volume meshing.";
    return false;
  }

  if (surface->GetNumberOfVerts() != 0 || surface->GetNumberOfLines() != 0 || surface->GetNumberOfStrips() != 0)
  {
    error = "The selected surface contains non-polygonal cells. Convert it to a polygonal surface before volume meshing.";
    return false;
  }

  const auto boundaryEdgeCount = CountTopologyEdges(surface, true);
  if (boundaryEdgeCount != 0)
  {
    error = "The selected surface is open: " + std::to_string(boundaryEdgeCount)
      + " boundary edge(s) were found. Volume meshing requires a closed surface.";
    return false;
  }

  const auto nonManifoldEdgeCount = CountTopologyEdges(surface, false);
  if (nonManifoldEdgeCount != 0)
  {
    error = "The selected surface is non-manifold: " + std::to_string(nonManifoldEdgeCount)
      + " non-manifold edge(s) were found. Repair the surface before volume meshing.";
    return false;
  }

  if (surface->GetNumberOfPoints() < 4 || surface->GetNumberOfPolys() < 4)
  {
    error = "The selected surface has too few points or polygon faces to enclose a volume.";
    return false;
  }

  return true;
}
