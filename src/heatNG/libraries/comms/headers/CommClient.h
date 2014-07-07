// Class header file for the comms class
#pragma once

#include "common.h"
#include "sisystem.h"
#include "siclient.h"

#include <string>
using namespace std;

// anonymous namespace to prevent automatic external linkage 
namespace
{
    class CommClient
    {
    private:
        siclient *m_client;

    public:
        CommClient(const wstring &serverId, unsigned long ulAUTProcessId);

        bool isAlive();
        bool sendCommand(byte bCmd, byte *data, unsigned long ulLen);
    };


    // --- implementation

    CommClient::CommClient(const wstring &serverId, unsigned long ulAUTProcessId) : m_client(NULL)
    {
        // the pipe name will always be a combination of the id and the process id
        if (!ulAUTProcessId) ulAUTProcessId = getCurrentProcessId();
        wchar_t pwszProcId[10]; _ltow(ulAUTProcessId, pwszProcId, 10);
        wstring serverName = wstring(L"sipipe") + serverId + wstring(pwszProcId);

        this->m_client = new siclient(serverName);
    }


    bool CommClient::isAlive()
    {
        return true;
    }

    bool CommClient::sendCommand(byte bCmd, byte *data, unsigned long ulLen)
    {
        // send command byte
        if (!m_client->writeByte(bCmd))
            return false;
        // send command data
        if (!data || !ulLen)
        {
            // point data to any junk value
            data = (byte*) &data; ulLen = 1;
        }
        if (!m_client->writeBytes(data, ulLen))
            return false;

        // read back command execution status
        byte bErr; m_client->readByte(bErr);
        if (bErr)
            return true;
        else
            return false;
    }
}   // namespace
