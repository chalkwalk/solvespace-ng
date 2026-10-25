//-----------------------------------------------------------------------------
// The OCCT kernel's B-rep (kernel/kernel.h's opaque KernelShape), for the kernel's own files
// (occtkernel.cpp, occtquadmesh.cpp): the shape, and each face's name and colour.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_KERNEL_OCCTSHAPE_H
#define SOLVESPACE_KERNEL_OCCTSHAPE_H

#if defined(_WIN32)
// Before any OCCT header: OCCT defines NOUSER and the like to trim windows.h, and the Windows
// SDK (10.0.26100) then fails in winuser.inl when an OCCT header includes it. winuser.h's
// DIFFERENCE would replace KernelCombine::DIFFERENCE.
#   include <windows.h>
#   undef DIFFERENCE
#endif
#include <NCollection_DataMap.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS_Shape.hxx>

namespace SolveSpace {

struct FaceInfo {
    uint32_t  face  = 0; // the face entity's handle, or 0
    RgbaColor color = {};
};

using FaceMap = NCollection_DataMap<TopoDS_Shape, FaceInfo, TopTools_ShapeMapHasher>;

struct KernelShape {
    TopoDS_Shape shape;
    FaceMap      faces;
};

}

#endif
