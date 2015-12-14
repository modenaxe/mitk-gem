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
#include "PaddingView.h"

// Qt
#include <QMessageBox>


const std::string PaddingView::VIEW_ID = "org.mitk.views.paddingview";

void PaddingView::CreateQtPartControl(QWidget *parent) {
    // create GUI widgets from the Qt Designer's .ui file
    m_Controls.setupUi(parent);

    // setup padding buttons
    connect(m_Controls.padLeft, SIGNAL(clicked()), this, SLOT(padLeftButtonPressed()));
    connect(m_Controls.padUp, SIGNAL(clicked()), this, SLOT(padUpButtonPressed()));
    connect(m_Controls.padRight, SIGNAL(clicked()), this, SLOT(padRightButtonPressed()));
    connect(m_Controls.padDown, SIGNAL(clicked()), this, SLOT(padDownButtonPressed()));
}

void PaddingView::SetFocus() {}

void PaddingView::OnSelectionChanged(berry::IWorkbenchPart::Pointer /*source*/, const QList <mitk::DataNode::Pointer> &nodes) {

    if(nodes.count() == 0){
        m_Controls.affectedImages->setText("no image selected");
        return;
    }
    else{
        QString selectedImageNames;

        bool firstImage = true;
        foreach(mitk::DataNode::Pointer
        node, nodes){
            if(!firstImage) {
                selectedImageNames.append(", ");
            } else{
                firstImage = false;
            }
            mitk::StringProperty* nameProperty= (mitk::StringProperty*)(node->GetProperty("name"));
            selectedImageNames.append(nameProperty->GetValue());
        }
        m_Controls.affectedImages->setText(selectedImageNames);
    }

}

void PaddingView::padLeftButtonPressed() {
    WorkbenchUtils::Axis axis = (WorkbenchUtils::Axis) m_Controls.axisComboBox->currentIndex();

    switch (axis) {
        case WorkbenchUtils::AXIAL:
            addPadding(WorkbenchUtils::SAGITTAL, false);
            return;
        case WorkbenchUtils::SAGITTAL:
            addPadding(WorkbenchUtils::CORONAL, false);
        case WorkbenchUtils::CORONAL:
            addPadding(WorkbenchUtils::SAGITTAL, false);
            return;
    }
}

void PaddingView::padRightButtonPressed() {
    WorkbenchUtils::Axis axis = (WorkbenchUtils::Axis) m_Controls.axisComboBox->currentIndex();

    switch (axis) {
        case WorkbenchUtils::AXIAL:
            addPadding(WorkbenchUtils::SAGITTAL, true);
            return;
        case WorkbenchUtils::SAGITTAL:
            addPadding(WorkbenchUtils::CORONAL, true);
        case WorkbenchUtils::CORONAL:
            addPadding(WorkbenchUtils::SAGITTAL, true);
            return;
    }

}

void PaddingView::padUpButtonPressed() {
    WorkbenchUtils::Axis axis = (WorkbenchUtils::Axis) m_Controls.axisComboBox->currentIndex();

    switch (axis) {
        case WorkbenchUtils::AXIAL:
            addPadding(WorkbenchUtils::CORONAL, false);
            return;
        case WorkbenchUtils::SAGITTAL:
            addPadding(WorkbenchUtils::AXIAL, true);
        case WorkbenchUtils::CORONAL:
            addPadding(WorkbenchUtils::AXIAL, true);
            return;
    }
}

void PaddingView::padDownButtonPressed() {
    WorkbenchUtils::Axis axis = (WorkbenchUtils::Axis) m_Controls.axisComboBox->currentIndex();

    switch (axis) {
        case WorkbenchUtils::AXIAL:
            addPadding(WorkbenchUtils::CORONAL, true);
            return;
        case WorkbenchUtils::SAGITTAL:
            addPadding(WorkbenchUtils::AXIAL, false);
        case WorkbenchUtils::CORONAL:
            addPadding(WorkbenchUtils::AXIAL, false);
            return;
    }
}

void PaddingView::addPadding(WorkbenchUtils::Axis axis, bool append) {
    // get nodes
    QList <mitk::DataNode::Pointer> nodes = this->GetDataManagerSelection();

    // get params
    float voxelValue = m_Controls.voxelValueSpinBox->value();
    unsigned int amountOfPadding = m_Controls.amountOfPaddingSpinBox->value();

    foreach(mitk::DataNode::Pointer
    node, nodes){
        mitk::Image::Pointer img = dynamic_cast<mitk::Image *>(node->GetData());
        img = WorkbenchUtils::addPadding(img, axis, append, amountOfPadding, voxelValue);
        node->SetData(img);
    }

    refreshBoundaries();
}

void PaddingView::refreshBoundaries() {
    mitk::RenderingManager::GetInstance()->InitializeViewsByBoundingObjects(this->GetDataStorage());
}