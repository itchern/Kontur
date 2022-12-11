#pragma once

//==============================================================================

bool CreateDrawKonturs(const AcDbEntity* pObjectMesh, const AcArray<AcGePoint3d*>& aObjectVertices,
                       const AcArray<AcGePoint3d*>& aObjectProjectionVertices, const AcDbVoidPtrArray* paObjectPtrProjectionRegions,
                       const AcGePlane& projectionPlane, const AcGeVector3d& projectionPlaneNormal,
                       const AcGePoint3d& minExtentPoint, const AcGePoint3d& maxExtentPoint);

//==============================================================================
