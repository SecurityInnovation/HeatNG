#pragma once

#include <windows.h>

// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the TESTMODULE_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// TESTMODULE_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef TESTMODULE_EXPORTS
#define TESTMODULE_API __declspec(dllexport)
#else
#define TESTMODULE_API __declspec(dllimport)
#endif

TESTMODULE_API HLOCAL WINAPI replacementLocalAlloc(UINT uFlags, SIZE_T uBytes);
