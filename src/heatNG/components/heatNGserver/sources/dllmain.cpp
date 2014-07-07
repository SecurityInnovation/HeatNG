// dllmain.cpp : Defines the entry point for the DLL application.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <set>

#define _IS_HEAT_SERVER

#include "sithread.h"
#include "sisystem.h"

bool   startupServer();
bool terminateServer();

bool   initRASForThread();
bool deinitRASForThread();

extern std::set<unsigned long> *heatThreadIds;
extern CRITICAL_SECTION csHeatThreads;


HANDLE hLoadingThread = NULL;


DWORD WINAPI suspendWorker(void*)
{
    // suspend main thread ASAP
    if (hLoadingThread)
        SuspendThread(hLoadingThread);

    // startup our server here after the main thread has been suspended
	DBG( debugOutput(L"DLLMAIN Thread: Starting server\n"); )
    startupServer();
    enableInterceptionInCurrentThread();
	DBG( debugOutput(L"DLLMAIN Thread: Done with initialization!\n"); )
    return 0;
}


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
    std::set<unsigned long>::iterator currentThread;
    std::set<unsigned long>::iterator endOfThreads;
    sithread *suspendWorkerThread = 0;

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
//        DebugBreak();
        debugOutput(L"In DLLmain of heatNGserver.dll! PROCESS_ATTACH\n");

        // initialize anything that doesn't require creating a thread
        // all thread creation inits should be run from the suspendWorker
        InitializeCriticalSection(&csHeatThreads);
        heatThreadIds = new std::set<unsigned long>();
        hLoadingThread = OpenThread(THREAD_ALL_ACCESS, false, GetCurrentThreadId());
        if (!updateCriticalSystemPointers())
            return FALSE;

        // suspend the current thread to maintain
        // the process in a suspended mode
        suspendWorkerThread = new sithread(suspendWorker, 0);

        // set this thread's priority to the highest; we need it to run and suspend us ASAP
        SetThreadPriority((HANDLE)(suspendWorkerThread->getThreadHandle()), THREAD_PRIORITY_TIME_CRITICAL);

        debugOutput(L"Leaving DLLmain of heatNGserver.dll! PROCESS_ATTACH\n");
        break;

    case DLL_THREAD_ATTACH:
        break;

    case DLL_THREAD_DETACH:
        THREAD_SAFE( currentThread = heatThreadIds->find(GetCurrentThreadId());, &csHeatThreads )
        THREAD_SAFE( endOfThreads  = heatThreadIds->end();,                      &csHeatThreads )

        if (currentThread != endOfThreads)
        {
            THREAD_SAFE( heatThreadIds->erase(currentThread);,                   &csHeatThreads )
        }
        break;

	case DLL_PROCESS_DETACH:
        if (!terminateServer())
            return FALSE;
		break;
    default:;
	}
	return TRUE;
}

