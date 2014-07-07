#include "sisystem.h"
#include "common.h"


// -- Common Process functions
void suspendCurrentThread()
{
    SuspendThread(GetCurrentThread());
}
bool runThread(unsigned long ulThreadId)
{
    HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, false, ulThreadId);
    if (!hThread) return false;
    ResumeThread(hThread);
    CloseHandle(hThread);

    return true;
}

bool getAllThreadIds(std::vector<unsigned long> &threads, unsigned long ulProcessId)
{
    DWORD dwCurrentProcessId;
    if (!ulProcessId)
        dwCurrentProcessId = GetCurrentProcessId();
    else
        dwCurrentProcessId = ulProcessId;

    THREADENTRY32 te = { 0 }; 
    threads.clear();
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, dwCurrentProcessId);
    if (hSnap == INVALID_HANDLE_VALUE) goto error;

    te.dwSize = sizeof(THREADENTRY32);
    if (!Thread32First(hSnap, &te)) goto error;

    do
    {
        if (te.th32OwnerProcessID == dwCurrentProcessId)
            threads.push_back(te.th32ThreadID);
    } while (Thread32Next(hSnap, &te));

    return true;
error:
    if (hSnap) CloseHandle(hSnap);
    return false;
}

IS bool processAllThreadsState(bool bSuspend, unsigned long ulProcessId, unsigned long ulExceptThreadId = 0)
{
    // set priority class to the highest; this doesn't "gaurantee"
    // that we won't be interupted, but it's the best we've got
	DWORD dwProcPriority = GetPriorityClass(GetCurrentProcess());
	DWORD dwThreadPriority = GetThreadPriority(GetCurrentThread());

	SetPriorityClass(GetCurrentProcess, REALTIME_PRIORITY_CLASS);
	SetThreadPriority(GetCurrentThread, THREAD_PRIORITY_TIME_CRITICAL);

    DWORD dwCurrentProcessId;
    if (!ulProcessId)
        dwCurrentProcessId = GetCurrentProcessId();
    else
        dwCurrentProcessId = ulProcessId;

    THREADENTRY32 te = { 0 }; 
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, dwCurrentProcessId);
    if (hSnap == INVALID_HANDLE_VALUE) goto error;

    te.dwSize = sizeof(THREADENTRY32);
    if (!Thread32First(hSnap, &te)) goto error;

    do
    {
        if (te.th32OwnerProcessID == dwCurrentProcessId)
        {
            if (te.th32ThreadID == GetCurrentThreadId() || te.th32ThreadID == ulExceptThreadId)
                continue;
            HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, false, te.th32ThreadID);

			if (!hThread) goto error;

			if (bSuspend)
				SuspendThread(hThread);
			else
				ResumeThread(hThread);
			CloseHandle(hThread);
        }
    } while (Thread32Next(hSnap, &te));

	SetPriorityClass(GetCurrentProcess(), dwProcPriority);
	SetThreadPriority(GetCurrentThread(), dwThreadPriority);

    return true;
error:
	SetPriorityClass(GetCurrentProcess(), dwProcPriority);
	SetThreadPriority(GetCurrentThread(), dwThreadPriority);
    if (hSnap) CloseHandle(hSnap);
    return false;
}

bool suspendAllThreads(unsigned long ulProcessId, unsigned long ulExceptThreadId)
{
    return processAllThreadsState(true, ulProcessId, ulExceptThreadId);
}
bool resumeAllThreads (unsigned long ulProcessId, unsigned long ulExceptThreadId)
{
    return processAllThreadsState(false, ulProcessId, ulExceptThreadId);
}



// -- Memory handling structures and functions

VirtualAllocType    o_VirtualAlloc      = VirtualAlloc;
VirtualAllocExType  o_VirtualAllocEx    = VirtualAllocEx;
VirtualQueryType    o_VirtualQuery      = VirtualQuery;
VirtualQueryExType  o_VirtualQueryEx    = VirtualQueryEx;
VirtualProtectType  o_VirtualProtect    = VirtualProtect;
VirtualFreeType     o_VirtualFree       = VirtualFree;
                                          
HeapAllocType       o_HeapAlloc         = HeapAlloc;
HeapFreeType        o_HeapFree          = HeapFree;
                                          
TlsAllocType        o_TlsAlloc          = TlsAlloc;
TlsGetValueType     o_TlsGetValue       = TlsGetValue;
TlsSetValueType     o_TlsSetValue       = TlsSetValue;


void *memoryReserve(void *pvAddress, unsigned long size)
{
    return o_VirtualAlloc(pvAddress, size, MEM_RESERVE, PAGE_EXECUTE_READWRITE);
}

void *findFreeMemNear(void *pvAddress, unsigned long size)
{
    MEMORY_BASIC_INFORMATION mbih = {0};
    MEMORY_BASIC_INFORMATION mbil = {0};

    if (o_VirtualQuery(pvAddress, &mbih, sizeof(mbih)) <= 0) return 0;

    byte *pbMem  = (byte*) pvAddress;
    byte *pbHigh = (byte*)mbih.BaseAddress + mbih.RegionSize + PAGE_SIZE;                           // next higher address
    byte *pbLow  = ((DWORD)mbih.BaseAddress > PAGE_SIZE) ? (byte*)mbih.BaseAddress - PAGE_SIZE : 0; // next lower address

    byte *pbHighFree = 0;
    byte *pbLowFree  = 0;

    while (true)
    {
        if (pbHigh) if (o_VirtualQuery(pbHigh, &mbih, sizeof(mbih)) <= 0) pbHigh = 0;
        if (pbLow)  if (o_VirtualQuery(pbLow,  &mbil, sizeof(mbil)) <= 0) pbLow  = 0;

        if (!pbHigh && !pbLow) return 0;

        if (mbih.State == MEM_FREE) pbHighFree = pbHigh;
        if (mbil.State == MEM_FREE) pbLowFree  = pbLow;

        if (!pbLowFree  && pbHighFree) if (memoryReserve(pbHighFree, size)) return pbHighFree; else pbHighFree = 0;
        if (!pbHighFree && pbLowFree ) if (memoryReserve(pbLowFree , size)) return pbLowFree ; else pbLowFree  = 0;

        if (pbHighFree && pbLowFree)   
            if ((pbHighFree - pbMem) > (pbLowFree - pbMem))
                if      (memoryReserve(pbLowFree , size)) return pbLowFree ;
                else if (memoryReserve(pbHighFree, size)) return pbHighFree;
                else { pbLowFree = 0; pbHighFree = 0; }
            else
                if       (memoryReserve(pbHighFree, size)) return pbHighFree;
                else if  (memoryReserve(pbLowFree , size)) return pbLowFree ;
                else { pbLowFree = 0; pbHighFree = 0; }

        pbHigh = (byte*)mbih.BaseAddress + mbih.RegionSize  + PAGE_SIZE;                            // go higher
        pbLow  = ((DWORD)mbil.BaseAddress > PAGE_SIZE) ? (byte*)mbil.BaseAddress - PAGE_SIZE : 0;   // go lower
    }
}

void *findFreeMemNear(HANDLE hProcess, void *pvAddress, unsigned long size)
{
    MEMORY_BASIC_INFORMATION mbi = {0};

    byte *pbMem  = (byte*) pvAddress;
    while (true)
    {
        if (o_VirtualQueryEx(hProcess, pbMem, &mbi, sizeof(mbi)) <= 0)
            return 0;

        if (mbi.State == MEM_FREE)
            if (o_VirtualAllocEx(hProcess, pbMem, size, MEM_RESERVE, PAGE_READWRITE))
                return pbMem;

        // else; try next page
        pbMem = (byte*)mbi.BaseAddress + mbi.RegionSize + PAGE_SIZE;
    }
}

void *memoryAlloc(unsigned long size)
{
    return o_HeapAlloc(GetProcessHeap(), 0, size);
}

void *memoryAllocNear(unsigned long size, void *pvAddress)
{
#ifdef WIN64
    if (!size) return 0;

    void *pvNearFree = findFreeMemNear(pvAddress, size);
    if (!pvNearFree) return 0;

    return o_VirtualAlloc(pvNearFree, size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
#else
    return o_VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
#endif
}

void *memoryAllocNearInProcess(HANDLE hProcess, unsigned long size, void *pvAddress)
{
#ifdef WIN64
    void *pvNearFree = findFreeMemNear(hProcess, pvAddress, size);
    if (!pvNearFree) return 0;

    return o_VirtualAllocEx(hProcess, pvNearFree, size, MEM_COMMIT, PAGE_READWRITE);
#else
    return o_VirtualAllocEx(hProcess, NULL, size, MEM_COMMIT, PAGE_READWRITE);
#endif
}

void *memoryAllocExecutable(unsigned long size)
{
    void *mem = memoryAlloc(size);
    if (!mem) return 0;

    if (!enablePageExecute(mem))
    {
        memoryFree(mem);
        return 0;
    }
    return mem;
}

void *memoryAllocExecutableNear(unsigned long size, void *pvAddress)
{
    void *mem = memoryAllocNear(size, pvAddress);
    if (!mem) return 0;

    if (!enablePageExecute(mem))
    {
        memoryFree(mem);
        return 0;
    }
    return mem;
}

void *memoryAllocExecutableIfFar(unsigned long size, void *pvMem, void *pvAddress)
{
    long long llDistance = (byte*) pvAddress - (byte*) pvMem;
    if (_abs64(llDistance) > 0x7fffffff)
    {
        return memoryAllocExecutableNear(size, pvAddress);
    }
    return pvMem;
}



bool memoryNearFree(void *address, unsigned long size)
{
    if (o_VirtualFree(address, size, MEM_RELEASE))
        return true;
    else
        return false;
}

bool memoryFree(void *address)
{
    if (o_HeapFree(GetProcessHeap(), 0, address))
        return true;
    else
        return false;
}

long enablePageExecute(void *address)
{
    DWORD dwOldProtect;
    if (!o_VirtualProtect(address, 4096, PAGE_EXECUTE_READWRITE, &dwOldProtect))
        return -1;

    return (long) dwOldProtect;
}

long enablePageWrite(void *address)
{
    DWORD dwOldProtect;
    if (!o_VirtualProtect(address, 4096, PAGE_READWRITE, &dwOldProtect))
        return -1;

    return (long) dwOldProtect;
}

bool restorePageProtection(void *address, long lProtection)
{
    DWORD dwOldProtect;
    if (!o_VirtualProtect(address, 4096, lProtection, &dwOldProtect))
        return false;
    // else
    return true;
}

