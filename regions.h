#pragma once

#include <dbptrar.h>
#include <geplane.h>

//==============================================================================

enum class EAddRegion { Done, NoUsed, Fail};
//------------------------------------------------------------------------------

[[nodiscard("The result must be taken into account.")]]
bool FormProjectionRegion(const int countFaceVertices, const AcGePoint3d mFaceVertices[],
                          AcArray<AcGePoint3d*>& aObjectVertices, AcArray<AcGePoint3d*>& aObjectProjectionVertices,
                          AcDbVoidPtrArray* paObjectPtrProjectionRegions,
                          const AcGePlane& projectionPlane, const AcGeVector3d& projectionPlaneNormal);
EAddRegion AddProjectionRegion(AcDbVoidPtrArray* paProjectionPtrRegions, const AcGePoint3d mProjectionFaceVertices[], const int countFaceVertices);
void EmptyDeleteDbVoidPtrArray(AcDbVoidPtrArray*& pVoidPtrArray);

//==============================================================================
