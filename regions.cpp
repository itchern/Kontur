#include "pch.h"

#include <geline3d.h>
#include <dbents.h>
#include <dbregion.h>

#include "regions.h"
#include "prints.h"

//==============================================================================
// 
// Проверка на коллинеарность проекций вершин грани: если все проекции вершин коллинеарны, то грань не учитывается.
// (Требуются дополнительные функции, если такие грани надо учитывать.)
// 
bool CheckNotCollinearity(const AcGePoint3d mProjectionFaceVertices[], const int countFaceVertices)
{
  AcGeLine3d gLine;

  for(int iv = 0; iv < countFaceVertices-1; iv++) {
    if(mProjectionFaceVertices[iv].isEqualTo(mProjectionFaceVertices[iv+1])) continue;
    gLine.set(mProjectionFaceVertices[iv], mProjectionFaceVertices[iv+1]);
    break;
  }

  for(int iv = 0; iv < countFaceVertices; iv++) {
    if(!gLine.isOn(mProjectionFaceVertices[iv]))  return true;
  }

  return false;
}
//------------------------------------------------------------------------------
//
// Добавление региона проекции грани в общий/общие регион[ы] *paProjectionPtrRegions.
//
EAddRegion AddProjectionRegion(AcDbVoidPtrArray* paProjectionPtrRegions, const AcGePoint3d mProjectionFaceVertices[], const int countFaceVertices)
{
  if(!CheckNotCollinearity(mProjectionFaceVertices, countFaceVertices))  return EAddRegion::NoUsed;

  Acad::ErrorStatus  errst;

// Указатель (удаляемый системой после создания региона) проекций рёбер грани.
//
  AcDbLine* pLines = new AcDbLine[countFaceVertices];

// Форомирование проекций рёбер грани.
//
  AcGePoint3d vertex1 {mProjectionFaceVertices[0]};    AcGePoint3d vertex2;
  for(int iv = 0; iv < countFaceVertices; iv++) {
    vertex2 = mProjectionFaceVertices[(iv+1)%countFaceVertices];
    if((errst = pLines[iv].setStartPoint(vertex1)) != Acad::eOk ||
       (errst = pLines[iv].setEndPoint(vertex2)) != Acad::eOk) {
      delete[]  pLines;
      PrintError(__func__, 1, errst);
      return  EAddRegion::Fail;
    }
    vertex1 = vertex2;
  }
  AcDbVoidPtrArray  projectionPtrLineArray;
  for(int i = 0; i < countFaceVertices; i++) {
    projectionPtrLineArray.append(&pLines[i]);
  }

  EAddRegion eAddRegion = EAddRegion::Done;

// Указатель на регион проекции грани.
//
  AcDbVoidPtrArray* paFaceProjectionPtrRegions = new AcDbVoidPtrArray();

// Создание региона из проекций рёбер грани.
//
  if((errst = AcDbRegion::createFromCurves(projectionPtrLineArray, *paFaceProjectionPtrRegions)) == Acad::eOk) {

// Добавление региона проекции грани к общему региону.
//
    for(int ir = 0; ir < paProjectionPtrRegions->length(); ir++) {
      if((errst = static_cast<AcDbRegion*>((*paProjectionPtrRegions)[ir])->
                                           booleanOper(AcDb::BoolOperType::kBoolUnite, static_cast<AcDbRegion*>((*paFaceProjectionPtrRegions)[0]))) != Acad::eOk) {
        eAddRegion = EAddRegion::Fail;
        PrintError(__func__, 2, errst);
        break;
      }
    }
  }
  else {
    eAddRegion = EAddRegion::Fail;
    PrintError(__func__, 3, errst);
  }

// Освобождение памяти.
//
  EmptyDeleteDbVoidPtrArray(paFaceProjectionPtrRegions);

// Закрытие объектов класса AcDbLine.
//
  for(int il = 0; il < projectionPtrLineArray.length(); il++)  {
    static_cast<AcDbLine*>(projectionPtrLineArray[il])->close();
  }

  projectionPtrLineArray.removeAll();

  return eAddRegion;
}
//------------------------------------------------------------------------------
// 
// Формирование региона проекции: функция добавлет регион проекции грани к формируемому региону.
//
bool FormProjectionRegion(const int countFaceVertices, const AcGePoint3d mFaceVertices[],
                          AcArray<AcGePoint3d*>& aObjectVertices, AcArray<AcGePoint3d*>& aObjectProjectionVertices,
                          AcDbVoidPtrArray* paObjectPtrProjectionRegions,
                          const AcGePlane& projectionPlane, const AcGeVector3d& projectionPlaneNormal)
{
  bool bResult = true;

// Формирование массива проекций вершин грани. 
//
  AcGePoint3d  mProjectionFaceVertices[4];
  for (int jv = 0; jv < countFaceVertices; jv++)  {
    mProjectionFaceVertices[jv] = mFaceVertices[jv].project(projectionPlane, projectionPlaneNormal);
  }

// Добавление региона проекции грани, определяемой вершинами *mProjectionFaceVertices, 
// к общему региону *paObjectPtrProjectionRegions проекции сети,
// и сохранение координат вершин сети и их проекций в соответствующих массивах,
// которые освобождаются в конце работы Команды функцией DeleteEntitiesFinish.
//
  EAddRegion eAddRegion;
  if((eAddRegion = AddProjectionRegion(paObjectPtrProjectionRegions, mProjectionFaceVertices, countFaceVertices)) == EAddRegion::Done) {
    for(int iv = 0; iv < countFaceVertices; iv++) {
      aObjectVertices.append(new AcGePoint3d(mFaceVertices[iv]));
      aObjectProjectionVertices.append(new AcGePoint3d(mProjectionFaceVertices[iv]));
    }
  }
  else if(eAddRegion == EAddRegion::Fail) {
    bResult = false;
    PrintError(__func__, 1, L"AddProjectionRegion: Fail");
  }

  return bResult;
}
//------------------------------------------------------------------------------
// 
// Освобождение памяти удалением объектов классов, производных от AcDbEntity (деструкторы у них виртуальные),
// а также удалением самого объекта *pVoidPtrArray.
//
void EmptyDeleteDbVoidPtrArray(AcDbVoidPtrArray*& pVoidPtrArray)
{
  for(int iptr = 0; iptr < pVoidPtrArray->length(); iptr++) {
    delete  static_cast<AcDbEntity*>((*pVoidPtrArray)[iptr]);
  }
  pVoidPtrArray->removeAll();

  delete pVoidPtrArray;  pVoidPtrArray = nullptr;
}
//==============================================================================
