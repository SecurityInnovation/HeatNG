#pragma once

#include "common.h"

class sinativeintercept
{
    void *m_pvOriginalFunction;
    void *m_pvOriginalFunctionHeaderCopy;
    void *m_pvReplacementFunction;
    void *m_pvReplacementStub;

    void *m_pvCallerStub;


    // Yes, this is horrible, but we want to make this
    // object easily marshallable across processes, so;
    wchar_t m_pwszOriginalFunctionModule[MAX_PATH];
    wchar_t m_pwszReplacementFunctionModule[MAX_PATH];

    char    m_pszOriginalFunction[MAX_PATH];
    char    m_pszReplacementFunction[MAX_PATH];

    bool    m_bIsFullyReentrant;

    bool resolveOriginalAddress();
    bool resolveReplacementAddress();

public:
sinativeintercept::sinativeintercept(const std::string &originalFunction,      const std::wstring &originalFunctionModule,      \
                                     const std::string &replacementFunction,   const std::wstring &replacementFunctionModule,   \
                                     bool bIsFullyReentrant = false)
                                    : m_pvOriginalFunction(0),    m_pvOriginalFunctionHeaderCopy(0),                            \
                                      m_pvReplacementFunction(0), m_pvCallerStub(0),                                            \
                                      m_bIsFullyReentrant(bIsFullyReentrant)
{
    strcpy(this->m_pszOriginalFunction, originalFunction.c_str());
    wcscpy(this->m_pwszOriginalFunctionModule, originalFunctionModule.c_str());

    strcpy(this->m_pszReplacementFunction, replacementFunction.c_str());
    wcscpy(this->m_pwszReplacementFunctionModule, replacementFunctionModule.c_str());
}

sinativeintercept::~sinativeintercept()
{
}


    void *originalFunction();
    void *replacementFunction();

    bool interceptFunction();

    // Accessors
    char    *getOriginalFunctionName()                                                  { return m_pszOriginalFunction; }
    void    *setOriginalFunctionName(char *pszOriginalFunction)                         { strcpy(m_pszOriginalFunction, pszOriginalFunction); }

    wchar_t *getOriginalFunctionModuleName()                                            { return m_pwszOriginalFunctionModule; }
    wchar_t *setOriginalFunctionModuleName(wchar_t *pwszOriginalFunctionModule)         { wcscpy(m_pwszOriginalFunctionModule, pwszOriginalFunctionModule); }

    char    *getReplacementFunctionName()                                               { return m_pszReplacementFunction; }
    void    *setReplacementFunctionName(char *pszReplacementFunction)                   { strcpy(m_pszReplacementFunction, pszReplacementFunction); }

    wchar_t *getReplacementFunctionModuleName()                                         { return m_pwszReplacementFunctionModule; }
    wchar_t *setReplacementFunctionModuleName(wchar_t *pwszReplacementFunctionModule)   { wcscpy(m_pwszReplacementFunctionModule, pwszReplacementFunctionModule); }

    void    *getCallerStub()                                                            { return this->m_pvCallerStub; }
};

