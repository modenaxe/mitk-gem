#pragma once

#include <string>

#include <QmitkAbstractView.h>

#include "ui_FemExportViewControls.h"

class FemExportView : public QmitkAbstractView
{
    Q_OBJECT

public:
    static const std::string VIEW_ID;

protected slots:
    void updateExportControls();
    void exportMaterialMethodSelectionChanged(int index);
    void exportFemModelClicked();

protected:
    void CreateQtPartControl(QWidget *parent) override;
    void SetFocus() override;

private:
    void updateExportPreview();

    Ui::FemExportViewControls m_Controls;
};
