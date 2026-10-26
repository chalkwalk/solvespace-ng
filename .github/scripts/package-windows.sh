#!/bin/sh -xe

# After build-windows.sh: the executable with ANGLE's DLLs and the OpenMP runtime, in a zip.
NAME=solvespace-ng-windows-x64

mkdir -p dist/${NAME}
cp build/bin/Release/solvespace-ng.exe build/bin/Release/libEGL.dll build/bin/Release/libGLESv2.dll \
   dist/${NAME}/
# OpenMP (/openmp) needs vcomp140.dll, which the static C runtime doesn't include.
vcomp=$(find "/c/Program Files/Microsoft Visual Studio/2022" \
             -path "*/x64/Microsoft.VC143.OpenMP/vcomp140.dll" | head -n 1)
[ -n "$vcomp" ] || { echo "vcomp140.dll not found" >&2; exit 1; }
cp "$vcomp" dist/${NAME}/
cp COPYING.txt dist/${NAME}/
cd dist
7z a -tzip ${NAME}.zip ${NAME}
rm -r ${NAME}
