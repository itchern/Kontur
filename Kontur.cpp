//==============================================================================
/*******************************************************************************
 
  This project is for creating an AutoCAD extension arx file containing the KONTUR command -
  the C++ application written using Autodesk's ObjectARX.

  Команда KONTUR создаёт и рисует полилинии, оконтуривающие одну выбранную сеть типа
  AcDbSubDMesh или AcDbPolyFaceMesh по её вершинам.
    Контуры определяются проекциями вершин сети на плоскость, перпендикулярную
  направлению сеть-наблюдатель. Наблюдатель находится в бесконечности (перспектива не учитывается).
    Оконтуриваются внешние и внутренние (дыры) края сети по регионам, полученным в результате
  вычетания региона проекции сети из региона проекции охватывающего сеть увеличенного параллелепипеда.
    Текущие ограничения:
      - не учитываются грани сети, видимые наблюдателем как отрезки прямых.

  Автор:  Юрий Чернявский
  Email:  itchern@gmail.com

********************************************************************************/
//==============================================================================

#include "pch.h"

#include <rxregsvc.h>
#include <accmd.h>
#include <dbSubD.h>
#include <dbents.h>
#include <dbobjptr.h>
#include <dbregion.h>
#include <adscodes.h>
#include <acedads.h>
#include <dbapserv.h>

#include "extents.h"
#include "regions.h"
#include "draw.h"
#include "prints.h"
#include "Kontur.h"

//==============================================================================
const ACHAR* cmdGroupName = L"TEST_KONTUR";
const ACHAR* cmdGlobalName = L"G_KONTUR";
const ACHAR* cmdLocalName = L"KONTUR";

//==============================================================================

void KONTUR_Command();

bool InitApp(void* appId);
inline bool UnloadApp() { return  acedRegCmds->removeGroup(cmdGroupName) == Acad::eOk ? true : false; }

int  GetSelectedObject(ads_name& entres, ads_point& ptres);
bool OpenObject(AcDbEntity*& pObjectMesh);
void DeleteEntitiesFinish(AcDbVoidPtrArray*& paObjectPtrProjectionRegions,
                          AcArray<AcGePoint3d*>& aObjectVertices, AcArray<AcGePoint3d*>& aObjectProjectionVertices,
                          const bool bResult);

//==============================================================================

extern "C" AcRx::AppRetCode __declspec(dllexport)
acrxEntryPoint(AcRx::AppMsgCode msg, void* appId)
{
  switch(msg) {
    case AcRx::kInitAppMsg:
      if(!InitApp(appId))  return AcRx::kRetError;
      break;
    case AcRx::kUnloadAppMsg:
      if(!UnloadApp())  return AcRx::kRetError;
      break;
    default:
      return AcRx::kRetError;
  }

  return AcRx::kRetOK;
}
//------------------------------------------------------------------------------
bool InitApp(void* appId)
{
//  if(!acrxUnlockApplication(appId))    return false;
  acrxUnlockApplication(appId);
  acrxRegisterAppMDIAware(appId);

  if(acedRegCmds->addCommand(cmdGroupName,
                             cmdGlobalName,
                             cmdLocalName,
                             ACRX_CMD_MODAL,
                             &KONTUR_Command) != Acad::eOk)  return false;
  return true;
}
//==============================================================================

void KONTUR_Command()
{

// Получение/открытие выбранного объекта.
//
  AcDbEntity* pObjectMesh = nullptr;
  if(!OpenObject(pObjectMesh)) {
    PrintCommandFinishStatus(false);
    return;
  }

// Если объект не сеть типа AcDbSubDMesh и AcDbPolyFaceMesh,
// то выдаётся предупреждение и предложение выбрать объект снова.
// 
  while(!pObjectMesh->isKindOf(AcDbSubDMesh::desc()) && !pObjectMesh->isKindOf(AcDbPolyFaceMesh::desc())) {
    pObjectMesh->close();
    std::wstring ws;  ws = L"\n";  ws += cmdLocalName; ws += L":  The entity must be an object of AcDbSubDMesh or AcDbPolyFaceMesh.";
    acutPrintf(ws.c_str());
    if(!OpenObject(pObjectMesh))  {
      PrintCommandFinishStatus(false);
      return;
    }
  }

// Получение координат вектора в направлении наблюдателя.
//
  int stcode;     ads_point normalVector;
  std::wstring ws;  ws = L"\n\n";  ws += cmdLocalName; ws += L":  Enter the vector in the observer direction <0,0,1>:  ";
  while((stcode = acedGetPoint(0, ws.c_str(), normalVector)) != RTNORM) {
    if(stcode == RTNONE) {
      normalVector[X] = 0.0;  normalVector[Y] = 0.0;  normalVector[Z] = 1.0;
      break;
    }
    if(stcode == RTCAN)  {
      pObjectMesh->close();
      PrintCommandFinishStatus(false);
      return;
    }
  }

// Нормаль в направлении наблюдателя и соответствующая плоскость проекции.
//
  AcGeVector3d projectionPlaneNormal(normalVector[X], normalVector[Y], normalVector[Z]);
  AcGePlane projectionPlane;

// Экстремальные точки увеличенного параллелепипеда, охватывающего сеть.
// Параллелепипед нужен для оконтуривания внутренних краёв сети;
// увеличенный - чтобы внешний регион проекции параллелепипеда
// после вычетания региона проекции сети не был бы нулевым или фрагментарным.
//
  AcGePoint3d  minExtentPoint, maxExtentPoint;

// Получение параметров плоскости проекции и экстремальных точек
// увеличенного охватывающего сеть прямоугольного параллелепипеда.
//
  if(!GetProjectionPlaneParams(projectionPlane, projectionPlaneNormal, minExtentPoint, maxExtentPoint,  pObjectMesh)) {
    pObjectMesh->close();
    PrintCommandFinishStatus(false);
    return;
  }

// Массивы вершин и их проекций, учитываемых при определении контуров.
//
  AcArray<AcGePoint3d*>  aObjectVertices;
  AcArray<AcGePoint3d*>  aObjectProjectionVertices;

// Массив, содержащий регион проекции сети.
//
  AcDbVoidPtrArray* paObjectPtrProjectionRegions = new AcDbVoidPtrArray();
  if(!paObjectPtrProjectionRegions) {
    pObjectMesh->close();
    PrintErrorMemory(__func__, 1);
    return;
  }
  paObjectPtrProjectionRegions->append(new AcDbRegion());

  bool bResult = true;

// Массив вершин грани.
//
  AcGePoint3d  mFaceVertices[4];

// Распределение кода по типу выбранной сети.
//
  if(pObjectMesh->isKindOf(AcDbSubDMesh::desc())) {

    Acad::ErrorStatus errst;

// Получение массива вершин.
//
    AcGePoint3dArray vertexArray;
    if((errst = AcDbSubDMesh::cast(pObjectMesh)->getVertices(vertexArray)) == Acad::eOk) {

// Получение массива списка граней.
//
      AcArray<Adesk::Int32> faceArrayInt32;
      if((errst = AcDbSubDMesh::cast(pObjectMesh)->getFaceArray(faceArrayInt32)) == Acad::eOk) {

// Обход сети по вершинам граней.
//
        for(int countFaceVertices = 0, ia = 0;  ia < faceArrayInt32.length();  ia += 1 + countFaceVertices)  {
          countFaceVertices = faceArrayInt32.at(ia);

// Формирование массива вершин грани.
//
          for (int jv = 0; jv < countFaceVertices; jv++)  {
            mFaceVertices[jv] = vertexArray.at(faceArrayInt32.at(ia + jv + 1));
          }

// Формирование региона проекции сети: функция добавлет регион проекции грани к формируемому региону проекции сети.
//
          if(!(bResult = FormProjectionRegion(countFaceVertices, mFaceVertices,
                                              aObjectVertices, aObjectProjectionVertices,
                                              paObjectPtrProjectionRegions,
                                              projectionPlane, projectionPlaneNormal)))  break;
        }
      }
      else {
        bResult = false;
        PrintError(__func__, 2, errst);
      }
    }
    else {
      bResult = false;
      PrintError(__func__, 3, errst);
    }
  }

  else if(pObjectMesh->isKindOf(AcDbPolyFaceMesh::desc())) {

// Получение итератора обхода вершин сети.
//
    AcArray<AcDbObjectId> aVertexIds;
    AcDbObjectIterator* pIterator = AcDbPolyFaceMesh::cast(pObjectMesh)->vertexIterator();

    if(pIterator) {

      for(; !pIterator->done(); pIterator->step())  {

// Сначала получаются все идентификаторы вершин.
//
        if(pIterator->objectId().objectClass() == AcDbPolyFaceMeshVertex::desc())  {
          aVertexIds.append(pIterator->objectId()); 
        }
        else if(pIterator->objectId().objectClass() == AcDbFaceRecord::desc())  {

// После перечисления вершин перечисляются указатели на вершины граней.
//
          AcDbObjectPointer<AcDbFaceRecord>  pFaceRecord(pIterator->objectId(), AcDb::kForRead);

// Формирование массива вершин грани.
//
          int countFaceVertices = 0;
          for(Adesk::UInt16 ui = 0; ui < 4u; ui++)  {
            Adesk::Int16 vertexIndex;
            if(pFaceRecord->getVertexAt(ui, vertexIndex) == Acad::eOk)   {

// Треугольная грань даёт одну вершину с индексом 0 - исключаем её.
              if(vertexIndex == 0)  continue;

              AcDbObjectPointer<AcDbPolyFaceMeshVertex>  pVertex(aVertexIds[abs(vertexIndex)-1], AcDb::kForRead);
              mFaceVertices[ui] = pVertex->position();
              countFaceVertices++;
            }
          }

// Формирование региона проекции сети: функция добавлет регион проекции грани к формируемому региону проекции сети.
//
          if(!(bResult = FormProjectionRegion(countFaceVertices, mFaceVertices,
                                              aObjectVertices, aObjectProjectionVertices,
                                              paObjectPtrProjectionRegions,
                                              projectionPlane, projectionPlaneNormal)))  break;
        }
      }

      delete pIterator;  pIterator = nullptr;
    }
    else {
      bResult = false;
      PrintErrorMemory(__func__, 4);
    }
  }
  else {
    bResult = false;
    PrintError(__func__, 5, L"Unsupported case.");
  }

  if(bResult) {

// Создание и рисование искомых контуров сети (количество контуров > 1 для сети с дырами).
//
    bResult = CreateDrawKonturs(pObjectMesh, aObjectVertices, aObjectProjectionVertices, paObjectPtrProjectionRegions,
                                projectionPlane, projectionPlaneNormal, minExtentPoint, maxExtentPoint);
  }

// Закрытие объекта сеть.
//
  pObjectMesh->close();

// Освобождение памяти при завершении Команды.
//
  DeleteEntitiesFinish(paObjectPtrProjectionRegions, aObjectVertices, aObjectProjectionVertices, bResult);
}
//==============================================================================
// 
// Получения выбранного объекта. Если объект не выбран, то предлагается его выбрать.
//
int GetSelectedObject(ads_name& name, ads_point& ptres)
{
  std::wstring ws;  ws = L"\n";  ws += cmdLocalName;  ws += L":  Please select an entity: ";
  int stcode = acedEntSel(ws.c_str(), name, ptres);
  while(stcode != RTNORM) {
    ws = L"\n";  ws += cmdLocalName;  ws += L":  Please select an entity again: ";
    stcode = acedEntSel(ws.c_str(), name, ptres);
    if(stcode == RTCAN)  break;
  }
  return stcode;
}
//------------------------------------------------------------------------------
// 
// Получение/открытие выбранного объекта.
//
bool OpenObject(AcDbEntity*& pObjectMesh)
{
  int stcode;
  ads_name name{};   ads_point ptres{};

  if((stcode = GetSelectedObject(name, ptres)) == RTCAN)  return false;

  AcDbObjectId objId;
  if(acdbGetObjectId(objId, name) != Acad::eOk)  return false;
  if(acdbOpenObject(pObjectMesh, objId, AcDb::kForRead) != Acad::eOk)  return false;

  return true;
}
//==============================================================================
//
// Удаление указателей созданных экземпляров классов и вывод информации
// о результирующем статусе выполненной Команды.
// 
void DeleteEntitiesFinish(AcDbVoidPtrArray*& paObjectPtrProjectionRegions,
                          AcArray<AcGePoint3d*>& aObjectVertices, AcArray<AcGePoint3d*>& aObjectProjectionVertices, const bool bResult)
{
assert(aObjectVertices.length() == aObjectProjectionVertices.length());
  for(int iv = 0; iv < aObjectVertices.length(); iv++) {
    delete aObjectVertices[iv];
    delete aObjectProjectionVertices[iv];
  }
  aObjectVertices.removeAll();
  aObjectProjectionVertices.removeAll();

  EmptyDeleteDbVoidPtrArray(paObjectPtrProjectionRegions);

  PrintCommandFinishStatus(bResult);
}
//==============================================================================
