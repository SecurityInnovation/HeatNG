#pragma once


#include "sicomms.h"

// Win32 Implementation of common system services
#if defined(WIN32) || defined (WIN64)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>


class siclient : public sicomms
{
public:
    siclient(std::wstring serverId);
    ~siclient();

};

#endif