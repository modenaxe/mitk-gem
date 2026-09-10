#include <algorithm>
#include <cmath>

#include <berryISelectionService.h>
#include <berryIWorkbenchWindow.h>
#include <QMessageBox>
#include <QFileDialog>
#include <QShortcut>
#include <QtConcurrentRun>
#include <QWidget>
#include <mitkException.h>
#include <mitkImage.h>
#include <mitkNodePredicateAnd.h>
#include <mitkNodePredicateDataType.h>
#include <mitkNodePredicateNot.h>
#include <mitkUnstructuredGrid.h>
#include <tinyxml2.h>

#include <vtkImageCast.h>
#include <vtkCellArray.h>
#include <vtkPointData.h>
#include <vtkUnstructuredGrid.h>

#include <exception>
#include <string>
#include <utility>

#include "MaterialMappingView.h"
#include "MaterialMappingHelper.h"
#include "MaterialMappingInputValidation.h"
#include "WorkbenchUtils.h"
#include "GuiHelpers.h"
#include "MaterialMappingFilter.h"
#include "PowerLawWidget.h"

namespace
{
    mitk::Image::Pointer CreateImageSnapshot(mitk::Image* image)
    {
        if (image == nullptr || !image->IsInitialized())
        {
            return nullptr;
        }

        // mitk::Image::Clone() copies image volumes and geometry.
        return image->Clone();
    }

    mitk::UnstructuredGrid::Pointer CreateMeshSnapshot(mitk::UnstructuredGrid* mesh)
    {
        if (mesh == nullptr || mesh->GetVtkUnstructuredGrid() == nullptr)
        {
            return nullptr;
        }

        auto snapshot = mitk::UnstructuredGrid::New();
        // Graft performs a VTK DeepCopy and retains geometry/properties.
        snapshot->Graft(mesh);
        return snapshot;
    }
}

const std::string MaterialMappingView::VIEW_ID = "org.mitk.views.materialmapping";
#ifdef MITK_GEM_ENABLE_GUI_TESTS
Ui::MaterialMappingViewControls *MaterialMappingView::controls = nullptr;
#endif

MaterialMappingView::~MaterialMappingView() {
    // The task owns deep-copied inputs and does not capture this view. Do not
    // block view destruction while the current algorithm call completes.
    m_WorkerWatcher.cancel();
}

void MaterialMappingView::CreateQtPartControl(QWidget *parent) {
    m_Controls.setupUi(parent);
    // table
    auto table = m_Controls.calibrationTableView;
    table->setModel(m_CalibrationDataModel.getQItemModel());
    auto setResizeMode = [=](int _column, QHeaderView::ResizeMode _mode) {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0) // renamed in 5.0
        table->horizontalHeader()->setResizeMode(_column, _mode);
#else
        table->horizontalHeader()->setSectionResizeMode(_column, _mode);
#endif
    };
    setResizeMode(0, QHeaderView::Stretch);
    setResizeMode(1, QHeaderView::Stretch);

    // Data selectors. The grid selector admits only unstructured grids, with
    // the 3-D-cell check performed below. The intensity selector deliberately
    // uses the exact MITK Image type: segmentation and specialized/vector
    // image types are not suitable CT inputs.
    m_Controls.unstructuredGridComboBox->SetPredicate(WorkbenchUtils::createIsUnstructuredGridTypePredicate());
    m_Controls.unstructuredGridComboBox->SetDataStorage(this->GetDataStorage());
    m_Controls.unstructuredGridComboBox->SetAutoSelectNewItems(false);

    auto intensityImagePredicate = mitk::NodePredicateAnd::New(
        mitk::NodePredicateDataType::New("Image"),
        mitk::NodePredicateNot::New(WorkbenchUtils::createIsBinaryImageTypePredicate()));
    m_Controls.greyscaleImageComboBox->SetPredicate(intensityImagePredicate);
    m_Controls.greyscaleImageComboBox->SetDataStorage(this->GetDataStorage());
    m_Controls.greyscaleImageComboBox->SetAutoSelectNewItems(false);

    // Optional GUI test controls are only compiled in testing builds.
#ifdef MITK_GEM_ENABLE_GUI_TESTS
    {
        controls = &m_Controls;
        m_Controls.testingGroup->show();
        m_Controls.expectedResultComboBox->SetDataStorage(this->GetDataStorage());
        m_Controls.expectedResultComboBox->SetAutoSelectNewItems(false);
        m_Controls.expectedResultComboBox->SetPredicate(WorkbenchUtils::createIsUnstructuredGridTypePredicate());
        m_Controls.expectedResultComboBox_2->SetDataStorage(this->GetDataStorage());
        m_Controls.expectedResultComboBox_2->SetAutoSelectNewItems(false);
        m_Controls.expectedResultComboBox_2->SetPredicate(WorkbenchUtils::createIsUnstructuredGridTypePredicate());

        m_TestRunner = std::unique_ptr<Testing::Runner>(new Testing::Runner());
        connect(m_Controls.runUnitTestsButton, SIGNAL(clicked()), m_TestRunner.get(), SLOT(runUnitTests()));
        connect(m_Controls.compareGridsButton, SIGNAL(clicked()), this, SLOT(compareGrids()));
        connect(m_Controls.createEMorganButton, SIGNAL(clicked()), this, SLOT(createEMorganImage()));
    }
#else
    m_Controls.testingGroup->hide();
#endif

    // hide custom erosion parameter
    m_Controls.uParamCheckBox->hide();
    m_Controls.label_15->hide();

    // hide custom dilation
    m_Controls.eParamSpinBox->hide();
    m_Controls.label->hide();

    // delete key on table
    QShortcut *shortcut = new QShortcut(QKeySequence(QKeySequence::Delete), table);
    connect(shortcut, SIGNAL(activated()), this, SLOT(deleteSelectedRows()));

    // power law widgets
    m_PowerLawWidgetManager = std::unique_ptr<PowerLawWidgetManager>(new PowerLawWidgetManager(m_Controls.powerLawWidgets));

    // signals
    connect(m_Controls.startButton, SIGNAL(clicked()), this, SLOT(startButtonClicked()));
    connect(m_Controls.saveParametersButton, SIGNAL(clicked()), this, SLOT(saveParametersButtonClicked()));
    connect(m_Controls.loadParametersButton, SIGNAL(clicked()), this, SLOT(loadParametersButtonClicked()));
    connect(&m_CalibrationDataModel, SIGNAL(dataChanged()), this, SLOT(tableDataChanged()));
    connect(m_Controls.addPowerLawButton, SIGNAL(clicked()), m_PowerLawWidgetManager.get(), SLOT(addPowerLaw()));
    connect(m_Controls.removePowerLawButton, SIGNAL(clicked()), m_PowerLawWidgetManager.get(), SLOT(removePowerLaw()));
    connect(m_Controls.unitSelectionComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(unitSelectionChanged(int)));
    connect(m_Controls.unstructuredGridComboBox, &QmitkDataStorageComboBox::OnSelectionChanged,
            this, [this](const mitk::DataNode*) { updateStartButtonState(); });
    connect(m_Controls.greyscaleImageComboBox, &QmitkDataStorageComboBox::OnSelectionChanged,
            this, [this](const mitk::DataNode*) { updateStartButtonState(); });
    connect(&m_WorkerWatcher, &QFutureWatcher<MappingResult>::finished,
            this, &MaterialMappingView::onMaterialMappingFinished, Qt::QueuedConnection);

    m_Controls.unitSelectionComboBox->setCurrentIndex(0);
    unitSelectionChanged(0);
    updateStartButtonState();

    for(auto *widget : m_Controls.scrollAreaWidgetContents->findChildren<QWidget*>()){
        widget->installEventFilter(this);
        widget->setFocusPolicy(Qt::StrongFocus); // prevents wheel from setting the focus
    }
}

MaterialMappingView::MappingResult MaterialMappingView::RunMaterialMapping(
    mitk::UnstructuredGrid::Pointer mesh,
    mitk::Image::Pointer image,
    MappingConfiguration configuration)
{
    MappingResult result;

    try
    {
        std::string validationError;
        if (!MaterialMappingInputValidation::ValidateVolumeMesh(mesh.GetPointer(), validationError)
            || !MaterialMappingInputValidation::ValidateIntensityImage(image.GetPointer(), validationError))
        {
            result.error = validationError;
            return result;
        }

        auto mappedMesh = MaterialMappingHelper::Compute(mesh,
                                                          image,
                                                          configuration.method,
                                                          std::move(configuration.densityFunctor),
                                                          std::move(configuration.powerLawFunctor),
                                                          configuration.minimumElementValue);

        if (mappedMesh.IsNull() || mappedMesh->GetVtkUnstructuredGrid() == nullptr
            || mappedMesh->GetVtkUnstructuredGrid()->GetNumberOfPoints() == 0
            || mappedMesh->GetVtkUnstructuredGrid()->GetNumberOfCells() == 0)
        {
            result.error = "Material mapping did not produce a valid volume mesh.";
            return result;
        }

        result.mesh = mappedMesh;
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
        result.error = "Material mapping failed with an unknown error.";
    }

    return result;
}

void MaterialMappingView::deleteSelectedRows() {
    auto selection = m_Controls.calibrationTableView->selectionModel();
    auto selectedItems = selection->selectedRows();
    std::set<int> rowsToDelete;

    for(auto &item : selectedItems) {
        rowsToDelete.insert(item.row());
    }

    for (std::set<int>::reverse_iterator rit = rowsToDelete.rbegin(); rit != rowsToDelete.rend(); ++rit) {
        m_CalibrationDataModel.removeRow(*rit);
    }

    tableDataChanged();
}

void MaterialMappingView::startButtonClicked() {
    MITK_INFO("ch.zhaw.materialmapping") << "processing input";
    if (m_WorkerWatcher.future().isValid() && !m_WorkerWatcher.future().isFinished())
    {
        QMessageBox::information(nullptr, "Material mapping in progress",
                                 "Wait for the current material-mapping task to finish.");
        return;
    }

    if (isValidSelection()) {
        mitk::DataNode *imageNode = m_Controls.greyscaleImageComboBox->GetSelectedNode();
        mitk::DataNode *ugridNode = m_Controls.unstructuredGridComboBox->GetSelectedNode();

        mitk::Image::Pointer image = dynamic_cast<mitk::Image *>(imageNode->GetData());
        mitk::UnstructuredGrid::Pointer ugrid = dynamic_cast<mitk::UnstructuredGrid *>(ugridNode->GetData());

        try
        {
            MappingConfiguration configuration{
                gui::getSelectedMappingMethod(m_Controls),
                gui::createDensityFunctor(m_Controls, m_CalibrationDataModel),
                m_PowerLawWidgetManager->createFunctor(),
                static_cast<float>(m_Controls.fParamSpinBox->value())
            };

            auto imageSnapshot = CreateImageSnapshot(image);
            auto meshSnapshot = CreateMeshSnapshot(ugrid);
            if (imageSnapshot.IsNull() || meshSnapshot.IsNull())
            {
                QMessageBox::warning(nullptr, "Invalid material-mapping input",
                                     "The selected image or volume mesh could not be copied for processing.");
                return;
            }

            // The worker receives only immutable snapshots and value objects;
            // it never accesses this view, its widgets, or DataStorage.
            m_Controls.scrollArea->setEnabled(false);
            m_WorkerWatcher.setFuture(QtConcurrent::run([meshSnapshot, imageSnapshot, configuration]() {
                return RunMaterialMapping(meshSnapshot, imageSnapshot, configuration);
            }));
        }
        catch (const mitk::Exception& exception)
        {
            QMessageBox::warning(nullptr, "Material mapping failed", exception.GetDescription());
        }
        catch (const std::exception& exception)
        {
            QMessageBox::warning(nullptr, "Material mapping failed", exception.what());
        }
        catch (...)
        {
            QMessageBox::warning(nullptr, "Material mapping failed",
                                 "The material-mapping task could not be prepared.");
        }
    }
}

void MaterialMappingView::onMaterialMappingFinished()
{
    const auto result = m_WorkerWatcher.result();
    std::string error = result.error;

    if (error.empty())
    {
        try
        {
            auto newNode = mitk::DataNode::New();
            newNode->SetData(result.mesh);
            newNode->SetProperty("name", mitk::StringProperty::New("material mapped mesh"));
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
            error = "The material-mapped mesh could not be added to the data storage.";
        }
    }

    m_Controls.scrollArea->setEnabled(true);

    if (!error.empty())
    {
        QMessageBox::warning(nullptr, "Material mapping failed", QString::fromStdString(error));
    }
}

void MaterialMappingView::tableDataChanged() {
    // in case new data was loaded from a file, we need to update the combo box
    int index = static_cast<int>(m_CalibrationDataModel.getUnit());
    m_Controls.unitSelectionComboBox->setCurrentIndex(index);

    auto linearEqParams = m_CalibrationDataModel.getFittedLine();
    m_Controls.linEQSlopeSpinBox->setValue(linearEqParams.slope);
    m_Controls.linEQOffsetSpinBox->setValue(linearEqParams.offset);

    if (m_CalibrationDataModel.hasExpectedValueRange()) {
        m_Controls.unitWarningLabel->hide();
    } else {
        m_Controls.unitWarningLabel->show();
    }
}

bool MaterialMappingView::isValidSelection() {
    // get the nodes selected
    mitk::DataNode *imageNode = m_Controls.greyscaleImageComboBox->GetSelectedNode();
    mitk::DataNode *ugridNode = m_Controls.unstructuredGridComboBox->GetSelectedNode();

    std::string imageError;
    std::string meshError;
    const bool imageIsValid = MaterialMappingInputValidation::ValidateIntensityImageNode(imageNode, imageError);
    const bool meshIsValid = MaterialMappingInputValidation::ValidateVolumeMeshNode(ugridNode, meshError);

    gui::setMandatoryQSSField(m_Controls.greyscaleSelector, !imageIsValid);
    gui::setMandatoryQSSField(m_Controls.meshSelector, !meshIsValid);

    if (imageIsValid && meshIsValid)
    {
        return true;
    }

    if ((imageNode != nullptr && !imageIsValid) || (ugridNode != nullptr && !meshIsValid))
    {
        QString message;
        if (imageNode != nullptr && !imageIsValid)
        {
            message += QString::fromStdString(imageError);
        }
        if (ugridNode != nullptr && !meshIsValid)
        {
            if (!message.isEmpty())
            {
                message += "\n\n";
            }
            message += QString::fromStdString(meshError);
        }
        QMessageBox::warning(nullptr, "Invalid material-mapping input", message);
    }

    MITK_INFO("ch.zhaw.materialmapping") << "invalid data selection";
    return false;
}

void MaterialMappingView::updateStartButtonState()
{
    std::string imageError;
    std::string meshError;
    const auto imageNode = m_Controls.greyscaleImageComboBox->GetSelectedNode();
    const auto meshNode = m_Controls.unstructuredGridComboBox->GetSelectedNode();
    const bool imageIsValid = MaterialMappingInputValidation::ValidateIntensityImageNode(imageNode, imageError);
    const bool meshIsValid = MaterialMappingInputValidation::ValidateVolumeMeshNode(meshNode, meshError);
    m_Controls.startButton->setEnabled(imageIsValid && meshIsValid);
}

void MaterialMappingView::unitSelectionChanged(int) {
    auto selectedText = m_Controls.unitSelectionComboBox->currentText();
    m_CalibrationDataModel.setUnit(selectedText);
    tableDataChanged();
}

#ifdef MITK_GEM_ENABLE_GUI_TESTS
void MaterialMappingView::compareGrids() {
    mitk::DataNode *expectedResultNode0 = m_Controls.expectedResultComboBox->GetSelectedNode();
    mitk::DataNode *expectedResultNode1 = m_Controls.expectedResultComboBox_2->GetSelectedNode();
    mitk::UnstructuredGrid::Pointer u0 = dynamic_cast<mitk::UnstructuredGrid *>(expectedResultNode0->GetData());
    mitk::UnstructuredGrid::Pointer u1 = dynamic_cast<mitk::UnstructuredGrid *>(expectedResultNode1->GetData());
    m_TestRunner->compareGrids(u0, u1);
}
#endif

void MaterialMappingView::createEMorganImage() {
    mitk::DataNode *imageNode = m_Controls.greyscaleImageComboBox->GetSelectedNode();
    mitk::Image::Pointer image = dynamic_cast<mitk::Image *>(imageNode->GetData());

    auto densityFunctor = gui::createDensityFunctor(m_Controls, m_CalibrationDataModel);
    auto powerLawFunctor = m_PowerLawWidgetManager->createFunctor();

    //// http://bugs.mitk.org/show_bug.cgi?id=5050
    auto mitkOrigin = image->GetGeometry()->GetOrigin();
    auto vtkImage = vtkSmartPointer<vtkImageData>::New();
    vtkImage->ShallowCopy(const_cast<vtkImageData *>(image->GetVtkImageData()));
    vtkImage->SetOrigin(mitkOrigin[0], mitkOrigin[1], mitkOrigin[2]);
    auto imageCast = vtkSmartPointer<vtkImageCast>::New();
    imageCast->SetInputData(vtkImage);
    imageCast->SetOutputScalarTypeToFloat();
    imageCast->Update();
    vtkImage = imageCast->GetOutput();

    for (auto i = 0; i < vtkImage->GetNumberOfPoints(); i++) {
        auto ct_val = vtkImage->GetPointData()->GetScalars()->GetTuple1(i);
        auto rho = densityFunctor(ct_val);
        auto val = powerLawFunctor(rho);
        vtkImage->GetPointData()->GetScalars()->SetTuple1(i, val);
    }

    // save results
    mitk::Image::Pointer result = mitk::Image::New();
    result->Initialize(vtkImage);
    result->SetVolume(vtkImage->GetScalarPointer());
    mitk::DataNode::Pointer newNode = mitk::DataNode::New();
    newNode->SetData(result);
    newNode->SetProperty("name", mitk::StringProperty::New("emorgan"));
    newNode->SetProperty("layer", mitk::IntProperty::New(1));
    this->GetDataStorage()->Add(newNode);
}

void MaterialMappingView::saveParametersButtonClicked() {
    auto filename = QFileDialog::getSaveFileName(0, tr("Save parameter file"), "", tr("parameter file (*.matmap)"));
    if (!filename.isNull()) {
        MITK_INFO << "saving parameters to file: " << filename.toUtf8().constData();

        tinyxml2::XMLDocument doc;
        auto root = doc.NewElement("MaterialMapping");
        root->SetAttribute("Version", WorkbenchUtils::getGemVersion().c_str());
        auto calibration = m_CalibrationDataModel.serializeToXml(doc);
        auto bonedensity = gui::serializeDensityGroupStateToXml(m_Controls, doc);
        auto powerlaws = m_PowerLawWidgetManager->serializeToXml(doc);
        auto options = gui::serializeOptionsGroupStateToXml(m_Controls, doc);
        root->InsertEndChild(calibration);
        root->InsertEndChild(bonedensity);
        root->InsertEndChild(powerlaws);
        root->InsertEndChild(options);

        doc.InsertEndChild(doc.NewDeclaration("xml version=\"1.0\" encoding=\"utf-8\""));
        doc.InsertEndChild(root);
        if (doc.SaveFile(filename.toUtf8().constData()) != tinyxml2::XML_SUCCESS) {
            QMessageBox::warning(0, "", "could not save parameter file.");
        }
    } else {
        MITK_INFO << "canceled file save dialog.";
    }
}

void MaterialMappingView::loadParametersButtonClicked() {
    auto filename = QFileDialog::getOpenFileName(0, tr("Open parameter file"), "", tr("parameter file (*.matmap)"));
    if (!filename.isNull()) {
        MITK_INFO << "loading parameters from file: " << filename.toUtf8().constData();

        tinyxml2::XMLDocument doc;
        if (doc.LoadFile(filename.toUtf8().constData()) != tinyxml2::XML_SUCCESS) {
            QMessageBox::warning(0, "", "could not read from file.");
            return;
        }
        auto root = doc.RootElement();
        if (root == nullptr) {
            QMessageBox::warning(0, "", "could not read parameter file contents.");
            return;
        }
        auto calibration = root->FirstChildElement("Calibration");
        if (calibration) {
            MITK_INFO << "loading calibration...";
            m_CalibrationDataModel.loadFromXml(calibration);
        }
        auto bonedensity = root->FirstChildElement("BoneDensityParameters");
        if (bonedensity) {
            MITK_INFO << "loading bone density parameters...";
            gui::loadDensityGroupStateFromXml(m_Controls, bonedensity);
        }
        auto powerlaws = root->FirstChildElement("PowerLaws");
        if (powerlaws) {
            MITK_INFO << "loading power laws...";
            m_PowerLawWidgetManager->loadFromXml(powerlaws);
        }
        auto options = root->FirstChildElement("Options");
        if (options) {
            MITK_INFO << "loading options...";
            gui::loadOptionsGroupStateFromXml(m_Controls, options);
        }
    } else {
        MITK_INFO << "canceled file open dialog.";
    }
}

bool MaterialMappingView::eventFilter(QObject *_obj, QEvent *_ev) {
    // ignores scroll wheel events on all spin boxes
    if(_ev->type() == QEvent::Wheel && qobject_cast<QAbstractSpinBox*>(_obj)){
        _ev->ignore();
        return true;
    }
    return QObject::eventFilter(_obj, _ev);
}
