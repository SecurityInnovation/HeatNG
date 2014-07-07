// siexception header file
#pragma once

#include "siexception.h"
#include "common.h"

// Win32 Implementation of common system services
#if defined(WIN32) || defined (WIN64)

// modifier definitions
#define FI __forceinline
#define IS __forceinline static

#if   defined(WIN64)
#define PAGE_SIZE 8192
#elif defined(WIN32)
#define PAGE_SIZE 4096
#else
#error Unsupported platform!
#endif


// header inclusions
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Tlhelp32.h>


// -- Debug functionality
FI void debugOutput(const wchar_t *string)
{
    OutputDebugStringW(string);
}

#endif

// sync access macro
#define THREAD_SAFE( code, waitObjectPtr )     EnterCriticalSection( waitObjectPtr );\
                                               code\
                                               LeaveCriticalSection( waitObjectPtr );


// -- Process/Thread functions
IS unsigned long getCurrentProcessId() { return GetCurrentProcessId(); }
IS unsigned long getCurrentThreadId()  { return GetCurrentThreadId();  }
IS unsigned long getLastError()        { return GetLastError();  }

void          suspendCurrentThread();
bool          runThread(unsigned long ulThreadId);

bool getAllThreadIds(std::vector<unsigned long> &threads, unsigned long ulProcessId = 0);
bool suspendAllThreads(unsigned long ulProcessId, unsigned long ulExceptThreadId = 0);
bool resumeAllThreads (unsigned long ulProcessId, unsigned long ulExceptThreadId = 0);


// -- Memory handling structures and functions
typedef LPVOID (WINAPI *VirtualAllocType)(LPVOID, SIZE_T, DWORD, DWORD);
typedef LPVOID (WINAPI *VirtualAllocExType)(HANDLE, LPVOID, SIZE_T, DWORD, DWORD);
typedef SIZE_T (WINAPI *VirtualQueryType)(LPCVOID, PMEMORY_BASIC_INFORMATION, SIZE_T);
typedef SIZE_T (WINAPI *VirtualQueryExType)(HANDLE, LPCVOID, PMEMORY_BASIC_INFORMATION, SIZE_T);
typedef BOOL   (WINAPI *VirtualProtectType)(LPVOID, SIZE_T, DWORD, PDWORD);
typedef BOOL   (WINAPI *VirtualFreeType)(LPVOID, SIZE_T, DWORD);

typedef LPVOID (WINAPI *HeapAllocType)(HANDLE, DWORD, SIZE_T);
typedef BOOL   (WINAPI *HeapFreeType)(HANDLE, DWORD, LPVOID);

typedef DWORD  (WINAPI *TlsAllocType)();
typedef LPVOID (WINAPI *TlsGetValueType)(DWORD);
typedef BOOL   (WINAPI *TlsSetValueType)(DWORD, LPVOID);

extern VirtualAllocType    o_VirtualAlloc;
extern VirtualAllocExType  o_VirtualAllocEx;
extern VirtualQueryType    o_VirtualQuery;
extern VirtualQueryExType  o_VirtualQueryEx;
extern VirtualProtectType  o_VirtualProtect;
extern VirtualFreeType     o_VirtualFree;
                                    
extern HeapAllocType       o_HeapAlloc;
extern HeapFreeType        o_HeapFree;
                                    
extern TlsAllocType        o_TlsAlloc;
extern TlsGetValueType     o_TlsGetValue;
extern TlsSetValueType     o_TlsSetValue;


IS unsigned long tlsAlloc()                                    { return o_TlsAlloc(); }
IS void *        tlsGetValue(unsigned long index)              { return o_TlsGetValue(index); }
IS int           tlsSetValue(unsigned long index, void *value) { return o_TlsSetValue(index, value); }

void *memoryAlloc               (unsigned long size);
void *memoryAllocNear           (unsigned long size, void *pvAddress);
void *memoryAllocNearInProcess  (HANDLE hProcess, unsigned long size, void *pvAddress);

void *memoryAllocExecutable     (unsigned long size);
void *memoryAllocExecutableNear (unsigned long size, void *pvAddress);
void *memoryAllocExecutableIfFar(unsigned long size, void *pvMem, void *pvAddress);

bool  memoryNearFree            (void *address, unsigned long size);
bool  memoryFree                (void *address);

long  enablePageExecute(void *address);
long  enablePageWrite(void *address);
bool restorePageProtection(void *address, long lProtection);



#ifdef _IS_HEAT_SERVER
#include "siheatsupport.h"
#endif

IS bool updateCriticalSystemPointers()
{
#ifdef _IS_HEAT_SERVER
    DBG( debugOutput(L"[enter] Generating headers for common functions.."); )
    void *pvStubsMem     = memoryAllocExecutableNear(PAGE_SIZE, VirtualAlloc);
    void *pvStubsMemBase = pvStubsMem;
    TRACEX( L"Memory for stubs: ", pvStubsMem )

    o_VirtualAlloc      = (VirtualAllocType  )generateFunctionHeaderCopy(VirtualAlloc, pvStubsMem  );
    if (((byte*) pvStubsMem - (byte*) pvStubsMemBase) > PAGE_SIZE) pvStubsMemBase = pvStubsMem = memoryAllocExecutableNear(PAGE_SIZE, VirtualAllocEx);

    o_VirtualAllocEx    = (VirtualAllocExType)generateFunctionHeaderCopy(VirtualAllocEx, pvStubsMem);
    if (((byte*) pvStubsMem - (byte*) pvStubsMemBase) > PAGE_SIZE) pvStubsMem = memoryAllocExecutableNear(PAGE_SIZE, VirtualQuery);

    o_VirtualQuery      = (VirtualQueryType  )generateFunctionHeaderCopy(VirtualQuery, pvStubsMem  );
    if (((byte*) pvStubsMem - (byte*) pvStubsMemBase) > PAGE_SIZE) pvStubsMemBase = pvStubsMem = memoryAllocExecutableNear(PAGE_SIZE, VirtualQueryEx);

    o_VirtualQueryEx    = (VirtualQueryExType)generateFunctionHeaderCopy(VirtualQueryEx, pvStubsMem);
    if (((byte*) pvStubsMem - (byte*) pvStubsMemBase) > PAGE_SIZE) pvStubsMemBase = pvStubsMem = memoryAllocExecutableNear(PAGE_SIZE, VirtualProtect);

    o_VirtualProtect    = (VirtualProtectType)generateFunctionHeaderCopy(VirtualProtect, pvStubsMem);
    if (((byte*) pvStubsMem - (byte*) pvStubsMemBase) > PAGE_SIZE) pvStubsMemBase = pvStubsMem = memoryAllocExecutableNear(PAGE_SIZE, VirtualFree);

    o_VirtualFree       = (VirtualFreeType   )generateFunctionHeaderCopy(VirtualFree, pvStubsMem   );
    if (((byte*) pvStubsMem - (byte*) pvStubsMemBase) > PAGE_SIZE) pvStubsMemBase = pvStubsMem = memoryAllocExecutableNear(PAGE_SIZE, HeapAlloc);

                                                                               
    o_HeapAlloc         = (HeapAllocType     )generateFunctionHeaderCopy(HeapAlloc, pvStubsMem     );
    if (((byte*) pvStubsMem - (byte*) pvStubsMemBase) > PAGE_SIZE) pvStubsMemBase = pvStubsMem = memoryAllocExecutableNear(PAGE_SIZE, HeapFree);

    o_HeapFree          = (HeapFreeType      )generateFunctionHeaderCopy(HeapFree, pvStubsMem      );
    if (((byte*) pvStubsMem - (byte*) pvStubsMemBase) > PAGE_SIZE) pvStubsMemBase = pvStubsMem = memoryAllocExecutableNear(PAGE_SIZE, TlsAlloc);

                                                                        
    o_TlsAlloc          = (TlsAllocType      )generateFunctionHeaderCopy(TlsAlloc, pvStubsMem      );
    if (((byte*) pvStubsMem - (byte*) pvStubsMemBase) > PAGE_SIZE) pvStubsMemBase = pvStubsMem = memoryAllocExecutableNear(PAGE_SIZE, TlsGetValue);

    o_TlsGetValue       = (TlsGetValueType   )generateFunctionHeaderCopy(TlsGetValue, pvStubsMem   );
    if (((byte*) pvStubsMem - (byte*) pvStubsMemBase) > PAGE_SIZE) pvStubsMemBase = pvStubsMem = memoryAllocExecutableNear(PAGE_SIZE, TlsSetValue);

    o_TlsSetValue       = (TlsSetValueType   )generateFunctionHeaderCopy(TlsSetValue, pvStubsMem   );

    DBG( debugOutput(L"[leave] Generating headers for common functions.."); )
    return true;
#else
    return true;
#endif
}
