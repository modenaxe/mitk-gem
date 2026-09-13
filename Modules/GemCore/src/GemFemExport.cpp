#include "GemFemExport.h"

#include <vtkCell.h>
#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkUnstructuredGrid.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
  const char *const MethodAArrayName = "GEM_METHOD_A";
  const char *const MethodBArrayName = "GEM_METHOD_B";

  struct MaterialTable
  {
    std::vector<double> youngsModuli;
    std::vector<unsigned int> elementMaterialIds;
  };

  struct PreparedMesh
  {
    int cellType = VTK_EMPTY_CELL;
    vtkIdType nodesPerElement = 0;
    MaterialTable materials;
  };

  std::string CellLabel(vtkIdType cellId)
  {
    return "cell " + std::to_string(static_cast<long long>(cellId + 1));
  }

  void ValidateOptions(const gem::io::ExportOptions &options)
  {
    if (options.maxMaterialDefinitions == 0)
      throw std::invalid_argument("Maximum material definitions must be greater than zero.");

    if (!std::isfinite(options.poissonRatio) || options.poissonRatio <= -1.0 || options.poissonRatio >= 0.5)
      throw std::invalid_argument("Poisson's ratio must be finite and strictly between -1 and 0.5.");
  }

  void ValidateTopology(vtkUnstructuredGrid *grid)
  {
    if (grid == nullptr)
      throw std::invalid_argument("The FEM exporter received a null mesh.");

    if (grid->GetNumberOfPoints() == 0 || grid->GetNumberOfCells() == 0)
      throw std::invalid_argument("The FEM exporter requires a non-empty tetrahedral volume mesh.");

    const int firstCellType = grid->GetCellType(0);
    if (firstCellType != VTK_TETRA && firstCellType != VTK_QUADRATIC_TETRA)
      throw std::invalid_argument(
        "Only VTK_TETRA and VTK_QUADRATIC_TETRA volume meshes can be exported; " + CellLabel(0) +
        " has VTK cell type " + std::to_string(firstCellType) + ".");

    const vtkIdType expectedPointCount = firstCellType == VTK_TETRA ? 4 : 10;
    for (vtkIdType cellId = 0; cellId < grid->GetNumberOfCells(); ++cellId)
    {
      const int cellType = grid->GetCellType(cellId);
      if (cellType != firstCellType)
        throw std::invalid_argument("Mixed element types are not supported; " + CellLabel(cellId) +
                                    " differs from the first element.");

      vtkCell *cell = grid->GetCell(cellId);
      if (cell == nullptr || cell->GetNumberOfPoints() != expectedPointCount)
        throw std::invalid_argument(CellLabel(cellId) + " does not have the expected number of tetrahedral nodes.");

      std::vector<vtkIdType> pointIds;
      pointIds.reserve(static_cast<std::size_t>(expectedPointCount));
      for (vtkIdType localPointId = 0; localPointId < expectedPointCount; ++localPointId)
      {
        const vtkIdType pointId = cell->GetPointId(localPointId);
        if (pointId < 0 || pointId >= grid->GetNumberOfPoints())
          throw std::invalid_argument(CellLabel(cellId) + " references an out-of-range node.");
        pointIds.push_back(pointId);
      }
      std::sort(pointIds.begin(), pointIds.end());
      if (std::adjacent_find(pointIds.begin(), pointIds.end()) != pointIds.end())
        throw std::invalid_argument(CellLabel(cellId) + " contains a repeated node and is degenerate.");

      double corners[4][3] = {};
      for (vtkIdType corner = 0; corner < 4; ++corner)
        grid->GetPoint(cell->GetPointId(corner), corners[corner]);

      double maximumEdgeSquared = 0.0;
      for (int first = 0; first < 4; ++first)
      {
        for (int second = first + 1; second < 4; ++second)
        {
          const double dx = corners[second][0] - corners[first][0];
          const double dy = corners[second][1] - corners[first][1];
          const double dz = corners[second][2] - corners[first][2];
          maximumEdgeSquared = std::max(maximumEdgeSquared, dx * dx + dy * dy + dz * dz);
        }
      }

      const double ax = corners[1][0] - corners[0][0];
      const double ay = corners[1][1] - corners[0][1];
      const double az = corners[1][2] - corners[0][2];
      const double bx = corners[2][0] - corners[0][0];
      const double by = corners[2][1] - corners[0][1];
      const double bz = corners[2][2] - corners[0][2];
      const double cx = corners[3][0] - corners[0][0];
      const double cy = corners[3][1] - corners[0][1];
      const double cz = corners[3][2] - corners[0][2];
      const double sixTimesSignedVolume =
        ax * (by * cz - bz * cy) - ay * (bx * cz - bz * cx) + az * (bx * cy - by * cx);
      const double lengthScale = std::sqrt(maximumEdgeSquared);
      const double volumeTolerance =
        64.0 * std::numeric_limits<double>::epsilon() * lengthScale * lengthScale * lengthScale;
      if (!std::isfinite(maximumEdgeSquared) || !std::isfinite(sixTimesSignedVolume) ||
          std::abs(sixTimesSignedVolume) <= volumeTolerance)
        throw std::invalid_argument(CellLabel(cellId) + " has zero or numerically degenerate volume.");
    }

    for (vtkIdType pointId = 0; pointId < grid->GetNumberOfPoints(); ++pointId)
    {
      double point[3] = {};
      grid->GetPoint(pointId, point);
      if (!std::isfinite(point[0]) || !std::isfinite(point[1]) || !std::isfinite(point[2]))
        throw std::invalid_argument("Node " + std::to_string(static_cast<long long>(pointId + 1)) +
                                    " has a non-finite coordinate.");
    }
  }

  std::vector<double> ReadMaterialValues(vtkUnstructuredGrid *grid, gem::io::MaterialMappingMethod method)
  {
    const char *arrayName = gem::io::GetMaterialArrayName(method);
    vtkDataArray *materialArray = grid->GetCellData()->GetArray(arrayName);
    if (materialArray == nullptr)
      throw std::invalid_argument(std::string("The selected material mapping array '") + arrayName +
                                  "' is missing. Run Material Mapping or select the other mapped method.");

    if (materialArray->GetNumberOfComponents() != 1 || materialArray->GetNumberOfTuples() != grid->GetNumberOfCells())
      throw std::invalid_argument(std::string("Material mapping array '") + arrayName +
                                  "' must contain exactly one Young's modulus for every element.");

    std::vector<double> values;
    values.reserve(static_cast<std::size_t>(grid->GetNumberOfCells()));
    for (vtkIdType cellId = 0; cellId < grid->GetNumberOfCells(); ++cellId)
    {
      const double value = materialArray->GetComponent(cellId, 0);
      if (!std::isfinite(value) || value <= 0.0)
        throw std::invalid_argument(std::string("Material mapping array '") + arrayName + "' contains an invalid " +
                                    "Young's modulus at " + CellLabel(cellId) + "; values must be finite and positive.");
      values.push_back(value);
    }
    return values;
  }

  MaterialTable QuantizeMaterials(const std::vector<double> &values, unsigned int maxMaterialDefinitions)
  {
    MaterialTable result;
    if (values.empty())
      return result;

    const std::pair<std::vector<double>::const_iterator, std::vector<double>::const_iterator> extrema =
      std::minmax_element(values.begin(), values.end());
    const double minimum = *extrema.first;
    const double maximum = *extrema.second;

    if (minimum == maximum || maxMaterialDefinitions == 1)
    {
      result.youngsModuli.push_back(minimum == maximum ? minimum : minimum + (maximum - minimum) * 0.5);
      result.elementMaterialIds.assign(values.size(), 1);
      return result;
    }

    const std::size_t levelCount =
      std::min<std::size_t>(static_cast<std::size_t>(maxMaterialDefinitions), values.size());
    std::vector<double> levels(levelCount);
    std::vector<double> thresholds(levelCount - 1);
    for (std::size_t level = 0; level < levelCount; ++level)
    {
      levels[level] = minimum + (maximum - minimum) * static_cast<double>(level) /
                                  static_cast<double>(levelCount - 1);
      if (level > 0)
        thresholds[level - 1] = levels[level - 1] + (levels[level] - levels[level - 1]) * 0.5;
    }

    std::vector<std::size_t> uncompressedAssignments;
    uncompressedAssignments.reserve(values.size());
    std::vector<bool> usedLevels(levelCount, false);
    for (std::vector<double>::const_iterator value = values.begin(); value != values.end(); ++value)
    {
      // At an exact midpoint, select the upper level. This matches the legacy
      // scripts' lower-inclusive bin semantics and guarantees that max is kept.
      const std::size_t level = static_cast<std::size_t>(
        std::upper_bound(thresholds.begin(), thresholds.end(), *value) - thresholds.begin());
      uncompressedAssignments.push_back(level);
      usedLevels[level] = true;
    }

    std::vector<unsigned int> compressedIds(levelCount, 0);
    for (std::size_t level = 0; level < levelCount; ++level)
    {
      if (usedLevels[level])
      {
        result.youngsModuli.push_back(levels[level]);
        compressedIds[level] = static_cast<unsigned int>(result.youngsModuli.size());
      }
    }

    result.elementMaterialIds.reserve(values.size());
    for (std::vector<std::size_t>::const_iterator level = uncompressedAssignments.begin();
         level != uncompressedAssignments.end();
         ++level)
      result.elementMaterialIds.push_back(compressedIds[*level]);

    return result;
  }

  PreparedMesh Prepare(vtkUnstructuredGrid *grid, const gem::io::ExportOptions &options)
  {
    ValidateOptions(options);
    ValidateTopology(grid);

    PreparedMesh result;
    result.cellType = grid->GetCellType(0);
    result.nodesPerElement = result.cellType == VTK_TETRA ? 4 : 10;
    result.materials = QuantizeMaterials(ReadMaterialValues(grid, options.materialMappingMethod),
                                         options.maxMaterialDefinitions);
    return result;
  }

  void ConfigureNumericOutput(std::ostream &output)
  {
    output.imbue(std::locale::classic());
    output << std::scientific << std::setprecision(15);
  }

  void CheckOutput(const std::ostream &output, const char *formatName)
  {
    if (!output.good())
      throw std::runtime_error(std::string("Failed while writing the ") + formatName + " export stream.");
  }

  void WriteAbaqusElementSet(std::ostream &output,
                             unsigned int materialId,
                             const std::vector<unsigned int> &elementMaterialIds)
  {
    output << "*Elset, elset=GEM_MAT_" << materialId << "\n";
    unsigned int entriesOnLine = 0;
    for (std::size_t element = 0; element < elementMaterialIds.size(); ++element)
    {
      if (elementMaterialIds[element] != materialId)
        continue;

      if (entriesOnLine > 0)
        output << ", ";
      output << element + 1;
      ++entriesOnLine;
      if (entriesOnLine == 16)
      {
        output << "\n";
        entriesOnLine = 0;
      }
    }
    if (entriesOnLine != 0)
      output << "\n";
  }
}

const char *gem::io::GetMaterialArrayName(MaterialMappingMethod method)
{
  return method == MaterialMappingMethod::MethodA ? MethodAArrayName : MethodBArrayName;
}

bool gem::io::CanExportFemMesh(vtkUnstructuredGrid *grid, std::string *reason)
{
  try
  {
    ValidateTopology(grid);
    try
    {
      ReadMaterialValues(grid, MaterialMappingMethod::MethodA);
    }
    catch (const std::invalid_argument &)
    {
      ReadMaterialValues(grid, MaterialMappingMethod::MethodB);
    }
    if (reason != nullptr)
      reason->clear();
    return true;
  }
  catch (const std::invalid_argument &exception)
  {
    if (reason != nullptr)
      *reason = exception.what();
    return false;
  }
}

void gem::io::WriteAbaqus(std::ostream &output, vtkUnstructuredGrid *grid, const ExportOptions &options)
{
  const PreparedMesh prepared = Prepare(grid, options);
  const char methodName = options.materialMappingMethod == MaterialMappingMethod::MethodA ? 'A' : 'B';

  output << "** MITK-GEM material-mapped volume mesh\n"
         << "** Material mapping method: " << methodName << "\n"
         << "** Units are inherited from the input mesh; no unit conversion is applied.\n"
         << "*Heading\n"
         << "MITK-GEM material-mapped volume mesh\n"
         << "*Preprint, echo=NO, model=NO, history=NO, contact=NO\n";
  ConfigureNumericOutput(output);

  for (std::size_t material = 0; material < prepared.materials.youngsModuli.size(); ++material)
  {
    output << "*Material, name=GEM_MAT_" << material + 1 << "\n"
           << "*Elastic\n"
           << prepared.materials.youngsModuli[material] << ", " << options.poissonRatio << "\n";
  }

  output << "*Part, name=MITK_GEM_PART\n"
         << "*Node\n";
  for (vtkIdType pointId = 0; pointId < grid->GetNumberOfPoints(); ++pointId)
  {
    double point[3] = {};
    grid->GetPoint(pointId, point);
    output << pointId + 1 << ", " << point[0] << ", " << point[1] << ", " << point[2] << "\n";
  }

  output << "*Element, type=" << (prepared.cellType == VTK_TETRA ? "C3D4" : "C3D10") << "\n";
  for (vtkIdType cellId = 0; cellId < grid->GetNumberOfCells(); ++cellId)
  {
    vtkCell *cell = grid->GetCell(cellId);
    output << cellId + 1;
    for (vtkIdType localPointId = 0; localPointId < prepared.nodesPerElement; ++localPointId)
      output << ", " << cell->GetPointId(localPointId) + 1;
    output << "\n";
  }

  for (unsigned int materialId = 1;
       materialId <= static_cast<unsigned int>(prepared.materials.youngsModuli.size());
       ++materialId)
  {
    WriteAbaqusElementSet(output, materialId, prepared.materials.elementMaterialIds);
    output << "*Solid Section, elset=GEM_MAT_" << materialId << ", material=GEM_MAT_" << materialId << "\n,\n";
  }

  output << "*End Part\n"
         << "*Assembly, name=ASSEMBLY\n"
         << "*Instance, name=MITK_GEM_INSTANCE, part=MITK_GEM_PART\n"
         << "*End Instance\n"
         << "*End Assembly\n";
  CheckOutput(output, "Abaqus");
}

void gem::io::WriteAnsys(std::ostream &output, vtkUnstructuredGrid *grid, const ExportOptions &options)
{
  const PreparedMesh prepared = Prepare(grid, options);
  const char methodName = options.materialMappingMethod == MaterialMappingMethod::MethodA ? 'A' : 'B';

  output << "/PREP7\n"
         << "! MITK-GEM material-mapped volume mesh\n"
         << "! Material mapping method: " << methodName << "\n"
         << "! Units are inherited from the input mesh; no unit conversion is applied.\n"
         << "ET,1," << (prepared.cellType == VTK_TETRA ? "SOLID285" : "SOLID187") << "\n";
  if (prepared.cellType == VTK_TETRA)
    output << "KEYOPT,1,1,1\n";
  ConfigureNumericOutput(output);

  for (std::size_t material = 0; material < prepared.materials.youngsModuli.size(); ++material)
  {
    output << "MP,EX," << material + 1 << "," << prepared.materials.youngsModuli[material] << "\n"
           << "MP,PRXY," << material + 1 << "," << options.poissonRatio << "\n";
  }

  for (vtkIdType pointId = 0; pointId < grid->GetNumberOfPoints(); ++pointId)
  {
    double point[3] = {};
    grid->GetPoint(pointId, point);
    output << "N," << pointId + 1 << "," << point[0] << "," << point[1] << "," << point[2] << "\n";
  }

  output << "TYPE,1\n";
  if (prepared.cellType == VTK_QUADRATIC_TETRA)
    output << "SHPP,OFF\n";

  for (vtkIdType cellId = 0; cellId < grid->GetNumberOfCells(); ++cellId)
  {
    vtkCell *cell = grid->GetCell(cellId);
    output << "MAT," << prepared.materials.elementMaterialIds[static_cast<std::size_t>(cellId)] << "\n"
           << "EN," << cellId + 1;
    const vtkIdType firstCommandNodeCount = std::min<vtkIdType>(prepared.nodesPerElement, 8);
    for (vtkIdType localPointId = 0; localPointId < firstCommandNodeCount; ++localPointId)
      output << "," << cell->GetPointId(localPointId) + 1;
    output << "\n";
    if (prepared.nodesPerElement > 8)
      output << "EMORE," << cell->GetPointId(8) + 1 << "," << cell->GetPointId(9) + 1 << "\n";
  }

  if (prepared.cellType == VTK_QUADRATIC_TETRA)
    output << "SHPP,ON\n";
  output << "ALLSEL,ALL\n"
         << "CM,GEM_ELEMENTS,ELEM\n"
         << "FINISH\n";
  CheckOutput(output, "ANSYS");
}
