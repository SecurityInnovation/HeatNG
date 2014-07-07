// Class header file for the comms handler class
#pragma once

#include "common.h"
#include "sisystem.h"

using namespace std;
// anonymous namespace to prevent automatic external linkage 
namespace
{
    class CommHandler
    {
    private:
    public:
        virtual bool cmdHandlerFunction(byte bCmd, byte *pbData, unsigned long ulLen) = 0;
    };
}   // namespace

