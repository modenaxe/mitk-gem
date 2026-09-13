#include <GemFemExport.h>

#include <mitkIOUtil.h>
#include <mitkIFileWriter.h>
#include <mitkUnstructuredGrid.h>

#include <vtkCellData.h>
#include <vtkDoubleArray.h>
#include <vtkPoints.h>
#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
  std::string Trim(const std::string &value)
  {
    const std::string whitespace = " \t\r\n";
    const std::size_t begin = value.find_first_not_of(whitespace);
    if (begin == std::string::npos)
      return std::string();
    return value.substr(begin, value.find_last_not_of(whitespace) - begin + 1);
  }

  std::vector<std::string> Split(const std::string &line)
  {
    std::vector<std::string> fields;
    std::istringstream stream(line);
    std::string field;
    while (std::getline(stream, field, ','))
      fields.push_back(Trim(field));
    return fields;
  }

  std::string ReadTextFile(const char *path)
  {
    std::ifstream input(path, std::ios::in | std::ios::binary);
    if (!input.is_open())
      throw std::runtime_error(std::string("Could not read fixture: ") + path);
    std::ostringstream contents;
    contents << input.rdbuf();
    std::string result = contents.str();
    // Keep fixture comparisons independent of the checkout's newline policy.
    std::string::size_type position = 0;
    while ((position = result.find("\r\n", position)) != std::string::npos)
      result.replace(position, 2, "\n");
    return result;
  }

  vtkSmartPointer<vtkUnstructuredGrid> ReadLegacyReferenceMesh(const char *path)
  {
    std::ifstream input(path);
    if (!input.is_open())
      throw std::runtime_error(std::string("Could not read legacy mesh fixture: ") + path);

    vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
    vtkSmartPointer<vtkUnstructuredGrid> grid = vtkSmartPointer<vtkUnstructuredGrid>::New();
    vtkSmartPointer<vtkDoubleArray> methodA = vtkSmartPointer<vtkDoubleArray>::New();
    vtkSmartPointer<vtkDoubleArray> methodB = vtkSmartPointer<vtkDoubleArray>::New();
    methodA->SetName(gem::io::GetMaterialArrayName(gem::io::MaterialMappingMethod::MethodA));
    methodB->SetName(gem::io::GetMaterialArrayName(gem::io::MaterialMappingMethod::MethodB));

    enum class Section
    {
      None,
      Nodes,
      Elements
    };
    Section section = Section::None;
    std::map<long long, vtkIdType> sourceNodeIds;
    std::string line;
    while (std::getline(input, line))
    {
      const std::string trimmed = Trim(line);
      if (trimmed == "#BEGIN NODES")
      {
        section = Section::Nodes;
        continue;
      }
      if (trimmed == "#BEGIN ELEMENTS 4")
      {
        section = Section::Elements;
        continue;
      }
      if (trimmed.rfind("#END", 0) == 0)
      {
        section = Section::None;
        continue;
      }
      if (trimmed.empty() || trimmed[0] == '#')
        continue;

      const std::vector<std::string> fields = Split(trimmed);
      if (section == Section::Nodes)
      {
        if (fields.size() != 5)
          throw std::runtime_error("Legacy node fixture must have id, x, y, z, and method C.");
        const long long sourceId = std::stoll(fields[0]);
        const vtkIdType vtkId = points->InsertNextPoint(std::stod(fields[1]), std::stod(fields[2]), std::stod(fields[3]));
        sourceNodeIds[sourceId] = vtkId;
      }
      else if (section == Section::Elements)
      {
        if (fields.size() != 7)
          throw std::runtime_error("Legacy tetra fixture must have id, four nodes, method A, and method B.");
        vtkIdType pointIds[4] = {};
        for (int i = 0; i < 4; ++i)
          pointIds[i] = sourceNodeIds.at(std::stoll(fields[static_cast<std::size_t>(i + 1)]));
        grid->InsertNextCell(VTK_TETRA, 4, pointIds);
        methodA->InsertNextValue(std::stod(fields[5]));
        methodB->InsertNextValue(std::stod(fields[6]));
      }
    }

    grid->SetPoints(points);
    grid->GetCellData()->AddArray(methodA);
    grid->GetCellData()->AddArray(methodB);
    return grid;
  }

  vtkSmartPointer<vtkUnstructuredGrid> CreateQuadraticTetra()
  {
    vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
    const std::array<std::array<double, 3>, 10> coordinates = {{{0.0, 0.0, 0.0},
                                                                {1.0, 0.0, 0.0},
                                                                {0.0, 1.0, 0.0},
                                                                {0.0, 0.0, 1.0},
                                                                {0.5, 0.0, 0.0},
                                                                {0.5, 0.5, 0.0},
                                                                {0.0, 0.5, 0.0},
                                                                {0.0, 0.0, 0.5},
                                                                {0.5, 0.0, 0.5},
                                                                {0.0, 0.5, 0.5}}};
    for (std::array<std::array<double, 3>, 10>::const_iterator point = coordinates.begin(); point != coordinates.end(); ++point)
      points->InsertNextPoint((*point)[0], (*point)[1], (*point)[2]);

    vtkIdType pointIds[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    vtkSmartPointer<vtkUnstructuredGrid> grid = vtkSmartPointer<vtkUnstructuredGrid>::New();
    grid->SetPoints(points);
    grid->InsertNextCell(VTK_QUADRATIC_TETRA, 10, pointIds);

    vtkSmartPointer<vtkDoubleArray> methodA = vtkSmartPointer<vtkDoubleArray>::New();
    methodA->SetName(gem::io::GetMaterialArrayName(gem::io::MaterialMappingMethod::MethodA));
    methodA->InsertNextValue(1234.0);
    grid->GetCellData()->AddArray(methodA);
    return grid;
  }

  vtkSmartPointer<vtkUnstructuredGrid> CreateTriangleWithMaterial()
  {
    vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
    points->InsertNextPoint(0.0, 0.0, 0.0);
    points->InsertNextPoint(1.0, 0.0, 0.0);
    points->InsertNextPoint(0.0, 1.0, 0.0);
    vtkIdType pointIds[3] = {0, 1, 2};

    vtkSmartPointer<vtkUnstructuredGrid> grid = vtkSmartPointer<vtkUnstructuredGrid>::New();
    grid->SetPoints(points);
    grid->InsertNextCell(VTK_TRIANGLE, 3, pointIds);
    vtkSmartPointer<vtkDoubleArray> methodA = vtkSmartPointer<vtkDoubleArray>::New();
    methodA->SetName(gem::io::GetMaterialArrayName(gem::io::MaterialMappingMethod::MethodA));
    methodA->InsertNextValue(100.0);
    grid->GetCellData()->AddArray(methodA);
    return grid;
  }

  template <typename TCallable>
  bool ThrowsInvalidArgument(const TCallable &callable)
  {
    try
    {
      callable();
    }
    catch (const std::invalid_argument &)
    {
      return true;
    }
    return false;
  }

  bool Contains(const std::string &text, const std::string &fragment)
  {
    return text.find(fragment) != std::string::npos;
  }

  std::size_t CountOccurrences(const std::string &text, const std::string &fragment)
  {
    std::size_t count = 0;
    std::string::size_type position = 0;
    while ((position = text.find(fragment, position)) != std::string::npos)
    {
      ++count;
      position += fragment.size();
    }
    return count;
  }

  class TestContext
  {
  public:
    void Check(bool condition, const std::string &description)
    {
      std::cout << (condition ? "[PASS] " : "[FAIL] ") << description << '\n';
      if (!condition)
        ++m_Failures;
    }

    int Result() const
    {
      return m_Failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }

  private:
    int m_Failures = 0;
  };
}

int main(int argc, char *argv[])
{
  TestContext test;

  test.Check(argc == 6, "The legacy, golden, and service-output paths were provided");
  if (argc != 6)
    return EXIT_FAILURE;

  vtkSmartPointer<vtkUnstructuredGrid> grid = ReadLegacyReferenceMesh(argv[1]);
  std::string reason;
  test.Check(gem::io::CanExportFemMesh(grid, &reason), "Mapped linear tetrahedra pass validation");
  test.Check(reason.empty(), "Successful validation clears the reason");

  gem::io::ExportOptions options;
  options.maxMaterialDefinitions = 2;
  options.poissonRatio = 0.3;

  std::ostringstream abaqus;
  gem::io::WriteAbaqus(abaqus, grid, options);
  test.Check(abaqus.str() == ReadTextFile(argv[2]),
             "Abaqus output matches the corrected legacy-reference fixture");

  std::ostringstream ansys;
  gem::io::WriteAnsys(ansys, grid, options);
  test.Check(ansys.str() == ReadTextFile(argv[3]),
             "ANSYS output matches the corrected legacy-reference fixture");

  mitk::UnstructuredGrid::Pointer mitkGrid = mitk::UnstructuredGrid::New();
  mitkGrid->SetVtkUnstructuredGrid(grid);
  mitk::IFileWriter::Options writerOptions;
  writerOptions["Material mapping method"] = std::string("Method A");
  writerOptions["Maximum material definitions"] = 2;
  writerOptions["Poisson's ratio"] = 0.3;
  mitk::IOUtil::Save(mitkGrid.GetPointer(), argv[4], writerOptions);
  mitk::IOUtil::Save(mitkGrid.GetPointer(), argv[5], writerOptions);
  test.Check(ReadTextFile(argv[4]) == ReadTextFile(argv[2]),
             "MITK Save As selects the registered Abaqus writer and applies its options");
  test.Check(ReadTextFile(argv[5]) == ReadTextFile(argv[3]),
             "MITK Save As selects the registered ANSYS writer and applies its options");
  std::remove(argv[4]);
  std::remove(argv[5]);

  std::ostringstream abaqusAgain;
  gem::io::WriteAbaqus(abaqusAgain, grid, options);
  test.Check(abaqusAgain.str() == abaqus.str(), "Abaqus output is deterministic");

  options.materialMappingMethod = gem::io::MaterialMappingMethod::MethodB;
  std::ostringstream methodB;
  gem::io::WriteAbaqus(methodB, grid, options);
  test.Check(Contains(methodB.str(), "** Material mapping method: B\n"), "Method B is selected explicitly");
  test.Check(Contains(methodB.str(), "*Elset, elset=GEM_MAT_1\n3\n"),
             "Method B assigns its minimum element to the first material");
  test.Check(Contains(methodB.str(), "*Elset, elset=GEM_MAT_2\n1, 2\n"),
             "The exact bin midpoint and maximum map to the upper material");

  vtkSmartPointer<vtkUnstructuredGrid> sparseLevels = vtkSmartPointer<vtkUnstructuredGrid>::New();
  sparseLevels->DeepCopy(grid);
  vtkDataArray *sparseMethodA = sparseLevels->GetCellData()->GetArray(
    gem::io::GetMaterialArrayName(gem::io::MaterialMappingMethod::MethodA));
  sparseMethodA->SetComponent(0, 0, 100.0);
  sparseMethodA->SetComponent(1, 0, 100.0);
  sparseMethodA->SetComponent(2, 0, 300.0);
  options.materialMappingMethod = gem::io::MaterialMappingMethod::MethodA;
  options.maxMaterialDefinitions = 3;
  std::ostringstream sparseOutput;
  gem::io::WriteAbaqus(sparseOutput, sparseLevels, options);
  test.Check(CountOccurrences(sparseOutput.str(), "*Material, name=") == 2,
             "Unused quantization levels are removed from the material table");
  test.Check(Contains(sparseOutput.str(), "*Elset, elset=GEM_MAT_1\n1, 2\n") &&
               Contains(sparseOutput.str(), "*Elset, elset=GEM_MAT_2\n3\n"),
             "Element assignments are remapped when empty material levels are compressed");

  vtkSmartPointer<vtkUnstructuredGrid> quadratic = CreateQuadraticTetra();
  options.materialMappingMethod = gem::io::MaterialMappingMethod::MethodA;
  std::ostringstream quadraticAbaqus;
  std::ostringstream quadraticAnsys;
  gem::io::WriteAbaqus(quadraticAbaqus, quadratic, options);
  gem::io::WriteAnsys(quadraticAnsys, quadratic, options);
  test.Check(Contains(quadraticAbaqus.str(), "*Element, type=C3D10\n1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10\n"),
             "Quadratic VTK tetrahedra map to Abaqus C3D10 without reordering");
  test.Check(Contains(quadraticAnsys.str(), "ET,1,SOLID187\n"),
             "Quadratic VTK tetrahedra map to ANSYS SOLID187");
  test.Check(Contains(quadraticAnsys.str(), "EN,1,1,2,3,4,5,6,7,8\nEMORE,9,10\n"),
             "ANSYS nodes beyond EN's eight-node limit are emitted with EMORE");

  vtkSmartPointer<vtkUnstructuredGrid> invalidMaterial = vtkSmartPointer<vtkUnstructuredGrid>::New();
  invalidMaterial->DeepCopy(grid);
  vtkDataArray *invalidArray = invalidMaterial->GetCellData()->GetArray(
    gem::io::GetMaterialArrayName(gem::io::MaterialMappingMethod::MethodA));
  invalidArray->SetComponent(0, 0, 0.0);
  test.Check(
    ThrowsInvalidArgument([&]() {
      std::ostringstream output;
      gem::io::WriteAbaqus(output, invalidMaterial, options);
    }),
    "Zero Young's modulus is rejected");

  vtkSmartPointer<vtkUnstructuredGrid> missingMaterials = vtkSmartPointer<vtkUnstructuredGrid>::New();
  missingMaterials->DeepCopy(grid);
  missingMaterials->GetCellData()->RemoveArray(gem::io::GetMaterialArrayName(gem::io::MaterialMappingMethod::MethodA));
  missingMaterials->GetCellData()->RemoveArray(gem::io::GetMaterialArrayName(gem::io::MaterialMappingMethod::MethodB));
  test.Check(!gem::io::CanExportFemMesh(missingMaterials, &reason),
             "A tetrahedral mesh without method A or B is not offered for FEM export");
  test.Check(!reason.empty(), "Rejected mesh validation explains the failure");

  vtkSmartPointer<vtkUnstructuredGrid> methodBOnly = vtkSmartPointer<vtkUnstructuredGrid>::New();
  methodBOnly->DeepCopy(grid);
  methodBOnly->GetCellData()->RemoveArray(gem::io::GetMaterialArrayName(gem::io::MaterialMappingMethod::MethodA));
  test.Check(gem::io::CanExportFemMesh(methodBOnly, &reason),
             "A valid method-B-only mesh remains eligible for FEM export");
  options.materialMappingMethod = gem::io::MaterialMappingMethod::MethodA;
  options.maxMaterialDefinitions = 2;
  test.Check(
    ThrowsInvalidArgument([&]() {
      std::ostringstream output;
      gem::io::WriteAbaqus(output, methodBOnly, options);
    }),
    "The exporter rejects a missing explicitly selected method instead of silently switching methods");

  vtkSmartPointer<vtkUnstructuredGrid> triangle = CreateTriangleWithMaterial();
  test.Check(!gem::io::CanExportFemMesh(triangle, &reason), "Surface triangles are rejected as FEM volume meshes");

  vtkSmartPointer<vtkUnstructuredGrid> zeroVolume = vtkSmartPointer<vtkUnstructuredGrid>::New();
  zeroVolume->DeepCopy(grid);
  zeroVolume->GetPoints()->SetPoint(3, 1.0, 1.0, 0.0);
  test.Check(!gem::io::CanExportFemMesh(zeroVolume, &reason),
             "A tetrahedron with four coplanar corner nodes is rejected");

  options.maxMaterialDefinitions = 0;
  test.Check(
    ThrowsInvalidArgument([&]() {
      std::ostringstream output;
      gem::io::WriteAnsys(output, grid, options);
    }),
    "A zero material-card limit is rejected");

  options.maxMaterialDefinitions = 2;
  options.poissonRatio = 0.5;
  test.Check(
    ThrowsInvalidArgument([&]() {
      std::ostringstream output;
      gem::io::WriteAnsys(output, grid, options);
    }),
    "An incompressible-limit Poisson ratio is rejected");

  return test.Result();
}
