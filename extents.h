#pragma once

#include <dbmain.h>

//==============================================================================

bool GetProjectionPlaneParams(AcGePlane& projectionPlane, const AcGeVector3d& projectionPlaneNormal,
                              AcGePoint3d& minExtentPoint, AcGePoint3d& maxExtentPoint,
                              const AcDbEntity* pObjectMesh);
bool GetExtentsProjectionRegion(AcDbVoidPtrArray* paExtentsPtrProjectionRegion, AcGePoint3d* mExtentProjectionFaceVertices,
                                const AcGePlane& projectionPlane, const AcGeVector3d& projectionPlaneNormal,
                                const AcGePoint3d& minExtentPoint, const AcGePoint3d& maxExtentPoint);

//==============================================================================
