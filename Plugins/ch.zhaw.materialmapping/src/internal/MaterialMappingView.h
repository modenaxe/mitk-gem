#pragma once

#include <berryISelectionListener.h>

#include <QmitkAbstractView.h>
#include <QFuture>

#include "ui_MaterialMappingViewControls.h"
#include "CalibrationDataModel.h"
#ifdef MITK_GEM_ENABLE_GUI_TESTS
#include "test/Runner.h"
#endif
#include "BoneDensityFunctor.h"
#include "PowerLawWidgetManager.h"

class MaterialMappingView : public QmitkAbstractView {
    Q_OBJECT

public:
    ~MaterialMappingView();

    static const std::string VIEW_ID;
    // enables building of the GUI unit testing
    static const bool TESTING = false;
#ifdef MITK_GEM_ENABLE_GUI_TESTS
    static Ui::MaterialMappingViewControls *controls;
#endif

protected slots:
    void deleteSelectedRows();
    void startButtonClicked();
    void tableDataChanged();
    void unitSelectionChanged(int);
#ifdef MITK_GEM_ENABLE_GUI_TESTS
    void compareGrids();
#endif
    void createEMorganImage();
    void loadParametersButtonClicked();
    void saveParametersButtonClicked();
    bool eventFilter(QObject *, QEvent *) override;

protected:
    virtual void CreateQtPartControl(QWidget *parent) override;
    virtual void SetFocus() override {}; // required by blueberry
    bool isValidSelection();

    Ui::MaterialMappingViewControls m_Controls;
    CalibrationDataModel m_CalibrationDataModel;

#ifdef MITK_GEM_ENABLE_GUI_TESTS
    std::unique_ptr<Testing::Runner> m_TestRunner;
#endif
    std::unique_ptr<PowerLawWidgetManager> m_PowerLawWidgetManager;

    QFuture<void> m_WorkerFuture;
};
