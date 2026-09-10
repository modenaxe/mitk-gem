/**
 *  MITK-GEM: Volume Mesher Plugin
 *
 *  Copyright (c) 2016, Zurich University of Applied Sciences, School of Engineering, T. Fitze, Y. Pauchard
 *  Copyright (c) 2016, ETH Zurich, Institute for Biomechanics, B. Helgason
 *  Copyright (c) 2016, University of Iceland, Mechanical Engineering and Computer Science, H. Pállson
 *
 *  Licensed under GNU General Public License 3.0 or later.
 *  Some rights reserved.
 */

#include "IMesher.h"
#include "SurfaceMeshValidation.h"
#include "SurfaceToUnstructuredGridFilter.h"
#include <mitkException.h>
#include <mitkSurface.h>
#include <mitkUnstructuredGrid.h>
#include <vtkPolyData.h>
#include <vtkUnstructuredGrid.h>

#include <string>

void SurfaceToUnstructuredGridFilter::SetInput(const mitk::Surface *_surface, std::shared_ptr <gem::IMesher> spMesher)
{
    this->ProcessObject::SetNthInput(0, const_cast<mitk::Surface *>(_surface));
    m_spMesher = spMesher;
}

const mitk::Surface *SurfaceToUnstructuredGridFilter::GetInput()
{
    // ProcessObject's generic SetInput overload remains publicly available to
    // support MITK pipelines. Do not assume that callers used the typed
    // overload above: an image or segmentation must fail preflight rather
    // than be cast to a Surface and handed to TetGen or CGAL.
    return dynamic_cast<const mitk::Surface *>(this->ProcessObject::GetInput(0));
}

void SurfaceToUnstructuredGridFilter::GenerateOutputInformation()
{
    // prevent the default implementation
}

void SurfaceToUnstructuredGridFilter::GenerateData()
{
    const auto* surface = GetInput();
    if (surface == nullptr)
    {
        mitkThrow() << "No surface has been selected for volume meshing.";
    }

    if (m_spMesher == nullptr)
    {
        mitkThrow() << "No volume mesher has been configured.";
    }

    auto* vtkSurface = surface->GetVtkPolyData();
    std::string validationError;
    if (!gem::ValidateSurfaceForVolumeMeshing(vtkSurface, validationError))
    {
        mitkThrow() << validationError;
    }

    auto vtkMesh = vtkSmartPointer<vtkUnstructuredGrid>::New();
    m_spMesher->SetInput(vtkSurface);
    m_spMesher->SetOutput(vtkMesh);
    m_spMesher->Compute();
    GetOutput()->SetVtkUnstructuredGrid(vtkMesh);
}
