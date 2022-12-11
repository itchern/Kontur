//==============================================================================
#include "pch.h"

#include <memory>
#include <xstring>
#include <acutads.h>

#include "Kontur.h"
#include "Prints.h"

//==============================================================================

const ACHAR* PwFinishUnsuccessText = L"***  Finished Unsuccessfully  ***";
//==============================================================================
// 
// Функции обработки текста
//
//------------------------------------------------------------------------------
// 
// Получение текста wchar_t из текста char
//
void GetWText(std::unique_ptr<ACHAR[]>& upwText, const char* pcText)
{
  size_t size;  mbstowcs_s(&size, NULL, 0, pcText, 0);
  upwText = std::make_unique<ACHAR[]>(size*sizeof(ACHAR));
  mbstowcs_s(NULL, upwText.get(), size, pcText, size);
}
//------------------------------------------------------------------------------
// 
// Печать текста ошибки с указанием функции, в которой произошла ошибка, и номера ошибки в этой функции.
//
void PrintError(const char* pcFunctionName, const int ordinalErrorNumber, const ACHAR* pErrorText)
{
  std::unique_ptr<ACHAR[]> upwFunctionName;
  GetWText(upwFunctionName, pcFunctionName);

  std::wstring ws;
  ws = L"\n";  ws += cmdLocalName;  ws += L":  **** ERROR ";
  ACHAR buf[8];
  _itow_s(ordinalErrorNumber, buf, 10);  ws += buf;  ws += L" ****  in \"";
  ws += upwFunctionName.get();  ws += L"\": \"%s\"";
  acutPrintf(ws.c_str(), pErrorText);
}
//------------------------------------------------------------------------------
void PrintError(const char* pFunctionName, const int ordinalErrorNumber, const char* pcErrorText)
{
  std::unique_ptr<ACHAR[]> upwErrorText;
  GetWText(upwErrorText, pcErrorText);
  PrintError(pFunctionName, ordinalErrorNumber, upwErrorText.get());
}
//------------------------------------------------------------------------------
void PrintErrorMemory(const char* pcFunctionName, const int ordinalErrorNumber)
{
  std::wstring ws;
  ws = L"\nMemory allocation error.  ";  ws += PwFinishUnsuccessText;
  PrintError(pcFunctionName, ordinalErrorNumber, ws.c_str());
}
//------------------------------------------------------------------------------
void PrintCommandFinishStatus(const bool bResult)
{
  std::wstring ws;  ws = L"\n";  ws += cmdLocalName;
  if(bResult) { ws += L":  Finished Successfully"; }
  else        { ws += L":  ";  ws += PwFinishUnsuccessText; }
  acutPrintf(ws.c_str());
}
//==============================================================================
