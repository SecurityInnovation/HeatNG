#pragma once

// C++ headers
#include <vector>
#include "HeatNGIntercept.h"

enum ProcessState { CREATED_SUSPENDED, PAUSED, RUNNING };

class HeatNGApp
{
    wchar_t        *m_pwszHeatServer;
    unsigned long   m_ulProcessId;
    unsigned long   m_ulThreadId;
    void           *m_client;

    ProcessState    m_state;
    unsigned long   ulExceptThreadId;

    bool injectIntoApp();
public:
    HeatNGApp(wchar_t      *m_pwszHeatServer, 
              unsigned long ulProcessId,
              unsigned long ulThreadId = 0,
              bool          isAppStarted = false);

    ~HeatNGApp();

    // intercept manipulation methods
    bool addIntercept (const HeatNGIntercept &intercept);
    bool addIntercepts(const std::vector<HeatNGIntercept*> &intercepts);

    bool removeInterceptByName (HeatNGFunction *intercept);
    bool removeInterceptsByName(const std::vector<HeatNGFunction*> &intercepts);

    bool removeAllIntercepts();


    // app manipulation methods
    bool runApp();
    bool pauseApp(unsigned long ulExceptThreadId = 0);
    bool terminateApp();

    ProcessState getAppState();
};

