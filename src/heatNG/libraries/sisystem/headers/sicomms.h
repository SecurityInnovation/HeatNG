#pragma once

#include "sisystem.h"


// Win32 Implementation of common system services
#if defined(WIN32) || defined (WIN64)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>


class sicomms
{
protected:
    HANDLE       m_hServerPipe;
    std::wstring m_pipeName;
public:
    sicomms(std::wstring serverId);
    bool readByte (byte &b);
    int  readBytes(byte *data, unsigned long size);

    bool writeByte (byte b);
    bool writeBytes(byte *data, unsigned long size);
};

#endif