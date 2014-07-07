// siexception header file
#pragma once

#include "common.h"
#include "siheatdefines.h"

#include "sistartwith.h"
#include "sicodegen.h"


// -- client support functions

IS bool injectIntoProcess(unsigned long ulProcessId, unsigned long ulThreadId, wchar_t *pwszModuleName)
{
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, false, ulProcessId);
    if (INVALID_HANDLE_VALUE == hProcess) return false;

    HANDLE hThread  = OpenThread(THREAD_ALL_ACCESS, false, ulThreadId);
    if (INVALID_HANDLE_VALUE == hThread) { CloseHandle(hProcess); return false; }

    try
    {
        unsigned long ulStrLen = (unsigned long) wcslen(pwszModuleName);
        char *pszModuleName = new char[ulStrLen + 1];
        wcstombs(pszModuleName, pwszModuleName, ulStrLen); pszModuleName[ulStrLen] = 0;

        // setup process's headers to load our dll
        sistartwith proc(hProcess, pszModuleName);

        // resume the process (our dll will re-suspend it when it loads)
        ResumeThread(hThread);

        delete[] pszModuleName;
    }
    catch(siexception e)
    {
        DBG( debugOutput(e.what().c_str()); )

        CloseHandle(hThread);
        CloseHandle(hProcess);
        return false;
    }

    CloseHandle(hThread);
    CloseHandle(hProcess);

    return true;
}


IS bool injectIntoRunningProcess(unsigned long ulProcessId, wchar_t *pwszModuleName)
{
    void *pvCode            = 0;
    void *pvTargetCode      = 0;
    void *pvTargetBuffer    = 0;

    HANDLE hProcess = 0;
    hProcess = OpenProcess(PROCESS_ALL_ACCESS, false, ulProcessId);
    if (!hProcess)
    {
        DBGLOG( L"Couldn't get process handle for process", ulProcessId )
        goto error;
    }

    suspendAllThreads(ulProcessId);

    pvCode            = VirtualAlloc  (          NULL, PAGE_SIZE, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    pvTargetCode      = VirtualAllocEx(hProcess, NULL, PAGE_SIZE, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    TRACEX( L"Target code is at: ", pvTargetCode )

    // use space at the end of our alloc'd mem for the string holding the lib name
    pvTargetBuffer    = (byte*) pvTargetCode + PAGE_SIZE - (wcslen(pwszModuleName) + 1) * sizeof(wchar_t);
    TRACEX( L"Target buffer is at: ", pvTargetBuffer )

    int offset = (DWORD) pvTargetCode - (DWORD) pvCode;
    if (!pvCode || !pvTargetCode) goto error;

    // create stub code block to load our library
    // note: We can directly call CreateRemoteThread with the address as LoadLibraryW
    // but we're creating this stub in case we want any additional functionality from
    // this stub at anytime in the future. The performance impact is almost zilch.
    sicodegen code((byte*) pvCode, 0, 0, (byte*) pvTargetCode - (byte*) pvCode);
//    code.writeByte(0xcc);
    code.writePushParam((unsigned long long) pvTargetBuffer, sizeof(pvTargetBuffer), 1);
    code.writeCall(LoadLibraryW);
    code.writeByte(RET_XX);
    code.writeWord(0x4);

    // write out the code and the string containing the filename of the module to load
    SIZE_T dwWritten;
    if (!WriteProcessMemory(hProcess, pvTargetCode, pvCode, code.getCodeSize(), &dwWritten)) goto error;

    if (!WriteProcessMemory(hProcess, pvTargetBuffer, pwszModuleName, (wcslen(pwszModuleName) + 1) * sizeof(wchar_t), &dwWritten)) goto error;

    // start a remote thread to call our stub code
    DWORD dwThreadId;
    HANDLE hRemoteThread = CreateRemoteThread(hProcess, NULL, NULL, (LPTHREAD_START_ROUTINE) pvTargetCode, NULL, NULL, &dwThreadId);
    WaitForSingleObject(hRemoteThread, SI_WAIT_TIMEOUT);
    if (!hRemoteThread) goto error;

    resumeAllThreads(ulProcessId);
    return true;
error:
    DBGTRACE( L"Error in injecting into a running process" )
    if (pvCode)           VirtualFree  (          pvCode,         PAGE_SIZE, MEM_FREE);
    if (pvTargetCode)     VirtualFreeEx(hProcess, pvTargetCode,   PAGE_SIZE, MEM_FREE);
    if (pvTargetBuffer)   VirtualFreeEx(hProcess, pvTargetBuffer, PAGE_SIZE, MEM_FREE);

    if (hProcess)         CloseHandle(hProcess); else return false;

    resumeAllThreads(ulProcessId);
    return false;
}




// --- keep this code around; it'll may be used for other types of injection if we need them
/*
IS void nudgeUI(unsigned long ulThreadId)
{
	HWND hWnd = NULL;
	EnumThreadWindows(ulThreadId, GetFirstWindowEnumProc, (LPARAM)&hWnd);
	if (hWnd)
		PostMessage(hWnd, WM_PAINT, NULL, NULL);
}


enum InjectionType { THREADCONTEXT, REMOTETHREAD, USERAPC };

IS InjectionType determineInjectionType(bool isAppRunning = false)
{
    if (isAppRunning)
        return REMOTETHREAD;

	OSVERSIONINFOEX osVersion = {0};
	osVersion.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	GetVersionEx((LPOSVERSIONINFO) &osVersion);

    // on Vista and above, if we use thread context injection, we can't create
    // a thread from the injection code; even from within the loaded DLL
	if ((osVersion.dwMajorVersion == 6) && (osVersion.dwMinorVersion == 0))
        return USERAPC;
    else
        return THREADCONTEXT;
}


IS bool runViaContext(void* pvTargetCode, unsigned long ulThreadId)
{
    HANDLE hThread = OpenThread (THREAD_ALL_ACCESS,  false, ulThreadId);
    if (!hThread)
    {
        DBGLOG( L"Couldn't get thread handle for thread", ulThreadId )
        return false;
    }

    CONTEXT cxt = {0};
    cxt.ContextFlags = CONTEXT_FULL;
    if (!GetThreadContext(hThread, &cxt))
    {
        DBGTRACE( L"Couldn't get context!" )
        return false;
    }

    cxt.EIP = (DWORD) pvTargetCode;

    if (!SetThreadContext(hThread, &cxt))
    {
        DBGLOG( L"Couldn't set context!", ulThreadId )
        CloseHandle(hThread);
        return false;
    }

    ResumeThread(hThread);
    return true;
}

IS bool runViaUserAPC(void* pvTargetCode, unsigned long ulThreadId)
{
    HANDLE hThread = OpenThread (THREAD_ALL_ACCESS,  false, ulThreadId);
    if (!hThread)
    {
        DBGLOG( L"Couldn't get thread handle for thread", ulThreadId )
        return false;
    }

    QueueUserAPC((PAPCFUNC) pvTargetCode, hThread, 0);
    return true;
}

IS bool runViaRemoteThread(void* pvTargetCode)
{
    return true;
}


IS bool injectIntoProcess32(unsigned long ulProcessId, unsigned long ulThreadId, wchar_t *pwszModuleName)
{
    void *pvCode            = 0;
    void *pvTargetCode      = 0;
    void *pvTargetBuffer    = 0;

    HANDLE hProcess = 0;
    CONTEXT cxt = {0};
    HANDLE hThread;
    hProcess = OpenProcess(PROCESS_ALL_ACCESS, false, ulProcessId);
    if (!hProcess)
    {
        DBGLOG( L"Couldn't get process handle for process", ulProcessId )
        goto error;
    }

    pvCode            = VirtualAlloc(NULL, 4096, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    pvTargetCode      = VirtualAllocEx(hProcess, NULL, 4096, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    pvTargetBuffer    = VirtualAllocEx(hProcess, NULL, 4096, MEM_COMMIT, PAGE_EXECUTE_READWRITE);

    int offset = (DWORD) pvTargetCode - (DWORD) pvCode;
    if (!pvCode || !pvTargetCode || !pvTargetBuffer)
    {
        DBGLOG( L"Couldn't alloc memory in process!", ulProcessId )
        goto error;
    }

    byte *pbCode = (byte*) pvCode;

//    writeByte(pbCode, 0xcc);    // breakpoint

    writeByte(pbCode, PUSHAD);
    writeByte(pbCode, PUSHFD);

    writeSaveLastError(pbCode, offset);

    writeByte(pbCode, 0x68);    // push imm32
   writeDWord(pbCode, (DWORD) pvTargetBuffer);

    writeByte(pbCode, 0xe8);    // call
    writeDisplacement(pbCode, LoadLibraryW, offset);

    writeRestoreLastError(pbCode, offset);
    writeByte(pbCode, POPFD);
    writeByte(pbCode, POPAD);

    InjectionType whichInjection = determineInjectionType();

    switch (whichInjection)
    {
    case THREADCONTEXT:
        cxt.ContextFlags = CONTEXT_FULL;
        hThread = OpenThread (THREAD_ALL_ACCESS,  false, ulThreadId);
        GetThreadContext(hThread, &cxt);

        writeByte(pbCode, JMP);
        writeDisplacement(pbCode, (void*)cxt.EIP, offset);
        break;
    case USERAPC:
    case REMOTETHREAD:
        writeByte(pbCode, RET_XX);
        writeByte(pbCode, 0x4);     // clear 4 bytes from the stack
        break;
    default:;
    }

    SIZE_T dwWritten = 0;
    if (!WriteProcessMemory(hProcess, pvTargetBuffer, pwszModuleName, (wcslen(pwszModuleName) + 1) * sizeof(wchar_t), &dwWritten))
    {
        DBGLOG( L"Couldn't write process mem for buffer!", ulProcessId )
        goto error;
    }

    if (!WriteProcessMemory(hProcess, pvTargetCode, pvCode, 4096, &dwWritten))
    {
        DBGLOG( L"Couldn't write process mem for code!", ulProcessId )
        goto error;
    }

    switch (whichInjection)
    {
    case THREADCONTEXT:
        nudgeUI(ulThreadId);
        runViaContext(pvTargetCode, ulThreadId);
        break;
    case USERAPC:
        nudgeUI(ulThreadId);
        runViaUserAPC(pvTargetCode, ulThreadId);
        break;
    case REMOTETHREAD:
        runViaRemoteThread(pvTargetCode);
        break;
    default:;

    }

    VirtualFree(pvCode, 4096, MEM_RELEASE);
    VirtualFreeEx(hProcess, pvTargetBuffer, 4096, MEM_FREE);

    // Don't free target code; execution will be continued even after we exit
//    VirtualFreeEx(hProcess, pvTargetCode, 4096, MEM_RELEASE);

    CloseHandle(hProcess);

    return true;

error:
    if (!hProcess)      return false;

    if (pvCode)         VirtualFree(pvCode, 4096, MEM_RELEASE);
    if (pvTargetCode)   VirtualFreeEx(hProcess, pvTargetCode, 4096, MEM_RELEASE);
    if (pvTargetBuffer) VirtualFreeEx(hProcess, pvTargetBuffer, 4096, MEM_RELEASE);

    CloseHandle(hProcess);
    return false;
}*/

