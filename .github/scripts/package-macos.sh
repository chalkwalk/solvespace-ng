#!/bin/sh -xe

# An Apple silicon app bundle, signed ad hoc (not notarized), in a zip. Without OpenMP: the only
# arm64 libomp CI can fetch is Homebrew's bottle for the newest macOS, which would raise the
# oldest macOS the app runs on.
NAME=solvespace-ng-macos-arm64

mkdir build
cd build
cmake \
  -D CMAKE_OSX_ARCHITECTURES="arm64" \
  -D CMAKE_BUILD_TYPE="Release" \
  -D ENABLE_OPENMP="OFF" \
  -D ENABLE_LTO="ON" \
  -D ENABLE_NG_GUI="ON" \
  ..
cmake --build . -j$(sysctl -n hw.logicalcpu)
make test_solvespace
cd ..
mkdir -p dist
ditto build/bin/solvespace-ng.app "dist/SolveSpace NG.app"
ditto -c -k --keepParent "dist/SolveSpace NG.app" dist/${NAME}.zip
rm -r "dist/SolveSpace NG.app"
