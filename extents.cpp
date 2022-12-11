#include "pch.h"

#include <dbregion.h>

#include "regions.h"
#include "extents.h"
#include "prints.h"

//==============================================================================
// 
// Получение параметров плоскости проекции и экстремальных точек увеличенного
// охватывающего сеть прямоугольного параллелепипеда.
//
bool GetProjectionPlaneParams(AcGePlane& projectionPlane, const AcGeVector3d& projectionPlaneNormal,
                              AcGePoint3d& minExtentPoint, AcGePoint3d& maxExtentPoint,
                              const AcDbEntity* pObjectMesh)
{
  Acad::ErrorStatus errst;

  AcDbExtents extents;
  if ((errst = pObjectMesh->getGeomExtents(extents)) != Acad::eOk) {
    PrintError(__func__, 1, errst);
    return false;
  }

// Величина отступа в стороны увеличения размеров охватывающего сеть
// прямоугольного параллелепипеда (берётся произвольно 1% диагонали).
//
  double deltaExtent = extents.maxPoint().distanceTo(extents.minPoint())*0.01;

// Новые экстремальные точки охватывающего сеть параллелепипеда.
//
  AcGeVector3d deltaVector(deltaExtent, deltaExtent, deltaExtent);
  extents.set(extents.minPoint()-=deltaVector, extents.maxPoint()+=deltaVector);
  minExtentPoint = extents.minPoint();
  maxExtentPoint = extents.maxPoint();

// Определение плоскости проекции как плоскости с нормалью, параллельной введённой нормали на наблюдателя,
// и касающейся сферы, описанной вокруг увеличенного параллелепипеда.
//
  AcGePoint3d projectionPlanePoint = minExtentPoint + (maxExtentPoint-minExtentPoint)/2.0 + AcGeVector3d(projectionPlaneNormal.normal()*maxExtentPoint.distanceTo(minExtentPoint)/2.0);
  projectionPlane.set(projectionPlanePoint, projectionPlaneNormal);

  return true;
}
//------------------------------------------------------------------------------
// 
// Добавление региона проекции грани параллелепипеда в формируемый общий регион
// проекции увеличенного параллелепипеда.
//
bool AddExtentProjectionRegion(AcDbVoidPtrArray* paExtentsPtrProjectionRegion,
                               const AcGePoint3d* mExtentFace4Vertices, AcGePoint3d* mExtentProjectionFace4Vertices,
                               const AcGePlane& projectionPlane, const AcGeVector3d& projectionPlaneNormal)
{
// Массив указателей регионов проекции грани; количество регионов - один.
//
  AcDbVoidPtrArray* paFaceProjectionPtrRegion = new AcDbVoidPtrArray();
  if(!paFaceProjectionPtrRegion)  {
    PrintErrorMemory(__func__, 1);
    return false;
  }
  paFaceProjectionPtrRegion->append(new AcDbRegion());

// Получение проекций вершин грани параллелепипеда.
//
  for(int iv = 0; iv < 4; iv++) {
    mExtentProjectionFace4Vertices[iv] = mExtentFace4Vertices[iv].project(projectionPlane, projectionPlaneNormal);
  }

  bool bResult = true;
  Acad::ErrorStatus errst;

// Добавление региона проекции грани к региону проекции параллелепипеда.
//
  EAddRegion eFormRegion = AddProjectionRegion(paFaceProjectionPtrRegion, mExtentProjectionFace4Vertices, 4);
  if(eFormRegion == EAddRegion::Done)  {
    if((errst = static_cast<AcDbRegion*>((*paExtentsPtrProjectionRegion)[0])->booleanOper(AcDb::BoolOperType::kBoolUnite,
                                                                                          static_cast<AcDbRegion*>((*paFaceProjectionPtrRegion)[0]))) != Acad::eOk) {
      bResult = false;
      PrintError(__func__, 2, errst);
    }
  }

// Освобождение памяти.
//
  EmptyDeleteDbVoidPtrArray(paFaceProjectionPtrRegion);

  return  bResult && eFormRegion != EAddRegion::Fail;
}
//------------------------------------------------------------------------------
// 
// Получение региона проекции увеличенного параллелепипеда.
//
bool GetExtentsProjectionRegion(AcDbVoidPtrArray* paExtentsPtrProjectionRegion, AcGePoint3d* mExtentProjectionFaceVertices,
                                const AcGePlane& projectionPlane, const AcGeVector3d& projectionPlaneNormal,
                                const AcGePoint3d& minExtentPoint, const AcGePoint3d& maxExtentPoint)
{

// Вершины параллелепипеда.
//
  AcGePoint3d  mExtentVertices[] { {minExtentPoint.x, minExtentPoint.y, minExtentPoint.z},
                                   {maxExtentPoint.x, minExtentPoint.y, minExtentPoint.z},
                                   {maxExtentPoint.x, maxExtentPoint.y, minExtentPoint.z},
                                   {minExtentPoint.x, maxExtentPoint.y, minExtentPoint.z},

                                   {minExtentPoint.x, minExtentPoint.y, maxExtentPoint.z},
                                   {maxExtentPoint.x, minExtentPoint.y, maxExtentPoint.z},
                                   {maxExtentPoint.x, maxExtentPoint.y, maxExtentPoint.z},
                                   {minExtentPoint.x, maxExtentPoint.y, maxExtentPoint.z} };

// Вершины граней параллелепипеда.
//
  AcGePoint3d  mExtentFaceVertices[] { mExtentVertices[0], mExtentVertices[1], mExtentVertices[2], mExtentVertices[3],
                                       mExtentVertices[0], mExtentVertices[1], mExtentVertices[5], mExtentVertices[4],
                                       mExtentVertices[1], mExtentVertices[2], mExtentVertices[6], mExtentVertices[5],
                                       mExtentVertices[2], mExtentVertices[3], mExtentVertices[7], mExtentVertices[6],
                                       mExtentVertices[3], mExtentVertices[0], mExtentVertices[4], mExtentVertices[7],
                                       mExtentVertices[4], mExtentVertices[5], mExtentVertices[6], mExtentVertices[7] };

// Формирование региона проекции параллелепипеда.
//
  for(int k = 0; k < 6; k++) {
    if(!AddExtentProjectionRegion(paExtentsPtrProjectionRegion, &mExtentFaceVertices[k*4], &mExtentProjectionFaceVertices[k*4], projectionPlane, projectionPlaneNormal))
      return false;
  }

  return true;
}
//==============================================================================
