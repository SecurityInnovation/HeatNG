// dllmain.cpp : Defines the entry point for the DLL application.
#include <windows.h>
#include <assert.h>

#ifdef WIN64
#define HEAT_DLL    L"E:\\stuff\\src\\repos\\trunk\\commercial\\HNG\\src\\heatNG\\build\\binaries\\x64\\Debug\\heatNGserver.dll"
#define TEST_MODULE L"E:\\stuff\\src\\repos\\trunk\\commercial\\HNG\\src\\heatNG\\build\\binaries\\x64\\Debug\\testModule.dll"
#else
#define HEAT_DLL    L"E:\\stuff\\src\\repos\\trunk\\commercial\\HNG\\src\\heatNG\\build\\binaries\\Win32\\Debug\\heatNGserver.dll"
#define TEST_MODULE L"E:\\stuff\\src\\repos\\trunk\\commercial\\HNG\\src\\heatNG\\build\\binaries\\Win32\\Debug\\testModule.dll"
#endif

typedef PVOID (WINAPI *getOriginalFunctionCallerType)(LPSTR pszFuncName, LPSTR pszFuncDLL);
getOriginalFunctionCallerType gofc = NULL;


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
    HMODULE hDll = NULL;
    HANDLE hEvent = NULL;
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
        OutputDebugString(L"In DLLMain for testModule.dll!\n");
        hDll = GetModuleHandle(HEAT_DLL);
        assert(hDll != NULL);
        if (!hDll)
            return FALSE;

        gofc = (getOriginalFunctionCallerType) GetProcAddress(hDll, "getOriginalFunctionCaller");
        assert(gofc != NULL);
        if (!gofc)
            return FALSE;

        break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

