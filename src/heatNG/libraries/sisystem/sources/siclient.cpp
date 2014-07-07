#include "siclient.h"


// si client class
siclient::siclient(std::wstring serverId) : sicomms(serverId)
{
    DWORD dwTotalTimeWaited = 0;

    // try to get the server handle; might need to wait till server comes up
    do
    {
        m_hServerPipe = CreateFile(m_pipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (m_hServerPipe != INVALID_HANDLE_VALUE)
            break;
        Sleep(100); dwTotalTimeWaited += 100;
    } while (m_hServerPipe == INVALID_HANDLE_VALUE && dwTotalTimeWaited < SI_COMMS_TIMEOUT);

    if (m_hServerPipe == INVALID_HANDLE_VALUE)
        throw siexception(std::wstring(L"CreateFile on Pipe failed!"), GetLastError() == ERROR_PIPE_BUSY, GetLastError());  // no checking
                                                                                                                            // for busy or
                                                                                                                            // waiting
}

// when the object goes out of scope or is deleted, get rid of the connection to free up the pipe
siclient::~siclient()
{
    if (m_hServerPipe != INVALID_HANDLE_VALUE)
        CloseHandle(m_hServerPipe);
}

