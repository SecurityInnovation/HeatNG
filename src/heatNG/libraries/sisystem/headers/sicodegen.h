#pragma once

#include "sisystem.h"
#include "siheatdefines.h"
#include "siinstrreloc.h"

void *allocRASInCurrentThread();


namespace
{


    class sicodegen
    {
#if   defined(WIN64)
        static const bool m_x64 = true;
#elif defined(WIN32)
        static const bool m_x64 = false;
#else
#error Platform unsupported!
#endif

        byte *m_pbCodeBase;       // pointer to the base of the code
        byte *m_pbCode;           // pointer to current m_llOffset in code

        DWORD m_dwTlsCheckInterceptIndex;
        DWORD m_dwTlsRASIndex;

        long long m_llOffset;       // used in case target code baseis different from our code base
                                // (typically used if the final code is in another process)


    public:
        sicodegen(byte *pbCodeBase, DWORD dwTlsCheckInterceptIndex, DWORD dwTlsRASIndex, long long llOffset = 0) : \
                                                                                                                   \
                  m_pbCodeBase(pbCodeBase), m_pbCode(pbCodeBase),                                                  \
                  m_dwTlsCheckInterceptIndex(dwTlsCheckInterceptIndex), m_dwTlsRASIndex(dwTlsRASIndex),            \
                  m_llOffset(llOffset)
        {
        }

        // accessors
        unsigned long getCodeSize()     { return (unsigned long)(m_pbCode - m_pbCodeBase); }
        byte*         getCurrentEIP()   { return m_pbCode; }

        // use these for manual intervention
        void writeByte (byte b)                  { ::writeByte (m_pbCode, b);    }
        void writeWord (unsigned short w)        { ::writeWord (m_pbCode, w);    }
        void writeDWord(unsigned long dw)        { ::writeDWord(m_pbCode, dw);   }
        void writeQWord(unsigned long long qw)   { ::writeQWord(m_pbCode, qw);   }

        // instruction creation helper code
        void writeAddEsp(byte bHowMuch)
        {
 if (m_x64) writeByte(REX_W);    // make the following reference 64 bit and with rex registers
            writeByte(ADDSUB); writeByte(0xc4);        // add esp, imm8
                               writeByte(bHowMuch);    // imm8 = bHowMuch
        }

        void writeSubEsp(byte bHowMuch)
        {
 if (m_x64) writeByte(REX_W);    // make the following reference 64 bit and with rex registers
            writeByte(ADDSUB); writeByte(0xec);        // sub esp, imm8
                               writeByte(bHowMuch);    // imm8 = bHowMuch
        }

        // call helper code
        
        // this needs to be static because we need access to it outside this class too
        static void writeDisplacement(byte *&pbCode, void *pvDst, long long m_llOffset = 0)
        {
            // displacement will always be from the "end" of the instruction (i.e., where our written displacement ends)
            byte *pbRealCode = m_x64 ? pbCode + m_llOffset : pbCode + (long) m_llOffset;
            ::writeDWord(pbCode, (DWORD) ((byte*)pvDst - (pbRealCode + 4)));
        }


        void writeDisplacement(void *pvDst)
        {
            writeDisplacement(m_pbCode, pvDst, m_llOffset);
        }

        // if ulParamSize = 0; that means that our param is in the EAX/RAX
        void writePushParam(unsigned long long ullParamValue, unsigned long ulParamSize, unsigned long ulPosition = 0)
        {
            if (m_x64)
            {
                assert(ulParamSize == 0 || ulParamSize == 1 || ulParamSize == 2 || ulParamSize == 4 || ulParamSize == 8);
                if (ulParamSize)
                {
                    if (ulParamSize == 8)
                        this->writeByte(REX_W);

                    switch (ulPosition)
                    {
                    case 1:
                        writeByte(MOV_ECX_Iv);
                        break;
                    case 2:
                        writeByte(MOV_EDX_Iv);
                        break;
                    case 3:
                        writeByte(MOV_R8_Iv);
                        break;
                    case 4:
                        writeByte(MOV_R9_Iv);
                        break;
                    default:
                        writeByte(PUSH_IMM32);
                    }
                    if (ulParamSize != 8 || ulPosition == 0)
                        writeDWord((DWORD) ullParamValue);
                    else
                        writeQWord(ullParamValue);
                }
                else
                {
                    
                    switch (ulPosition)
                    {
                        case 1:
                            writeByte(REX_W);
                            writeByte(0x8b); writeByte(0xc8);   // mov rcx, rax
                            break;
                        case 2:
                            writeByte(REX_W);
                            writeByte(0x8b); writeByte(0xd0);   // mov rdx, rax
                            break;
                        case 3:
                            writeByte(REX_WB);
                            writeByte(0x89); writeByte(0xc0);   // mov r8, rax
                            break;
                        case 4:
                            writeByte(REX_WB);
                            writeByte(0x89); writeByte(0xc1);   // mov r9, rax
                            break;
                        default:
                            writeByte(PUSH_EAX);
                    }
                }
            }
            else
            {
                assert(ulParamSize == 0 || ulParamSize == 1 || ulParamSize == 2 || ulParamSize == 4);
                if (ulParamSize)
                {
                    writeByte(PUSH_IMM32);
                   writeDWord((DWORD) ullParamValue);
                }
                else
                {
                    writeByte(PUSH_EAX);
                }
            }
        }

        void writeCall(void *pvDst)
        {
            if (m_x64)
            {
                long long llDistance = (byte*)pvDst - (m_pbCode + 5); // can be 6, but to be on the safe side, we pick the higher distance

                if (_abs64(llDistance) < 0x7fffffffull)             // again, take margin of 1 here too
                {
                    writeByte(CALL);
                   writeDisplacement(pvDst);
                }
                else
                {
                    // fuck around with the stack to make this code "somehow" make a call

                    // first make space for a.) a return address b.) a target address (the ret trick is the only way to make a 64bit jump)
                  writeSubEsp(8 + 8);
                    
                    // how save our ebx for later restoration
                    writeByte(PUSH_EBX);

                    // bring back our stack pointer to where it was before we made space
                  writeAddEsp(8 + 8 + 8);

                    // push on return address
                    writeByte(REX_W);
                    writeByte(MOV_EBX_Iv);

                    byte *pbReturnAddress = this->m_pbCode;    // mark this spot; we need to put the final return address here
                   writeQWord(0);      // fill this in later

                    writeByte(PUSH_EBX);

                    // push on target address
                    writeByte(REX_W);
                    writeByte(MOV_EBX_Iv);
                   writeQWord((DWORD64) pvDst);
                    writeByte(PUSH_EBX);

                    // okay, time to get back the RBX; move esp down and then just pop the ebx off from where we stored it
                  writeSubEsp(8);
                    writeByte(POP_EBX);

                    // we're all set, go ahead and ret to the target
                    writeByte(RET);

                    // now our instruction pointer is where we should be coming back to; update the the return address we wrote
                 ::writeQWord(pbReturnAddress, (DWORD64) this->m_pbCode);
                }
            }
            else
            {
                writeByte(CALL);
               writeDisplacement(pvDst);
            }
        }

        // misc instruction sequences
        void writeGetInIntercept()
        {
            writePushParam (m_dwTlsCheckInterceptIndex, sizeof(m_dwTlsCheckInterceptIndex), 1);
            writeCall      ((void*) TlsGetValue);
        }

        void writeSetInIntercept(void *value)
        {
            writePushParam ((DWORD64)value, sizeof(value), 2);
            writePushParam (m_dwTlsCheckInterceptIndex, sizeof(m_dwTlsCheckInterceptIndex), 1);
            writeCall      ((void*) TlsSetValue);
        }

        void writeGetRAS()
        {
            // Get our return address stack (RAS)
            writePushParam (m_dwTlsRASIndex, sizeof(m_dwTlsRASIndex), 1);
            writeCall      ((void*) TlsGetValue);
        }

        void writeSetRAS()
        {
            // pick up RAS address from eax/rax
            writePushParam (0, 0, 2);
            writePushParam (m_dwTlsRASIndex, sizeof(m_dwTlsRASIndex), 1);
            writeCall      ((void*) TlsSetValue);
        }

        // Destroys EAX, EDX and possibly LastError
        void writePushRAtoRAS(byte bOffsetInStack)
        {
            // RAS address will arrive in eax/rax
            writeGetRAS();

            // check if RAS exists[testing eax (even in 64b) is enough, since any >0 value will show in the lower bits first]
            writeByte(TEST); writeByte(0xc0);               // test eax, eax

            // if not zero (JNE=JNZ), then continue onto intercept func
            writeByte(JNE_IMM8);
            byte* pbJumpToContinue = this->m_pbCode;
            // leave an empty space here, we'll update this once we have the next set of code in place once we calculate the required m_llOffset
            m_pbCode++;

            // else, call the initRAS function here
            writeCall((void*)allocRASInCurrentThread);
            // okay, now that we have a brand new RAS; it's location is stored in rax

            ::writeByte(pbJumpToContinue, (byte)(m_pbCode - (pbJumpToContinue + 1)));


            // save the return address to the ras
 if (m_x64) writeByte(REX_W);    // make the following reference 64 bit and with rex registers
            writeByte(MOV_GvEv); writeByte(0x54);           // mov edx, 
                                 writeByte(0x24);           // mov edx, dword ptr[esp + imm8]
                                 writeByte(bOffsetInStack);
            
 if (m_x64) writeByte(REX_W);    // make the following reference 64 bit and with rex registers
            writeByte(MOV_EvGv); writeByte(0x10);           // mov [eax], edx


            // increment RAS pointer
 if (m_x64) writeByte(REX_W);    // make the following reference 64 bit and with rex registers
            writeByte(ADD_EAX); 
           writeDWord(sizeof(void*));

            // save the RAS back to TLS
            writeSetRAS();
        }

        // Destroys EAX, EDX and possibly LastError
        void writePopRAfromRAS(byte bOffsetInStack)
        {
            // Get our return address stack (RAS) in eax
            writeGetRAS();

            // decrement the RAS pointer
 if (m_x64) writeByte(REX_W);    // make the following reference 64 bit and with rex registers
            writeByte(SUB_EAX); 
           writeDWord(sizeof(void*));


            // get the return address from the top of the RAS
 if (m_x64) writeByte(REX_W);    // make the following reference 64 bit and with rex registers
            writeByte(MOV_GvEv); writeByte(0x10);           // mov edx, dword ptr[eax]

 if (m_x64) writeByte(REX_W);    // make the following reference 64 bit and with rex registers
            writeByte(MOV_EvGv); writeByte(0x54);           // mov [esp + imm8], edx
                                 writeByte(0x24);
                                 writeByte(bOffsetInStack);

            // write back RAS to TLS
            writeSetRAS();
        }

        // returns stack space used
        byte writeSaveRegsAndLastError()
        {
            byte bStackUsed = 0;
            if (m_x64)
            {
                // a lot more registers need to be saved for x64 since called
                // functions may wipe out the parameter registers
                                   writeByte(PUSH_EAX); bStackUsed += 8;
                                   writeByte(PUSH_ECX); bStackUsed += 8;
                                   writeByte(PUSH_EDX); bStackUsed += 8;
                                   writeByte(PUSH_EBP); bStackUsed += 8;
                writeByte(REX_WB); writeByte(PUSH_EAX); bStackUsed += 8; // pushes R8
                writeByte(REX_WB); writeByte(PUSH_ECX); bStackUsed += 8; // pushes R9
                writeByte(REX_WB); writeByte(PUSH_EDX); bStackUsed += 8; // pushes R10
                writeByte(REX_WB); writeByte(PUSH_EBX); bStackUsed += 8; // pushes R11

                // get and save last error
                writeCall((void*) GetLastError);
                writeByte(PUSH_EAX); bStackUsed += 8;
            }
            else
            {
                writeByte(PUSH_EAX); bStackUsed += 4;
                writeByte(PUSH_EDX); bStackUsed += 4;
                writeCall((void*) GetLastError);
                writeByte(PUSH_EAX); bStackUsed += 4;
            }

            return bStackUsed;
        }

        // returns stack space freed
        byte writeRestoreRegsAndLastError()
        {
            byte bStackFreed = 0;
            if (m_x64)
            {
                // on x64, the stack will NOT be used for the first param;
                // so we get and clean it off the stack ourselves
                writeByte(POP_EAX); bStackFreed += 8;
                writePushParam(0, 0, 1);
                writeCall((void*) SetLastError);

                writeByte(REX_WB); writeByte(POP_EBX); bStackFreed += 8;// pops R11
                writeByte(REX_WB); writeByte(POP_EDX); bStackFreed += 8;// pops R10
                writeByte(REX_WB); writeByte(POP_ECX); bStackFreed += 8;// pops R9
                writeByte(REX_WB); writeByte(POP_EAX); bStackFreed += 8;// pops R8
                                   writeByte(POP_EBP); bStackFreed += 8;
                                   writeByte(POP_EDX); bStackFreed += 8;
                                   writeByte(POP_ECX); bStackFreed += 8;
                                   writeByte(POP_EAX); bStackFreed += 8;
            }
            else
            {
                // this call will clear the extra EAX from the stack
                writeCall((void*) SetLastError); bStackFreed += 4;

                writeByte(POP_EDX); bStackFreed += 4;
                writeByte(POP_EAX); bStackFreed += 4;
            }

            return bStackFreed;
        }
    };
}

