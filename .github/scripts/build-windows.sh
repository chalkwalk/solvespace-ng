#!/bin/sh -xe

# Release: Visual Studio builds the vendored Open CASCADE in the same configuration, and only a
# Release one installs its libraries where we look for them (lib/, not libd/ or libi/).
BUILD_TYPE=Release

mkdir build
cd build

cmake \
    -G "Visual Studio 17 2022" \
    -DCMAKE_GENERATOR_PLATFORM="x64" \
    -DENABLE_OPENMP="ON" \
    -DENABLE_NG_GUI=ON \
    ..

cmake --build . --config "${BUILD_TYPE}" -- -maxcpucount
cmake --build . --config "${BUILD_TYPE}" -t test_solvespace -- -maxcpucount
