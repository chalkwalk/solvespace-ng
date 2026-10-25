#!/bin/sh -xe

# A relocatable tarball: bin/solvespace-ng finds its resources in ../share/solvespace.
NAME=solvespace-ng-linux-x86_64

mkdir build
cd build
cmake \
  -DCMAKE_BUILD_TYPE="Release" \
  -DENABLE_OPENMP="ON" \
  -DENABLE_LTO="ON" \
  -DENABLE_NG_GUI="ON" \
  -DENABLE_TESTS="OFF" \
  -DCMAKE_INSTALL_PREFIX=/ \
  ..
make -j$(nproc)
DESTDIR=../dist/${NAME} cmake --install . --strip
cd ..
cp COPYING.txt dist/${NAME}/
tar -C dist -czf dist/${NAME}.tar.gz ${NAME}
rm -r dist/${NAME}
