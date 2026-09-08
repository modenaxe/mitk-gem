# MITK-GEM Modernization TODO

This roadmap is organized into five phases. The order is intentional: preserve the scientific behavior first, then modernize the platform around it.

## Phase 1: Stabilize and preserve behavior

- [ ] Collect representative CT, segmentation, surface-mesh, volume-mesh, and material-mapping datasets.
- [ ] Create versioned golden outputs for graph-cut masks, surfaces, tetrahedral meshes, material arrays, and exporters.
- [ ] Define acceptable numerical and mesh-topology tolerances.
- [ ] Document coordinate systems, image geometry, physical units, and the meaning of material-mapping methods A-E.
- [ ] Fix confirmed undefined behavior, including uninitialized progress, power-law, and verbose-output members.
- [ ] Fix padding switch fall-through and other confirmed logic errors.
- [ ] Add malformed-input tests for XML, images, surfaces, meshes, and custom export files.
- [ ] Establish a reproducible baseline before changing algorithms or dependencies.

## Phase 2: Modernize the build and dependency stack

- [ ] Select and pin a supported official MITK release instead of the historical private MITK fork.
- [ ] Port the project from the MITK 2016/Qt 5.5 API boundary to the selected MITK release.
- [ ] Migrate Qt code to the supported Qt 6 API, removing deprecated Qt 5 APIs and obsolete modules.
- [ ] Raise the CMake minimum to the version required by the selected MITK release.
- [ ] Add `CMakePresets.json` for supported developer and CI configurations.
- [ ] Replace global compiler flags, global include paths, and `link_directories()` with target-based dependencies.
- [ ] Pin and verify all external sources and third-party versions.
- [ ] Add CI builds for supported Windows and Linux configurations.
- [ ] Review whether TetGen, CGAL, MaxFlow, TinyXML, and other vendored dependencies should be upgraded, replaced, or externalized.

## Phase 3: Separate computational cores from the user interface

- [ ] Create independent, testable components for:
  - [ ] Graph-cut segmentation
  - [ ] Surface extraction
  - [ ] Volume meshing
  - [ ] Material mapping
  - [ ] File export
- [ ] Remove Qt widget dependencies from computational code.
- [ ] Remove direct MITK DataStorage access from worker code.
- [ ] Use immutable input snapshots and explicit result/error objects.
- [ ] Move all widget and DataStorage updates to the GUI thread through queued callbacks.
- [ ] Add cancellation, progress reporting, and safe lifetime handling to long-running operations.
- [ ] Replace blocking view destructors with cancellable job management.
- [ ] Keep MITK plugins as thin adapters around the computational components.

## Phase 4: Modernize data and geometry handling

- [ ] Use current MITK segmentation and label-image abstractions.
- [ ] Create one shared geometry-validation utility for dimensions, spacing, origin, direction, regions, and time geometry.
- [ ] Validate that images, masks, surfaces, and meshes share compatible physical coordinate systems.
- [ ] Preserve image direction and metadata during padding and resampling.
- [ ] Replace unchecked casts, `const_cast`, raw VTK ownership, and manual `New/Delete` patterns where practical.
- [ ] Validate triangular closed surfaces before meshing.
- [ ] Validate tetrahedral output for connectivity, orientation, duplicates, zero volume, and inverted elements.
- [ ] Validate finite values, power-law ranges, divisors, histogram parameters, and interpolation inputs.
- [ ] Store units, coordinate conventions, mapping parameters, and provenance with generated outputs.

## Phase 5: Modernize testing, export, documentation, and distribution

- [ ] Move tests out of GUI-only code and expose them as CTest targets.
- [ ] Replace obsolete embedded test frameworks with a maintained framework.
- [ ] Add unit, integration, end-to-end, geometry, export round-trip, and regression tests.
- [ ] Add sanitizer and static-analysis jobs where supported.
- [ ] Replace the undocumented ASCII export assumptions with a versioned schema and full precision.
- [ ] Implement or remove the currently incomplete ANSYS exporter.
- [ ] Align Abaqus, ANSYS, LS-DYNA, and VTU exporters with the current material-array model.
- [ ] Port Python 2 scripts to Python 3 with argument validation and automated tests.
- [ ] Review and modernize or retire the MATLAB scripts.
- [ ] Add developer documentation, user documentation, contribution guidelines, and a third-party license inventory.
- [ ] Produce reproducible installers/packages for each supported platform.

## Current MITK reference baseline

The official MITK repository currently lists MITK v2026.06 as the latest release. Its release notes identify:

- Qt 6.10 compatibility.
- VTK 9.5.2 at commit `7c0494a68bff379d32d6b1fbaa3d10d27a73af54`.
- ITK 5.4.6 with MITK patches.
- CMake 3.28 minimum, or CMake 3.31 on Windows.

These versions should be treated as an initial compatibility target, not independently upgraded without validating the complete MITK stack.
