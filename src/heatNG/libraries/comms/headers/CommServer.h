// Class header file for the comms class
#pragma once

#include "common.h"
#include "sisystem.h"
#include "siserver.h"
#include "sithread.h"

#include "CommHandler.h"


// anonymous namespace to prevent automatic external linkage 
namespace
{
    void *serverFunction(void *pvServer);

    class CommServer
    {
    private:
        CommHandler *m_handler;
        siserver    *m_server;
        sithread    *m_serverThread;

        wstring      m_serverName;
    public:
        CommServer(const wstring &serverId, CommHandler *m_handler);
        ~CommServer();

        void startServer();
        void  stopServer();


        bool         isRunning()    { return !(m_server == NULL); }

        // Accessors
        CommHandler *getHandler()   { return m_handler;           }
        siserver    *getServer()    { return  m_server;           }
    };


    // --- implementation

    CommServer::CommServer(const wstring &serverId, CommHandler *handler) : m_handler (handler), m_server(0), m_serverThread(0)
    {
        // the pipe name will always be a combination of the id and the process id
        wchar_t pwszProcId[10]; _ltow(getCurrentProcessId(), pwszProcId, 10);
        this->m_serverName = wstring(L"sipipe") + serverId + wstring(pwszProcId);
    }

    CommServer::~CommServer()
    {
        if (m_serverThread)
            delete m_serverThread;
        if (m_server)
            delete m_server;
    }

    void CommServer::startServer()
    {
        if (this->m_server)
        {
            assert("Trying to start a running m_server!");
            return; // m_server already started
        }

        DBG( debugOutput(L"Starting siserver.."); )
        // create the m_server
        this->m_server = new siserver(m_serverName);

        DBG( debugOutput(L"Creating m_server thread.."); )
        // start thread with the m_server function
        m_serverThread = new sithread(serverFunction, (void*) this);
    }

    void CommServer::stopServer()
    {
        if (this->m_server)
            delete this->m_server;
        this->m_server = 0;
    }

    void *serverFunction(void *pvServer)
    {
        CommServer *m_server = (CommServer *) pvServer;

        DBG( debugOutput(L"SERVER THREAD: Waiting for connection.."); )
        // wait for the m_server to get a connection
        if (!m_server->getServer()->waitForConnection())
        {
            assert("Server connection failed!");
            return 0;  // connection didn't go through; no where to throw an exception too; fail
        }

        DBG( debugOutput(L"Server Thread: Connected!"); )
        // connected, process commands
        while (1)
        {
            // read command byte
            byte bCmd;
            if (!m_server->getServer()->readByte(bCmd))
            {
                assert("Couldn't read command byte!");
                return 0;  // connection didn't go through; no where to throw an exception to; fail
            }
            DBG( debugOutput(L"SERVER THREAD: Got command byte!"); )

            // read data bytes and length
            byte pbData[SI_COMMS_BUFFER_SIZE];
            int iDataLen = m_server->getServer()->readBytes(pbData, SI_COMMS_BUFFER_SIZE);

            // send command and associated data to the m_handler
            if (!m_server->getHandler()->cmdHandlerFunction(bCmd, pbData, iDataLen))
                // if failed, write back a fail code
                m_server->getServer()->writeByte(0);
            else
                // else, write back a success code
                m_server->getServer()->writeByte(1);
            DBG( debugOutput(L"SERVER THREAD: Written status! Back to waiting.."); )
        }

        return 0;
    }
}   // namespace

