#include "pch.h"

#include <dbapserv.h>
#include <acedads.h>
#include <dbents.h>
#include <dbregion.h>

#include "Kontur.h"
#include "extents.h"
#include "regions.h"
#include "prints.h"
#include "draw.h"

//==============================================================================
//
// Модифицированная функция из help-а.
//
bool DrawPolyline(AcGePoint3dArray& aKonturVertices)
{
  Acad::ErrorStatus errst;

  AcDbBlockTable* pBlockTable = nullptr;
  if((errst = acdbHostApplicationServices()->workingDatabase()->getSymbolTable(pBlockTable, AcDb::kForRead)) != Acad::eOk) {
    PrintError(__func__, 1, errst);
    return false;
  }

  bool bResult = true;

  AcDb3dPolyline* pPolyline = new AcDb3dPolyline(AcDb::Poly3dType::k3dSimplePoly, aKonturVertices, Adesk::kTrue);
  if(!pPolyline)  {
    PrintError(__func__, 2, errst);
    bResult = false;
  }
  else {
    pPolyline->setColorIndex(3);

    AcDbBlockTableRecord* pBlockTableRecord = nullptr;
    if((errst = pBlockTable->getAt(ACDB_MODEL_SPACE, pBlockTableRecord, AcDb::kForWrite)) == Acad::eOk) {
      AcDbObjectId objId {};
      if((errst = pBlockTableRecord->appendAcDbEntity(objId, pPolyline)) == Acad::eOk) {
        pPolyline->close();
      }
      else  {
        PrintError(__func__, 3, errst);
        bResult = false;
      }
      pBlockTableRecord->close();
    }
    else {
      PrintError(__func__, 4, errst);
      bResult = false;
    }

    if(!bResult)  delete pPolyline;
  }

  pBlockTable->close();

  return bResult;
}
//------------------------------------------------------------------------------
bool DrawKonturs(const AcDbVoidPtrArray* paFinalPtrProjectionRegions,
                 const AcArray<AcGePoint3d*>& aObjectVertices,
                 const AcArray<AcGePoint3d*>& aObjectProjectionVertices,
                 const AcGePoint3d mExtentProjectVertices[],
                 const AcGePlane& projectionPlane,  const AcGeVector3d& projectionPlaneNormal)
{
  Acad::ErrorStatus errst;

// Массив индексов вершин сети, имеюжщх одинаковые координаты проекций.
//
  AcArray<int> aSameProjectionVertexIndices;

// Перебор всех полученных регионов. Один регион определяет один контур сети.
//
  for(int ir = 0; ir < paFinalPtrProjectionRegions->length(); ir++) {

// Указатель на массив указателей на объекты класса AcDbRegion
// в случае сети с дырами, или на объекты класса AcDbLine для региона.
// (Указатели на объекты захватываются и удаляются системой.)
//
    AcDbVoidPtrArray* paPtrEntitySet = new AcDbVoidPtrArray();

// Массив вершин сети, через которые будет проходить один контур.
//
    AcGePoint3dArray aKonturVertices;

// Получение множества регионов или линий.
//
    if((errst = (static_cast<AcDbEntity*>((*paFinalPtrProjectionRegions)[ir]))->explode(*paPtrEntitySet)) == Acad::eOk) {

      AcGePoint3d  projectionPoint;

// Перебор всех объектов классов в paPtrEntitySet с целью формирования
// массива вершин сети, через которые будут проходить искомые контуры.
//
      for(int ie = 0; ie < paPtrEntitySet->length(); ie++) {

// В случае если в paPtrEntitySet находятся указатели на объекты класса AcDbRegion,
// осуществляется рекурсивный вызов функции DrawKonturs.
//
        if(static_cast<AcDbEntity*>(paPtrEntitySet->at(ie))->isKindOf(AcDbRegion::desc())) {
          if(DrawKonturs(paPtrEntitySet, aObjectVertices, aObjectProjectionVertices, mExtentProjectVertices,
                         projectionPlane, projectionPlaneNormal))
            return true;
          PrintError(__func__, 1, L"Recursive function call failed.");
          return false;
        }

// В случае если в paPtrEntitySet находятся указатели на объекты класса AcDbLine,
// осуществляется поиск вершин сети, через которые будет проходить контур.
// Вершины находятся методом сравнения вершин регионов проекций с точками проекций вершин сети.
//
        if(static_cast<AcDbEntity*>(paPtrEntitySet->at(ie))->isKindOf(AcDbLine::desc())) {
          projectionPoint = static_cast<AcDbLine*>(paPtrEntitySet->at(ie))->startPoint();

// Если вершина региона проекции совпадает с точкой проекции параллелепипеда, то она не учитывается.
//
          bool bres = false;
          for(int iv = 0; iv < 6*4; iv++) {
            if(!projectionPoint.isEqualTo(mExtentProjectVertices[iv]))  continue;
            bres = true;
            break;
          }
          if(bres)  continue;

// Заполнение массива aSameProjectionVertexIndices индексами вершин сети с одинаковыми координатами проекций.
//
          for(int ivp = 0; ivp < aObjectProjectionVertices.length(); ivp++) {
            if(projectionPoint.isEqualTo(*aObjectProjectionVertices.at(ivp)))
              aSameProjectionVertexIndices.append(ivp);
          }

// Определение индекса вершины сети, имеющей минимальное расстояние до плоскости проекции и помещение его в массив aKonturVertices.
//
          if(!aSameProjectionVertexIndices.isEmpty()) {  int ivm {};
            if(aSameProjectionVertexIndices.length() == 1)  ivm = aSameProjectionVertexIndices[0];
            else {
              double minDistance = DBL_MAX;  double distance;
              for (int jv = 0; jv < aSameProjectionVertexIndices.length(); jv++) {
                if((distance = aObjectVertices.at(aSameProjectionVertexIndices.at(jv))->project(projectionPlane, projectionPlaneNormal).
                                                                                        distanceTo(*aObjectVertices.at(aSameProjectionVertexIndices.at(jv)))) < minDistance) {
                  minDistance = distance;
                  ivm = aSameProjectionVertexIndices.at(jv);
                }
              }
            }
            aKonturVertices.append(*aObjectVertices.at(ivm));
            aSameProjectionVertexIndices.removeAll();
          }
        }
        delete  static_cast<AcDbEntity*>((*paPtrEntitySet).at(ie));
      }

      paPtrEntitySet->removeAll();
      delete paPtrEntitySet;   paPtrEntitySet = nullptr;
    }
    else {
      delete paPtrEntitySet;   paPtrEntitySet = nullptr;
      PrintError(__func__, 2, errst);
      return false;
    }

    bool bResult = DrawPolyline(aKonturVertices);

    aKonturVertices.removeAll();

    if(!bResult)  return false;
  }

  return true;
}
//------------------------------------------------------------------------------
// 
// Создание и рисование искомых контуров сети (количество контуров > 1 для сети с дырами).
//
bool CreateDrawKonturs(const AcDbEntity* pObjectMesh, const AcArray<AcGePoint3d*>& aObjectVertices,
                       const AcArray<AcGePoint3d*>& aObjectProjectionVertices, const AcDbVoidPtrArray* paObjectPtrProjectionRegions,
                       const AcGePlane& projectionPlane, const AcGeVector3d& projectionPlaneNormal,
                       const AcGePoint3d& minExtentPoint, const AcGePoint3d& maxExtentPoint)
{
  bool bResult = false;
  Acad::ErrorStatus errst;

// Массив вершин охватывающего сеть увеличенного прямоугольного параллелепипеда.
//
  AcGePoint3d  mExtentProjectionFaceVertices[6*4] {}; // 6 граней по 4 вершины

// Массив результирующих регионов для построения требуемых контуров.
//
  AcDbVoidPtrArray*  paFinalPtrProjectionRegions = new AcDbVoidPtrArray();  paFinalPtrProjectionRegions->append(new AcDbRegion());

// Получение региона проекции параллелепипеда.
//
  if(GetExtentsProjectionRegion(paFinalPtrProjectionRegions, mExtentProjectionFaceVertices,
                                projectionPlane, projectionPlaneNormal,
                                minExtentPoint, maxExtentPoint))  {

// Получение результирующих регионов для построения требуемых контуров
// методом вычитания региона проекции сети из региона проекции параллелепипеда.
//
    for(int k = 0; k < paObjectPtrProjectionRegions->length(); k++) {
      if((errst = static_cast<AcDbRegion*>((*paFinalPtrProjectionRegions)[0])->booleanOper(AcDb::BoolOperType::kBoolSubtract,
                                                                                           static_cast<AcDbRegion*>((*paObjectPtrProjectionRegions)[k]))) != Acad::eOk) {
        PrintError(__func__, 1, errst);
        break;
      }
    }

// Неподдерживаемый случай.
//
    if(paObjectPtrProjectionRegions->length() > 1 && paFinalPtrProjectionRegions->length() > 1)  {
      EmptyDeleteDbVoidPtrArray(paFinalPtrProjectionRegions);

      std::wstring ws;  ws = L"The selected object is not supported by the ";  ws += cmdLocalName;
      ws += L" command.\nIt is necessary to refine the command for complex objects.";
      acedAlert(ws.c_str());

      return false;
    }

    bResult = DrawKonturs(paFinalPtrProjectionRegions, aObjectVertices, aObjectProjectionVertices, mExtentProjectionFaceVertices, projectionPlane, projectionPlaneNormal);
  }

  EmptyDeleteDbVoidPtrArray(paFinalPtrProjectionRegions);

  return bResult;
}

//==============================================================================
