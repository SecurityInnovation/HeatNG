// heatNGserver.cpp : Defines the exported functions for the DLL application.
//
#define _IS_HEAT_SERVER

#include "heatNGserver.h"
#include <vector>


std::set<unsigned long> *heatThreadIds = 0;
#if defined(WIN32) || defined (WIN64)
CRITICAL_SECTION csHeatThreads;
#else
#error Unsupported platform!
#endif


class heatServerHandler : public CommHandler
{
    std::vector<sinativeintercept*> m_intercepts;

    bool interceptFunction(sinativeintercept *intercept)
    {
        if (!intercept->interceptFunction()) return false;
        DBG( debugOutput(L"Done intercepting.."); )

        m_intercepts.push_back(intercept);
        return true;
    }

    bool readAndIntercept(byte *pbData, unsigned long ulLen)
    {
        assert(sizeof(sinativeintercept) == ulLen);

        sinativeintercept *intercept;
        intercept = (sinativeintercept*) memoryAlloc(sizeof(sinativeintercept));
        memcpy(intercept, (void*) pbData, ulLen);

        DBG( debugOutput(L"Received intercept; intercepting.."); )
        if (!interceptFunction(intercept))
        {
            memoryFree(intercept);
            return false;
        }

        return true;
    }

public:
    heatServerHandler()
    {
        m_intercepts.clear();
    }

    bool cmdHandlerFunction(byte bCmd, byte *pbData, unsigned long ulLen)
    {
        switch (bCmd)
        {
        case CMD_INTERCEPT:
            return readAndIntercept(pbData, ulLen);
            break;

        case CMD_PAUSEAPP:
            DBG( debugOutput(L"pausing AUT!\n"); )
            return suspendAllThreads(0);
            break;

        case CMD_RESUMEAPP:
            DBG( debugOutput(L"resuming AUT!\n"); )
            return resumeAllThreads(0);
            break;

        default:
            DBG( debugOutput(L"Unrecognized command"); )
            return false;
        }
        return true;
    }

    sinativeintercept *getInterceptByName(char *pszInterceptName, wchar_t *pwszModuleName)
    {
        sinativeintercept *si = 0;
        DBG( wchar_t msg[100]; )

        DBG( wsprintf(msg, L"Number of intercepts recorded is %d", m_intercepts.size()); )
        DBG( debugOutput(msg); )
        for (unsigned long i = 0; i < m_intercepts.size(); i++)
        {
            si = m_intercepts[i];
            if (wcsicmp(pwszModuleName, si->getOriginalFunctionModuleName()) == 0)
                if (stricmp(pszInterceptName, si->getOriginalFunctionName()) == 0)
                    break;
        }
        return si;
    }
} serverHandler;


void *allocRASInCurrentThread()
{
    // Create replacement stack and place it's pointer in it's TLS location
    void *pvRAS = memoryAlloc(PAGE_SIZE);
    if (!pvRAS)                                 return 0;

    // set a canary null value to identify the beginning of the allocated memory
    // (use a pointer type to store to confirm accurate pointer size)
    byte **ppbRAS = (byte**) pvRAS;
    *ppbRAS = 0; ppbRAS++;

    // save the the RAS in TLS
    if (!tlsSetValue(g_dwTlsRASIndex, ppbRAS))   return pvRAS;

    return pvRAS;
}



// start comms server
bool startupServer()
{
    DBG( debugOutput(L"Starting server"); )
    try
    {
        g_heatNGserver = new CommServer(wstring(L"heatNGserver"), &serverHandler);
        g_heatNGserver->startServer();
    }
    catch(siexception e)
    {
        DBG( debugOutput(e.what().c_str()); )
        return false;
    }
    DBG( debugOutput(L"Started server"); )

    DBG( debugOutput(L"Allocating TLS slots"); )
    // setup TLS indexes
    if (!g_dwTlsCheckInterceptIndex) g_dwTlsCheckInterceptIndex = tlsAlloc();
    if (!g_dwTlsRASIndex)            g_dwTlsRASIndex            = tlsAlloc();

    if (g_dwTlsCheckInterceptIndex == TLS_OUT_OF_INDEXES || g_dwTlsRASIndex == TLS_OUT_OF_INDEXES)
        return false;

    DBG( debugOutput(L"Server started!"); )
    return true;
}

bool terminateServer()
{
    DBG( debugOutput(L"Terminating server"); )
    try
    {
        if (g_heatNGserver && g_heatNGserver->isRunning())
            g_heatNGserver->stopServer();
    }
    catch(siexception e)
    {
        DBG( debugOutput(e.what().c_str()); )
        return false;
    }

    DBG( debugOutput(L"Terminated server"); )
    return true;
}

bool initRASForThread()
{
    if (!allocRASInCurrentThread()) return false;
    TRACEX( L"RASCODE: Created RAS for thread id:", GetCurrentThreadId() )
    return true;
}

bool deinitRASForThread()
{
    // permanently disable interception for the rest of the thread execution
    disableInterceptionInCurrentThread();

    // Free the replacement stack
    void *pvRAS = tlsGetValue(g_dwTlsRASIndex);

    if (pvRAS)
    {
        // find the first 0 pointer on the RAS stack
        byte **ppbRAS = (byte**) pvRAS;
        while (*ppbRAS && ((byte**)pvRAS - ppbRAS) < PAGE_SIZE) ppbRAS--;

        if (!*ppbRAS)
            if (!memoryFree(ppbRAS))
                return false;
            else;
        else
            return false;
    }

    TRACEX( L"RASCODE: Destroyed RAS for thread id:", GetCurrentThreadId() )
    return true;
}

PVOID __declspec(dllexport) WINAPI getOriginalFunctionCaller(LPSTR pszFuncName, LPSTR pszFuncDLL)
{
    // just in case any functions we use are intercepted
    disableInterceptionInCurrentThread();

    unsigned long ulStrLen = (unsigned long) strlen(pszFuncDLL);
    wchar_t *pwszModuleName = new wchar_t[ulStrLen + 1];
    mbstowcs(pwszModuleName, pszFuncDLL, ulStrLen); pwszModuleName[ulStrLen] = 0;

	DBG( debugOutput(L"Looking for a module:\n"); )
	DBG( debugOutput(pwszModuleName); )

    sinativeintercept *si = serverHandler.getInterceptByName(pszFuncName, pwszModuleName);
    delete[] pwszModuleName;

    assert(si != 0);

    // back to normal
    enableInterceptionInCurrentThread();
	TRACEX( L"Found it in", si->getCallerStub() )
    if (!si) return 0;
    return si->getCallerStub();
}


DWORD __declspec(dllexport) WINAPI disableInterceptionInCurrentThread()
{
    DWORD dwNewValue = (DWORD) tlsGetValue(g_dwTlsCheckInterceptIndex) + 1;
	tlsSetValue(g_dwTlsCheckInterceptIndex, (PVOID)dwNewValue);
    TRACEX( L"Disabled intercption in thread:", GetCurrentThreadId() )
	return dwNewValue;
}

DWORD __declspec(dllexport) WINAPI enableInterceptionInCurrentThread()
{
	DWORD dwNewValue = (DWORD)tlsGetValue(g_dwTlsCheckInterceptIndex) - 1;
	tlsSetValue(g_dwTlsCheckInterceptIndex, (PVOID)dwNewValue);
    TRACEX( L"Enabled intercption in thread:", GetCurrentThreadId() )
	return dwNewValue;
}

// -- the following functions are purely provided for backwards compatibility

//*************************************************************************
// Method:		IsHeatCreatedThread
// Description: determines whether a thread was created by heat
//
// Parameters:
//	dwThreadID - the thread id to check
//
// Return Value: true if the thread was created by heat, false otherwise
//*************************************************************************
bool __stdcall IsHeatCreatedThread(DWORD dwThreadID)
{
    std::set<unsigned long>::iterator currentThread;
    std::set<unsigned long>::iterator endOfThreads;

    THREAD_SAFE( currentThread = heatThreadIds->find(GetCurrentThreadId());, &csHeatThreads )
    THREAD_SAFE( endOfThreads  = heatThreadIds->end();,                      &csHeatThreads )

    if (currentThread != endOfThreads)
    {
		TRACEX( L"ISHEATCREATETHREAD: Thread is ours", dwThreadID )
        return true;
    }
	TRACEX( L"ISHEATCREATETHREAD: Thread is NOT ours", dwThreadID )
    return false;
}


//*************************************************************************
// Method:		HeatCreateThread
// Description: creates a thread and logs it as created by heat
//
// Parameters:
//	routine - the thread routine to execute
//	param - the parameter to pass to the thread function
//	pdwThreadID (out) - the thread id of the new thread
//
// Return Value: the handle to the created thread
//*************************************************************************
HANDLE __stdcall HeatCreateThread(LPTHREAD_START_ROUTINE routine, LPVOID param, DWORD *pdwThreadID)
{
	// Initialize a security descriptor.
	PSECURITY_DESCRIPTOR pSD = (PSECURITY_DESCRIPTOR) LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
	InitializeSecurityDescriptor(pSD, SECURITY_DESCRIPTOR_REVISION);
	SetSecurityDescriptorDacl(pSD, TRUE, NULL, TRUE);
	// Initialize a security attributes structure.
	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof (SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = pSD;
	sa.bInheritHandle = FALSE;

    sithread *thread = new sithread(routine, param, &sa);
	TRACEX( L"HEAT_CREATETHREAD: Thread created", thread->getThreadId() )
    if (!thread)
        return 0;
    else
        return thread->getThreadHandle();
}

//*************************************************************************
// Method:		getCallLevel
// Description: Returns the current call level (how many intercepted
//				functions are currently on the call stack)
//
// Parameters:
//	None
//
// Return Value: Current call level
//*************************************************************************
DWORD __declspec(dllexport) WINAPI getCallLevel()
{
    return 1;
}


//*************************************************************************
// Method:		incrementCallLevel
// Description: Increments the call level for this thread.
//
// Parameters:
//	None
//
// Return Value: New call level
//*************************************************************************
DWORD __declspec(dllexport) WINAPI incrementCallLevel()
{
    return 1;
}

//*************************************************************************
// Method:		decrementCallLevel
// Description: Decerements the call level for this thread
//
// Parameters:
//	None
//
// Return Value: New call level
//*************************************************************************
DWORD __declspec(dllexport) WINAPI decrementCallLevel()
{
    return 1;
}

