#pragma once

#include <mitkAbstractFileWriter.h>

class FebioFileWriterService : public mitk::AbstractFileWriter
{
public:
    FebioFileWriterService();
    ~FebioFileWriterService() override;

    using mitk::AbstractFileWriter::Write;
    void Write() override;
    ConfidenceLevel GetConfidenceLevel() const override;

private:
    FebioFileWriterService(const FebioFileWriterService &other);
    FebioFileWriterService *Clone() const override;
};
