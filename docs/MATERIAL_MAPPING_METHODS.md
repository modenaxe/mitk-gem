# Material mapping methods

MITK-GEM exposes two related but distinct choices:

1. **Current vs. improved implementation** controls how the image-derived
   material field is eroded and extended around the bone surface.
2. **Methods A-E** are the five named point or element arrays written to every
   material-mapped mesh.

The selected implementation is used to calculate all five output arrays.

## Current and improved implementations

### Current method (published legacy algorithm)

The current method preserves the historical implementation associated with
the published MITK-GEM workflow. It uses a slice-oriented `3 x 3 x 1` erosion
kernel and the original C implementation for weighted 3-D material extension.

Select this method when reproducing results obtained with the historical
software or when comparability with the published workflow is required.

### Improved method (software default)

The improved method uses a full `3 x 3 x 3` erosion kernel and performs the
weighted 3-D extension through VTK image operations. It treats adjacent slices
as part of the erosion neighbourhood and avoids the legacy array-buffer code.

This implementation is intended to be more spatially consistent, but it has
not been independently validated as numerically equivalent to the published
method. It should therefore be reported explicitly in scientific workflows.

## Output arrays A-E

| Method | Storage | Current MITK-GEM implementation |
|---|---|---|
| A | Element / VTK cell data | No erosion, followed by three material-extension steps. Interpolated nodal Young's modulus is converted to one value per element. |
| B | Element / VTK cell data | One erosion/peel step, followed by three material-extension steps. Element values are calculated from the Method C nodal field. |
| C | Node / VTK point data | One erosion/peel step and three material-extension steps. Young's modulus remains associated with mesh nodes. |
| D | Node / VTK point data | Currently an exact copy of Method C. The separate cortical assignment described by historical export scripts is not implemented. |
| E | Element / VTK cell data | Currently an exact copy of Method A. MITK-GEM does not currently generate a distinct shell model for this field. |

The in-memory VTK array names are `GEM_METHOD_A` through `GEM_METHOD_E`.

## Minimum Young's modulus E

In the Options section, **E means Young's modulus, not Method E**. The value is
a lower bound applied when image values are interpolated onto mesh nodes. Any
smaller modulus is replaced with the selected minimum before element values
are calculated. A value of `0` disables the effective floor.

The unit is inherited from the configured density-to-modulus power law. With
the usual MITK-GEM power-law parameters this is typically MPa; the application
does not perform an independent unit conversion at this step.

## FEM export support

- FEBio exports the selected continuous element map A, B, or E.
- Abaqus and ANSYS currently export element maps A or B and discretize them
  into material definitions.
- Point-based Methods C and D are retained in the mesh for visualization and
  further processing but are not currently supported by the native exporters.
