// siexception header file
#pragma once

#include "common.h"
#include "siheatdefines.h"

#include "siinstrreloc.h"
#include "sithread.h"
#include "sistartwith.h"
#include "sicodegen.h"

extern DWORD g_dwTlsCheckInterceptIndex;
extern DWORD g_dwTlsRASIndex;


#if   defined(WIN64)
#define SIZE_OF_JMP 9
#elif defined(WIN32)
#define SIZE_OF_JMP 5
#else
#error Please compile for either Win32 or Win64 targets
#endif



// -- server support functions

IS void *generateFunctionHeaderCopy(void *pvFunction, void* &pvMem)
{
    if (!pvFunction) return false;

    // allocate memory for the function header
    void *pvCopy = 0;
    if (!pvMem)
    {
        pvCopy = memoryAllocExecutableNear(PAGE_SIZE, pvFunction); if (!pvCopy) return 0;
    }
    else
    {
        pvCopy = pvMem;
    }


    unsigned long ulSizeCopied = 0;

    byte *pbBase = (byte*) pvCopy;

    byte *pbDst  = (byte*) pvCopy;
    byte *pbSrc  = (byte*) pvFunction;

    do
    {
        // The reason we have the new src and destination pointers
        // is because sometimes we may move by different sizes on
        // the source and the destination (example, a near jmp when
        // copied translates to a larger jmp instruction)
        byte *pbNewSrc, *pbNewDst;
        pbNewDst = relocInstruction(pbDst, pbSrc, &pbNewSrc);
        ulSizeCopied += (unsigned long) (pbNewSrc - pbSrc);

        DBG( debugOutput(L"Copied an instruction\n"); )

        // rinse, repeat
        pbSrc = pbNewSrc;
        pbDst = pbNewDst;
    } while (ulSizeCopied < SIZE_OF_JMP);


    // place a jmp back to the original function (offset by the size
    // of the instructions we've already executed by now)
#if defined(WIN64)
    writeJump(pbDst, (byte*) pvFunction + ulSizeCopied);
#else
    writeByte(pbDst, JMP);
    sicodegen::writeDisplacement(pbDst, (byte*) pvFunction + ulSizeCopied);
#endif

    if (pvMem)
        pvMem = (void*) ((byte*) pvMem + (pbDst - pbBase));
    
    // copy done; return the destination pointer
    return pvCopy;
}




IS void *generateReplacementStub(void *pvOriginalFunction, void *pvReplacementFunction, void* &pvMem) // protective reentrance
{
    if (!pvOriginalFunction || !pvReplacementFunction) return false;
    
    // allocate memory for the replacement stub
    void *pvStub = 0;
    if (!pvMem)
    {
        pvStub = memoryAllocExecutableNear(PAGE_SIZE, pvOriginalFunction); if (!pvStub) return 0;
    }
    else
    {
        pvStub = pvMem;
    }


    byte *pbCode = (byte*) pvStub;
    sicodegen code(pbCode, g_dwTlsCheckInterceptIndex, g_dwTlsRASIndex);

    // breakpoint
//    code.writeByte(0xcc);

    // save the registers we might use and the LastError
    byte bOffsetToRA = code.writeSaveRegsAndLastError();

    // get TLS value to check if we're intercepting (returns in eax)
    code.writeGetInIntercept();

    // check if we should intercept or not [testing eax (even in 64b) is enough, since any >0 value will show in the lower bits first]
    code.writeByte(TEST); code.writeByte(0xc0);               // test eax, eax

    // if zero (JE=JZ), then continue onto intercept func
    code.writeByte(JE_IMM8);
    byte* pbJmpToIntercept = code.getCurrentEIP();
    // write just a placeholder here, we'll update this once we have the next set of code in place once we calculate the required offset
    code.writeByte(0);

    // restore last error + registers, clear old RA from stack and call replacement
    code.writeRestoreRegsAndLastError();

    // with the stack clear, last error restored, go ahead and jump to the original function
    code.writeByte(JMP);
    code.writeDisplacement(pvOriginalFunction);

    // that's it, our exit to real function code is over;
    // ---- pick up the jmp to continue intercepts marker and update it
    writeByte(pbJmpToIntercept, (byte)(code.getCurrentEIP() - pbJmpToIntercept - 1));

// intercept:
    // mark is intercepting before proceeding
    code.writeSetInIntercept((void*) 1);

    // save the return address (from 12 bytes into the stack)
    code.writePushRAtoRAS(bOffsetToRA);


    // Now restore last error + registers, clear old RA from stack and call replacement
    code.writeRestoreRegsAndLastError();

    // remove old RA from the stack
    code.writeAddEsp(sizeof(void*));

    code.writeCall(pvReplacementFunction);

    // we're back! now, restore the old return address from the RAS, clear out is intercepting flag, and return

    // first, make an extra space on the stack to store back the original return address
    code.writeSubEsp(sizeof(void*));

    // save our registers + last error again
    bOffsetToRA = code.writeSaveRegsAndLastError();

    // get old return address from the RAS (store RA to 12 bytes off the stack
    code.writePopRAfromRAS(bOffsetToRA);

    // now clear out is intercepting flag
    code.writeSetInIntercept((void*) 0);

    // -- complete restore time, last error and used registers

    // restore our registers and last error again
    code.writeRestoreRegsAndLastError();

    // ret out to the original return address, which should now be in that space we allocated for it
    code.writeByte(RET);

    // return the function stub
    if (pvMem)
        pvMem = (void*) ((byte*) pvMem + code.getCodeSize());
    return pvStub;
}


IS void *generateCallerStub(void *pvOriginalFunctionHeaderCopy) // full reentrance
{
    if (!pvOriginalFunctionHeaderCopy) return false;
    // allocate safe size (adjust if this if the stub gets too big)
    void *pvStub = memoryAllocExecutableNear(PAGE_SIZE, pvOriginalFunctionHeaderCopy); if (!pvStub) return 0;

    byte *pbCode = (byte*) pvStub;
    sicodegen code(pbCode, g_dwTlsCheckInterceptIndex, g_dwTlsRASIndex);

    // breakpoint
//    code.writeByte(0xcc);

    // save the registers we might use and the LastError
    byte bOffsetToRA = code.writeSaveRegsAndLastError();

    // save the return address to the RAS
    code.writePushRAtoRAS(bOffsetToRA);

    // allow intercepts from within original function which'll now be called
    code.writeSetInIntercept(0);

    // restore last error + registers, clear old RA from stack and call the original function
    code.writeRestoreRegsAndLastError();

    // remove old RA from the stack
    code.writeAddEsp(sizeof(void*));

    // call the original function (header copy)
    code.writeCall(pvOriginalFunctionHeaderCopy);

    // we're back! now, restore the old return address from the RAS, clear out is intercepting flag, and return

    // first, make an extra space on the stack to store back the original return address
    code.writeSubEsp(sizeof(void*));

    // save our registers + last error again
    bOffsetToRA = code.writeSaveRegsAndLastError();

    // get old return address from the RAS (store RA to 12 bytes off the stack
    code.writePopRAfromRAS(bOffsetToRA);

    // disable intercepts again for the rest of the replacement code
    code.writeSetInIntercept((void*) 1);

    // -- complete restore time, last error and used registers
    code.writeRestoreRegsAndLastError();

    // ret out to the original return address, which should now be in that space we allocated for it
    code.writeByte(RET);

    return pvStub;
}

IS bool writeRedirection(void *pvOriginalFunction, void *pvReplacementStub)
{
    byte *pbOriginalFunction = (byte*) pvOriginalFunction;

    long lOldProtection = enablePageExecute(pvOriginalFunction);
    if (lOldProtection < 0) return false;

    writeByte(pbOriginalFunction, JMP);
    sicodegen::writeDisplacement(pbOriginalFunction, pvReplacementStub);

    restorePageProtection(pvOriginalFunction, lOldProtection);
    return true;
}



// -- Other low level system routines

IS BOOL CALLBACK GetFirstWindowEnumProc(HWND hWnd, LPARAM pRet)
{
	*((HWND*)pRet) = hWnd;
	return FALSE;
}


typedef void (WINAPI *VOIDFUNCTION)();

static VOID CALLBACK APCProc(ULONG_PTR pulParam)
{
    ((VOIDFUNCTION)pulParam)();
}


IS bool runInThread(unsigned long ulThreadId, void *function)
{
    TRACE( L"RUN_IN_THREAD: Running function in thread: ", ulThreadId )
    // in called for the current thread, just run the function
    if (GetCurrentThreadId() == ulThreadId)
    {
        DBG( debugOutput(L"RUN_IN_THREAD: Running function in current thread!"); )
        VOIDFUNCTION f = (VOIDFUNCTION) function;
        f();
        return true;
    }

    HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, false, ulThreadId);
    if (hThread == INVALID_HANDLE_VALUE)
        return false;

    QueueUserAPC(APCProc, hThread, (ULONG_PTR) function);
    CloseHandle(hThread);
    return true;


/*    // Get thread's context
    CONTEXT cxt = {0}; cxt.ContextFlags = CONTEXT_FULL;
    if (!GetThreadContext(hThread, &cxt)) goto error;

    // Create calling stub
    void *pvStub = memoryAllocExecutableNear(PAGE_SIZE, (void*)cxt.EIP); if (!pvStub) goto error;
    byte *pbStub = (byte*) pvStub;
    sicodegen code(pbStub, g_dwTlsCheckInterceptIndex, g_dwTlsRASIndex);

    { wchar_t msg[100]; wsprintf(msg, L"Address of RIT code is      0x%p\n", pvStub);     debugOutput(msg); }
    { wchar_t msg[100]; wsprintf(msg, L"Address of function code is 0x%p\n", function);   debugOutput(msg); }
    // breakpoint
//    code.writeByte(0xcc);

    // save state
#ifndef WIN64
    code.writeByte(PUSHAD);
#endif
    code.writeByte(PUSHFD);
    
    // save last error
    code.writeSaveLastError();

    // call the function to execute
    code.writeCall(function);

    // when thread is resumed, restore state and jump back

    // restore last error from LE stored on the top of the stack
    code.writeRestoreLastError();

    // restore registers
    code.writeByte(POPFD);
#ifndef WIN64
    code.writeByte(POPAD);
#else
    code.writeByte(REX_W); code.writeByte(MOV_EAX_Iv); code.writeQWord(cxt.EAX);
    code.writeByte(REX_W); code.writeByte(MOV_EBX_Iv); code.writeQWord(cxt.EBX);
    code.writeByte(REX_W); code.writeByte(MOV_ECX_Iv); code.writeQWord(cxt.ECX);
    code.writeByte(REX_W); code.writeByte(MOV_EDX_Iv); code.writeQWord(cxt.EDX);
    code.writeByte(REX_W); code.writeByte(MOV_ESI_Iv); code.writeQWord(cxt.ESI);
    code.writeByte(REX_W); code.writeByte(MOV_EDI_Iv); code.writeQWord(cxt.EDI);
    code.writeByte(REX_W); code.writeByte(MOV_EBP_Iv); code.writeQWord(cxt.EBP);
    code.writeByte(REX_W); code.writeByte(MOV_ESP_Iv); code.writeQWord(cxt.ESP);

    code.writeByte(REX_WB); code.writeByte(MOV_R8_Iv);  code.writeQWord(cxt.R8);
    code.writeByte(REX_WB); code.writeByte(MOV_R9_Iv);  code.writeQWord(cxt.R9);
    code.writeByte(REX_WB); code.writeByte(MOV_R10_Iv); code.writeQWord(cxt.R10);
    code.writeByte(REX_WB); code.writeByte(MOV_R11_Iv); code.writeQWord(cxt.R11);
    code.writeByte(REX_WB); code.writeByte(MOV_R12_Iv); code.writeQWord(cxt.R12);
    code.writeByte(REX_WB); code.writeByte(MOV_R13_Iv); code.writeQWord(cxt.R13);
    code.writeByte(REX_WB); code.writeByte(MOV_R14_Iv); code.writeQWord(cxt.R14);
    code.writeByte(REX_WB); code.writeByte(MOV_R15_Iv); code.writeQWord(cxt.R15);
#endif

    code.writeByte(JMP);
    code.writeDisplacement((void*) cxt.EIP);

    // now the stub exists, pause the thread and change it's context
    SuspendThread(hThread);
    DBG( debugOutput(L"RUN_IN_THREAD: Suspended thread.."); )

    // Thread is set to execute our stub now
#if   defined(WIN64)
    cxt.EIP = (DWORD64) pvStub;
#elif defined(WIN32)
    cxt.EIP = (DWORD) pvStub;
#else
#error Unsupported platform
#endif
    if (!SetThreadContext(hThread, &cxt)) goto error;

    // resumeThread to get the thread moving again
    ResumeThread(hThread);
    DBG( debugOutput(L"RUN_IN_THREAD: Resumed thread.."); )

    CloseHandle(hThread);

    // can't free the memory for the stub; that has to stay
    // this is an unfortunate side effect but if we deallocate it
    // our recovery code is lost too
    return true;

error:
    DBG( debugOutput(L"RUN_IN_THREAD: Error in runInThread!"); )
    if (hThread != INVALID_HANDLE_VALUE) CloseHandle(hThread);
    if (pvStub) memoryFree(pvStub);
    return false;
    */
}

/*
IS bool runInThreadNow(unsigned long ulThreadId, void *function)
{
    // in called for the current thread, just run the function
    if (GetCurrentThreadId() == ulThreadId)
    {
        DBG( debugOutput(L"Running function in current thread!"); )
        VOIDFUNCTION f = (VOIDFUNCTION) function;
        f();
        return true;
    }

    HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, false, ulThreadId);
    if (hThread == INVALID_HANDLE_VALUE)
        return false;

    wchar_t pwszFinished[100]; wsprintf(pwszFinished, L"threadfinished%d", ulThreadId);
    HANDLE hFinished = CreateEvent(NULL, FALSE, FALSE, pwszFinished);

    // Get thread's context
    CONTEXT cxt = {0}; cxt.ContextFlags = CONTEXT_FULL;
    if (!GetThreadContext(hThread, &cxt)) goto error;

    // Create calling stub
    void *pvStub = memoryAllocExecutable(256);
    byte *pbStub = (byte*) pvStub;

    // save state
    writeByte(pbStub, PUSHAD);
    writeByte(pbStub, PUSHFD);
    
    // save last error
    writeSaveLastError(pbStub);

    // call the function to execute
    writeByte(pbStub, CALL);
    writeDisplacement(pbStub, function);

    // after we're done, signal the caller and suspend the thread

    // we're in the same process, so just go ahead and signal the event
    writeByte(pbStub, PUSH_IMM32);
    writeDWord(pbStub, (DWORD) hFinished);

    writeByte(pbStub, CALL);
    writeDisplacement(pbStub, SetEvent);

    // suspend the thread
    writeByte(pbStub, PUSH_IMM32);
   writeDWord(pbStub, (DWORD) hThread);

    writeByte(pbStub, CALL);
    writeDisplacement(pbStub, SuspendThread);

    // when thread is resumed, restore state and jump back

    // restore last error from LE stored on the top of the stack
    writeRestoreLastError(pbStub);

    // restore registers
    writeByte(pbStub, POPFD);
    writeByte(pbStub, POPAD);

    writeByte(pbStub, JMP);
    writeDisplacement(pbStub, (void*) cxt.EIP);


    // now the stub exists, pause the thread and change it's context
    SuspendThread(hThread);
    DBG( debugOutput(L"Suspended thread.."); )

    // Thread is set to execute our stub now
    cxt.EIP = (DWORD) pvStub;
    if (!SetThreadContext(hThread, &cxt)) goto error;

    // resume thread, keeping a track of the resume count
    DWORD dwSuspendCount = 0;
    while (ResumeThread(hThread) > 1)
        dwSuspendCount++;

    // Send a message to a window in the thread so it won't get stuck waiting for a message
    HWND hWnd = NULL;
    if (!EnumThreadWindows(ulThreadId, GetFirstWindowEnumProc, (LPARAM)&hWnd)) goto error;
    if (hWnd)
        PostMessage(hWnd, WM_PAINT, NULL, NULL);

    // wait for thread to get finished
    if (WaitForSingleObject(hFinished, SI_WAIT_TIMEOUT) == WAIT_FAILED) goto error;

    // give it 100ms to make sure that set event finishes and the thread suspends again
    Sleep(100);

    // suspend the thread again (one less suspend since we manually supended the thread once ourselves)
    for (; dwSuspendCount > 1; dwSuspendCount--)
        SuspendThread(hThread);

    // one final resume thread; to get the thread moving from where we suspended it
    ResumeThread(hThread);
    DBG( debugOutput(L"Resumed thread.."); )

    // deallocate all that we're done with
    CloseHandle(hFinished);
    CloseHandle(hThread);

    // can't free the memory for the stub; that has to stay
    // this is an unfortunate side effect but if we deallocate it
    // our recovery code is lost too
    return true;

error:
    DBG( debugOutput(L"Error in runInThread!"); )
    if (hFinished) CloseHandle(hFinished);
    if (hThread != INVALID_HANDLE_VALUE) CloseHandle(hThread);
    if (pvStub) memoryFree(pvStub);
    return false;
}
  */

