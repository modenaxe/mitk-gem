#include "FemExportView.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>

#include <mitkException.h>
#include <mitkIOUtil.h>
#include <mitkRenderingManager.h>
#include <mitkUnstructuredGrid.h>

#include <vtkCellData.h>
#include <vtkUnstructuredGrid.h>

#include <exception>
#include <string>

#include <GemFemExport.h>

#include "FemFileWriterServiceUtils.h"
#include "WorkbenchUtils.h"

namespace
{
constexpr int FEBIO_FORMAT_INDEX = 2;
}

const std::string FemExportView::VIEW_ID = "org.mitk.views.femexport";

void FemExportView::CreateQtPartControl(QWidget *parent)
{
    m_Controls.setupUi(parent);

    m_Controls.exportMeshComboBox->SetPredicate(WorkbenchUtils::createIsUnstructuredGridTypePredicate());
    m_Controls.exportMeshComboBox->SetDataStorage(this->GetDataStorage());
    // Material Mapping creates a new output node. Selecting newly added grids
    // makes that output immediately available when the user opens this tab.
    m_Controls.exportMeshComboBox->SetAutoSelectNewItems(true);

    connect(m_Controls.exportMeshComboBox, &QmitkDataStorageComboBox::OnSelectionChanged,
            this, [this](const mitk::DataNode*) { updateExportControls(); });
    connect(m_Controls.exportFormatComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(updateExportControls()));
    connect(m_Controls.exportMaterialMethodComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(exportMaterialMethodSelectionChanged(int)));
    connect(m_Controls.exportFemButton, SIGNAL(clicked()), this, SLOT(exportFemModelClicked()));

    m_Controls.exportFormatComboBox->setCurrentIndex(FEBIO_FORMAT_INDEX);
    updateExportControls();
}

void FemExportView::SetFocus()
{
    updateExportControls();
}

void FemExportView::updateExportControls()
{
    const auto meshNode = m_Controls.exportMeshComboBox->GetSelectedNode();
    auto *mesh = meshNode == nullptr ? nullptr : dynamic_cast<mitk::UnstructuredGrid*>(meshNode->GetData());
    auto *grid = mesh == nullptr ? nullptr : mesh->GetVtkUnstructuredGrid();
    const bool isFebio = m_Controls.exportFormatComboBox->currentIndex() == FEBIO_FORMAT_INDEX;

    const QString selectedMethod = m_Controls.exportMaterialMethodComboBox->currentText();
    m_Controls.exportMaterialMethodComboBox->blockSignals(true);
    m_Controls.exportMaterialMethodComboBox->clear();

    if (grid != nullptr && grid->GetCellData() != nullptr)
    {
        const auto addMethodIfAvailable = [this, grid](gem::io::MaterialMappingMethod method,
                                                       const QString &label)
        {
            if (grid->GetCellData()->GetArray(gem::io::GetMaterialArrayName(method)) != nullptr)
            {
                m_Controls.exportMaterialMethodComboBox->addItem(label, static_cast<int>(method));
            }
        };

        addMethodIfAvailable(gem::io::MaterialMappingMethod::MethodA, "Method A");
        addMethodIfAvailable(gem::io::MaterialMappingMethod::MethodB, "Method B");
        if (isFebio)
        {
            addMethodIfAvailable(gem::io::MaterialMappingMethod::MethodE, "Method E");
        }
    }

    const int previousMethodIndex = m_Controls.exportMaterialMethodComboBox->findText(selectedMethod);
    if (previousMethodIndex >= 0)
    {
        m_Controls.exportMaterialMethodComboBox->setCurrentIndex(previousMethodIndex);
    }
    m_Controls.exportMaterialMethodComboBox->blockSignals(false);

    updateExportPreview();

    std::string reason;
    const bool meshIsExportable = grid != nullptr
      && (isFebio ? gem::io::CanExportFebioMesh(grid, &reason)
                  : gem::io::CanExportFemMesh(grid, &reason));
    const bool hasMaterialMethod = m_Controls.exportMaterialMethodComboBox->count() > 0;
    m_Controls.exportFemButton->setEnabled(meshIsExportable && hasMaterialMethod);

    if (meshNode == nullptr)
    {
        m_Controls.exportStatusLabel->setText("Select a material-mapped volume mesh to export.");
    }
    else if (!meshIsExportable)
    {
        m_Controls.exportStatusLabel->setText(QString::fromStdString(reason));
    }
    else if (!hasMaterialMethod)
    {
        m_Controls.exportStatusLabel->setText(
          "The selected mesh has no supported element material map for this format.");
    }
    else
    {
        m_Controls.exportStatusLabel->setText(
          isFebio
            ? "FEBio exports the selected continuous element material map."
            : "Abaqus and ANSYS discretize the selected element material map into material cards.");
    }
}

void FemExportView::exportMaterialMethodSelectionChanged(int)
{
    updateExportPreview();
}

void FemExportView::updateExportPreview()
{
    const auto meshNode = m_Controls.exportMeshComboBox->GetSelectedNode();
    if (meshNode == nullptr)
    {
        return;
    }

    const auto methodData = m_Controls.exportMaterialMethodComboBox->currentData();
    if (!methodData.isValid())
    {
        return;
    }

    const auto method = static_cast<gem::io::MaterialMappingMethod>(methodData.toInt());
    if (method != gem::io::MaterialMappingMethod::MethodA
        && method != gem::io::MaterialMappingMethod::MethodB
        && method != gem::io::MaterialMappingMethod::MethodE)
    {
        return;
    }

    // The export selection is also the 3-D preview selection, so the user sees
    // exactly the continuous element field that will be serialized.
    WorkbenchUtils::configureUnstructuredGridForRendering(meshNode);
    if (!WorkbenchUtils::activateUnstructuredGridCellData(
            meshNode, gem::io::GetMaterialArrayName(method)))
    {
        return;
    }

    meshNode->SetVisibility(true);
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void FemExportView::exportFemModelClicked()
{
    const auto meshNode = m_Controls.exportMeshComboBox->GetSelectedNode();
    auto *mesh = meshNode == nullptr ? nullptr : dynamic_cast<mitk::UnstructuredGrid*>(meshNode->GetData());
    auto *grid = mesh == nullptr ? nullptr : mesh->GetVtkUnstructuredGrid();
    const bool isFebio = m_Controls.exportFormatComboBox->currentIndex() == FEBIO_FORMAT_INDEX;

    std::string reason;
    const bool meshIsExportable = grid != nullptr
      && (isFebio ? gem::io::CanExportFebioMesh(grid, &reason)
                  : gem::io::CanExportFemMesh(grid, &reason));
    if (!meshIsExportable || m_Controls.exportMaterialMethodComboBox->currentText().isEmpty())
    {
        QMessageBox::warning(nullptr, "Invalid FEM export input",
                             QString::fromStdString(reason.empty()
                               ? "Select a material-mapped tetrahedral volume mesh and a supported material map."
                               : reason));
        return;
    }

    QString extension;
    QString filter;
    switch (m_Controls.exportFormatComboBox->currentIndex())
    {
      case 0:
        extension = ".inp";
        filter = tr("Abaqus input deck (*.inp)");
        break;
      case 1:
        extension = ".cdb";
        filter = tr("ANSYS Mechanical APDL command file (*.cdb)");
        break;
      default:
        extension = ".feb";
        filter = tr("FEBio model (*.feb)");
        break;
    }

    QString fileName = QFileDialog::getSaveFileName(
      nullptr, tr("Export mapped FEM model"), QString(), filter);
    if (fileName.isEmpty())
    {
        return;
    }
    if (!fileName.endsWith(extension, Qt::CaseInsensitive))
    {
        fileName += extension;
    }

    try
    {
        mitk::IFileWriter::Options options = isFebio
          ? gem::io::writer_options::FebioDefaults()
          : gem::io::writer_options::Defaults();
        options[gem::io::writer_options::MaterialMethod] =
          m_Controls.exportMaterialMethodComboBox->currentText().toStdString();
        mitk::IOUtil::Save(mesh, fileName.toStdString(), options);
        QMessageBox::information(
          nullptr, "FEM export complete",
          tr("The mapped FEM model was written to:\n%1").arg(QFileInfo(fileName).absoluteFilePath()));
    }
    catch (const mitk::Exception &exception)
    {
        QMessageBox::warning(nullptr, "FEM export failed", exception.GetDescription());
    }
    catch (const std::exception &exception)
    {
        QMessageBox::warning(nullptr, "FEM export failed", exception.what());
    }
}
