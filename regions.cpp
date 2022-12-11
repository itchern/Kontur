#include "pch.h"

#include <geline3d.h>
#include <dbents.h>
#include <dbregion.h>

#include "regions.h"
#include "prints.h"

//==============================================================================
// 
// �������� �� �������������� �������� ������ �����: ���� ��� �������� ������ �����������, �� ����� �� �����������.
// (��������� �������������� �������, ���� ����� ����� ���� ���������.)
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
// ���������� ������� �������� ����� � �����/����� ������[�] *paProjectionPtrRegions.
//
EAddRegion AddProjectionRegion(AcDbVoidPtrArray* paProjectionPtrRegions, const AcGePoint3d mProjectionFaceVertices[], const int countFaceVertices)
{
  if(!CheckNotCollinearity(mProjectionFaceVertices, countFaceVertices))  return EAddRegion::NoUsed;

  Acad::ErrorStatus  errst;

// ��������� (��������� �������� ����� �������� �������) �������� ���� �����.
//
  AcDbLine* pLines = new AcDbLine[countFaceVertices];

// ������������� �������� ���� �����.
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

// ��������� �� ������ �������� �����.
//
  AcDbVoidPtrArray* paFaceProjectionPtrRegions = new AcDbVoidPtrArray();

// �������� ������� �� �������� ���� �����.
//
  if((errst = AcDbRegion::createFromCurves(projectionPtrLineArray, *paFaceProjectionPtrRegions)) == Acad::eOk) {

// ���������� ������� �������� ����� � ������ �������.
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

// ������������ ������.
//
  EmptyDeleteDbVoidPtrArray(paFaceProjectionPtrRegions);

// �������� �������� ������ AcDbLine.
//
  for(int il = 0; il < projectionPtrLineArray.length(); il++)  {
    static_cast<AcDbLine*>(projectionPtrLineArray[il])->close();
  }

  projectionPtrLineArray.removeAll();

  return eAddRegion;
}
//------------------------------------------------------------------------------
// 
// ������������ ������� ��������: ������� �������� ������ �������� ����� � ������������ �������.
//
bool FormProjectionRegion(const int countFaceVertices, const AcGePoint3d mFaceVertices[],
                          AcArray<AcGePoint3d*>& aObjectVertices, AcArray<AcGePoint3d*>& aObjectProjectionVertices,
                          AcDbVoidPtrArray* paObjectPtrProjectionRegions,
                          const AcGePlane& projectionPlane, const AcGeVector3d& projectionPlaneNormal)
{
  bool bResult = true;

// ������������ ������� �������� ������ �����. 
//
  AcGePoint3d  mProjectionFaceVertices[4];
  for (int jv = 0; jv < countFaceVertices; jv++)  {
    mProjectionFaceVertices[jv] = mFaceVertices[jv].project(projectionPlane, projectionPlaneNormal);
  }

// ���������� ������� �������� �����, ������������ ��������� *mProjectionFaceVertices, 
// � ������ ������� *paObjectPtrProjectionRegions �������� ����,
// � ���������� ��������� ������ ���� � �� �������� � ��������������� ��������,
// ������� ������������� � ����� ������ ������� �������� DeleteEntitiesFinish.
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
// ������������ ������ ��������� �������� �������, ����������� �� AcDbEntity (����������� � ��� �����������),
// � ����� ��������� ������ ������� *pVoidPtrArray.
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
