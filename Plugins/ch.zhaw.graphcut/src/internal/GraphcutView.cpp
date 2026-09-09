/**
 *  MITK-GEM: Graphcut Plugin
 *
 *  Copyright (c) 2016, Zurich University of Applied Sciences, School of Engineering, T. Fitze, Y. Pauchard
 *
 *  Licensed under GNU General Public License 3.0 or later.
 *  Some rights reserved.
 */

// Blueberry
#include <berryISelectionService.h>
#include <berryIWorkbenchWindow.h>

// Qmitk
#include <QmitkDataStorageComboBox.h>
#include "GraphcutView.h"

// MITK
#include <mitkNodePredicateDataType.h>
#include <mitkNodePredicateAnd.h>
#include <mitkNodePredicateOr.h>
#include <mitkImageCast.h>
#include <mitkITKImageImport.h>
#include <mitkLabelSetImage.h>
#include <mitkMultiLabelPredicateHelper.h>
#include <mitkNodePredicateNot.h>
#include <mitkTimeGeometry.h>

// Qt
#include <QComboBox>
#include <QThreadPool>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QVariant>

// STL
#include <cmath>
#include <cstdint>
#include <memory>

// Graphcut
#include "lib/GraphCut3D/ImageGraphCut3DFilter.h"
#include "GraphcutWorker.h"


const std::string GraphcutView::VIEW_ID = "org.mitk.views.imagegraphcut3dsegmentation";

void GraphcutView::SetFocus() {
}

void GraphcutView::CreateQtPartControl(QWidget *parent) {
    // create GUI widgets from the Qt Designer's .ui file
    m_Controls.setupUi(parent);

    // init image selectors
    initializeImageSelector(m_Controls.greyscaleImageSelector);
    initializeImageSelector(m_Controls.foregroundImageSelector);
    initializeImageSelector(m_Controls.backgroundImageSelector);

    // set predicates to filter which images are selectable
    auto greyscalePredicate = mitk::NodePredicateAnd::New(
      WorkbenchUtils::createIsImageTypePredicate(),
      mitk::NodePredicateNot::New(mitk::GetMultiLabelSegmentationPredicate()));
    m_Controls.greyscaleImageSelector->SetPredicate(greyscalePredicate);
    auto seedPredicate = mitk::NodePredicateOr::New();
    seedPredicate->AddPredicate(WorkbenchUtils::createIsBinaryImageTypePredicate());
    seedPredicate->AddPredicate(mitk::GetMultiLabelSegmentationPredicate());
    m_Controls.foregroundImageSelector->SetPredicate(seedPredicate);
    m_Controls.backgroundImageSelector->SetPredicate(seedPredicate);

    // setup signals
    connect(m_Controls.startButton, SIGNAL(clicked()), this, SLOT(startButtonPressed()));
    connect(m_Controls.refreshTimeButton, SIGNAL(clicked()), this, SLOT(refreshButtonPressed()));
    connect(m_Controls.refreshMemoryButton, SIGNAL(clicked()), this, SLOT(refreshButtonPressed()));
    connect(m_Controls.greyscaleImageSelector, SIGNAL(OnSelectionChanged (const mitk::DataNode *)), this, SLOT(imageSelectionChanged()));
    connect(m_Controls.foregroundImageSelector, SIGNAL(OnSelectionChanged (const mitk::DataNode *)), this, SLOT(foregroundImageSelectionChanged()));
    connect(m_Controls.backgroundImageSelector, SIGNAL(OnSelectionChanged (const mitk::DataNode *)), this, SLOT(backgroundImageSelectionChanged()));
    connect(m_Controls.foregroundLabelSelector, SIGNAL(currentIndexChanged(int)), this, SLOT(imageSelectionChanged()));
    connect(m_Controls.backgroundLabelSelector, SIGNAL(currentIndexChanged(int)), this, SLOT(imageSelectionChanged()));

    // init default state
    m_currentlyActiveWorkerCount = 0;
    foregroundImageSelectionChanged();
    backgroundImageSelectionChanged();
    lockGui(false);
}

void GraphcutView::OnSelectionChanged(berry::IWorkbenchPart::Pointer, const QList <mitk::DataNode::Pointer> &) {
    MITK_DEBUG("ch.zhaw.graphcut") << "selection changed";
}

void GraphcutView::startButtonPressed() {
    MITK_INFO("ch.zhaw.graphcut") << "start button pressed";

    if (!isValidSelection()) {
        return;
    }

    MITK_INFO("ch.zhaw.graphcut") << "processing input";

    auto greyscaleImageNode = m_Controls.greyscaleImageSelector->GetSelectedNode();
    auto greyscaleImage = dynamic_cast<mitk::Image *>(greyscaleImageNode->GetData());

    GraphcutSegmentationUtils::SeedSelection foregroundSeed;
    GraphcutSegmentationUtils::SeedSelection backgroundSeed;
    QString selectionError;
    if (!getSeedSelection(m_Controls.foregroundImageSelector,
                          m_Controls.foregroundLabelSelector,
                          foregroundSeed,
                          selectionError)
        || !getSeedSelection(m_Controls.backgroundImageSelector,
                             m_Controls.backgroundLabelSelector,
                             backgroundSeed,
                             selectionError)) {
        QMessageBox::critical(nullptr, "GraphCut3D", selectionError);
        return;
    }

    // QThreadPool owns the worker after start(). Keep it in a smart pointer
    // until all image casts have succeeded, so failures cannot leak it.
    auto worker = std::make_unique<GraphcutWorker>();

    try {
        MITK_INFO("ch.zhaw.graphcut") << "cast the images to ITK";
        GraphcutWorker::InputImageType::Pointer greyscaleImageItk;
        GraphcutWorker::MaskImageType::Pointer foregroundMaskItk;
        GraphcutWorker::MaskImageType::Pointer backgroundMaskItk;
        mitk::CastToItkImage(greyscaleImage, greyscaleImageItk);
        // Selected multi-label seeds are binary (0/1) temporary masks. The
        // cast below guarantees the unsigned-char representation MAXFLOW uses.
        mitk::CastToItkImage(foregroundSeed.image, foregroundMaskItk);
        mitk::CastToItkImage(backgroundSeed.image, backgroundMaskItk);

        worker->setInputImage(greyscaleImageItk);
        worker->setForegroundMask(foregroundMaskItk);
        worker->setBackgroundMask(backgroundMaskItk);
    }
    catch (const itk::ExceptionObject& exception) {
        QMessageBox::critical(nullptr,
                              "GraphCut3D",
                              QString("Could not prepare the GraphCut input images: %1").arg(exception.GetDescription()));
        return;
    }
    catch (const std::exception& exception) {
        QMessageBox::critical(nullptr,
                              "GraphCut3D",
                              QString("Could not prepare the GraphCut input images: %1").arg(exception.what()));
        return;
    }
    catch (...) {
        QMessageBox::critical(nullptr,
                              "GraphCut3D",
                              "Could not prepare the GraphCut input images due to an unknown error.");
        return;
    }

    worker->setSigma(m_Controls.paramSigmaSpinBox->value());
    worker->setBoundaryDirection(static_cast<GraphcutWorker::BoundaryDirection>(m_Controls.paramBoundaryDirectionComboBox->currentIndex()));
    worker->setForegroundPixelValue(static_cast<GraphcutWorker::BinaryPixelType>(m_Controls.paramLabelValueSpinBox->value()));

    MITK_INFO("ch.zhaw.graphcut") << "register signals";
    qRegisterMetaType<itk::DataObject::Pointer>("itk::DataObject::Pointer");
    QObject::connect(worker.get(), SIGNAL(started(unsigned int)), this, SLOT(workerHasStarted(unsigned int)));
    QObject::connect(worker.get(), SIGNAL(finished(itk::DataObject::Pointer, unsigned int)), this, SLOT(workerIsDone(itk::DataObject::Pointer, unsigned int)));
    QObject::connect(worker.get(), SIGNAL(progress(float, unsigned int)), this, SLOT(workerProgressUpdate(float, unsigned int)));

    m_Controls.progressBar->setValue(0);
    m_Controls.progressBar->setMinimum(0);
    m_Controls.progressBar->setMaximum(100);

    const auto workerId = worker->id;
    m_referenceImageNodes.emplace(workerId, greyscaleImageNode);
    ++m_currentlyActiveWorkerCount;
    lockGui(true);

    MITK_INFO("ch.zhaw.graphcut") << "start the worker";
    QThreadPool::globalInstance()->start(worker.release(), QThread::HighestPriority);
}

void GraphcutView::workerHasStarted(unsigned int workerId) {
    MITK_DEBUG("ch.zhaw.graphcut") << "worker " << workerId << " started";
}

void GraphcutView::workerIsDone(itk::DataObject::Pointer data, unsigned int workerId){
    MITK_DEBUG("ch.zhaw.graphcut") << "worker " << workerId << " finished";

    auto referenceImageNodeIt = m_referenceImageNodes.find(workerId);
    mitk::DataNode::Pointer referenceImageNode;
    if (referenceImageNodeIt != m_referenceImageNodes.end()) {
        referenceImageNode = referenceImageNodeIt->second;
    }

    auto finishWorker = [this, workerId]() {
        m_referenceImageNodes.erase(workerId);
        if (m_currentlyActiveWorkerCount > 0 && --m_currentlyActiveWorkerCount == 0) {
            lockGui(false);
        }
    };

    auto *resultImageItk = dynamic_cast<GraphcutWorker::OutputImageType *>(data.GetPointer());
    if (resultImageItk == nullptr) {
        QMessageBox::critical(nullptr,
                              "GraphCut3D",
                              "GraphCut3D did not produce an output image. Check the log for the underlying error.");
        finishWorker();
        return;
    }

    try {
        auto resultImage = mitk::GrabItkImageMemory(resultImageItk, nullptr, nullptr, false);
        auto resultNode = GraphcutSegmentationUtils::CreateResultSegmentationNode(
          resultImage, referenceImageNode, this->GetDataStorage(), "GraphCut segmentation");
        if (resultNode.IsNull()) {
            QMessageBox::critical(nullptr,
                                  "GraphCut3D",
                                  "Could not convert the GraphCut output to a modern MITK segmentation.");
            finishWorker();
            return;
        }

        if (referenceImageNode.IsNotNull()) {
            this->GetDataStorage()->Add(resultNode, referenceImageNode);
        }
        else {
            this->GetDataStorage()->Add(resultNode);
        }
    }
    catch (const itk::ExceptionObject& exception) {
        QMessageBox::critical(nullptr,
                              "GraphCut3D",
                              QString("Could not create the GraphCut segmentation: %1").arg(exception.GetDescription()));
        finishWorker();
        return;
    }
    catch (const std::exception& exception) {
        QMessageBox::critical(nullptr,
                              "GraphCut3D",
                              QString("Could not create the GraphCut segmentation: %1").arg(exception.what()));
        finishWorker();
        return;
    }
    catch (...) {
        QMessageBox::critical(nullptr,
                              "GraphCut3D",
                              "Could not create the GraphCut segmentation due to an unknown error.");
        finishWorker();
        return;
    }

    finishWorker();
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void GraphcutView::imageSelectionChanged() {
    MITK_DEBUG("ch.zhaw.graphcut") << "selector changed image";

    // estimate required memory and computation time
    auto greyscaleImageNode = m_Controls.greyscaleImageSelector->GetSelectedNode();
    auto *greyscaleImage = greyscaleImageNode != nullptr
      ? dynamic_cast<mitk::Image *>(greyscaleImageNode->GetData())
      : nullptr;
    if (greyscaleImage == nullptr || greyscaleImage->GetDimension() != 3) {
        m_Controls.estimatedMemory->setText("-");
        m_Controls.estimatedTime->setText("-");
        setWarningField(m_Controls.estimatedMemory, false);
        setErrorField(m_Controls.estimatedMemory, false);
        setWarningField(m_Controls.estimatedTime, false);
        setErrorField(m_Controls.estimatedTime, false);
        return;
    }

    const auto x = static_cast<std::uint64_t>(greyscaleImage->GetDimension(0));
    const auto y = static_cast<std::uint64_t>(greyscaleImage->GetDimension(1));
    const auto z = static_cast<std::uint64_t>(greyscaleImage->GetDimension(2));
    if (x == 0 || y == 0 || z == 0) {
        return;
    }

    const auto numberOfVertices = x * y * z;

    // A 6-connected graph has three undirected edges per voxel. MAXFLOW 3.04
    // stores each edge in both directions.
    auto numberOfEdges = (3 * x) - 1;
    numberOfEdges = (numberOfEdges * y) - x;
    numberOfEdges = (numberOfEdges * z) - (x * y);
    numberOfEdges *= 2;

    const auto itkImageSizeInMemory = numberOfVertices * (sizeof(short) + (2 * sizeof(unsigned char)));

    // Node and arc sizes are the MAXFLOW 3.04 implementation's current
    // in-memory representation, not the size of the input images alone.
    const auto memoryRequiredInBytes = static_cast<double>(numberOfVertices * 48)
      + static_cast<double>(numberOfEdges * 28)
      + static_cast<double>(itkImageSizeInMemory);

    MITK_INFO("ch.zhaw.graphcut") << "Image has " << numberOfVertices << " vertices and " << numberOfEdges << " edges";

    updateMemoryRequirements(memoryRequiredInBytes);
    updateTimeEstimate(static_cast<long long>(numberOfEdges));
}

void GraphcutView::foregroundImageSelectionChanged() {
    updateSeedLabelSelector(m_Controls.foregroundImageSelector->GetSelectedNode(), m_Controls.foregroundLabelSelector);
    imageSelectionChanged();
}

void GraphcutView::backgroundImageSelectionChanged() {
    updateSeedLabelSelector(m_Controls.backgroundImageSelector->GetSelectedNode(), m_Controls.backgroundLabelSelector);
    imageSelectionChanged();
}

void GraphcutView::updateSeedLabelSelector(mitk::DataNode *node, QComboBox *labelSelector) {
    QSignalBlocker signalBlocker(labelSelector);
    labelSelector->clear();

    auto *segmentation = node != nullptr
      ? dynamic_cast<mitk::MultiLabelSegmentation *>(node->GetData())
      : nullptr;
    if (segmentation == nullptr) {
        labelSelector->setVisible(false);
        return;
    }

    const auto activeLabel = segmentation->GetActiveLabel();
    const auto activeLabelValue = activeLabel != nullptr
      ? activeLabel->GetValue()
      : mitk::MultiLabelSegmentation::UNLABELED_VALUE;

    for (const auto labelValue : segmentation->GetAllLabelValues()) {
        if (segmentation->IsEmpty(labelValue)) {
            continue;
        }

        const auto label = segmentation->GetLabel(labelValue);
        QString labelName = label != nullptr ? QString::fromStdString(label->GetName()) : QString();
        if (labelName.isEmpty()) {
            labelName = "Unnamed label";
        }

        QString displayName = QString("%1 [%2]").arg(labelName).arg(static_cast<qulonglong>(labelValue));
        if (segmentation->GetNumberOfGroups() > 1) {
            displayName.append(QString(" (group %1)").arg(segmentation->GetGroupIndexOfLabel(labelValue) + 1));
        }
        labelSelector->addItem(displayName, QVariant::fromValue(static_cast<qulonglong>(labelValue)));
    }

    if (labelSelector->count() == 0) {
        labelSelector->addItem("No painted labels");
        labelSelector->setEnabled(false);
    }
    else {
        labelSelector->setEnabled(true);
        const auto activeIndex = labelSelector->findData(QVariant::fromValue(static_cast<qulonglong>(activeLabelValue)));
        labelSelector->setCurrentIndex(activeIndex >= 0 ? activeIndex : 0);
    }

    labelSelector->setVisible(true);
}

bool GraphcutView::getSeedSelection(QmitkDataStorageComboBox *nodeSelector,
                                    QComboBox *labelSelector,
                                    GraphcutSegmentationUtils::SeedSelection &selection,
                                    QString &error) const {
    auto node = nodeSelector->GetSelectedNode();
    auto *segmentation = node != nullptr
      ? dynamic_cast<mitk::MultiLabelSegmentation *>(node->GetData())
      : nullptr;

    auto labelValue = mitk::MultiLabelSegmentation::UNLABELED_VALUE;
    if (segmentation != nullptr) {
        bool hasLabelValue = false;
        const auto selectedValue = labelSelector->currentData().toULongLong(&hasLabelValue);
        if (!hasLabelValue) {
            error = "Select a painted label for the selected segmentation.";
            return false;
        }
        labelValue = static_cast<GraphcutSegmentationUtils::LabelValueType>(selectedValue);
    }

    std::string internalError;
    if (!GraphcutSegmentationUtils::CreateSeedSelection(node, labelValue, selection, internalError)) {
        error = QString::fromStdString(internalError);
        return false;
    }

    return true;
}

void GraphcutView::updateMemoryRequirements(double memoryRequiredInBytes){
    QString memory = QString::number(memoryRequiredInBytes / 1024.0 / 1024.0, 'f', 0);
    memory.append("MB");
    m_Controls.estimatedMemory->setText(memory);
    if(memoryRequiredInBytes > 4096000000){
        setErrorField(m_Controls.estimatedMemory, true);
    } else if(memoryRequiredInBytes > 2048000000){
        setErrorField(m_Controls.estimatedMemory, false);
        setWarningField(m_Controls.estimatedMemory, true);
    } else{
        setErrorField(m_Controls.estimatedMemory, false);
        setWarningField(m_Controls.estimatedMemory, false);
    }
    MITK_INFO("ch.zhaw.graphcut") <<  "Representing the full graph will require " << memoryRequiredInBytes << " Bytes of memory to compute.";
}

void GraphcutView::updateTimeEstimate(long long numberOfEdges){
    // trendlines based on dataset of 50 images with incremental sizes calculated on a 32GB machine

    // graph init / reading results. linear
    // y = c0*x + c1
    double c0 = 2.0e-07;
    double c1 = 0.1148;
    double x = numberOfEdges;
    double estimatedSetupAndBreakdownTimeInSeconds = c0*x + c1;

    // the max flow computation.
    // c0*x^(c1)
    c0 = 2.0e-18;
    c1 = 2.4;
    x = numberOfEdges;

    double estimatedComputeTimeInSeconds = c0 * std::pow(x, c1);
    double estimateInSeconds;

    // max flow on < 30mega edges has a irregular time complexity and is thus excluded from the trendline
    if(numberOfEdges < 30000000){
        // max flow is very (<0.03s) fast in this range
        estimateInSeconds = estimatedSetupAndBreakdownTimeInSeconds;
    } else{
        estimateInSeconds = estimatedSetupAndBreakdownTimeInSeconds + estimatedComputeTimeInSeconds * 2; // * 2 because the estimation is off anyways. better estimate pessimistically
    }

    QString time = QString::number(estimateInSeconds, 'f', 2);
    time.append("s");
    m_Controls.estimatedTime->setText(time);
    if(estimateInSeconds > 60){
        setErrorField(m_Controls.estimatedTime, true);
    } else if (estimateInSeconds > 30) {
        setErrorField(m_Controls.estimatedTime, false);
        setWarningField(m_Controls.estimatedTime, true);
    }else {
        setErrorField(m_Controls.estimatedTime, false);
        setWarningField(m_Controls.estimatedTime, false);
    }

    MITK_INFO("ch.zhaw.graphcut") << "Graphcut computation will take about " << estimateInSeconds << " seconds.";
}

void GraphcutView::initializeImageSelector(QmitkDataStorageComboBox *selector){
    selector->SetDataStorage(this->GetDataStorage());
    selector->SetAutoSelectNewItems(false);
}

void GraphcutView::setMandatoryField(QWidget *widget, bool bEnabled){
    setQStyleSheetField(widget, "mandatoryField", bEnabled);
}

void GraphcutView::setWarningField(QWidget *widget, bool bEnabled){
    setQStyleSheetField(widget, "warningField", bEnabled);
}

void GraphcutView::setErrorField(QWidget *widget, bool bEnabled){
    setQStyleSheetField(widget, "errorField", bEnabled);
}

void GraphcutView::setQStyleSheetField(QWidget *widget, const char *fieldName, bool bEnabled){
    widget->setProperty(fieldName, bEnabled);
    widget->style()->unpolish(widget); // need to do this since we changed the stylesheet
    widget->style()->polish(widget);
    widget->update();
}

bool GraphcutView::isValidSelection() {
    auto greyscaleImageNode = m_Controls.greyscaleImageSelector->GetSelectedNode();
    auto foregroundMaskNode = m_Controls.foregroundImageSelector->GetSelectedNode();
    auto backgroundMaskNode = m_Controls.backgroundImageSelector->GetSelectedNode();

    setMandatoryField(m_Controls.greyscaleSelector, greyscaleImageNode == nullptr);
    setMandatoryField(m_Controls.foregroundSelector, foregroundMaskNode == nullptr);
    setMandatoryField(m_Controls.backgroundSelector, backgroundMaskNode == nullptr);
    setErrorField(m_Controls.greyscaleSelector, false);
    setErrorField(m_Controls.foregroundSelector, false);
    setErrorField(m_Controls.backgroundSelector, false);

    if (greyscaleImageNode == nullptr || foregroundMaskNode == nullptr || backgroundMaskNode == nullptr) {
        MITK_ERROR("ch.zhaw.graphcut") << "invalid selection: missing input.";
        return false;
    }

    auto *greyscaleImage = dynamic_cast<mitk::Image *>(greyscaleImageNode->GetData());
    if (greyscaleImage == nullptr || dynamic_cast<mitk::MultiLabelSegmentation *>(greyscaleImage) != nullptr) {
        setErrorField(m_Controls.greyscaleSelector, true);
        QMessageBox::warning(nullptr, "GraphCut3D", "The selected greyscale node must be a regular MITK image, not a segmentation.");
        return false;
    }

    GraphcutSegmentationUtils::SeedSelection foregroundSeed;
    GraphcutSegmentationUtils::SeedSelection backgroundSeed;
    QString selectionError;
    if (!getSeedSelection(m_Controls.foregroundImageSelector,
                          m_Controls.foregroundLabelSelector,
                          foregroundSeed,
                          selectionError)) {
        setErrorField(m_Controls.foregroundSelector, true);
        QMessageBox::warning(nullptr, "GraphCut3D", selectionError);
        return false;
    }

    if (!getSeedSelection(m_Controls.backgroundImageSelector,
                          m_Controls.backgroundLabelSelector,
                          backgroundSeed,
                          selectionError)) {
        setErrorField(m_Controls.backgroundSelector, true);
        QMessageBox::warning(nullptr, "GraphCut3D", selectionError);
        return false;
    }

    std::string validationError;
    if (!GraphcutSegmentationUtils::ValidateGraphCutInputs(greyscaleImage,
                                                            foregroundSeed,
                                                            backgroundSeed,
                                                            validationError)) {
        setErrorField(m_Controls.greyscaleSelector, true);
        setErrorField(m_Controls.foregroundSelector, true);
        setErrorField(m_Controls.backgroundSelector, true);
        QMessageBox::warning(nullptr, "GraphCut3D", QString::fromStdString(validationError));
        return false;
    }

    MITK_DEBUG("ch.zhaw.graphcut") << "valid selection";
    return true;
}

void GraphcutView::lockGui(bool b) {
    m_Controls.parentWidget->setEnabled(!b);
    m_Controls.progressBar->setVisible(b);
    m_Controls.startButton->setVisible(!b);
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void GraphcutView::workerProgressUpdate(float progress, unsigned int){
    int progressInt = (int) (progress * 100.0f);
    m_Controls.progressBar->setValue(progressInt);
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void GraphcutView::refreshButtonPressed(){
    foregroundImageSelectionChanged();
    backgroundImageSelectionChanged();
    imageSelectionChanged();
}
