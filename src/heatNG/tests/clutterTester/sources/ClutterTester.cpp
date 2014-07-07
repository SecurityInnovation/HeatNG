// ClutterTester.cpp : Defines the entry point for the console application.
// The clutter tester is a generic tester which'll test anything

#include "stdafx.h"
#include "conio.h"

#include "CommServer.h"
#include "common.h"
#include "HeatNGApp.h"
#include "CHeatApp.h"

void testFunction()
{
    printf("Thread id is 0x%x\n", GetCurrentThreadId());
}


DWORD WINAPI simpleThread(void *param)
{
    while (1)
    {
        printf(".");
        Sleep(200);
    }
    return 0;
}


unsigned long myFunction1() { return 32; };
unsigned long myFunction2() { return 64; };

static unsigned long myFunction()
{
    DWORD dwProcessId = GetCurrentProcessId();
    DWORD myReturnValue = myFunction1() + myFunction2() + dwProcessId;

    printf("retval before the for loop is %d\n", myReturnValue);
    for (int i = 0; i < 5; i++)
    {
        myReturnValue += (dwProcessId - myFunction2());
        printf("retval in the for loop is     %d\n", myReturnValue);
    }
    printf("retval after the for loop is  %d\n", myReturnValue);

    return myReturnValue;
}
/*
typedef unsigned long (__stdcall *MYFUNCTYPE)();
bool runDisasmTest()
{
    // IS byte *relocInstruction(byte *dest, byte *src, byte **updatedSrcPtr)
    void *pvNewFunc = memoryAllocExecutableNear(4096, myFunction);

    byte *pbNewDst = 0;
    byte *pbNewSrc = 0;

    byte *pbDst = (byte*) pvNewFunc;
    byte *pbSrc = (byte*) myFunction;

    unsigned long ulSizeCopied = 0;
    while (*pbSrc != 0xc2 && *pbSrc != 0xc3 && ulSizeCopied < 4095)
    {
        pbNewDst = relocInstruction(pbDst, pbSrc, &pbNewSrc);
        ulSizeCopied += (unsigned long) (pbNewSrc - pbSrc);
        pbDst = pbNewDst; pbSrc = pbNewSrc;
    }

    if (ulSizeCopied == 4095) return false;
    else                      relocInstruction(pbDst, pbSrc, NULL);

    printf("Size copied is %d\n", ulSizeCopied);
    printf("Original function: 0x%llx\n", myFunction);
    printf("Copied function  : 0x%llx\n", pvNewFunc);
    DebugBreak();
    unsigned long myfret  = myFunction();
    unsigned long newfret = ((MYFUNCTYPE)pvNewFunc)();

    printf("myFunction returned:   0x%d\n", myfret);
    printf("new function returned: 0x%d\n", newfret);

    return (myfret == newfret);
}
*/
class MyHandler : public CommHandler
{
public:
    bool cmdHandlerFunction(byte bCmd, byte *pbData, unsigned long ulLen)
    {
        printf("CMD = 0x%x\n", bCmd);
        printf("Data len = %d\n", ulLen);
        printf("Data is  = %s\n", (char*) pbData);
        return true;
    }
};

#ifdef WIN64
#define HEAT_DLL    L"E:\\stuff\\src\\repos\\trunk\\commercial\\HNG\\src\\heatNG\\build\\binaries\\x64\\Debug\\heatNGserver.dll"
#define TEST_MODULE L"E:\\stuff\\src\\repos\\trunk\\commercial\\HNG\\src\\heatNG\\build\\binaries\\x64\\Debug\\testModule.dll"
#define TEST_MODULEA "E:\\stuff\\src\\repos\\trunk\\commercial\\HNG\\src\\heatNG\\build\\binaries\\x64\\Debug\\testModule.dll"
#else
#define HEAT_DLL    L"E:\\stuff\\src\\repos\\trunk\\commercial\\HNG\\src\\heatNG\\build\\binaries\\Win32\\Debug\\heatNGserver.dll"
#define TEST_MODULE L"E:\\stuff\\src\\repos\\trunk\\commercial\\HNG\\src\\heatNG\\build\\binaries\\Win32\\Debug\\testModule.dll"
#define TEST_MODULEA "E:\\stuff\\src\\repos\\trunk\\commercial\\HNG\\src\\heatNG\\build\\binaries\\Win32\\Debug\\testModule.dll"
#endif

int __cdecl heatTest_tmain(int argc, _TCHAR* argv[])
//int __cdecl _tmain(int argc, _TCHAR* argv[])
{
/*    MyHandler *handler = new MyHandler();

    CommServer server(wstring(L"heatNGserver"), handler);
    server.startServer();

    CommClient client(wstring(L"heatNGserver"), 0);
    client.sendCommand(0x12, (byte*)"test", 5);

    printf("Press any key to exit..\n"); getch();
    return -1;*/

/*    if (!runDisasmTest())
        printf("Disasm test failed!!\n");
    else
        printf("Disasm test passed!!\n");

    return -1;*/

    HeatNGIntercept *i1 = new HeatNGIntercept(new HeatNGNativeFunction(std::string("LocalAlloc"),     std::wstring(L"kernel32.dll")), new HeatNGNativeFunction(std::string("replacementLocalAlloc"),     std::wstring(TEST_MODULE)));
    HeatNGIntercept *i2 = new HeatNGIntercept(new HeatNGNativeFunction(std::string("LoadLibraryW"),   std::wstring(L"kernel32.dll")), new HeatNGNativeFunction(std::string("replacementLoadLibraryW"),   std::wstring(TEST_MODULE)), true);
    HeatNGIntercept *i3 = new HeatNGIntercept(new HeatNGNativeFunction(std::string("LoadLibraryExW"), std::wstring(L"kernel32.dll")), new HeatNGNativeFunction(std::string("replacementLoadLibraryExW"), std::wstring(TEST_MODULE)), true);
    HeatNGIntercept *i4 = new HeatNGIntercept(new HeatNGNativeFunction(std::string("CreateFileW"),    std::wstring(L"kernel32.dll")), new HeatNGNativeFunction(std::string("replacementCreateFileW"),    std::wstring(TEST_MODULE)), true);

    std::vector<HeatNGIntercept*> intercepts;
    intercepts.clear();

//    intercepts.push_back(i1);
    intercepts.push_back(i2);
    intercepts.push_back(i3);
    intercepts.push_back(i4);

    STARTUPINFO si = {0}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};

//    if (!CreateProcess(L"c:\\Program Files (x86)\\Internet Explorer\\iexplore.exe", NULL, NULL, NULL, false, CREATE_SUSPENDED, NULL, NULL, &si, &pi))
//    if (!CreateProcess(L"c:\\Program Files\\Internet Explorer\\iexplore.exe", NULL, NULL, NULL, false, CREATE_SUSPENDED, NULL, NULL, &si, &pi))
    if (!CreateProcess(L"c:\\windows\\notepad.exe", NULL, NULL, NULL, false, CREATE_SUSPENDED, NULL, NULL, &si, &pi))
    {
        printf("Create process failed! GLE = %d\n", GetLastError());
        return -1;
    }

    printf("About to instantiate heat app..\n");
    printf("Press any key to continue..\n"); getch();
    HeatNGApp *app = 0;
    try
    {
        app = new HeatNGApp(HEAT_DLL, pi.dwProcessId, pi.dwThreadId);
    }
    catch(siexception e)
    {
        wprintf(L"Exception thrown from constructor!\n%s\n", e.what());
        return -1;
    }
    app->addIntercepts(intercepts);
    printf("Press any key to run the app...\n"); getch();
    app->runApp();

    printf("Press any key to terminate...\n"); getch();
    TerminateProcess(pi.hProcess, 0);

	return 0;
}



//int __cdecl bcompat_tmain(int argc, _TCHAR* argv[])
int __cdecl _tmain(int argc, _TCHAR* argv[])
{
/*    MyHandler *handler = new MyHandler();

    CommServer server(wstring(L"heatNGserver"), handler);
    server.startServer();

    CommClient client(wstring(L"heatNGserver"), 0);
    client.sendCommand(0x12, (byte*)"test", 5);

    printf("Press any key to exit..\n"); getch();
    return -1;*/

/*    if (!runDisasmTest())
        printf("Disasm test failed!!\n");
    else
        printf("Disasm test passed!!\n");

    return -1;*/


    DWORD dwRet;
    CHeatApp *app = new CHeatApp((unsigned short*)HEAT_DLL);

    printf("About to instantiate heat app..\n");
//    if ((dwRet = app->initializeApp("c:\\windows", "notepad.exe")) != HEAT_SUCCESS)
    STARTUPINFO si = {0}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi;

//    if (!CreateProcess(L"c:\\Windows\\notepad.exe", NULL, NULL, NULL, false, NULL, NULL, NULL, &si, &pi))
    if (!CreateProcess(L"c:\\Program Files (x86)\\Internet Explorer\\iexplore.exe", NULL, NULL, NULL, false, NULL, NULL, NULL, &si, &pi))
    {
        printf("Couldn't create process! GLE = %d\n", GetLastError());
        return -1;
    }
    printf("Process id is %d\n", pi.dwProcessId);

//    if ((dwRet = app->initializeApp("c:\\Program Files (x86)\\Internet Explorer", "iexplore.exe")) != HEAT_SUCCESS)
    if ((dwRet = app->attachToRunningApp(pi)) != HEAT_SUCCESS)
    {
        
        printf("Couldn't init app!\n");
        return -1;
    }

    app->interceptFunc("LocalAlloc", "kernel32.dll", "replacementLocalAlloc", TEST_MODULEA);

    

/*    HeatNGIntercept *i1 = new HeatNGIntercept(new HeatNGNativeFunction(std::string("LocalAlloc"),     std::wstring(L"kernel32.dll")), new HeatNGNativeFunction(std::string("replacementLocalAlloc"),     std::wstring(TEST_MODULE)));
    HeatNGIntercept *i2 = new HeatNGIntercept(new HeatNGNativeFunction(std::string("LoadLibraryW"),   std::wstring(L"kernel32.dll")), new HeatNGNativeFunction(std::string("replacementLoadLibraryW"),   std::wstring(TEST_MODULE)), true);
    HeatNGIntercept *i3 = new HeatNGIntercept(new HeatNGNativeFunction(std::string("LoadLibraryExW"), std::wstring(L"kernel32.dll")), new HeatNGNativeFunction(std::string("replacementLoadLibraryExW"), std::wstring(TEST_MODULE)), true);
    HeatNGIntercept *i4 = new HeatNGIntercept(new HeatNGNativeFunction(std::string("CreateFileW"),    std::wstring(L"kernel32.dll")), new HeatNGNativeFunction(std::string("replacementCreateFileW"),    std::wstring(TEST_MODULE)), true);*/

    printf("Press any key to continue..\n"); getch();
    app->forceInject();
    printf("Press any key to run the app...\n"); getch();
    app->runApp();

    printf("Press any key to terminate...\n"); getch();
    app->terminateApp();
//    TerminateProcess(pi.hProcess, 0);

	return 0;
}
