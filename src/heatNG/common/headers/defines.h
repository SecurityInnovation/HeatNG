// Common defines header file
#pragma once


typedef unsigned char byte;


#define SI_COMMS_TIMEOUT            5000    // milliseconds
#define SI_WAIT_TIMEOUT             10000   // milliseconds
#define SI_COMMS_BUFFER_SIZE        8192    // bytes


#define CMD_INTERCEPT               0xa8
#define CMD_PAUSEAPP                0x3c
#define CMD_RESUMEAPP               0xf2



#if defined(_DEBUG)
#define DBG(x) x
#else
//#define DBG(x) x
#define DBG(x) 
#endif

#if defined(_DEBUG)
#define DBGOUT(x) { debugOutput(x); }
#else
//#define DBGOUT(x) { debugOutput(x); }
#define DBGOUT(x) 
#endif


#if defined(_DEBUG)
#define TRACE(x, y) { wchar_t msg[256]; wsprintf(msg, L"%s: %d\n", x, y); debugOutput(msg); }
#else
//#define TRACE(x, y) { wchar_t msg[256]; wsprintf(msg, L"%s: %d\n", x, y); debugOutput(msg); }
#define TRACE(x, y) 
#endif

#if defined(_DEBUG)
#define DBGTRACE(x) { wchar_t msg[256]; wsprintf(msg, L"%s: GLE = %d\n", x, GetLastError()); debugOutput(msg); }
#else
//#define DBGTRACE(x) { wchar_t msg[256]; wsprintf(msg, L"%s: GLE = %d\n", x, GetLastError()); debugOutput(msg); }
#define DBGTRACE(x) 
#endif

#if defined(_DEBUG)
#define DBGLOG(x, y) { wchar_t msg[256]; wsprintf(msg, L"%s: %d, GLE = %d\n", x, y, GetLastError()); debugOutput(msg); }
#else
//#define DBGLOG(x, y) { wchar_t msg[256]; wsprintf(msg, L"%s: %d, GLE = %d\n", x, y, GetLastError()); debugOutput(msg); }
#define DBGLOG(x, y) 
#endif

#if defined(_DEBUG)
#define TRACEX(x, y) { wchar_t msg[256]; wsprintf(msg, L"%s: 0x%p\n", x, y); debugOutput(msg); }
#else
//#define TRACEX(x, y) { wchar_t msg[256]; wsprintf(msg, L"%s: 0x%p\n", x, y); debugOutput(msg); }
#define TRACEX(x, y) 
#endif


#ifndef MAX_PATH
#define MAX_PATH 260
#endif