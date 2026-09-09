## ch.zhaw.graphcut MITK Workbench Plugin
Reimplementation of https://github.engineering.zhaw.ch/VisualComputingLab/MyMITKProject.

## Build instructions
1. Drop the repo into your existing MITK applications MITK-build/Plugin folder.
2. Add `Plugins/ch.zhaw.graphcut:ON` to the `PROJECT_PLUGINS` variable in your 'Plugins.cmake' file.
3. Build your application

## Seed selection

GraphCut3D accepts either legacy MITK binary-mask nodes or modern
`MultiLabelSegmentation` nodes. For a modern segmentation, select the painted
foreground or background label next to the node selector. One modern
segmentation can provide both seed classes when two different labels are
selected. Each selected label is converted to a temporary binary mask before
MAXFLOW runs.

The source image and both seed masks must be single 3D volumes with matching
dimensions, spacing, origin, and orientation. Results are saved as a modern
`MultiLabelSegmentation` child of the source image rather than as a legacy
binary image.

## Focused regression test

The legacy project test suite is not required for GraphCut coverage. Configure
the focused regression target independently:

```powershell
cmake -S . -B b\gem -DBUILD_TESTING=OFF -DMITK_GEM_BUILD_GRAPHCUT_REGRESSION_TESTS=ON
cmake --build b\gem --config Release --target GraphcutModernizationTest
ctest --test-dir b\gem -C Release --output-on-failure -R GraphcutModernizationTest
```
