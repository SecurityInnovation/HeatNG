#pragma once

#include "common.h"
#include "siheatsupport.h"

// Win32 Implementation of common system services
#if defined(WIN32) || defined (WIN64)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>


#pragma warning(disable: 4996)
class siintercept
{
    void *pvOriginalFunction;
    void *pvOriginalFunctionHeaderCopy;
    void *pvReplacementFunction;
    void *pvReplacementStub;

    void *pvCallerStub;


    // Yes, this is horrible, but we want to make this
    // object easily marshallable across processes, so;
    wchar_t pwszOriginalFunctionModule[MAX_PATH];
    wchar_t pwszReplacementFunctionModule[MAX_PATH];

    char    pszOriginalFunction[MAX_PATH];
    char    pszReplacementFunction[MAX_PATH];

    bool    bIsFullyReentrant;

    void resolveOriginalAddress()
    {
        HMODULE hMod = ::GetModuleHandle(pwszOriginalFunctionModule);
        if (!hMod)
            hMod = ::LoadLibrary(pwszOriginalFunctionModule);

        if (!hMod)
        {
            DBG( debugOutput(L"Not Found hMod (original)!"); )
            return;
        }

        pvOriginalFunction = (void*) ::GetProcAddress(hMod, pszOriginalFunction);
    }

    void resolveReplacementAddress()
    {
        HMODULE hMod = ::GetModuleHandle(pwszReplacementFunctionModule);
        if (!hMod)
            hMod = ::LoadLibrary(pwszReplacementFunctionModule);

        if (!hMod)
        {
            DBG( debugOutput(L"Not Found hMod (replacement)!"); )
            return;
        }

        pvReplacementFunction = (void*) ::GetProcAddress(hMod, pszReplacementFunction);
        DBG( if (!pvReplacementFunction) debugOutput(L"Not Found pvReplacementFunction!"); )
    }

public:
    siintercept(bool bIsFullyReentrant = false) \
                : pvOriginalFunction(0), pvOriginalFunctionHeaderCopy(0), pvReplacementFunction(0), pvCallerStub(0), bIsFullyReentrant(bIsFullyReentrant)
    {
        *this->pszOriginalFunction = 0;
        *this->pszReplacementFunction = 0;

        *this->pwszOriginalFunctionModule = 0;
        *this->pwszReplacementFunctionModule = 0;
    }

    siintercept(std::string originalFunction,      std::wstring originalFunctionModule,        \
                std::string replacementFunction,   std::wstring replacementFunctionModule,     \
                bool bIsFullyReentrant = false)
                : pvOriginalFunction(0), pvOriginalFunctionHeaderCopy(0), pvReplacementFunction(0), pvCallerStub(0), bIsFullyReentrant(bIsFullyReentrant)
    {
        strcpy(this->pszOriginalFunction, originalFunction.c_str());
        wcscpy(this->pwszOriginalFunctionModule, originalFunctionModule.c_str());

        strcpy(this->pszReplacementFunction, replacementFunction.c_str());
        wcscpy(this->pwszReplacementFunctionModule, replacementFunctionModule.c_str());
    }

    ~siintercept()
    {
    }

    void *originalFunction()
    {
        if (pvOriginalFunction)
            return pvOriginalFunction;
        // else
        this->resolveOriginalAddress();
        return pvOriginalFunction;
    }
    void *replacementFunction()
    {
        if (pvReplacementFunction)
            return pvReplacementFunction;
        // else
        resolveReplacementAddress();
        return pvReplacementFunction;
    }


    bool interceptFunction()
    {
        // create a copy of the original function header
        this->pvOriginalFunctionHeaderCopy = generateFunctionHeaderCopy(originalFunction());
        if (!this->pvOriginalFunctionHeaderCopy) goto error;

        DBG( wchar_t dbg[100]; )
        DBG( wsprintf(dbg, L"Original header is at: 0x%x\n", this->pvOriginalFunctionHeaderCopy); )
        DBG( debugOutput(dbg); )

        // create the replacement function stub
        this->pvReplacementStub = generateReplacementStub(this->pvOriginalFunctionHeaderCopy, replacementFunction());
        if (!this->pvReplacementStub) goto error;

        DBG( wsprintf(dbg, L"Replacement stub is at: 0x%x\n", this->pvReplacementStub); )
        DBG( debugOutput(dbg); )

        // if function is fully reentrant, create a caller stub for it too
        if (bIsFullyReentrant)
        {
            this->pvCallerStub = generateCallerStub(this->pvOriginalFunctionHeaderCopy);
            if (!this->pvCallerStub) goto error;
            DBG( wsprintf(dbg, L"Caller stub is at: 0x%x\n", this->pvReplacementStub); )
            DBG( debugOutput(dbg); )
        }
        else
        {
            // else point the caller stub pointer to the original function header copy
            // using this will give the replacement function an additional performance gain
            this->pvCallerStub = this->pvOriginalFunctionHeaderCopy;
        }

        // replace the original function starting instructions with a jmp to our replacement stub
        if (!writeRedirection(this->pvOriginalFunction, this->pvReplacementStub)) goto error;
        DBG( debugOutput(L"Interception successful!\n"); )

        return true;
error:
        if (this->pvOriginalFunctionHeaderCopy) memoryFree(this->pvOriginalFunctionHeaderCopy);
        if (this->pvReplacementStub)            memoryFree(this->pvReplacementStub);
        if (this->pvCallerStub)                 memoryFree(this->pvCallerStub);
        return false;
    }


    // Accessors
    char    *getOriginalFunctionName()                                                  { return this->pszOriginalFunction; }
    void    *setOriginalFunctionName(char *pszOriginalFunction)                         { strcpy(this->pszOriginalFunction, pszOriginalFunction); }

    wchar_t *getOriginalFunctionModuleName()                                            { return this->pwszOriginalFunctionModule; }
    wchar_t *setOriginalFunctionModuleName(wchar_t *pwszOriginalFunctionModule)         { wcscpy(this->pwszOriginalFunctionModule, pwszOriginalFunctionModule); }

    char    *getReplacementFunctionName()                                               { return this->pszReplacementFunction; }
    void    *setReplacementFunctionName(char *pszReplacementFunction)                   { strcpy(this->pszReplacementFunction, pszReplacementFunction); }

    wchar_t *getReplacementFunctionModuleName()                                         { return this->pwszReplacementFunctionModule; }
    wchar_t *setReplacementFunctionModuleName(wchar_t *pwszReplacementFunctionModule)   { wcscpy(this->pwszReplacementFunctionModule, pwszReplacementFunctionModule); }

    void    *getCallerStub()                                                            { return this->pvCallerStub; }
};

#endif
