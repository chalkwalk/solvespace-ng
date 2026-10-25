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
  ..
make -j$(nproc)
# --prefix, not DESTDIR with a prefix of /: GNUInstallDirs would make that usr/.
cmake --install . --strip --prefix "$(pwd)/../dist/${NAME}"
cd ..
cp COPYING.txt dist/${NAME}/
tar -C dist -czf dist/${NAME}.tar.gz ${NAME}
rm -r dist/${NAME}
