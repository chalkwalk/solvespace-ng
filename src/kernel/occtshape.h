//-----------------------------------------------------------------------------
// The OCCT kernel's B-rep (kernel/kernel.h's opaque KernelShape), for the kernel's own files
// (occtkernel.cpp, occtquadmesh.cpp): the shape, and each face's name and colour.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_KERNEL_OCCTSHAPE_H
#define SOLVESPACE_KERNEL_OCCTSHAPE_H

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
