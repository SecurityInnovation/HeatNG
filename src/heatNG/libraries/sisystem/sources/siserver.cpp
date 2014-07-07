#include "siserver.h"


// si server class
siserver::siserver(std::wstring serverId) : sicomms(serverId)
{
    m_hServerPipe = CreateNamedPipe(m_pipeName.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1,\
                                  SI_COMMS_BUFFER_SIZE, SI_COMMS_BUFFER_SIZE, SI_COMMS_TIMEOUT, NULL);

    if (m_hServerPipe == INVALID_HANDLE_VALUE)
        throw siexception(std::wstring(L"CreateNamedPipe failed!"), 0, GetLastError());
}


// On destruct, destroy the pipe
siserver::~siserver()
{
    if (m_hServerPipe != INVALID_HANDLE_VALUE)
    {
        FlushFileBuffers(m_hServerPipe);      // Clean out buffers
        DisconnectNamedPipe(m_hServerPipe);   // Get rid of the client
        CloseHandle(m_hServerPipe);           // Close the Pipe handle, destroying it
    }
}


bool siserver::waitForConnection()
{
    if (!ConnectNamedPipe(m_hServerPipe, NULL))
        if (GetLastError() == ERROR_PIPE_CONNECTED)
            return true;
        else
            return false;
    return true;
}

