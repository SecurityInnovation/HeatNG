// testModule.cpp : Defines the exported functions for the DLL application.
//

#include "testModule.h"

#include "common.h"
#include "sisystem.h"

#include <windows.h>

typedef HMODULE (WINAPI *LoadLibraryExWTYPE)(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
typedef HMODULE (WINAPI *LoadLibraryWTYPE)  (LPCWSTR lpLibFileName);

typedef HANDLE  (WINAPI *CreateFileWTYPE)   (LPCWSTR lpFileName,
                                                    DWORD dwDesiredAccess,
                                                    DWORD dwShareMode,
                                                    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                                    DWORD dwCreationDisposition,
                                                    DWORD dwFlagsAndAttributes,
                                                    HANDLE hTemplateFile);



typedef PVOID (WINAPI *getOriginalFunctionCallerType)(LPSTR pszFuncName, LPSTR pszFuncDLL);

LoadLibraryExWTYPE LoadLibraryExWOriginal = NULL;
LoadLibraryWTYPE   LoadLibraryWOriginal   = NULL;
CreateFileWTYPE    CreateFileWOriginal    = NULL;

extern getOriginalFunctionCallerType gofc;

TESTMODULE_API HLOCAL WINAPI replacementLocalAlloc(UINT uFlags, SIZE_T uBytes)
{
    DBG( debugOutput(L"Entering into  replacementLocalAlloc"); )
    HLOCAL h = LocalAlloc(uFlags, uBytes);
    DBG( debugOutput(L"Returning from replacementLocalAlloc"); )
    return h;
}


TESTMODULE_API HMODULE WINAPI replacementLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
    wchar_t *pwszLibFileName = new wchar_t[wcslen(lpLibFileName) + 1];
    wcscpy(pwszLibFileName, lpLibFileName);

    TRACEX( L"[enter] replacementLoadLibraryExW", GetCurrentThreadId() )

    if (!LoadLibraryExWOriginal)
        LoadLibraryExWOriginal = (LoadLibraryExWTYPE) gofc("LoadLibraryExW", "kernel32.dll");

    assert(LoadLibraryExWOriginal != NULL);

    HMODULE h = LoadLibraryExW(pwszLibFileName, hFile, dwFlags);

    DBG( if (!h) debugOutput(L"LoadLibraryExW failed!"); )
    delete[] pwszLibFileName;

    TRACEX( L"[leave] replacementLoadLibraryExW", GetCurrentThreadId() )
    return h;
}


TESTMODULE_API HMODULE WINAPI replacementLoadLibraryW(LPCWSTR lpLibFileName)
{
    wchar_t *pwszLibFileName = new wchar_t[wcslen(lpLibFileName) + 1];
    wcscpy(pwszLibFileName, lpLibFileName);

    TRACEX( L"[enter] replacementLoadLibraryW", GetCurrentThreadId() )


    if (!LoadLibraryWOriginal)
        LoadLibraryWOriginal = (LoadLibraryWTYPE) gofc("LoadLibraryW", "kernel32.dll");

    assert(LoadLibraryWOriginal != NULL);

    HMODULE h = LoadLibraryWOriginal(pwszLibFileName);
    delete[] pwszLibFileName;

    TRACEX( L"[leave] replacementLoadLibraryW", GetCurrentThreadId() )
    return h;
}


TESTMODULE_API HANDLE WINAPI replacementCreateFileW(LPCWSTR lpFileName,
                                                    DWORD dwDesiredAccess,
                                                    DWORD dwShareMode,
                                                    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                                    DWORD dwCreationDisposition,
                                                    DWORD dwFlagsAndAttributes,
                                                    HANDLE hTemplateFile)
{
    wchar_t *pwszFileName = new wchar_t[wcslen(lpFileName) + 1];
    wcscpy(pwszFileName, lpFileName);

    TRACEX( L"[enter] replacementCreateFileW", GetCurrentThreadId() )

    if (!CreateFileWOriginal)
        CreateFileWOriginal = (CreateFileWTYPE) gofc("CreateFileW", "kernel32.dll");

    assert(CreateFileWOriginal != NULL);

    HANDLE h = CreateFileWOriginal(pwszFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);

    TRACEX( L"[leave] replacementCreateFileW", GetCurrentThreadId() )
    delete[] pwszFileName;
    return h;
}
