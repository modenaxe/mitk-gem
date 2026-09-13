#include "AbaqusFileWriterService.h"
#include "FemFileWriterServiceUtils.h"
#include "GemIOMimeTypes.h"

#include <mitkCustomMimeType.h>
#include <mitkExceptionMacro.h>
#include <mitkUnstructuredGrid.h>

#include <fstream>
#include <stdexcept>

AbaqusFileWriterService::AbaqusFileWriterService()
  : mitk::AbstractFileWriter(mitk::UnstructuredGrid::GetStaticNameOfClass(),
                             GemIOMimeTypes::ABAQUS_MIMETYPE(),
                             "Abaqus material-mapped tetrahedral mesh")
{
    SetDefaultOptions(gem::io::writer_options::Defaults());
    RegisterService();
}

AbaqusFileWriterService::AbaqusFileWriterService(const AbaqusFileWriterService &other)
  : mitk::AbstractFileWriter(other)
{
}

AbaqusFileWriterService::~AbaqusFileWriterService() = default;

void AbaqusFileWriterService::Write()
{
    const mitk::UnstructuredGrid *input = dynamic_cast<const mitk::UnstructuredGrid *>(GetInput());
    if (input == nullptr)
        mitkThrow() << "Abaqus export requires a MITK unstructured grid.";

    ValidateOutputLocation();
    try
    {
        mitk::AbstractFileWriter::LocalFile localFile(this);
        std::ofstream output(localFile.GetFileName().c_str(), std::ios::out | std::ios::trunc);
        if (!output.is_open())
            mitkThrow() << "Could not open '" << GetOutputLocation() << "' for Abaqus export.";

        mitk::UnstructuredGrid *mutableInput = const_cast<mitk::UnstructuredGrid *>(input);
        gem::io::WriteAbaqus(output,
                             mutableInput->GetVtkUnstructuredGrid(),
                             gem::io::writer_options::Read(GetOptions()));
        output.flush();
        if (!output.good())
            mitkThrow() << "Failed to finish writing the Abaqus export to '" << GetOutputLocation() << "'.";
    }
    catch (const mitk::Exception &)
    {
        throw;
    }
    catch (const std::exception &exception)
    {
        mitkThrow() << "Abaqus export failed: " << exception.what();
    }
}

mitk::IFileWriter::ConfidenceLevel AbaqusFileWriterService::GetConfidenceLevel() const
{
    if (mitk::AbstractFileWriter::GetConfidenceLevel() == Unsupported)
        return Unsupported;

    const mitk::UnstructuredGrid *input = dynamic_cast<const mitk::UnstructuredGrid *>(GetInput());
    if (input == nullptr)
        return Unsupported;

    mitk::UnstructuredGrid *mutableInput = const_cast<mitk::UnstructuredGrid *>(input);
    return gem::io::CanExportFemMesh(mutableInput->GetVtkUnstructuredGrid()) ? Supported : Unsupported;
}

AbaqusFileWriterService *AbaqusFileWriterService::Clone() const
{
    return new AbaqusFileWriterService(*this);
}
