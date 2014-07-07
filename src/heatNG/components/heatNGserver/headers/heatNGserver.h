#pragma once

// C++ headers
#include <map>


// Our headers
#include "common.h"

#include "CommHandler.h"
#include "CommServer.h"

#include "sinativeintercept.h"

bool   startupServer();
bool terminateServer();

bool   initRASForThread();
bool deinitRASForThread();


CommServer *g_heatNGserver = NULL;


// exports
PVOID __declspec(dllexport) WINAPI getOriginalFunctionCaller(LPSTR pszFuncName, LPSTR pszFuncDLL);
DWORD __declspec(dllexport) WINAPI disableInterceptionInCurrentThread();
DWORD __declspec(dllexport) WINAPI enableInterceptionInCurrentThread();

