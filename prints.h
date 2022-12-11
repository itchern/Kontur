#pragma once

#include <acadstrc.h>
#include <acestext.h>

//==============================================================================
// 
// Функции обработки текста
//
void PrintError(const char* pcFunctionName, const int ordinalErrorNumber, const ACHAR* pErrorText);
inline void PrintError(const char* pcFunctionName, const int ordinalErrorNumber, const Acad::ErrorStatus errst) {
  PrintError(pcFunctionName, ordinalErrorNumber, acadErrorStatusText(errst));
}
void PrintErrorMemory(const char* pcFunctionName, const int ordinalErrorNumber);

void PrintCommandFinishStatus(const bool bResult);
//==============================================================================
