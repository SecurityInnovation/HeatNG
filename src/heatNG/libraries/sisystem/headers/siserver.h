#pragma once

#include "sicomms.h"

// Win32 Implementation of common system services
#if defined(WIN32) || defined (WIN64)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

class siserver : public sicomms
{
public:
    siserver(std::wstring serverId);
    ~siserver();

    bool waitForConnection();
};

#endif