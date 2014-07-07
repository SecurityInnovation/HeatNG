#include "sicomms.h"


// si comms base class
sicomms::sicomms(std::wstring serverId) : m_hServerPipe(NULL)
{
    m_pipeName = std::wstring(L"\\\\.\\pipe\\)") + serverId;
    if (m_pipeName.length() > 256)
        throw siexception(std::wstring(L"ServerId too long!"), 0, 0);
}


bool sicomms::readByte(byte &b)
{
    DWORD dwBytesRead;
    BOOL bResult = ReadFile(m_hServerPipe, &b, 1, &dwBytesRead, NULL);
    if (!bResult || !dwBytesRead)
        return false;
    return true;
}

int  sicomms::readBytes(byte *data, unsigned long size)
{
    DWORD dwBytesRead;
    BOOL bResult = ReadFile(m_hServerPipe, data, size, &dwBytesRead, NULL);
    if (!bResult)
        return -1;
    return dwBytesRead;
}


bool sicomms::writeByte(byte b)
{
    DWORD dwBytesWritten;
    BOOL bResult = WriteFile(m_hServerPipe, &b, 1, &dwBytesWritten, NULL);
    if (!bResult || !dwBytesWritten)
        return false;
    return true;
}

bool sicomms::writeBytes(byte *data, unsigned long size)
{
    DWORD dwBytesWritten;
    BOOL bResult = WriteFile(m_hServerPipe, data, size, &dwBytesWritten, NULL);
    if (!bResult || !dwBytesWritten)
        return false;
    return true;
}

