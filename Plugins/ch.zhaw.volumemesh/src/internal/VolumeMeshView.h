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

#pragma once

#include <berryISelectionListener.h>
#include <QmitkAbstractView.h>
#include <QFutureWatcher>

#include <mitkSurface.h>
#include <mitkUnstructuredGrid.h>

#include <memory>
#include <string>

#include "ui_VolumeMeshViewControls.h"
#include "TetgenOptionGrid.h"

namespace gem
{
  class IMesher;
}

class VolumeMeshView : public QmitkAbstractView {
    Q_OBJECT

public:
    ~VolumeMeshView();

    static const std::string VIEW_ID;

protected slots:
    void generateButtonClicked();
    void onMeshingFinished();
    void onSurfaceSelectionChanged(const mitk::DataNode* node);

protected:
    virtual void CreateQtPartControl(QWidget *parent) override;
    virtual void SetFocus() override;

private:
    struct MeshingResult
    {
        mitk::UnstructuredGrid::Pointer mesh;
        std::string error;
    };

    static MeshingResult RunMeshing(mitk::Surface::Pointer surface,
                                    std::shared_ptr<gem::IMesher> mesher);

    Ui::VolumeMeshViewControls m_Controls;
    TetgenOptionGrid m_TetgenOptionGrid;

    QFutureWatcher<MeshingResult> m_WorkerWatcher;
};
