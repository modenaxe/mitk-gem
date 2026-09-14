#include "FebioFileWriterService.h"

#include "FemFileWriterServiceUtils.h"
#include "GemIOMimeTypes.h"

#include <mitkCustomMimeType.h>
#include <mitkExceptionMacro.h>
#include <mitkUnstructuredGrid.h>

#include <fstream>
#include <stdexcept>

FebioFileWriterService::FebioFileWriterService()
  : mitk::AbstractFileWriter(mitk::UnstructuredGrid::GetStaticNameOfClass(),
                             GemIOMimeTypes::FEBIO_MIMETYPE(),
                             "FEBio model with material-mapped tetrahedral mesh")
{
    SetDefaultOptions(gem::io::writer_options::FebioDefaults());
    RegisterService();
}

FebioFileWriterService::FebioFileWriterService(const FebioFileWriterService &other)
  : mitk::AbstractFileWriter(other)
{
}

FebioFileWriterService::~FebioFileWriterService() = default;

void FebioFileWriterService::Write()
{
    const mitk::UnstructuredGrid *input = dynamic_cast<const mitk::UnstructuredGrid *>(GetInput());
    if (input == nullptr)
        mitkThrow() << "FEBio export requires a MITK unstructured grid.";

    ValidateOutputLocation();
    try
    {
        mitk::AbstractFileWriter::LocalFile localFile(this);
        std::ofstream output(localFile.GetFileName().c_str(), std::ios::out | std::ios::trunc);
        if (!output.is_open())
            mitkThrow() << "Could not open '" << GetOutputLocation() << "' for FEBio export.";

        mitk::UnstructuredGrid *mutableInput = const_cast<mitk::UnstructuredGrid *>(input);
        gem::io::WriteFebio(output,
                            mutableInput->GetVtkUnstructuredGrid(),
                            gem::io::writer_options::ReadFebio(GetOptions()));
        output.flush();
        if (!output.good())
            mitkThrow() << "Failed to finish writing the FEBio export to '" << GetOutputLocation() << "'.";
    }
    catch (const mitk::Exception &)
    {
        throw;
    }
    catch (const std::exception &exception)
    {
        mitkThrow() << "FEBio export failed: " << exception.what();
    }
}

mitk::IFileWriter::ConfidenceLevel FebioFileWriterService::GetConfidenceLevel() const
{
    if (mitk::AbstractFileWriter::GetConfidenceLevel() == Unsupported)
        return Unsupported;

    const mitk::UnstructuredGrid *input = dynamic_cast<const mitk::UnstructuredGrid *>(GetInput());
    if (input == nullptr)
        return Unsupported;

    mitk::UnstructuredGrid *mutableInput = const_cast<mitk::UnstructuredGrid *>(input);
    return gem::io::CanExportFebioMesh(mutableInput->GetVtkUnstructuredGrid()) ? Supported : Unsupported;
}

FebioFileWriterService *FebioFileWriterService::Clone() const
{
    return new FebioFileWriterService(*this);
}
