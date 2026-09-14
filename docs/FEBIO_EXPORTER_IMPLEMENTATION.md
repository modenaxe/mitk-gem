# FEBio exporter implementation brief

Status: implementation-ready research
Prepared: 2026-09-14
Compatibility target: FEBio 4.13 and FEBio Studio 3.2

## Decision summary

MITK-GEM should export a native FEBio solver input file (`.feb`) using the
FEBio 4.0 XML schema. FEBio Studio opens `.feb` files directly, while its
`.fsm` format is a Studio project format coupled to Studio internals. A `.feb`
writer is therefore the smaller, documented, portable integration and does
not require linking MITK-GEM to FEBio or FEBio Studio.

The first implementation should export:

- one tetrahedral volume-mesh domain;
- one isotropic elastic material;
- one explicitly selected, cell-based Young's-modulus map;
- a constant Poisson ratio;
- physical point coordinates and full tetrahedral connectivity;
- provenance and unit metadata.

The output will open as an editable model in FEBio Studio. It will not be a
complete runnable analysis until the user adds an analysis step, boundary
conditions, and loads in Studio. That limitation should be made clear in the
writer name and documentation.

There is enough information in the public specifications and source code to
implement and test this exporter without further format research.

## Why `.feb`, not `.fsm`

FEBio Studio documents `.feb` among the model formats it can load, and the
FEBio format documentation defines the solver's XML input structure. The
current stable Studio exporter itself writes `<febio_spec version="4.0">` and
the standard FEBio sections. By contrast, `.fsm` is Studio's own project
format. Exporting `.feb` gives MITK-GEM a native FEBio hand-off that is usable
by both FEBio and FEBio Studio and remains human-readable and independently
validatable. [FEBio Studio loading documentation][studio-load]
[FEBio 4 format overview][format-overview]
[FEBio Studio 3.2 v4 exporter][studio-export-root]

As of this research, the latest stable public releases are
[FEBio 4.13][febio-release] and [FEBio Studio 3.2][studio-release]. The writer
should target their common FEBio 4.0 schema rather than the moving `develop`
branches.

## Required FEBio document model

The geometry-and-material hand-off needs these sections, in this order:

1. `Module`
2. `Material`
3. `Mesh`
4. `MeshDomains`
5. `MeshData`

`Module` must identify a solid-mechanics model. `Mesh` defines global nodes
and an element set. `MeshDomains` turns that element set into a solid domain
and associates it with the material. `MeshData` defines the scalar,
element-wise modulus map. [Module section][module-section]
[Mesh section][mesh-section] [MeshDomains section][domains-section]
[MeshData section][meshdata-section]

The minimum output should have this shape:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<febio_spec version="4.0">
  <Module type="solid">
    <units>mm-N-s</units>
  </Module>
  <Material>
    <material id="1" name="Bone" type="isotropic elastic">
      <E type="map">GEM_METHOD_A</E>
      <v>0.3</v>
    </material>
  </Material>
  <Mesh>
    <Nodes name="MITK_GEM_NODES">
      <node id="1">0,0,0</node>
      <node id="2">1,0,0</node>
      <node id="3">0,1,0</node>
      <node id="4">0,0,1</node>
    </Nodes>
    <Elements type="tet4" name="BoneDomain">
      <elem id="1">1,2,3,4</elem>
    </Elements>
  </Mesh>
  <MeshDomains>
    <SolidDomain name="BoneDomain" mat="Bone"/>
  </MeshDomains>
  <MeshData>
    <ElementData name="GEM_METHOD_A" elem_set="BoneDomain">
      <e lid="1">12500</e>
    </ElementData>
  </MeshData>
</febio_spec>
```

The link between the constitutive parameter and the field is
`<E type="map">MAP_NAME</E>`. FEBio deliberately resolves this named map after
the XML sections have been read. [Mapped-parameter documentation][mapped-param]
[FEBio mapped-parameter parser][mapped-param-source]

The scalar `ElementData` map may omit `data_type`; scalar is the default. If
the writer emits the attribute, it must use `data_type="scalar"`. FEBio
Studio's own exporter uses `<e lid="...">value</e>` entries, so MITK-GEM
should use that spelling even though documentation examples may also show
`elem`. [FEBio Studio ElementData writer][studio-elementdata]

Important identifier rules:

- global node and element IDs are one-based, unique, and emitted in increasing
  order;
- `ElementData/@elem_set` must exactly match `Elements/@name`;
- each `lid` is a one-based *local index inside that element set*, not a global
  VTK or FEBio element ID;
- one map value must be written for every element in the referenced set;
- all generated names must be XML-escaped and should also be normalized to
  conservative FEBio identifiers.

FEBio 4's parser checks local indices and the number of supplied entries, so a
partial map must be rejected before writing. [FEBio 4 MeshData parser][meshdata-parser]

## Material model and units

For the first version, use FEBio's registered `isotropic elastic` solid
material:

```xml
<material id="1" name="Bone" type="isotropic elastic">
  <E type="map">GEM_METHOD_A</E>
  <v>0.3</v>
</material>
```

`E` is pressure-valued and must be positive. Poisson ratio `v` must be in
`[-1, 0.5)`. A default of `0.3` is reasonable but must be a writer option, not
a hidden constant. FEBio documents this model as suitable for small strains
(including potentially large rotations), not as a physically appropriate
large-strain tissue model. [Isotropic elastic material][isotropic-elastic]
[FEBio implementation][isotropic-source]

MITK and VTK physical coordinates in this workflow are expressed in
millimetres. `mm-N-s` therefore makes the corresponding pressure unit
N/mm², i.e. MPa. The current GEM VTK arrays do not carry formal unit metadata,
however. The exporter must make the following contract explicit:

- default FEBio unit system: `mm-N-s`;
- geometry scale factor: `1.0`;
- modulus scale factor: `1.0`;
- the user confirms that the selected GEM values are MPa, or deliberately
  changes the scale;
- the chosen units and scale factors are recorded in an XML comment.

No silent conversion should occur. FEBio unit declarations provide context;
they cannot repair numerically inconsistent input data.

## MITK-GEM material-map selection

MITK-GEM currently creates five named arrays:

| Method | VTK association | Meaning in current implementation | FEBio v1 status |
|---|---|---|---|
| `GEM_METHOD_A` | cell data | element modulus; no erosion, three dilation steps | selectable; preferred default |
| `GEM_METHOD_B` | cell data | element modulus; one erosion, three dilation steps | selectable |
| `GEM_METHOD_C` | point data | nodal modulus | reject for v1 |
| `GEM_METHOD_D` | point data | duplicate of method C | reject for v1 |
| `GEM_METHOD_E` | cell data | duplicate of method A | selectable, but label as equivalent to A |

These meanings are defined by
[`MaterialMappingHelper.cpp`](../Plugins/ch.zhaw.materialmapping/src/internal/MaterialMappingHelper.cpp)
and the canonical names by
[`GemIOResources.h`](../Modules/GemCore/autoload/IO/GemIOResources.h).

The exporter must never choose a map merely because it is the active VTK
scalar. It should expose a `Young's modulus map` option and bind the exact
selected array name in both `Material/E` and `MeshData/ElementData`.

Recommended first UI/API behavior:

- choices: Method A, Method B, and Method E;
- preselect Method A for continuity with the current FEM exporters;
- show that Method E is currently identical to Method A;
- fail with a precise message if the requested array is absent or invalid;
- do not offer methods C or D as element maps.

Internally, selection should be stored by canonical VTK array name rather than
by translated display text. A later generic mode may enumerate arbitrary
eligible scalar cell arrays, but the fixed A/B/E choices fit MITK's static
file-writer option mechanism and avoid accidentally treating unrelated cell
data as Young's modulus.

The initial exporter should write only the selected map. FEBio can store
multiple named element maps, but a material's `E` parameter can reference only
one map at a time. A future `include all eligible maps` option could preserve
additional fields for comparison in Studio while retaining a separate,
explicit `active E map` selection.

Methods C and D must not be silently converted. Supporting nodal material data
requires a deliberate decision about interpolation at element integration
points and a verified FEBio representation; that is a separate feature.

## Mesh topology and node ordering

The existing FEM exporter already restricts input to a uniform collection of
`VTK_TETRA` or `VTK_QUADRATIC_TETRA` cells. FEBio supports the corresponding
`tet4` and `tet10` element types. That validation can be refactored into shared
code, but the FEBio path must not use the existing material quantization.

For `tet4`, output the four VTK point IDs after converting them to the
exporter's one-based FEBio node-ID table.

For `tet10`, VTK and FEBio use the same midside-node order:

| Local node | Edge |
|---:|---|
| 4 | 0-1 |
| 5 | 1-2 |
| 6 | 2-0 |
| 7 | 0-3 |
| 8 | 1-3 |
| 9 | 2-3 |

No normal-order remapping is needed. This was checked against both the exact
[VTK checkout used by MITK-GEM][vtk-tet-order] and
[FEBio 4.13's tetrahedral edge table][febio-tet-order].

The writer should compute the signed volume/Jacobian from corner nodes before
serialization:

- reject zero or near-zero tetrahedra as degenerate;
- either reject inverted cells with their source cell IDs, or normalize them
  consistently and issue a warning;
- if normalization is selected, swapping corner nodes 1 and 2 changes a
  `tet4` order to `[0,2,1,3]`;
- the corresponding `tet10` permutation is
  `[0,2,1,3,6,5,4,7,9,8]`, preserving every midside edge.

A strict rejection policy is simplest and most auditable for the first
release. Orientation normalization can be added if real MITK-GEM output shows
that otherwise valid meshes commonly arrive inverted.

## Export validation contract

The core writer must validate everything before opening or replacing the
destination file. It should reject:

- null or empty grids;
- zero points or zero cells;
- non-finite coordinates;
- any non-tetrahedral cell;
- mixed `tet4`/`tet10` topology in one domain;
- cells with the wrong node count;
- invalid point references or duplicate corner-node IDs;
- degenerate tetrahedra and, under the initial policy, inverted tetrahedra;
- a missing selected material array;
- a point-data array used where cell data is required;
- a non-numeric array, more than one component, or a tuple count different
  from the number of cells;
- NaN, infinity, zero, or negative modulus values;
- non-finite Poisson ratio or a value outside `[-1, 0.5)`;
- a non-positive geometry or modulus scale factor;
- output names that cannot be made into safe, unambiguous identifiers.

Errors should name the array and the first offending cell/value. Validation
must not partially write the requested output. Prefer writing to a temporary
file and atomically replacing the destination after success if the MITK writer
service permits that workflow.

## Numerical and serialization requirements

- Stream the XML directly; do not duplicate the full mesh or map in an XML DOM.
- Imbue the stream with `std::locale::classic()` so decimal points never depend
  on the Windows locale.
- Write floating-point values with `std::numeric_limits<double>::max_digits10`
  (or an explicitly justified equivalent).
- Emit UTF-8 and XML-escape every user-derived string.
- Keep the selected map continuous: write one original, optionally scaled
  value per cell. Do **not** call `QuantizeMaterials` and do not apply
  `maxMaterialDefinitions`.
- Add a concise XML comment containing MITK-GEM version, source map, declared
  unit system, geometry scale, and modulus scale.
- For large models, report progress/cancellation if MITK's writer API makes it
  available, but do not compromise deterministic output.

No FEBio or FEBio Studio library is needed. A small streaming writer using the
C++ standard library is sufficient and avoids a new runtime dependency. If an
existing XML utility is already available through GemCore's current link
surface, it may be used, but adding a large dependency solely for this simple
document is not justified.

## Proposed MITK-GEM integration

Follow the architecture already used by the ANSYS and Abaqus writers:

1. Add FEBio options and a `WriteFebio(std::ostream&, vtkUnstructuredGrid*,
   const FebioExportOptions&)` core function near
   [`GemFemExport.h`](../Modules/GemCore/include/GemFemExport.h) and
   [`GemFemExport.cpp`](../Modules/GemCore/src/GemFemExport.cpp).
2. Extract topology and numeric-array validation that is genuinely common;
   keep quantization exclusive to ANSYS/Abaqus.
3. Add `FebioFileWriterService.h/.cpp` under
   `Modules/GemCore/autoload/IO`.
4. Add a `.feb` MIME type in `GemIOMimeTypes` and describe it as
   `FEBio model (mesh and mapped material)`.
5. Register the writer in `mitkNewModuleIOActivator.cpp` and add it to the
   autoload `files.cmake`.
6. Expose writer options through MITK's `IFileWriter::Options`:
   - `Young's modulus map`: Method A / Method B / Method E;
   - `Poisson ratio`: default `0.3`;
   - `FEBio unit system`: default `mm-N-s`;
   - `Geometry scale`: default `1.0`;
   - `Young's modulus scale`: default `1.0`.
7. Use the writer's confidence check to accept only non-empty tetrahedral
   `mitk::UnstructuredGrid` input with at least one valid supported cell map.

The service should clone and preserve options in the same way as the existing
`AbaqusFileWriterService` and `AnsysFileWriterService`.

## Test plan

### Core automated tests

Extend `Modules/GemCore/test/GemFemExportTest.cpp` or add a focused FEBio test
executable covering:

1. a one-element `tet4` document with exact IDs, domain, material, and map;
2. a `tet10` document and exact connectivity/order;
3. explicit selection of A, B, and E;
4. exact binding of the chosen map name in both `E type="map"` and
   `ElementData`;
5. preservation of distinct continuous values, proving no quantization;
6. tuple-to-`lid` alignment for a multi-element mesh;
7. classic-locale decimal output and round-trip-safe precision;
8. XML escaping or safe normalization of names;
9. every rejection listed in the validation contract;
10. no destination replacement on validation or write failure.

Keep a small reviewed golden fixture, for example
`Modules/GemCore/test/fixtures/febio-method-a.feb`, but also parse generated XML
in tests so failures are structural rather than only byte-for-byte formatting
differences.

### Compatibility tests

Before calling the feature complete:

- open generated `tet4` and `tet10` fixtures in FEBio Studio 3.2;
- verify the solid part/domain, isotropic elastic material, named MeshData
  field, and mapped `E` parameter in Studio;
- re-export from Studio and compare node coordinates, connectivity, domain
  membership, selected map values, and Poisson ratio semantically;
- validate with FEBio 4.13. Since a mesh-and-material hand-off has no analysis
  step, maintain one separate, tiny end-to-end fixture with constraints, load,
  and step if an actual solver run is desired;
- record the FEBio/FEBio Studio versions used by the compatibility test.

An optional future CI job may validate fixtures with official FEBio binaries,
but neither the normal build nor the exporter should depend on them.

## Multi-domain and future extensions

The first implementation should support one `Elements` set and one
`SolidDomain`, matching the current MITK-GEM mapped-mesh model.

If a future VTK cell array identifies multiple anatomical/material domains,
the exporter can split elements into several named sets and emit one
`SolidDomain` per set. ElementData `lid` numbering then restarts at one for
each set. FEBio's readers can merge same-named data maps across domains, but
this behavior needs dedicated fixtures before it is exposed.

Other deliberately deferred features are:

- nodal methods C/D and integration-point interpolation;
- multiple active material laws;
- density maps and nonlinear bone constitutive models;
- surfaces, node sets, boundary conditions, contacts, loads, and analysis
  steps;
- direct `.fsm` project generation;
- importing results back into MITK-GEM.

These are not needed to deliver the requested native FEBio Studio hand-off.

## Definition of done

The exporter is ready to ship when:

- Save offers `.feb` for an eligible mapped volumetric mesh;
- the user explicitly controls which supported element map drives Young's
  modulus;
- a valid `tet4` or `tet10` model opens in FEBio Studio 3.2 without repair;
- FEBio resolves the material's mapped `E` field and retains every cell value;
- unit assumptions and Poisson ratio are visible and configurable;
- invalid topology, invalid maps, and invalid material values fail safely with
  actionable messages;
- automated tests cover topology, selection, map alignment, numeric fidelity,
  formatting, and negative cases;
- no new FEBio/FEBio Studio runtime dependency is introduced.

## Official sources

- [FEBio 4 format overview][format-overview]
- [FEBio `Module` and units][module-section]
- [FEBio `Mesh` section][mesh-section]
- [FEBio `MeshDomains` section][domains-section]
- [FEBio `MeshData` section][meshdata-section]
- [FEBio mapped parameters][mapped-param]
- [FEBio isotropic elastic feature][isotropic-elastic]
- [FEBio Studio loading a model][studio-load]
- [FEBio Studio 3.2 source and release][studio-release]
- [FEBio 4.13 source and release][febio-release]
- [FEBio Studio v4 XML exporter][studio-export-root]
- [FEBio Studio scalar ElementData writer][studio-elementdata]
- [FEBio 4.13 ElementData parser][meshdata-parser]
- [FEBio 4.13 mapped-parameter parser][mapped-param-source]
- [FEBio 4.13 isotropic elastic implementation][isotropic-source]
- [VTK quadratic-tetra node ordering][vtk-tet-order]
- [FEBio tetrahedral node ordering][febio-tet-order]

[format-overview]: https://febiosoftware.github.io/febio-docs/user/chapter3/3.1-free-format-overview/
[module-section]: https://febiosoftware.github.io/febio-docs/user/chapter3/3.2-module-section/
[mesh-section]: https://febiosoftware.github.io/febio-docs/user/chapter3/3.6-mesh-section/
[domains-section]: https://febiosoftware.github.io/febio-docs/user/chapter3/3.7-meshdomains-section/
[meshdata-section]: https://febiosoftware.github.io/febio-docs/user/chapter3/3.8-meshdata-section/
[mapped-param]: https://febiosoftware.github.io/febio-docs/user/chapterA/A.2-mapped-parameters/
[isotropic-elastic]: https://febiosoftware.github.io/febio-feature-manual/features/solid_material_isotropic_elastic/
[studio-load]: https://febiosoftware.github.io/febio-docs/studio/chapter4/4.2-loading-a-model/
[studio-release]: https://github.com/febiosoftware/FEBioStudio/releases/tag/v3.2
[febio-release]: https://github.com/febiosoftware/FEBio/releases/tag/v4.13
[studio-export-root]: https://github.com/febiosoftware/FEBioStudio/blob/33174763212bd0ca4d97d6e30dacb091bf66e526/FEBio/FEBioExport4.cpp#L640-L710
[studio-elementdata]: https://github.com/febiosoftware/FEBioStudio/blob/33174763212bd0ca4d97d6e30dacb091bf66e526/FEBio/FEBioExport4.cpp#L2583-L2635
[meshdata-parser]: https://github.com/febiosoftware/FEBio/blob/32ae206ff4881dfb54f62296cd1558e58ed9fcc6/FEBioXML/FEBioMeshDataSection4.cpp#L266-L397
[mapped-param-source]: https://github.com/febiosoftware/FEBio/blob/32ae206ff4881dfb54f62296cd1558e58ed9fcc6/FEBioXML/FileImport.cpp#L973-L1045
[isotropic-source]: https://github.com/febiosoftware/FEBio/blob/32ae206ff4881dfb54f62296cd1558e58ed9fcc6/FEBioMech/FEIsotropicElastic.cpp#L33-L37
[vtk-tet-order]: https://github.com/Kitware/VTK/blob/7c0494a68bff379d32d6b1fbaa3d10d27a73af54/Common/DataModel/vtkQuadraticTetra.cxx#L69-L76
[febio-tet-order]: https://github.com/febiosoftware/FEBio/blob/32ae206ff4881dfb54f62296cd1558e58ed9fcc6/FECore/FEEdgeList.cpp#L177-L182
