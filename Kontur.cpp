//==============================================================================
/*******************************************************************************
 
  This project is for creating an AutoCAD extension arx file containing the KONTUR command -
  the C++ application written using Autodesk's ObjectARX.

  ������� KONTUR ������ � ������ ���������, �������������� ���� ��������� ���� ����
  AcDbSubDMesh ��� AcDbPolyFaceMesh �� � ��������.
    ������� ������������ ���������� ������ ���� �� ���������, ����������������
  ����������� ����-�����������. ����������� ��������� � ������������� (����������� �� �����������).
    �������������� ������� � ���������� (����) ���� ���� �� ��������, ���������� � ����������
  ��������� ������� �������� ���� �� ������� �������� ������������� ���� ������������ ���������������.
    ������� �����������:
      - �� ����������� ����� ����, ������� ������������ ��� ������� ������.

  �����:  ���� ����������
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

// ���������/�������� ���������� �������.
//
  AcDbEntity* pObjectMesh = nullptr;
  if(!OpenObject(pObjectMesh)) {
    PrintCommandFinishStatus(false);
    return;
  }

// ���� ������ �� ���� ���� AcDbSubDMesh � AcDbPolyFaceMesh,
// �� ������� �������������� � ����������� ������� ������ �����.
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

// ��������� ��������� ������� � ����������� �����������.
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

// ������� � ����������� ����������� � ��������������� ��������� ��������.
//
  AcGeVector3d projectionPlaneNormal(normalVector[X], normalVector[Y], normalVector[Z]);
  AcGePlane projectionPlane;

// ������������� ����� ������������ ���������������, ������������� ����.
// �������������� ����� ��� ������������� ���������� ���� ����;
// ����������� - ����� ������� ������ �������� ���������������
// ����� ��������� ������� �������� ���� �� ��� �� ������� ��� �������������.
//
  AcGePoint3d  minExtentPoint, maxExtentPoint;

// ��������� ���������� ��������� �������� � ������������� �����
// ������������ ������������� ���� �������������� ���������������.
//
  if(!GetProjectionPlaneParams(projectionPlane, projectionPlaneNormal, minExtentPoint, maxExtentPoint,  pObjectMesh)) {
    pObjectMesh->close();
    PrintCommandFinishStatus(false);
    return;
  }

// ������� ������ � �� ��������, ����������� ��� ����������� ��������.
//
  AcArray<AcGePoint3d*>  aObjectVertices;
  AcArray<AcGePoint3d*>  aObjectProjectionVertices;

// ������, ���������� ������ �������� ����.
//
  AcDbVoidPtrArray* paObjectPtrProjectionRegions = new AcDbVoidPtrArray();
  if(!paObjectPtrProjectionRegions) {
    pObjectMesh->close();
    PrintErrorMemory(__func__, 1);
    return;
  }
  paObjectPtrProjectionRegions->append(new AcDbRegion());

  bool bResult = true;

// ������ ������ �����.
//
  AcGePoint3d  mFaceVertices[4];

// ������������� ���� �� ���� ��������� ����.
//
  if(pObjectMesh->isKindOf(AcDbSubDMesh::desc())) {

    Acad::ErrorStatus errst;

// ��������� ������� ������.
//
    AcGePoint3dArray vertexArray;
    if((errst = AcDbSubDMesh::cast(pObjectMesh)->getVertices(vertexArray)) == Acad::eOk) {

// ��������� ������� ������ ������.
//
      AcArray<Adesk::Int32> faceArrayInt32;
      if((errst = AcDbSubDMesh::cast(pObjectMesh)->getFaceArray(faceArrayInt32)) == Acad::eOk) {

// ����� ���� �� �������� ������.
//
        for(int countFaceVertices = 0, ia = 0;  ia < faceArrayInt32.length();  ia += 1 + countFaceVertices)  {
          countFaceVertices = faceArrayInt32.at(ia);

// ������������ ������� ������ �����.
//
          for (int jv = 0; jv < countFaceVertices; jv++)  {
            mFaceVertices[jv] = vertexArray.at(faceArrayInt32.at(ia + jv + 1));
          }

// ������������ ������� �������� ����: ������� �������� ������ �������� ����� � ������������ ������� �������� ����.
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

// ��������� ��������� ������ ������ ����.
//
    AcArray<AcDbObjectId> aVertexIds;
    AcDbObjectIterator* pIterator = AcDbPolyFaceMesh::cast(pObjectMesh)->vertexIterator();

    if(pIterator) {

      for(; !pIterator->done(); pIterator->step())  {

// ������� ���������� ��� �������������� ������.
//
        if(pIterator->objectId().objectClass() == AcDbPolyFaceMeshVertex::desc())  {
          aVertexIds.append(pIterator->objectId()); 
        }
        else if(pIterator->objectId().objectClass() == AcDbFaceRecord::desc())  {

// ����� ������������ ������ ������������� ��������� �� ������� ������.
//
          AcDbObjectPointer<AcDbFaceRecord>  pFaceRecord(pIterator->objectId(), AcDb::kForRead);

// ������������ ������� ������ �����.
//
          int countFaceVertices = 0;
          for(Adesk::UInt16 ui = 0; ui < 4u; ui++)  {
            Adesk::Int16 vertexIndex;
            if(pFaceRecord->getVertexAt(ui, vertexIndex) == Acad::eOk)   {

// ����������� ����� ��� ���� ������� � �������� 0 - ��������� �.
              if(vertexIndex == 0)  continue;

              AcDbObjectPointer<AcDbPolyFaceMeshVertex>  pVertex(aVertexIds[abs(vertexIndex)-1], AcDb::kForRead);
              mFaceVertices[ui] = pVertex->position();
              countFaceVertices++;
            }
          }

// ������������ ������� �������� ����: ������� �������� ������ �������� ����� � ������������ ������� �������� ����.
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

// �������� � ��������� ������� �������� ���� (���������� �������� > 1 ��� ���� � ������).
//
    bResult = CreateDrawKonturs(pObjectMesh, aObjectVertices, aObjectProjectionVertices, paObjectPtrProjectionRegions,
                                projectionPlane, projectionPlaneNormal, minExtentPoint, maxExtentPoint);
  }

// �������� ������� ����.
//
  pObjectMesh->close();

// ������������ ������ ��� ���������� �������.
//
  DeleteEntitiesFinish(paObjectPtrProjectionRegions, aObjectVertices, aObjectProjectionVertices, bResult);
}
//==============================================================================
// 
// ��������� ���������� �������. ���� ������ �� ������, �� ������������ ��� �������.
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
// ���������/�������� ���������� �������.
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
// �������� ���������� ��������� ����������� ������� � ����� ����������
// � �������������� ������� ����������� �������.
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
