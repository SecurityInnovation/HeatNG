#define _WIN32_WINNT 0x400

#include <windows.h>
#include <Tlhelp32.h>
#include <stdio.h>
#include <dbghelp.h>
#include "CHeatApp.h"

#include "HeatNGApp.h"


//********************************************************************
//
// |Routine Name              modifyProcessState
//
// |Description               Suspends or Resumes a process
//                            if bSuspend is true, it suspends
//                            else it resumes
//
// |Return Value              success or failuire
//
// |PreCondition(s)           If called with bSuspend false, the process
//                            should be already paused
//
// |PostCondition(s)          Process is suspended or resumed
//
// |Developed by   Rahul Chaturvedi
//                            Team Lead - HEAT Base Development Team
//
// |Date of Creation          03-17-2001
//
// |Date of Last Update       03-17-2001
//
// |Version                   1.0.0
//
//********************************************************************

BOOL modifyProcessState(DWORD dwProcessId, BOOL bSuspend)
{
	DWORD dwProcPriority = GetPriorityClass(GetCurrentProcess());
	DWORD dwThreadPriority = GetThreadPriority(GetCurrentThread());

	SetPriorityClass(GetCurrentProcess, REALTIME_PRIORITY_CLASS);
	SetThreadPriority(GetCurrentThread, THREAD_PRIORITY_TIME_CRITICAL);

	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, NULL);
	if (!hSnap)
		return FALSE;

	THREADENTRY32 first = {0};
	first.dwSize = sizeof(first);

	if (!Thread32First(hSnap, &first))
	{
		CloseHandle(hSnap);
		return FALSE;
	}

	do
	{
		if (first.th32OwnerProcessID == dwProcessId)
		{
			HANDLE hThread =
			OpenThread(THREAD_SUSPEND_RESUME, false, first.th32ThreadID);

			if (!hThread)
			{
				CloseHandle(hSnap);
				return FALSE;
			}

			if (bSuspend)
				SuspendThread(hThread);
			else
				ResumeThread(hThread);
			CloseHandle(hThread);

		}
		ZeroMemory(&first, sizeof(first));
		first.dwSize = sizeof(first);
	} while (Thread32Next(hSnap, &first));

	CloseHandle(hSnap);

	SetPriorityClass(GetCurrentProcess(), dwProcPriority);
	SetThreadPriority(GetCurrentThread(), dwThreadPriority);

	return TRUE;
}

DWORD getThreadIdFromProcessId(DWORD dwProcessId)
{
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, NULL);
	if (!hSnap)
		return NULL;

	THREADENTRY32 first = {0};
	first.dwSize = sizeof(first);

	if (!Thread32First(hSnap, &first))
	{
		CloseHandle(hSnap);
		return 0;
	}

	do
	{
		if (first.th32OwnerProcessID == dwProcessId)
		{
			CloseHandle(hSnap);
			return first.th32ThreadID;
		}
		ZeroMemory(&first, sizeof(first));
		first.dwSize = sizeof(first);
	} while (Thread32Next(hSnap, &first));

	CloseHandle(hSnap);
	return 0;
}

CHeatApp::CHeatApp(unsigned short *pwszHeatServerPath) :                                                         \
                                                  m_bReady(false), m_bRunning(false), m_bInjected(false), \
                                                  m_bUseRemoteThread(false), m_hProcess(0), m_heatNGapp(0)\
{
	HANDLE Token;

    TOKEN_PRIVILEGES privs; 
    privs.PrivilegeCount = 1; 

    // Get our access token 
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &Token)) 
            printf ("Couldn't get access to process access token\n"); 

    // Get LUID of SeDebugName privilege 
    if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &privs.Privileges[0].Luid)) 
            printf("Error getting LUID of SeDebugPrivilege privilege\n"); 
    
    // Find this privilege in the array and mark it as enabled 
    privs.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED; 
    AdjustTokenPrivileges(Token, FALSE, &privs, sizeof(privs), NULL, NULL);

    if (pwszHeatServerPath)
    {
        m_pwszHeatServerPath = new wchar_t[wcslen((wchar_t*)pwszHeatServerPath) + 1];
        wcscpy(m_pwszHeatServerPath, (wchar_t*) pwszHeatServerPath);
    }
    else
    {
        m_pwszHeatServerPath = new wchar_t[strlen("HeatNGserver.dll") + 1];
        wcscpy(m_pwszHeatServerPath, L"HeatNGserver.dll");
    }

	CloseHandle(Token);
}

CHeatApp::~CHeatApp()
{
	try
	{
		CloseHandle(m_hProcess);
		//make sure we don't leave the app in a paused state.
		resumeApp();
	}
	catch(...)
	{
	}
}

// Creates a suspended application
DWORD CHeatApp::initializeApp(LPSTR pszAppPath, LPSTR pszAppName)
{
	if (m_bReady)
		return HEAT_ALREADY_ATTACHED;

	if (!pszAppPath)
		return HEAT_INVALID_PARAMETERS;
	if (!pszAppName)
		return HEAT_INVALID_PARAMETERS;

	// Construct the full path to the application that is being created
	char* pszFullPath = new char[strlen(pszAppPath) + strlen(pszAppName) + 2];
	strcpy(pszFullPath, pszAppPath);
	if ((strlen(pszAppPath) > 0) && (pszAppPath[strlen(pszAppPath) - 1] != '\\'))
		strcat(pszFullPath, "\\");		// If '\' is not last char of path, append a '\'
	strcat(pszFullPath, pszAppName);

	STARTUPINFOA si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_SHOW;

	if (!CreateProcessA(NULL, pszFullPath, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, pszAppPath,
		&si, &m_piProcInfo))
	{
		delete[] pszFullPath;
		return HEAT_FAILED_CREATE_PROCESS;
	}

	m_hProcess = m_piProcInfo.hProcess;

	m_bUseRemoteThread = false;
	m_bReady = true;

	delete[] pszFullPath;
	return HEAT_SUCCESS;
}

// Attach to a process
DWORD CHeatApp::attachToApp(const PROCESS_INFORMATION& procInfo)
{
	if (m_bReady)
		return HEAT_ALREADY_ATTACHED;

	if (procInfo.dwProcessId == GetCurrentProcessId())
		return HEAT_FAILED_INJECT_DLL;

	if (!(m_hProcess = OpenProcess(PROCESS_ALL_ACCESS, false, procInfo.dwProcessId)))
		return HEAT_FAILED_OPEN_PROCESS;
	else
		startMonitorThread();

	m_piProcInfo = procInfo;

	m_bUseRemoteThread = false;
	m_bReady = true;

	return HEAT_SUCCESS;
}

// Attach to a process
DWORD CHeatApp::attachToApp(DWORD dwProcessId)
{
	if (m_bReady)
		return HEAT_ALREADY_ATTACHED;

	if (dwProcessId == GetCurrentProcessId())
		return HEAT_FAILED_INJECT_DLL;

	if (!(m_hProcess = OpenProcess(PROCESS_ALL_ACCESS, false, dwProcessId)))
		return HEAT_FAILED_OPEN_PROCESS;
	else
		startMonitorThread();

	m_piProcInfo.dwProcessId = dwProcessId;
	m_piProcInfo.dwThreadId = getThreadIdFromProcessId(dwProcessId);

	if (!m_piProcInfo.dwThreadId)
		return HEAT_FAILED_OPEN_PROCESS;

	m_bUseRemoteThread = false;
	m_bReady = true;

	return HEAT_SUCCESS;
}

// Attach to an already running app/service
DWORD CHeatApp::attachToRunningApp(const PROCESS_INFORMATION& procInfo)
{
	if (m_bReady)
		return HEAT_ALREADY_ATTACHED;

	if (procInfo.dwProcessId == GetCurrentProcessId())
		return HEAT_FAILED_INJECT_DLL;

	if (!(m_hProcess = OpenProcess(PROCESS_ALL_ACCESS, false, procInfo.dwProcessId)))
		return HEAT_FAILED_OPEN_PROCESS;
	else
		startMonitorThread();

	m_piProcInfo = procInfo;

	m_bUseRemoteThread = true;
	m_bReady = true;
	m_bRunning = true;

	return HEAT_SUCCESS;
}

// Attach to an already running app/service
DWORD CHeatApp::attachToRunningApp(DWORD dwProcessId)
{
	if (m_bReady)
		return HEAT_ALREADY_ATTACHED;

	if (dwProcessId == GetCurrentProcessId())
		return HEAT_FAILED_INJECT_DLL;

	if (!(m_hProcess = OpenProcess(PROCESS_ALL_ACCESS, false, dwProcessId)))
		return HEAT_FAILED_OPEN_PROCESS;
	else
		startMonitorThread();

	m_piProcInfo.dwProcessId = dwProcessId;
	m_piProcInfo.dwThreadId = getThreadIdFromProcessId(dwProcessId);

	if (!m_piProcInfo.dwThreadId)
		return HEAT_FAILED_OPEN_PROCESS;

	m_bUseRemoteThread = true;
	m_bReady = true;
	m_bRunning = true;

	return HEAT_SUCCESS;
}

// Detach from a process
DWORD CHeatApp::detachFromApp()
{
	if (!m_bReady)
		return HEAT_NOT_READY;

	if (!m_bInjected)
	{
		// Wasn't injected, so we're done
		m_bReady = false;
		return HEAT_SUCCESS;
	}

	DWORD dwErr;

	// Make sure there are no intercepted functions
	CNode *pNode = m_ciIntercepts.getInterceptList();
	while (pNode)
	{
		if (pNode->getSent())
		{
            HeatNGNativeFunction original(std::string(pNode->getFuncName()), std::wstring(pNode->getFuncDLLW()));
			if ((dwErr = sendUnintercept(&original)) != HEAT_SUCCESS)
				return dwErr;
		}
		pNode = pNode->getNext();
	}

	m_ciIntercepts.markAllUnsent();

	bool bResumeWhenDone = false;

	if (m_bRunning && (!m_bUseRemoteThread))
	{
		bResumeWhenDone = true;
		if ((dwErr = pauseApp()) != HEAT_SUCCESS)
			return dwErr;
	}

	if (bResumeWhenDone)
	{
		if ((dwErr = resumeApp()) != HEAT_SUCCESS)
			return dwErr;
	}

	m_bReady = false;

	return HEAT_SUCCESS;
}

// Terminates the process and clears all interceptions
DWORD CHeatApp::deinitializeApp()
{
	if (!m_bReady)
		return HEAT_NOT_READY;

	DWORD dwErr;
	if ((dwErr = terminateApp()) != HEAT_SUCCESS)
		return dwErr;

	m_ciIntercepts.removeAllIntercepts();

	return HEAT_SUCCESS;
}

DWORD CHeatApp::sendIntercepts()
{
	if (!m_bReady)
		return HEAT_NOT_READY;

	CNode *pNode = m_ciIntercepts.getInterceptList();
	while (pNode)
	{
		if (!pNode->getSent())
		{
            HeatNGNativeFunction original(std::string(pNode->getFuncName()), std::wstring(pNode->getFuncDLLW()));
            HeatNGNativeFunction replacement(std::string(pNode->getHandlerFunc()), std::wstring(pNode->getHandlerDLLW()));
            HeatNGIntercept intercept(&original, &replacement, true);

            ((HeatNGApp*)m_heatNGapp)->addIntercept(intercept);
			pNode->setSent(true);
		}
		else if (pNode->getDeleted())
		{

            HeatNGNativeFunction original(std::string(pNode->getFuncName()), std::wstring(pNode->getFuncDLLW()));
			sendUnintercept(&original);
			CNode *pNextNode = pNode->getNext();
			m_ciIntercepts.removeIntercept(pNode->getFuncName(), pNode->getFuncDLL());
			pNode = pNextNode;
			continue;
		}

		pNode = pNode->getNext();
	}
	return HEAT_SUCCESS;
}

// Allocates memory in another process and copies the data pointed to by pData into that memory.
// If pData is NULL, no data is copied.
PVOID CHeatApp::allocDataInProc(HANDLE hProcess, PVOID pData, DWORD dwLen)
{
	PVOID pDataOther = VirtualAllocEx(hProcess, NULL, dwLen, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (!pDataOther)
		return NULL;

	if (pData)
	{
		if (!WriteProcessMemory(hProcess, pDataOther, pData, dwLen, NULL))
		{
			VirtualFreeEx(hProcess, pDataOther, dwLen, MEM_RELEASE);
			return NULL;
		}
	}
	return pDataOther;
}


// Injects the HEAT DLL into the process
DWORD CHeatApp::forceInject()
{
	if (!m_bReady)
		return HEAT_NOT_READY;
	if (m_bInjected)
		return HEAT_ALREADY_INJECTED;

	PSECURITY_DESCRIPTOR pSD;
	SECURITY_ATTRIBUTES sa;

	pSD = (PSECURITY_DESCRIPTOR) LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
	InitializeSecurityDescriptor(pSD, SECURITY_DESCRIPTOR_REVISION);
	SetSecurityDescriptorDacl(pSD, TRUE, NULL, TRUE);
	// Initialize a security attributes structure.
	sa.nLength = sizeof (SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = pSD;
	sa.bInheritHandle = FALSE;

	char eventName[30];
	_snprintf(eventName, 30, "HEATInjectionDone_%d", this->m_piProcInfo.dwProcessId);
	HANDLE injectionCompleteEvent = CreateEventA(&sa, TRUE, FALSE, eventName);

	LocalFree(pSD);

	try
	{
		m_heatNGapp = (void*) new HeatNGApp(m_pwszHeatServerPath, m_piProcInfo.dwProcessId, m_piProcInfo.dwThreadId, m_bRunning);
	}
	catch(...)
	{
		return HEAT_FAILED_INJECT_DLL;
	}
    sendIntercepts();
	m_bInjected = true;

	// HACK: STUPID, STUPID, STUPID design flaw in Heat4; this event is created here solely for the consumption of the LogSender
	// class within HDEE (loaded in the AUT) to know when injection is complete. Please note that logging will not work in HDEE
	// if this event is not set; there is no 'method' call for this, the user was intended to 'guess' the event name, I think
	if (!SetEvent(injectionCompleteEvent))
		return HEAT_FAILED_SET_EVENT;

	return HEAT_SUCCESS;
}

// Beings execution of the process
DWORD CHeatApp::runApp()
{
	if (!m_bReady)
		return HEAT_NOT_READY;
	if (m_bRunning)
		return HEAT_ALREADY_RUNNING;

	DWORD dwErr;

	if ((dwErr = resumeApp()) != HEAT_SUCCESS)
		return dwErr;

	return HEAT_SUCCESS;
}

// Suspends execution of the process
DWORD CHeatApp::pauseApp()
{
	if (!m_bReady)
		return HEAT_NOT_READY;
	if (!m_bRunning)
		return HEAT_ALREADY_PAUSED;

	if (modifyProcessState(m_piProcInfo.dwProcessId, true))
	{
		m_bRunning = false;
		char *pszEventName = new char[MAX_PATH];
		sprintf(pszEventName, "ProcessPausedEvent%d", getProcessId());
		HANDLE eventHandle = CreateEventA(NULL, false, false, pszEventName);
		SetEvent(eventHandle);
		delete[] pszEventName;
		return HEAT_SUCCESS;
	}
	return HEAT_FAILED_PAUSE_PROCESS;
}

// For externally setting the pause state of the application
DWORD CHeatApp::setPauseState(bool value)
{
	if (m_bRunning != value)
		return HEAT_SUCCESS;

	m_bRunning = !value;
	char *pszEventName = new char[MAX_PATH];
	if (m_bRunning)
		sprintf(pszEventName, "ProcessResumedEvent%d", getProcessId());
	else
		sprintf(pszEventName, "ProcessPausedEvent%d", getProcessId());

	HANDLE eventHandle = CreateEventA(NULL, false, false, pszEventName);
	SetEvent(eventHandle);
	delete[] pszEventName;

	return HEAT_SUCCESS;
}

// Continues execution of the process
DWORD CHeatApp::resumeApp()
{
	if (!m_bReady)
		return HEAT_NOT_READY;
	if (m_bRunning)
		return HEAT_ALREADY_RUNNING;

	if (modifyProcessState(m_piProcInfo.dwProcessId, false))
	{
		m_bRunning = true;
		char *pszEventName = new char[MAX_PATH];
		sprintf(pszEventName, "ProcessResumedEvent%d", getProcessId());
		HANDLE eventHandle = CreateEventA(NULL, false, false, pszEventName);
		SetEvent(eventHandle);
		delete[] pszEventName;
		return HEAT_SUCCESS;
	}
	return HEAT_FAILED_RESUME_PROCESS;
}

// Terminates execution of the process
DWORD CHeatApp::terminateApp()
{
	if (!m_bReady)
		return HEAT_NOT_READY;

	HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, false, m_piProcInfo.dwProcessId);
	if (!hProcess)
		return HEAT_FAILED_TERMINATE_PROCESS;

	if (!TerminateProcess(hProcess, 0))
	{
		CloseHandle(hProcess);
		return HEAT_FAILED_TERMINATE_PROCESS;
	}

	CloseHandle(hProcess);

	m_ciIntercepts.markAllUnsent();

	m_bReady = false;
	m_bRunning = false;
	m_bInjected = false;

	return HEAT_SUCCESS;
}


// Begins intercepting the function given by pszFuncToIntercept, rerouting calls to the function given by pszHandlerFunc
DWORD CHeatApp::interceptFunc(LPSTR pszFuncToIntercept, LPSTR pszFuncDLL, LPSTR pszHandlerFunc, LPSTR pszHandlerDLL)
{
	if (!pszFuncToIntercept)
		return HEAT_INVALID_PARAMETERS;
	if (!pszFuncDLL)
		return HEAT_INVALID_PARAMETERS;
	if (!pszHandlerFunc)
		return HEAT_INVALID_PARAMETERS;
	if (!pszHandlerDLL)
		return HEAT_INVALID_PARAMETERS;

	if (m_ciIntercepts.getIntercept(pszFuncToIntercept, pszFuncDLL))
	{
		CNode *pNode = m_ciIntercepts.getIntercept(pszFuncToIntercept, pszFuncDLL);
		if (pNode->getDeleted())
			m_ciIntercepts.removeIntercept(pszFuncToIntercept, pszFuncDLL);	// go ahead and delete the old entry
		else
			return HEAT_ALREADY_INTERCEPTED;
	}

	if (!m_ciIntercepts.addIntercept(pszFuncToIntercept, pszFuncDLL, pszHandlerFunc, pszHandlerDLL))
		return HEAT_FAILED_INTERCEPT;

	if ((m_bReady) && (m_bInjected))
	{
		DWORD dwErr;
		if ((dwErr = sendIntercepts()) != HEAT_SUCCESS)
			return dwErr;
	}

	return HEAT_SUCCESS;
}

DWORD CHeatApp::sendUnintercept(void *function)
{
    ((HeatNGApp*)m_heatNGapp)->removeInterceptByName((HeatNGFunction*)function);
	return HEAT_SUCCESS;
}

// Calls deinit if the process specified terminates
DWORD CHeatApp::deInitOnProcessTerminate(DWORD pid)
{
    // TODO: Add another command to the heatNGserver to handle this
/*
	BYTE ucCommand = CMD_DEINIT_ON_PROCESS_TERMINATE;
	DWORD dwBytes;

	WriteFile(hFile, &ucCommand, 1, &dwBytes, NULL);
	if (dwBytes != 1)
	{
		CloseHandle(hFile);
		return HEAT_FAILED_SEND_DEINIT;
	}

	WriteFile(hFile, &pid, 4, &dwBytes, NULL);
	if (dwBytes != 4)
	{
		CloseHandle(hFile);
		return HEAT_FAILED_SEND_DEINIT;
	}

	char pszProcessedEventName[MAX_PATH];
	sprintf(pszProcessedEventName, "HEATCmdProcessed%d", m_piProcInfo.dwProcessId);
	HANDLE hProcessedEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, pszProcessedEventName);
	WaitForSingleObject(hProcessedEvent, INFINITE);
	CloseHandle(hProcessedEvent);
*/
	return HEAT_SUCCESS;
}

// Returns true if the application is ready to run/intercept
bool CHeatApp::isReady()
{
	return m_bReady;
}

// Returns true if the application is currently running, false otherwise
bool CHeatApp::isRunning()
{
	return m_bRunning;
}

// Returns the process ID of the current process
DWORD CHeatApp::getProcessId()
{
	return m_piProcInfo.dwProcessId;
}

// Returns the thread ID of the current process
DWORD CHeatApp::getThreadId()
{
	return m_piProcInfo.dwThreadId;
}

DWORD CHeatApp::getThreadsInCurrentProcess(DWORD *threadList)
{
	int threadCount = 0;
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, NULL);	
	if (!hSnap)
		return 0;

	THREADENTRY32 threadInfo = {0};
	threadInfo.dwSize = sizeof(threadInfo);

	if (!Thread32First(hSnap, &threadInfo))
	{
		CloseHandle(hSnap);
		return 0;
	}

	do
	{
		if (threadInfo.th32OwnerProcessID == m_piProcInfo.dwProcessId)
		{
			if (threadList)
				threadList[threadCount] = threadInfo.th32ThreadID;
			threadCount++;
		}
		
		ZeroMemory(&threadInfo, sizeof(threadInfo));
		threadInfo.dwSize = sizeof(threadInfo);
	} while (Thread32Next(hSnap, &threadInfo));

	CloseHandle(hSnap);
	return threadCount;

}

void CHeatApp::startMonitorThread()
{
	CreateThread(NULL, 0, MonitorThreadProc, this, 0, NULL);
}

DWORD WINAPI MonitorThreadProc(LPVOID lpParameter)
{
	CHeatApp *pHeatApi = (CHeatApp*) lpParameter;
	WaitForSingleObject(pHeatApi->getProcessHandle(), INFINITE);
	
	char *pszEventName = new char[MAX_PATH];
	sprintf(pszEventName, "ProcessTerminatedEvent%d", pHeatApi->getProcessId());
	HANDLE eventHandle = CreateEventA(NULL, false, false, pszEventName);
	SetEvent(eventHandle);
	pHeatApi->setIsRunning(false);
	pHeatApi->setIsReady(false);
	pHeatApi->setIsInjected(false);

	delete[] pszEventName;
	return 0;
}
