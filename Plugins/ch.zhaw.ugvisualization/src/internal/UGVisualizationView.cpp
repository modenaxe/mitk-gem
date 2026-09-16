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
#include <mitkBaseRenderer.h>
#include <mitkProperties.h>
#include <mitkPropertyList.h>
#include <mitkVtkScalarModeProperty.h>
#include <mitkPropertyObserver.h>
#include <mitkUnstructuredGridVtkMapper3D.h>
#include <mitkVtkGLMapperWrapper.h>
#include <mitkUnstructuredGridMapper2D.h>

#include <QmitkUGCombinedRepresentationPropertyWidget.h>
#include <QmitkBoolPropertyWidget.h>
#include <ctkDoubleRangeSlider.h>
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

namespace
{
constexpr int SECTION_OVERLAY_LAYER = 900;
constexpr const char *SECTION_OVERLAY_MANAGED_PROPERTY = "gem.ugvisualization.section-overlay-managed";
constexpr const char *SECTION_OVERLAY_HAD_LOCAL_LAYER_PROPERTY =
  "gem.ugvisualization.section-overlay-had-local-layer";
constexpr const char *SECTION_OVERLAY_PREVIOUS_LAYER_PROPERTY =
  "gem.ugvisualization.section-overlay-previous-layer";

bool UsesPointScalars(int scalarMode)
{
  return scalarMode == VTK_SCALAR_MODE_USE_POINT_DATA ||
         scalarMode == VTK_SCALAR_MODE_USE_POINT_FIELD_DATA;
}

bool HasDataArrays(vtkDataSetAttributes *fieldData)
{
  return fieldData != nullptr && fieldData->GetNumberOfArrays() > 0;
}
}

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
    // Material Mapping can change the active VTK array while this view is not
    // focused. Refreshing here keeps the selector and scalar legend aligned
    // with the map that is actually rendered.
    UpdateGUI();
}

void UGVisualizationView::CreateConnections() {
    connect(m_Controls.m_RepresentationComboBox, SIGNAL(activated(int)), this, SLOT(UpdateRenderWindow()));
    connect(m_Controls.renderingCheckbox, SIGNAL(clicked(bool)), this, SLOT(RenderingCheckboxClicked(bool)));
    connect(m_Controls.sectionsCheckbox, SIGNAL(clicked(bool)), this, SLOT(SectionsCheckboxClicked(bool)));
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
    m_Controls.sectionsCheckbox->setChecked(false);
    m_Controls.sectionsCheckbox->setEnabled(false);
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
    const bool has3DMapper = _node->GetMapper(mitk::BaseRenderer::Standard3D) != nullptr;
    const bool has2DMapper = _node->GetMapper(mitk::BaseRenderer::Standard2D) != nullptr;

    if(has3DMapper){
        EnsureRenderingProperties(_node);
    }

    // update gui components
    m_Controls.m_SelectedLabel->setText(QString("Selected UG: ") + _node->GetName().c_str());
    m_Controls.m_SelectedLabel->setVisible(true);
    m_Controls.m_ErrorLabel->setVisible(false);
    m_Controls.renderingCheckbox->setEnabled(true);
    m_Controls.renderingCheckbox->setChecked(has3DMapper);
    m_Controls.sectionsCheckbox->setEnabled(has3DMapper);
    m_Controls.sectionsCheckbox->setChecked(has2DMapper);
    m_Controls.m_ContainerWidget->setEnabled(has3DMapper);

    vtkDataSetAttributes *selectedFieldData = nullptr;
    bool usePointData = false;

    // Material mapping stores its values as cell data. More generally, honour
    // the node's current scalar association so that the array listed here is
    // the array rendered by the 3D mapper, rather than simply the first array
    // encountered in VTK's field-data collection.
    {
        QSignalBlocker blockSignals(m_Controls.scalarModeComboBox);
        auto* grid = _ugrid->GetVtkUnstructuredGrid();
        if(grid != nullptr){
            auto *pointData = grid->GetPointData();
            auto *cellData = grid->GetCellData();

            mitk::VtkScalarModeProperty *scalarModeProperty = nullptr;
            _node->GetProperty(scalarModeProperty, "scalar mode");
            usePointData = scalarModeProperty != nullptr &&
                           UsesPointScalars(scalarModeProperty->GetVtkScalarMode());

            // Retain a usable association for meshes imported without a
            // scalar-mode property, or for data where the saved association
            // no longer contains arrays.
            if(usePointData && !HasDataArrays(pointData) && HasDataArrays(cellData)){
                usePointData = false;
            } else if(!usePointData && !HasDataArrays(cellData) && HasDataArrays(pointData)){
                usePointData = true;
            }

            if(usePointData){
                selectedFieldData = pointData;
            } else {
                selectedFieldData = cellData;
            }
        }
        m_Controls.scalarModeComboBox->setCurrentIndex(usePointData ? 0 : 1);
    }
    UpdateFieldDataComboBoxes(_ugrid);

    if(has3DMapper){
        if(m_Controls.fieldDataComboBox->count() == 0){
            m_Controls.warningLabel->setVisible(true);
        } else {
            m_Controls.m_TransferFunctionWidget->setVisible(true);

            const auto selectedArrayName = m_Controls.fieldDataComboBox->currentText();
            auto *selectedArray = selectedFieldData == nullptr
              ? nullptr
              : selectedFieldData->GetArray(selectedArrayName.toStdString().c_str());
            UpdateTransferFunctionWidget(_node, selectedArray, selectedArrayName, usePointData);
        }

        m_VolumeMode = false;
        _node->GetBoolProperty("volumerendering", m_VolumeMode);

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

    const bool hasMapper = m_SelectedNode->GetMapper(mitk::BaseRenderer::Standard3D) != nullptr;
    const bool isChecked = m_Controls.renderingCheckbox->isChecked();
    if(isChecked && !hasMapper){
        m_SelectedNode->SetMapper(mitk::BaseRenderer::Standard3D, mitk::UnstructuredGridVtkMapper3D::New());

        EnsureRenderingProperties(m_SelectedNode);
        m_SelectedNode->SetProperty(
            "grid representation", mitk::GridRepresentationProperty::New(mitk::GridRepresentationProperty::SURFACE));

        auto renderer = mitk::BaseRenderer::GetInstance(mitk::BaseRenderer::GetRenderWindowByName("stdmulti.widget4"));
        m_SelectedNode->SetProperty("outline polygons", mitk::BoolProperty::New(false));
        m_SelectedNode->AddProperty("material.specularCoefficient", mitk::FloatProperty::New(0.0), renderer, true);
    } else if(!isChecked && hasMapper){
        SetSectionOverlayLayer(m_SelectedNode, false);
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

void UGVisualizationView::SectionsCheckboxClicked(bool checked) {
    if(m_SelectedNode.IsNull() || !m_Controls.renderingCheckbox->isChecked()){
        QSignalBlocker blockSignals(m_Controls.sectionsCheckbox);
        m_Controls.sectionsCheckbox->setChecked(false);
        return;
    }

    const bool has2DMapper = m_SelectedNode->GetMapper(mitk::BaseRenderer::Standard2D) != nullptr;
    if(checked && !has2DMapper){
        m_SelectedNode->SetMapper(
            mitk::BaseRenderer::Standard2D,
            mitk::VtkGLMapperWrapper::New(mitk::UnstructuredGridMapper2D::New().GetPointer()));
        m_SelectedNode->SetProperty("outline polygons", mitk::BoolProperty::New(true));
    } else if(!checked && has2DMapper){
        m_SelectedNode->SetMapper(mitk::BaseRenderer::Standard2D, nullptr);
        m_SelectedNode->SetProperty("outline polygons", mitk::BoolProperty::New(false));
    }

    // The legacy section mapper paints in the opaque rendering pass. Give it
    // a renderer-local layer above image slices only while sections are
    // explicitly requested; this avoids hiding medical images and restores
    // the previous layer exactly when the option is switched off.
    SetSectionOverlayLayer(m_SelectedNode, checked);

    if(checked && IsRenderable(m_SelectedNode) && m_Controls.fieldDataComboBox->count() > 0){
        FieldDataSelectionChanged(m_Controls.fieldDataComboBox->currentIndex());
    }

    m_SelectedNode->Modified();
    UpdateRenderWindow();
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
    if(_ugrid.IsNull() || _ugrid->GetVtkUnstructuredGrid() == nullptr){
        SetFieldDataComboBoxEntries(nullptr);
        return;
    }

    auto *grid = _ugrid->GetVtkUnstructuredGrid();
    switch (m_Controls.scalarModeComboBox->currentIndex()){
        case 0: {
            auto *pointData = grid->GetPointData();
            auto *activeScalars = pointData == nullptr ? nullptr : pointData->GetScalars();
            SetFieldDataComboBoxEntries(
              pointData, activeScalars != nullptr && activeScalars->GetName() != nullptr
                ? QString::fromUtf8(activeScalars->GetName())
                : QString());
            break;
        }
        case 1: {
            auto *cellData = grid->GetCellData();
            auto *activeScalars = cellData == nullptr ? nullptr : cellData->GetScalars();
            SetFieldDataComboBoxEntries(
              cellData, activeScalars != nullptr && activeScalars->GetName() != nullptr
                ? QString::fromUtf8(activeScalars->GetName())
                : QString());
            break;
        }
        default:
            SetFieldDataComboBoxEntries(nullptr);
            break;
    }
}

void UGVisualizationView::SetFieldDataComboBoxEntries(vtkFieldData *_data,
                                                       const QString &preferredArrayName) {
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

    int selectedIndex = m_Controls.fieldDataComboBox->findText(preferredArrayName);
    if(selectedIndex < 0){
        selectedIndex = m_Controls.fieldDataComboBox->findText(previouslySelectedName);
    }
    if(selectedIndex < 0 && m_Controls.fieldDataComboBox->count() > 0){
        selectedIndex = 0;
    }
    m_Controls.fieldDataComboBox->setCurrentIndex(selectedIndex);
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

    UpdateTransferFunctionWidget(_node, data, _name, usePointData);

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

void UGVisualizationView::UpdateTransferFunctionWidget(mitk::DataNode::Pointer _node,
                                                        vtkDataArray *_data,
                                                        const QString &_name,
                                                        bool pointData) {
    if(_node.IsNull() || _data == nullptr || _name.isEmpty()){
        m_Controls.m_TransferFunctionWidget->setVisible(false);
        return;
    }

    const auto *rawRange = _data->GetRange();
    if(rawRange == nullptr || !std::isfinite(rawRange[0]) || !std::isfinite(rawRange[1])){
        m_Controls.m_TransferFunctionWidget->setVisible(false);
        return;
    }

    double lower = rawRange[0];
    double upper = rawRange[1];
    if(lower == upper){
        const double padding = lower == 0.0 ? 1.0 : std::abs(lower) * 0.01;
        lower -= padding;
        upper += padding;
    }

    if(_node->GetProperty("TransferFunction") == nullptr){
        _node->SetProperty("TransferFunction", WorkbenchUtils::createColorTransferFunction(rawRange[0], rawRange[1]));
    }

    const auto association = pointData ? tr("point data") : tr("cell data");
    m_Controls.m_TransferFunctionWidget->SetScalarLabel(
      QStringLiteral("%1 (%2)").arg(_name).arg(association));
    m_Controls.m_TransferFunctionWidget->SetDataNode(_node);

    {
        QSignalBlocker blockSignals(m_Controls.m_TransferFunctionWidget->m_RangeSlider);
        m_Controls.m_TransferFunctionWidget->m_RangeSlider->setMinimum(lower);
        m_Controls.m_TransferFunctionWidget->m_RangeSlider->setMaximum(upper);
        m_Controls.m_TransferFunctionWidget->m_RangeSlider->setMinimumValue(lower);
        m_Controls.m_TransferFunctionWidget->m_RangeSlider->setMaximumValue(upper);
    }

    m_Controls.m_TransferFunctionWidget->UpdateStepSize();
    m_Controls.m_TransferFunctionWidget->UpdateRanges();
    m_Controls.m_TransferFunctionWidget->OnUpdateCanvas();
    m_Controls.m_TransferFunctionWidget->setVisible(true);
}

void UGVisualizationView::SetSectionOverlayLayer(mitk::DataNode::Pointer _node, bool enabled) {
    if(_node.IsNull()){
        return;
    }

    bool changed = false;
    const auto renderers = mitk::BaseRenderer::GetAll2DRenderWindows();
    for(const auto &entry : renderers){
        auto *renderer = entry.second;
        if(renderer == nullptr){
            continue;
        }

        auto *propertyList = _node->GetPropertyList(renderer);
        if(propertyList == nullptr){
            continue;
        }

        bool managed = false;
        propertyList->GetBoolProperty(SECTION_OVERLAY_MANAGED_PROPERTY, managed);

        if(enabled){
            if(!managed){
                int previousLayer = 0;
                const bool hadLocalLayer = propertyList->GetIntProperty("layer", previousLayer);
                propertyList->SetBoolProperty(SECTION_OVERLAY_MANAGED_PROPERTY, true);
                propertyList->SetBoolProperty(SECTION_OVERLAY_HAD_LOCAL_LAYER_PROPERTY, hadLocalLayer);
                if(hadLocalLayer){
                    propertyList->SetIntProperty(SECTION_OVERLAY_PREVIOUS_LAYER_PROPERTY, previousLayer);
                }
            }

            _node->SetIntProperty("layer", SECTION_OVERLAY_LAYER, renderer);
            changed = true;
        } else if(managed){
            bool hadLocalLayer = false;
            propertyList->GetBoolProperty(SECTION_OVERLAY_HAD_LOCAL_LAYER_PROPERTY, hadLocalLayer);
            if(hadLocalLayer){
                int previousLayer = 0;
                if(propertyList->GetIntProperty(SECTION_OVERLAY_PREVIOUS_LAYER_PROPERTY, previousLayer)){
                    _node->SetIntProperty("layer", previousLayer, renderer);
                } else {
                    propertyList->DeleteProperty("layer");
                }
            } else {
                propertyList->DeleteProperty("layer");
            }

            propertyList->DeleteProperty(SECTION_OVERLAY_MANAGED_PROPERTY);
            propertyList->DeleteProperty(SECTION_OVERLAY_HAD_LOCAL_LAYER_PROPERTY);
            propertyList->DeleteProperty(SECTION_OVERLAY_PREVIOUS_LAYER_PROPERTY);
            changed = true;
        }
    }

    if(changed){
        _node->Modified();
    }
}
