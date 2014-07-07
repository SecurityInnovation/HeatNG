#include "HeatNGApp.h"

// Our headers
#include "common.h"

#include "CommClient.h"
#include "siinject.h"

#include "siexception.h"
#include "siheatsupport.h"
#include "sinativeintercept.h"



// public methods
HeatNGApp::HeatNGApp(wchar_t *m_pwszHeatServer, 
                     unsigned long ulProcessId,
                     unsigned long ulThreadId,
                     bool isAppStarted) : m_pwszHeatServer(0), m_ulProcessId(ulProcessId), m_ulThreadId(ulThreadId), m_client(0), ulExceptThreadId(0)
{
    if (!m_pwszHeatServer)
        throw siexception(wstring(L"Invalid heatserver path!"), 0, 0);

    this->m_pwszHeatServer = new wchar_t[wcslen(m_pwszHeatServer) + 1];
    wcscpy(this->m_pwszHeatServer, m_pwszHeatServer);

    if (!isAppStarted) m_state = CREATED_SUSPENDED;
    else               m_state = RUNNING;

    if (!m_ulThreadId)
    {
        std::vector<unsigned long> threads;
        if (!getAllThreadIds(threads), m_ulProcessId || !threads.size())
            throw siexception(wstring(L"Couldn't get thread id's!"), 0, getLastError());
        this->m_ulThreadId = threads[0];
    }

    // Inject into the process
    injectIntoApp();

    // Connect a client to the process
    m_client = new CommClient(wstring(L"heatNGserver"), m_ulProcessId);
    if (!m_client)
        throw siexception(wstring(L"Couldn't connect client!"), 0, getLastError());
}


bool HeatNGApp::addIntercept(const HeatNGIntercept &intercept)
{
    if (!((CommClient*)m_client))
        return false;

    switch (intercept.getType())
    {
    case NATIVE:
        DBG( debugOutput(L"Sending intercept.."); )
            if (!((CommClient*)m_client)->sendCommand(CMD_INTERCEPT, (byte*) intercept.getInterceptObject(), sizeof(sinativeintercept)))
            return false;

        DBG( debugOutput(L"Intercept sent and added.."); )
        return true;
        break;
    default:
        throw siexception(L"Unexpected type!", 0, 0);
    }
    return false;
}

bool HeatNGApp::addIntercepts(const std::vector<HeatNGIntercept*> &intercepts)
{
    if (!((CommClient*)m_client)->sendCommand(CMD_PAUSEAPP, NULL, 0))  return false;

    for (std::vector<HeatNGIntercept*>::const_iterator i = intercepts.begin(); i != intercepts.end(); ++i)
        if (!addIntercept(**i))
            return false;

    if (!((CommClient*)m_client)->sendCommand(CMD_RESUMEAPP, NULL, 0)) return false;
    return true;
}

bool HeatNGApp::runApp()
{
    if      (RUNNING           == m_state) return true;
    else if (CREATED_SUSPENDED == m_state) return runThread(this->m_ulThreadId);
    else if (PAUSED            == m_state) return resumeAllThreads(this->m_ulProcessId, this->ulExceptThreadId);

    return false;
}

bool HeatNGApp::pauseApp(unsigned long ulExceptThreadId)
{
    if (RUNNING == m_state)
    {
        if (ulExceptThreadId) this->ulExceptThreadId = ulExceptThreadId;
        return suspendAllThreads(this->m_ulProcessId, ulExceptThreadId);
    }
    else
    {
        return false;
    }
}

ProcessState HeatNGApp::getAppState()
{
    return this->m_state;
}


// private methods
bool HeatNGApp::injectIntoApp()
{
    if (CREATED_SUSPENDED == m_state)
        return injectIntoProcess       (this->m_ulProcessId, this->m_ulThreadId, this->m_pwszHeatServer);
    else
        return injectIntoRunningProcess(this->m_ulProcessId,                     this->m_pwszHeatServer);
}


bool HeatNGApp::removeInterceptByName (HeatNGFunction *intercept)
{
    return true;
}

