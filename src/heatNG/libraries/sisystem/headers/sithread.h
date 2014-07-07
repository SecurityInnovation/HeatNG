#pragma once

#include "sisystem.h"
#ifdef _IS_HEAT_SERVER
#include "siheatsupport.h"
#endif


// Win32 Implementation of common system services
#if defined(WIN32) || defined (WIN64)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef _IS_HEAT_SERVER
#include <set>
extern std::set<unsigned long> *heatThreadIds;
extern CRITICAL_SECTION csHeatThreads;

DWORD __declspec(dllexport) WINAPI disableInterceptionInCurrentThread();
DWORD __declspec(dllexport) WINAPI enableInterceptionInCurrentThread();
#endif

namespace
{
    // -- Thread classes, structures and functions

#ifdef _IS_HEAT_SERVER
	struct ThreadWrapper
	{
		void *pvParam;
		void *pvThreadFunc;
	};

	DWORD WINAPI threadFuncStub(void *pvParam)
	{
		disableInterceptionInCurrentThread();
		ThreadWrapper *actualThread = (ThreadWrapper*) pvParam;

		DWORD dwRet = ((DWORD(WINAPI *)(void*)) actualThread->pvThreadFunc)(actualThread->pvParam);

		delete actualThread;
		return dwRet;
	}
#endif

    class sithread
    {
        HANDLE m_hThread;
        DWORD  m_dwThreadId;

    public:
        sithread(void *pvThreadFunc, void *pvParameter, void *pvSecurityDescriptor = 0)
        {
			void *pvThreadFuncStub = pvThreadFunc;
			void *pvParamStub      = pvParameter;
#ifdef _IS_HEAT_SERVER
            // HACK: generate a stub for the original thread function which first disables
            // interception in the current thread, then jumps into the thread function
            // runInThread for some reason doesn't seem to work here; go Microsoft!

			ThreadWrapper *actualThread = new ThreadWrapper;
			actualThread->pvThreadFunc  = pvThreadFunc;
			actualThread->pvParam       = pvParameter;

			pvThreadFuncStub = threadFuncStub;
			pvParamStub      = (void*) actualThread;
#endif
            // create the thread suspended; so we can do any heat related markups for it before it executes
            if (pvSecurityDescriptor)
                m_hThread = CreateThread((SECURITY_ATTRIBUTES*) pvSecurityDescriptor,   \
                                          NULL,                                         \
                                          (LPTHREAD_START_ROUTINE) pvThreadFuncStub,    \
                                          pvParamStub,                                  \
                                          CREATE_SUSPENDED,                             \
                                          &m_dwThreadId);
            else
                m_hThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE) pvThreadFuncStub, pvParamStub, CREATE_SUSPENDED, &m_dwThreadId);

            if (!m_hThread)
                throw siexception(std::wstring(L"Couldn't create thread!"), 0, getLastError());
            TRACEX( L"Created thread!", m_dwThreadId )
#ifdef _IS_HEAT_SERVER
            TRACE( L"Creating thread from heat server! Disabling intercepts", m_dwThreadId )
            THREAD_SAFE( heatThreadIds->insert(m_dwThreadId);, &csHeatThreads )
#endif
            ResumeThread(m_hThread);
        }
        ~sithread()
        {
            if (!TerminateThread(m_hThread, 0))
                throw siexception(std::wstring(L"Couldn't terminate thread!"), m_dwThreadId, GetLastError());
        }
        // accessors
        unsigned long getThreadId()     { return m_dwThreadId;      }
        void*         getThreadHandle() { return (void*) m_hThread; }
    };
}   // namespace

#endif
