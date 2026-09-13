#pragma once

#include <mitkAbstractFileWriter.h>

class AbaqusFileWriterService : public mitk::AbstractFileWriter
{
public:
    AbaqusFileWriterService();
    ~AbaqusFileWriterService() override;

    using mitk::AbstractFileWriter::Write;
    void Write() override;
    ConfidenceLevel GetConfidenceLevel() const override;

private:
    AbaqusFileWriterService(const AbaqusFileWriterService &other);
    AbaqusFileWriterService *Clone() const override;
};
