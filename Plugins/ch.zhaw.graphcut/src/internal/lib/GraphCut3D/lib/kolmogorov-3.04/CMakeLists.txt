# Build the official MAXFLOW v3.04 implementation for the standalone
# ImageGraphCut3DSegmentation project. The MITK-GEM plug-in compiles these
# sources directly through files.cmake.
add_library(KolmogorovMaxFlow graph.cpp maxflow.cpp)
