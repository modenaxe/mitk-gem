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

#include "MesherCGAL.h"
#include "MesherTetgen.h"
#include "SurfaceMeshValidation.h"
#include "SurfaceToUnstructuredGridFilter.h"
#include "VolumeMeshView.h"
#include "WorkbenchUtils.h"
#include <berryISelectionService.h>
#include <berryIWorkbenchWindow.h>
#include <mitkGridRepresentationProperty.h>
#include <mitkException.h>
#include <mitkImage.h>
#include <mitkProgressBar.h>
#include <mitkSurface.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>
#include <QButtonGroup>
#include <QMessageBox>
#include <QtConcurrentRun>

#include <exception>
#include <string>

namespace
{
    mitk::Surface::Pointer CreateSurfaceSnapshot(mitk::Surface* surface)
    {
        if (surface == nullptr || surface->GetVtkPolyData() == nullptr)
        {
            return nullptr;
        }

        auto polyDataSnapshot = vtkSmartPointer<vtkPolyData>::New();
        polyDataSnapshot->DeepCopy(surface->GetVtkPolyData());

        auto surfaceSnapshot = mitk::Surface::New();
        surfaceSnapshot->SetVtkPolyData(polyDataSnapshot);
        return surfaceSnapshot;
    }
}

const std::string VolumeMeshView::VIEW_ID = "org.mitk.views.volumemesher";

VolumeMeshView::~VolumeMeshView() {
    // The task owns immutable input and does not capture this view. Cancelling
    // discards its result without blocking the GUI thread while a third-party
    // mesher finishes its current operation.
    m_WorkerWatcher.cancel();
}

void VolumeMeshView::SetFocus() {
    m_Controls.generateButton->setFocus();
}

void VolumeMeshView::CreateQtPartControl(QWidget *parent) {
    m_Controls.setupUi(parent);

    // Only mitk::Surface nodes may reach either volume-meshing backend. Set
    // the predicate before attaching the data storage so the initial reset is
    // already filtered as well.
    m_Controls.surfaceComboBox->SetPredicate(WorkbenchUtils::createIsSurfaceTypePredicate());
    m_Controls.surfaceComboBox->SetDataStorage(this->GetDataStorage());
    m_Controls.surfaceComboBox->SetAutoSelectNewItems(false);

    // The generated UI marks TetGen as checked, but a QButtonGroup makes the
    // intended default and exclusivity explicit instead of relying on widget
    // parentage or generated-widget initialization order.
    auto* mesherButtonGroup = new QButtonGroup(m_Controls.frame);
    mesherButtonGroup->setExclusive(true);
    mesherButtonGroup->addButton(m_Controls.radioTetgen);
    mesherButtonGroup->addButton(m_Controls.radioCGAL);
    m_Controls.radioTetgen->setChecked(true);
    m_Controls.settingsGroup->setVisible(true);
    m_Controls.settingsCGAL->setVisible(false);

    // tetgen options
    tetgenbehavior options;
    options.plc = 1;
    options.quality = 1;
    options.nobisect = 1;
    options.fixedvolume = 1;

    m_TetgenOptionGrid.setDefaultOptions(options);
    m_TetgenOptionGrid.addOption("-p", "Tetrahedralizes a piecewise linear complex (PLC).", &tetgenbehavior::plc);
    m_TetgenOptionGrid.addOption("-q", "Refines mesh (to improve mesh quality).", &tetgenbehavior::quality);
    m_TetgenOptionGrid.addOption("-Y", "Preserves the input surface mesh (does not modify it).", &tetgenbehavior::nobisect);
    m_TetgenOptionGrid.addOption("-a", "Applies a maximum tetrahedron volume constraint. Assumes uniform mesh density on the surface.", &tetgenbehavior::fixedvolume);
    m_Controls.settingsGroup->layout()->addWidget(&m_TetgenOptionGrid);

    // CGAL options
    m_Controls.settingsCGAL->hide();
    gem::MesherCGAL::SOptions optionsCGAL;
    m_Controls.spinBoxSize->setValue(optionsCGAL.fEdgeSize);
    m_Controls.spinBoxRadiusEdgeRatio->setValue(optionsCGAL.fRadiusEdgeRatio);

    // signals
    connect(m_Controls.generateButton, SIGNAL(clicked()), this, SLOT(generateButtonClicked()));
    connect(m_Controls.surfaceComboBox, &QmitkDataStorageComboBox::OnSelectionChanged,
            this, &VolumeMeshView::onSurfaceSelectionChanged);
    connect(&m_WorkerWatcher, &QFutureWatcher<MeshingResult>::finished,
            this, &VolumeMeshView::onMeshingFinished, Qt::QueuedConnection);

    onSurfaceSelectionChanged(m_Controls.surfaceComboBox->GetSelectedNode().GetPointer());
}

void VolumeMeshView::onSurfaceSelectionChanged(const mitk::DataNode* node)
{
    const auto* surface = node == nullptr ? nullptr : dynamic_cast<const mitk::Surface*>(node->GetData());
    m_Controls.generateButton->setEnabled(surface != nullptr);
}

VolumeMeshView::MeshingResult VolumeMeshView::RunMeshing(mitk::Surface::Pointer surface,
                                                          std::shared_ptr<gem::IMesher> mesher)
{
    MeshingResult result;

    try
    {
        auto meshFilter = SurfaceToUnstructuredGridFilter::New();
        meshFilter->SetInput(surface, std::move(mesher));
        meshFilter->Update();

        mitk::UnstructuredGrid::Pointer mesh = meshFilter->GetOutput();
        if (mesh.IsNull() || mesh->GetVtkUnstructuredGrid() == nullptr
            || mesh->GetVtkUnstructuredGrid()->GetNumberOfPoints() == 0
            || mesh->GetVtkUnstructuredGrid()->GetNumberOfCells() == 0)
        {
            result.error = "Volume meshing did not produce any tetrahedral elements.";
            return result;
        }

        result.mesh = mesh;
    }
    catch (const mitk::Exception& exception)
    {
        result.error = exception.GetDescription();
    }
    catch (const std::exception& exception)
    {
        result.error = exception.what();
    }
    catch (...)
    {
        result.error = "Volume meshing failed with an unknown error.";
    }

    return result;
}

void VolumeMeshView::generateButtonClicked() {
    if (m_WorkerWatcher.future().isValid() && !m_WorkerWatcher.future().isFinished())
    {
        QMessageBox::information(nullptr, "Volume meshing in progress",
                                 "Wait for the current volume-meshing task to finish.");
        return;
    }

    auto surfaceNode = m_Controls.surfaceComboBox->GetSelectedNode();

    if (surfaceNode.IsNull())
    {
        QMessageBox::warning(nullptr, "Invalid surface for volume meshing",
                             "Select a surface mesh before generating a volume mesh.");
        return;
    }

    mitk::Surface::Pointer surface = dynamic_cast<mitk::Surface*>(surfaceNode->GetData());
    if (surface.IsNull())
    {
        QMessageBox::warning(nullptr, "Invalid surface for volume meshing",
                             "The selected data node does not contain a surface mesh."
                             " Images and segmentations cannot be volume-meshed directly.");
        return;
    }

    std::string validationError;
    if (!gem::ValidateSurfaceForVolumeMeshing(surface->GetVtkPolyData(), validationError))
    {
        QMessageBox::warning(nullptr, "Invalid surface for volume meshing",
                             QString::fromStdString(validationError));
        return;
    }

    std::shared_ptr<gem::IMesher> spMesher;
    if (m_Controls.radioTetgen->isChecked())
    {
        spMesher = std::make_shared<gem::MesherTetgen>(m_TetgenOptionGrid.getOptionsFromGui());
    }
    else if (m_Controls.radioCGAL->isChecked())
    {
        gem::MesherCGAL::SOptions options;
        options.fEdgeSize = m_Controls.spinBoxSize->value();
        options.fRadiusEdgeRatio = m_Controls.spinBoxRadiusEdgeRatio->value();
        spMesher = std::make_shared<gem::MesherCGAL>(options);
    }
    else
    {
        QMessageBox::warning(nullptr, "No volume mesher selected",
                             "Select TetGen or CGAL before generating a volume mesh.");
        return;
    }

    auto surfaceSnapshot = CreateSurfaceSnapshot(surface);
    if (surfaceSnapshot.IsNull())
    {
        QMessageBox::warning(nullptr, "Invalid surface for volume meshing",
                             "The selected surface does not contain polygonal data.");
        return;
    }

    // All GUI state is captured before scheduling the task. The worker
    // receives only immutable data and never accesses this view.
    m_Controls.container->setEnabled(false);
    mitk::ProgressBar::GetInstance()->AddStepsToDo(2);
    mitk::ProgressBar::GetInstance()->Progress();

    m_WorkerWatcher.setFuture(QtConcurrent::run([surfaceSnapshot, spMesher]() {
        return RunMeshing(surfaceSnapshot, spMesher);
    }));
}

void VolumeMeshView::onMeshingFinished()
{
    const auto result = m_WorkerWatcher.result();
    std::string error = result.error;

    if (error.empty())
    {
        try
        {
            auto newNode = mitk::DataNode::New();
            newNode->SetData(result.mesh);
            newNode->SetProperty("name", mitk::StringProperty::New("tetrahedral mesh"));
            newNode->SetProperty("layer", mitk::IntProperty::New(1));
            GetDataStorage()->Add(newNode);
        }
        catch (const mitk::Exception& exception)
        {
            error = exception.GetDescription();
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
        }
        catch (...)
        {
            error = "The volume mesh was created, but could not be added to the data storage.";
        }
    }

    mitk::ProgressBar::GetInstance()->Progress();
    m_Controls.container->setEnabled(true);

    if (!error.empty())
    {
        QMessageBox::warning(nullptr, "Volume meshing failed", QString::fromStdString(error));
    }
}
