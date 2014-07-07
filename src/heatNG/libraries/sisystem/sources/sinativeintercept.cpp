#include "common.h"
#include "siheatsupport.h"
#include "sinativeintercept.h"

// Win32 Implementation of common system services
#if defined(WIN32) || defined (WIN64)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>


bool sinativeintercept::resolveOriginalAddress()
{
    HMODULE hMod = ::GetModuleHandle(m_pwszOriginalFunctionModule);
    if (!hMod)
        hMod = ::LoadLibrary(m_pwszOriginalFunctionModule);

    if (!hMod)
    {
        DBG( debugOutput(L"Not Found hMod (original)!"); )
        DBG( debugOutput(m_pwszOriginalFunctionModule); )
        return false;
    }
    TRACEX( L"Original DLL is      @", hMod )
    m_pvOriginalFunction = (void*) ::GetProcAddress(hMod, m_pszOriginalFunction);

    TRACEX( L"Original function is @", m_pvOriginalFunction )
    return true;
}

bool sinativeintercept::resolveReplacementAddress()
{
    HMODULE hMod = ::GetModuleHandle(m_pwszReplacementFunctionModule);
    if (!hMod)
	{
        hMod = ::LoadLibrary(m_pwszReplacementFunctionModule);

		// HACK: backwards compatibility hack
		void (*init)() = (void(*)())GetProcAddress(hMod, "ReplacementLibraryAttach");
		if (init)
			init();
	}

    if (!hMod)
    {
        DBG( debugOutput(L"Not Found hMod (replacement)!"); )
        return false;
    }

    m_pvReplacementFunction = (void*) ::GetProcAddress(hMod, m_pszReplacementFunction);
    DBG( if (!m_pvReplacementFunction) debugOutput(L"Not Found pvReplacementFunction!"); )
    return true;
}

void* sinativeintercept::originalFunction()
{
    if (m_pvOriginalFunction)
        return m_pvOriginalFunction;
    // else
    if (!resolveOriginalAddress()) return 0;
    return m_pvOriginalFunction;
}
void* sinativeintercept::replacementFunction()
{
    if (m_pvReplacementFunction)
        return m_pvReplacementFunction;
    // else
    if (!resolveReplacementAddress()) return 0;
    return m_pvReplacementFunction;
}


// this function will get called only from one
// thread, ever so using statics is safe
bool sinativeintercept::interceptFunction()
{
    // create a copy of the original function header
    // use whatever memory for this, doesn't matter; we have plenty
    // of space to write out long and complicated jump sequences
    void *pvStubMem = 0;
    m_pvOriginalFunctionHeaderCopy = generateFunctionHeaderCopy(originalFunction(), pvStubMem);
    if (!m_pvOriginalFunctionHeaderCopy) goto error;

    TRACEX( L"Original code is at  :", originalFunction() )
    TRACEX( L"Original header is at:", m_pvOriginalFunctionHeaderCopy )

    // create the replacement function stub

    // create and use memory we can re-use since we are not always
    // sure to find too many pages near the system function and we
    // DO NOT have all the space in the world to write long and 
    // complicated jump sequences
    static void *pvMem = memoryAllocExecutableNear(PAGE_SIZE, originalFunction());
    static void *pvMemBase = pvMem;

    // check if we are near the original function, if not, alloc near again
    pvMem = memoryAllocExecutableIfFar(PAGE_SIZE, pvMem, originalFunction());

    // check if we have space left on the page; our stub code can go upto 250h-300h bytes
    if ((byte*) pvMem - (byte*) pvMemBase > (PAGE_SIZE - 0x300)) pvMemBase = pvMem = memoryAllocExecutableNear(PAGE_SIZE, originalFunction());

    m_pvReplacementStub = generateReplacementStub(m_pvOriginalFunctionHeaderCopy, replacementFunction(), pvMem);

    if (!m_pvReplacementStub) goto error;
    TRACEX( L"Replacement stub is at:", m_pvReplacementStub )

    // if function is fully reentrant, create a caller stub for it too
    if (m_bIsFullyReentrant)
    {
        // use whatever memory for this, doesn't matter; we have plenty
        // of space to write out long and complicated jump sequences

        m_pvCallerStub = generateCallerStub(m_pvOriginalFunctionHeaderCopy);
        if (!m_pvCallerStub) goto error;
        TRACEX( L"Caller stub is at     :", m_pvReplacementStub )
    }
    else
    {
        // else point the caller stub pointer to the original function header copy
        // using this will give the replacement function an additional performance gain
        m_pvCallerStub = m_pvOriginalFunctionHeaderCopy;
    }

    // replace the original function starting instructions with a jmp to our replacement stub
    if (!writeRedirection(m_pvOriginalFunction, m_pvReplacementStub)) goto error;
    DBG( debugOutput(L"Interception successful!\n"); )

    return true;
error:
    if (m_pvOriginalFunctionHeaderCopy) memoryNearFree(m_pvOriginalFunctionHeaderCopy, PAGE_SIZE);

	// Don't free replacement stub; it could be a pointer into
	// the replacement stub buffer mem we have allocated
    //if (m_pvReplacementStub)            memoryNearFree(m_pvReplacementStub, PAGE_SIZE);

    if (m_pvCallerStub)                 memoryNearFree(m_pvCallerStub, PAGE_SIZE);
    return false;
}


#endif
