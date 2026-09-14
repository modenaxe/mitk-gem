#include <GemFemExport.h>

#include <mitkIOUtil.h>
#include <mitkIFileWriter.h>
#include <mitkUnstructuredGrid.h>

#include <vtkCellData.h>
#include <vtkDoubleArray.h>
#include <vtkPointData.h>
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

  vtkSmartPointer<vtkUnstructuredGrid> CreateInvertedTetraWithMaterial()
  {
    vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
    points->InsertNextPoint(0.0, 0.0, 0.0);
    points->InsertNextPoint(1.0, 0.0, 0.0);
    points->InsertNextPoint(0.0, 1.0, 0.0);
    points->InsertNextPoint(0.0, 0.0, 1.0);
    vtkIdType pointIds[4] = {0, 2, 1, 3};

    vtkSmartPointer<vtkUnstructuredGrid> grid = vtkSmartPointer<vtkUnstructuredGrid>::New();
    grid->SetPoints(points);
    grid->InsertNextCell(VTK_TETRA, 4, pointIds);
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

  test.Check(argc == 8, "The legacy, golden, and service-output paths were provided");
  if (argc != 8)
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

  gem::io::FebioExportOptions febioOptions;
  std::ostringstream febio;
  gem::io::WriteFebio(febio, grid, febioOptions);
  test.Check(febio.str() == ReadTextFile(argv[4]),
             "FEBio output matches the native FEBio 4 material-map fixture");
  test.Check(Contains(febio.str(), "<E type=\"map\">GEM_METHOD_A</E>"),
             "FEBio material binds Young's modulus to the selected element map");
  test.Check(CountOccurrences(febio.str(), "<e lid=") == 3,
             "FEBio output preserves one continuous modulus value per element");

  mitk::UnstructuredGrid::Pointer mitkGrid = mitk::UnstructuredGrid::New();
  mitkGrid->SetVtkUnstructuredGrid(grid);
  mitk::IFileWriter::Options writerOptions;
  writerOptions["Material mapping method"] = std::string("Method A");
  writerOptions["Maximum material definitions"] = 2;
  writerOptions["Poisson's ratio"] = 0.3;
  mitk::IOUtil::Save(mitkGrid.GetPointer(), argv[5], writerOptions);
  mitk::IOUtil::Save(mitkGrid.GetPointer(), argv[6], writerOptions);

  mitk::IFileWriter::Options febioWriterOptions;
  febioWriterOptions["Material mapping method"] = std::string("Method A");
  febioWriterOptions["Poisson's ratio"] = 0.3;
  febioWriterOptions["FEBio unit system"] = std::string("mm-N-s");
  febioWriterOptions["FEBio geometry scale"] = 1.0;
  febioWriterOptions["FEBio Young's modulus scale"] = 1.0;
  mitk::IOUtil::Save(mitkGrid.GetPointer(), argv[7], febioWriterOptions);
  test.Check(ReadTextFile(argv[5]) == ReadTextFile(argv[2]),
             "MITK Save As selects the registered Abaqus writer and applies its options");
  test.Check(ReadTextFile(argv[6]) == ReadTextFile(argv[3]),
             "MITK Save As selects the registered ANSYS writer and applies its options");
  test.Check(ReadTextFile(argv[7]) == ReadTextFile(argv[4]),
             "MITK Save As selects the registered FEBio writer and applies its options");
  std::remove(argv[5]);
  std::remove(argv[6]);
  std::remove(argv[7]);

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

  febioOptions.materialMappingMethod = gem::io::MaterialMappingMethod::MethodB;
  std::ostringstream febioMethodB;
  gem::io::WriteFebio(febioMethodB, grid, febioOptions);
  test.Check(Contains(febioMethodB.str(), "<E type=\"map\">GEM_METHOD_B</E>"),
             "FEBio selects method B explicitly instead of falling back to method A");
  test.Check(Contains(febioMethodB.str(), "<ElementData name=\"GEM_METHOD_B\""),
             "FEBio names the material data field after the selected map");
  test.Check(Contains(febioMethodB.str(), "<e lid=\"1\">3.00000000000000000e+02</e>") &&
               Contains(febioMethodB.str(), "<e lid=\"2\">2.00000000000000000e+02</e>") &&
               Contains(febioMethodB.str(), "<e lid=\"3\">1.00000000000000000e+02</e>"),
             "FEBio preserves the original method-B values without material binning");

  febioOptions.unitSystem = "SI";
  febioOptions.geometryScale = 2.0;
  febioOptions.youngsModulusScale = 0.01;
  std::ostringstream scaledFebio;
  gem::io::WriteFebio(scaledFebio, grid, febioOptions);
  test.Check(Contains(scaledFebio.str(), "<units>SI</units>") &&
               Contains(scaledFebio.str(), "<node id=\"2\">2.00000000000000000e+00,0.00000000000000000e+00,0.00000000000000000e+00</node>") &&
               Contains(scaledFebio.str(), "<e lid=\"1\">3.00000000000000000e+00</e>"),
             "FEBio unit and scale options are applied to the serialized model");
  febioOptions.unitSystem = "mm-N-s";
  febioOptions.geometryScale = 1.0;
  febioOptions.youngsModulusScale = 1.0;

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

  febioOptions.materialMappingMethod = gem::io::MaterialMappingMethod::MethodA;
  std::ostringstream quadraticFebio;
  gem::io::WriteFebio(quadraticFebio, quadratic, febioOptions);
  test.Check(Contains(quadraticFebio.str(),
                      "<Elements type=\"tet10\" name=\"MITK_GEM_BONE_DOMAIN\">\n"
                      "      <elem id=\"1\">1,2,3,4,5,6,7,8,9,10</elem>\n"),
             "Quadratic VTK tetrahedra map to FEBio tet10 without reordering");

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

  vtkSmartPointer<vtkUnstructuredGrid> methodEOnly = vtkSmartPointer<vtkUnstructuredGrid>::New();
  methodEOnly->DeepCopy(grid);
  vtkDataArray *methodEReference = methodEOnly->GetCellData()->GetArray(
    gem::io::GetMaterialArrayName(gem::io::MaterialMappingMethod::MethodA));
  vtkSmartPointer<vtkDoubleArray> methodEArray = vtkSmartPointer<vtkDoubleArray>::New();
  methodEArray->SetName(gem::io::GetMaterialArrayName(gem::io::MaterialMappingMethod::MethodE));
  for (vtkIdType cellId = 0; cellId < methodEOnly->GetNumberOfCells(); ++cellId)
    methodEArray->InsertNextValue(methodEReference->GetComponent(cellId, 0));
  methodEOnly->GetCellData()->RemoveArray(gem::io::GetMaterialArrayName(gem::io::MaterialMappingMethod::MethodA));
  methodEOnly->GetCellData()->RemoveArray(gem::io::GetMaterialArrayName(gem::io::MaterialMappingMethod::MethodB));
  methodEOnly->GetCellData()->AddArray(methodEArray);
  test.Check(!gem::io::CanExportFemMesh(methodEOnly, &reason),
             "Method E alone does not change the legacy Abaqus and ANSYS eligibility contract");
  test.Check(gem::io::CanExportFebioMesh(methodEOnly, &reason),
             "Method E alone is eligible for FEBio's element-wise material map");
  febioOptions.materialMappingMethod = gem::io::MaterialMappingMethod::MethodE;
  std::ostringstream febioMethodE;
  gem::io::WriteFebio(febioMethodE, methodEOnly, febioOptions);
  test.Check(Contains(febioMethodE.str(), "<E type=\"map\">GEM_METHOD_E</E>"),
             "FEBio exports the explicitly selected method-E element map");
  options.materialMappingMethod = gem::io::MaterialMappingMethod::MethodE;
  test.Check(
    ThrowsInvalidArgument([&]() {
      std::ostringstream output;
      gem::io::WriteAbaqus(output, methodEOnly, options);
    }),
    "Method E remains unavailable to the legacy material-card exporters");
  options.materialMappingMethod = gem::io::MaterialMappingMethod::MethodA;

  vtkSmartPointer<vtkUnstructuredGrid> nodalOnly = vtkSmartPointer<vtkUnstructuredGrid>::New();
  nodalOnly->DeepCopy(grid);
  nodalOnly->GetCellData()->RemoveArray(gem::io::GetMaterialArrayName(gem::io::MaterialMappingMethod::MethodA));
  nodalOnly->GetCellData()->RemoveArray(gem::io::GetMaterialArrayName(gem::io::MaterialMappingMethod::MethodB));
  vtkSmartPointer<vtkDoubleArray> methodCArray = vtkSmartPointer<vtkDoubleArray>::New();
  methodCArray->SetName("GEM_METHOD_C");
  methodCArray->SetNumberOfValues(nodalOnly->GetNumberOfPoints());
  methodCArray->FillValue(100.0);
  nodalOnly->GetPointData()->AddArray(methodCArray);
  test.Check(!gem::io::CanExportFebioMesh(nodalOnly, &reason),
             "Nodal material maps are not misrepresented as FEBio element data");

  vtkSmartPointer<vtkUnstructuredGrid> triangle = CreateTriangleWithMaterial();
  test.Check(!gem::io::CanExportFemMesh(triangle, &reason), "Surface triangles are rejected as FEM volume meshes");

  vtkSmartPointer<vtkUnstructuredGrid> zeroVolume = vtkSmartPointer<vtkUnstructuredGrid>::New();
  zeroVolume->DeepCopy(grid);
  zeroVolume->GetPoints()->SetPoint(3, 1.0, 1.0, 0.0);
  test.Check(!gem::io::CanExportFemMesh(zeroVolume, &reason),
             "A tetrahedron with four coplanar corner nodes is rejected");

  vtkSmartPointer<vtkUnstructuredGrid> inverted = CreateInvertedTetraWithMaterial();
  test.Check(!gem::io::CanExportFebioMesh(inverted, &reason),
             "FEBio export rejects tetrahedra with inverted orientation");
  febioOptions.materialMappingMethod = gem::io::MaterialMappingMethod::MethodA;
  test.Check(
    ThrowsInvalidArgument([&]() {
      std::ostringstream output;
      gem::io::WriteFebio(output, inverted, febioOptions);
    }),
    "FEBio rejects inverted tetrahedra before writing XML");

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

  febioOptions.materialMappingMethod = gem::io::MaterialMappingMethod::MethodA;
  febioOptions.geometryScale = 0.0;
  test.Check(
    ThrowsInvalidArgument([&]() {
      std::ostringstream output;
      gem::io::WriteFebio(output, grid, febioOptions);
    }),
    "A non-positive FEBio geometry scale is rejected");

  febioOptions.geometryScale = 1.0;
  febioOptions.youngsModulusScale = 0.0;
  test.Check(
    ThrowsInvalidArgument([&]() {
      std::ostringstream output;
      gem::io::WriteFebio(output, grid, febioOptions);
    }),
    "A non-positive FEBio Young's modulus scale is rejected");

  febioOptions.youngsModulusScale = 1.0;
  febioOptions.unitSystem = "unsupported-unit-system";
  test.Check(
    ThrowsInvalidArgument([&]() {
      std::ostringstream output;
      gem::io::WriteFebio(output, grid, febioOptions);
    }),
    "An unsupported FEBio unit system is rejected");

  return test.Result();
}
