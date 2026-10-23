#!/bin/sh -xe

mkdir build
cd build
cmake \
  -DCMAKE_BUILD_TYPE="Debug" \
  -DENABLE_OPENMP="ON" \
  -DENABLE_SANITIZERS="ON" \
  -DENABLE_NG_GUI="ON" \
  ..
make -j$(nproc)
make test_solvespace
