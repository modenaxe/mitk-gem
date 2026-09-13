#pragma once

#include <GemFemExport.h>

#include <mitkIFileWriter.h>

#include <stdexcept>
#include <string>
#include <typeinfo>
#include <vector>

namespace gem
{
  namespace io
  {
    namespace writer_options
    {
      const char *const MaterialMethod = "Material mapping method";
      const char *const MaterialMethodEnum = "Material mapping method.enum";
      const char *const MaximumMaterials = "Maximum material definitions";
      const char *const PoissonRatio = "Poisson's ratio";

      inline mitk::IFileWriter::Options Defaults()
      {
        mitk::IFileWriter::Options options;
        options[MaterialMethod] = std::string("Method A");
        options[MaterialMethodEnum] = std::vector<std::string>{"Method A", "Method B"};
        // Use a signed int because MITK renders this option with QSpinBox.
        // QSpinBox cannot represent the full unsigned-int range.
        options[MaximumMaterials] = 500;
        options[PoissonRatio] = 0.3;
        return options;
      }

      inline ExportOptions Read(const mitk::IFileWriter::Options &options)
      {
        ExportOptions result;

        const mitk::IFileWriter::Options::const_iterator method = options.find(MaterialMethod);
        if (method != options.end())
        {
          const std::string value = method->second.ToString();
          if (value == "Method A")
            result.materialMappingMethod = MaterialMappingMethod::MethodA;
          else if (value == "Method B")
            result.materialMappingMethod = MaterialMappingMethod::MethodB;
          else
            throw std::invalid_argument("Material mapping method must be 'Method A' or 'Method B'.");
        }

        const mitk::IFileWriter::Options::const_iterator maximumMaterials = options.find(MaximumMaterials);
        if (maximumMaterials != options.end())
        {
          if (maximumMaterials->second.Type() == typeid(unsigned int))
            result.maxMaterialDefinitions = us::any_cast<unsigned int>(maximumMaterials->second);
          else if (maximumMaterials->second.Type() == typeid(int))
          {
            const int value = us::any_cast<int>(maximumMaterials->second);
            if (value < 0)
              throw std::invalid_argument("Maximum material definitions cannot be negative.");
            result.maxMaterialDefinitions = static_cast<unsigned int>(value);
          }
          else
            throw std::invalid_argument("Maximum material definitions must be an integer.");
        }

        const mitk::IFileWriter::Options::const_iterator poissonRatio = options.find(PoissonRatio);
        if (poissonRatio != options.end())
        {
          if (poissonRatio->second.Type() == typeid(double))
            result.poissonRatio = us::any_cast<double>(poissonRatio->second);
          else if (poissonRatio->second.Type() == typeid(float))
            result.poissonRatio = static_cast<double>(us::any_cast<float>(poissonRatio->second));
          else
            throw std::invalid_argument("Poisson's ratio must be a number.");
        }

        return result;
      }
    }
  }
}
