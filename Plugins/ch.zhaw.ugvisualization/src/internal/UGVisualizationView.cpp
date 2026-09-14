/*===================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center,
Division of Medical and Biological Informatics.
All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt or http://www.mitk.org for details.

===================================================================*/

// Blueberry
#include <berryISelectionService.h>
#include <berryIWorkbenchWindow.h>

// Qmitk
#include "UGVisualizationView.h"

#include <mitkGridRepresentationProperty.h>
#include <mitkGridVolumeMapperProperty.h>
#include <mitkVtkScalarModeProperty.h>
#include <mitkPropertyObserver.h>
#include <mitkUnstructuredGridVtkMapper3D.h>
#include <mitkVtkGLMapperWrapper.h>
#include <mitkUnstructuredGridMapper2D.h>

#include <QmitkUGCombinedRepresentationPropertyWidget.h>
#include <QmitkBoolPropertyWidget.h>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QWidgetAction>

#include <WorkbenchUtils.h>
#include <vtkDataArray.h>
#include <vtkDataSetAttributes.h>
#include <vtkCellData.h>
#include <vtkMapper.h>
#include <vtkPointData.h>
#include <vtkUnstructuredGrid.h>

#include <cmath>

class UGVisVolumeObserver : public mitk::PropertyView {
public:
    UGVisVolumeObserver(mitk::BoolProperty *property, UGVisualizationView *view)
              : PropertyView(property)
              , m_View(view)
              , m_BoolProperty(property)
    {
    }

protected:
    virtual void PropertyChanged() override {
        m_View->m_VolumeMode = m_BoolProperty->GetValue();
    }

    virtual void PropertyRemoved() override {
        m_View->m_VolumeMode = false;
        m_Property = 0;
        m_BoolProperty = 0;
    }

    UGVisualizationView *m_View;
    mitk::BoolProperty *m_BoolProperty;
};

const std::string UGVisualizationView::VIEW_ID = "ch.zhaw.ugvisualization";

UGVisualizationView::UGVisualizationView()
          : m_FirstVolumeRepId(-1)
          , m_VolumeModeObserver(0)
{
}

UGVisualizationView::~UGVisualizationView() {
    delete m_VolumeModeObserver;
}


void UGVisualizationView::CreateQtPartControl(QWidget *parent) {
    m_Controls.setupUi(parent);

    m_Controls.m_TransferFunctionWidget->ShowScalarOpacityFunction(false);
    m_Controls.m_TransferFunctionWidget->ShowColorFunction(true);
    m_Controls.m_TransferFunctionWidget->ShowGradientOpacityFunction(false);
    m_Controls.m_TransferFunctionWidget->SetScalarOpacityFunctionEnabled(false);
    m_Controls.m_TransferFunctionWidget->SetGradientOpacityFunctionEnabled(false);

    m_Controls.m_TransferFunctionWidget->SetScalarLabel("Scalar value");

    this->UpdateGUI();

    CreateConnections();
}

void UGVisualizationView::SetFocus() {
}

void UGVisualizationView::CreateConnections() {
    connect(m_Controls.m_RepresentationComboBox, SIGNAL(activated(int)), this, SLOT(UpdateRenderWindow()));
    connect(m_Controls.renderingCheckbox, SIGNAL(clicked(bool)), this, SLOT(RenderingCheckboxClicked(bool)));
    connect(m_Controls.scalarModeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(ScalarModeSelectionChanged(int)));
    connect(m_Controls.fieldDataComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(FieldDataSelectionChanged(int)));
}

void UGVisualizationView::UpdateRenderWindow() {
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void UGVisualizationView::UpdateGUI() {
    ResetGUI();

    auto nodes = this->GetDataManagerSelection();
    if(!nodes.empty()){
        auto node = nodes.front();
        if(node){
            if(node->GetData()){
                auto ugrid =  dynamic_cast<mitk::UnstructuredGrid *>(node->GetData());
                if(ugrid){
                    SelectUG(ugrid, node);
                    return;
                }
            }
        }
    }

    m_Controls.m_SelectedLabel->setVisible(false);
    m_Controls.m_ErrorLabel->setVisible(true);
    m_Controls.renderingCheckbox->setChecked(false);
    m_Controls.renderingCheckbox->setEnabled(false);
    m_Controls.m_ContainerWidget->setEnabled(false);
}
void UGVisualizationView::ResetGUI(){
    m_SelectedNode = nullptr;
    m_Controls.fieldDataComboBox->clear();
    m_Controls.warningLabel->setVisible(false);
    m_Controls.m_TransferFunctionWidget->setVisible(false);
}

void UGVisualizationView::EnsureRenderingProperties(mitk::DataNode::Pointer node) {
    if(node.IsNull()){
        return;
    }

    // Unstructured grids are not handled by MITK's core object factory, so
    // attaching a mapper alone does not create its representation properties.
    // Without these properties, the combined representation control only
    // exposes surface modes and volume rendering cannot be selected.
    mitk::UnstructuredGridVtkMapper3D::SetDefaultProperties(node, nullptr, false);
}

void UGVisualizationView::SelectUG(mitk::UnstructuredGrid::Pointer _ugrid, mitk::DataNode::Pointer _node) {
    m_SelectedNode = _node;
    bool has3DMapper = _node->GetMapper(mitk::BaseRenderer::Standard3D);

    if(has3DMapper){
        EnsureRenderingProperties(_node);
    }

    // update gui components
    m_Controls.m_SelectedLabel->setText(QString("Selected UG: ") + _node->GetName().c_str());
    m_Controls.m_SelectedLabel->setVisible(true);
    m_Controls.m_ErrorLabel->setVisible(false);
    m_Controls.renderingCheckbox->setEnabled(true);
    m_Controls.renderingCheckbox->setChecked(has3DMapper);
    m_Controls.m_ContainerWidget->setEnabled(has3DMapper);
    UpdateFieldDataComboBoxes(_ugrid);

    if(has3DMapper){
        if(m_Controls.fieldDataComboBox->count() == 0){
            m_Controls.warningLabel->setVisible(true);
        } else {
            m_Controls.m_TransferFunctionWidget->setVisible(true);
        }

        m_VolumeMode = false;
        _node->GetBoolProperty("volumerendering", m_VolumeMode);

        m_Controls.m_TransferFunctionWidget->SetDataNode(_node);

        mitk::GridRepresentationProperty *gridRepProp = 0;
        mitk::GridVolumeMapperProperty *gridVolumeProp = 0;
        mitk::BoolProperty *volumeProp = 0;
        _node->GetProperty(gridRepProp, "grid representation");
        _node->GetProperty(gridVolumeProp, "volumerendering.mapper");
        _node->GetProperty(volumeProp, "volumerendering");
        m_Controls.m_RepresentationComboBox->SetProperty(gridRepProp, gridVolumeProp, volumeProp);

        if (m_VolumeModeObserver) {
            delete m_VolumeModeObserver;
            m_VolumeModeObserver = 0;
        }

        if (volumeProp) {
            m_VolumeModeObserver = new UGVisVolumeObserver(volumeProp, this);
        }
    }
}

void UGVisualizationView::OnSelectionChanged(berry::IWorkbenchPart::Pointer,
                                              const QList<mitk::DataNode::Pointer>&) {
    UpdateGUI();
}

void UGVisualizationView::RenderingCheckboxClicked(bool) {
    if(m_SelectedNode.IsNull()){
        return;
    }

    bool hasMapper = m_SelectedNode->GetMapper(mitk::BaseRenderer::Standard3D);
    bool isChecked = m_Controls.renderingCheckbox->isChecked();
    if(isChecked && !hasMapper){
        m_SelectedNode->SetMapper(mitk::BaseRenderer::Standard2D, mitk::VtkGLMapperWrapper::New(mitk::UnstructuredGridMapper2D::New().GetPointer()));
        m_SelectedNode->SetMapper(mitk::BaseRenderer::Standard3D, mitk::UnstructuredGridVtkMapper3D::New());

        EnsureRenderingProperties(m_SelectedNode);
        m_SelectedNode->SetProperty(
            "grid representation", mitk::GridRepresentationProperty::New(mitk::GridRepresentationProperty::SURFACE));

        auto renderer = mitk::BaseRenderer::GetInstance(mitk::BaseRenderer::GetRenderWindowByName("stdmulti.widget4"));
        m_SelectedNode->SetProperty("outline polygons", mitk::BoolProperty::New(true));
        m_SelectedNode->AddProperty("material.specularCoefficient", mitk::FloatProperty::New(0.0), renderer, true);
    } else if(!isChecked && hasMapper){
        m_SelectedNode->SetMapper(mitk::BaseRenderer::Standard2D, nullptr);
        m_SelectedNode->SetMapper(mitk::BaseRenderer::Standard3D, nullptr);
    }
    UpdateGUI();

    if(isChecked && IsRenderable(m_SelectedNode)){
        FieldDataSelectionChanged(m_Controls.fieldDataComboBox->currentIndex());
    } else {
        UpdateRenderWindow();
    }
}

void UGVisualizationView::ScalarModeSelectionChanged(int) {
    if(m_SelectedNode.IsNull()){
        return;
    }

    auto ugrid = dynamic_cast<mitk::UnstructuredGrid *>(m_SelectedNode->GetData());
    if(!ugrid){
        return;
    }

    UpdateFieldDataComboBoxes(ugrid);
    FieldDataSelectionChanged(m_Controls.fieldDataComboBox->currentIndex());
}

void UGVisualizationView::UpdateFieldDataComboBoxes(mitk::UnstructuredGrid::Pointer _ugrid) {
    switch (m_Controls.scalarModeComboBox->currentIndex()){
        case 0:
            SetFieldDataComboBoxEntries(_ugrid->GetVtkUnstructuredGrid()->GetPointData());
            break;
        case 1:
            SetFieldDataComboBoxEntries(_ugrid->GetVtkUnstructuredGrid()->GetCellData());
            break;
    }
}

void UGVisualizationView::SetFieldDataComboBoxEntries(vtkFieldData *_data) {
    QSignalBlocker blockSignals(m_Controls.fieldDataComboBox);
    const auto previouslySelectedName = m_Controls.fieldDataComboBox->currentText();
    m_Controls.fieldDataComboBox->clear();

    if(!_data){
        return;
    }

    vtkFieldData::Iterator it(_data);
    vtkDataArray *data;
    for(data = it.Begin(); !it.End(); data=it.Next()){
        if(data && data->GetNumberOfComponents() == 1 && data->GetName()){
            auto name = data->GetName();
            m_Controls.fieldDataComboBox->addItem(name);
        }
    }

    const auto previousIndex = m_Controls.fieldDataComboBox->findText(previouslySelectedName);
    m_Controls.fieldDataComboBox->setCurrentIndex(previousIndex >= 0 ? previousIndex : 0);
}

void UGVisualizationView::FieldDataSelectionChanged(int) {
    auto name = m_Controls.fieldDataComboBox->currentText();
    if(!(name.isNull()) && !(name.isEmpty())){
        if(IsRenderable(m_SelectedNode)){
            ActivateFieldData(m_SelectedNode, name);
        }
    }
    UpdateRenderWindow();
}

bool UGVisualizationView::IsRenderable(mitk::DataNode::Pointer _node) {
    if(_node){
        bool hasMapper = _node->GetMapper(mitk::BaseRenderer::Standard3D);
        bool isChecked = m_Controls.renderingCheckbox->isChecked();
        return hasMapper && isChecked;
    }
    return false;
}

void UGVisualizationView::ActivateFieldData(mitk::DataNode::Pointer _node, QString _name) {
    if(_node.IsNull()){
        return;
    }

    auto ugrid = dynamic_cast<mitk::UnstructuredGrid *>(_node->GetData());
    if(!ugrid){
        return;
    }

    const auto name = _name.toStdString();

    vtkDataSetAttributes *fieldData = nullptr;
    mitk::VtkScalarModeProperty::Pointer scalarMode = mitk::VtkScalarModeProperty::New();
    bool usePointData = false;
    switch(m_Controls.scalarModeComboBox->currentIndex()){
        case 0:
            fieldData = ugrid->GetVtkUnstructuredGrid()->GetPointData();
            scalarMode->SetScalarModeToPointData();
            usePointData = true;
            break;
        case 1:
            fieldData = ugrid->GetVtkUnstructuredGrid()->GetCellData();
            scalarMode->SetScalarModeToCellData();
            break;
        default:
            QMessageBox::warning(NULL, "Error", "Invalid scalar mode selection.");
            return;
    }

    auto *data = fieldData ? fieldData->GetArray(name.c_str()) : nullptr;
    if(!data){
        QMessageBox::warning(NULL, "Error", "The selected scalar array is no longer available on this mesh.");
        return;
    }

    const double *range = data->GetRange();
    if(!range || !std::isfinite(range[0]) || !std::isfinite(range[1])){
        QMessageBox::warning(NULL, "Error", "The selected scalar array has no finite value range.");
        return;
    }

    // The section mapper consumes VTK's active scalar array, while the 3D
    // mapper can use it through the normal point/cell scalar modes. Keeping
    // both on the same active array prevents a 2D view from silently falling
    // back to an unrelated cell array.
    if(fieldData->SetActiveScalars(name.c_str()) < 0){
        QMessageBox::warning(NULL, "Error", "The selected array cannot be used as active scalar data.");
        return;
    }

    _node->SetProperty("scalar mode", scalarMode);
    _node->SetProperty("scalar visibility", mitk::BoolProperty::New(true));
    _node->SetProperty("TransferFunction", WorkbenchUtils::createColorTransferFunction(range[0], range[1]));
    ugrid->GetVtkUnstructuredGrid()->Modified();
    ugrid->Modified();

    // Apply the same association immediately when an actor already exists.
    // The node properties above remain the source of truth for subsequently
    // created 3D actors and all 2D renderers.
    vtkActor *actor = WorkbenchUtils::getVtk3dActor(_node);
    if(actor && actor->GetMapper()){
        if(usePointData){
            actor->GetMapper()->SetScalarModeToUsePointData();
        } else {
            actor->GetMapper()->SetScalarModeToUseCellData();
        }
        actor->GetMapper()->SetScalarVisibility(true);
        actor->GetMapper()->SelectColorArray(name.c_str());
        actor->GetMapper()->Modified();
    }
}
