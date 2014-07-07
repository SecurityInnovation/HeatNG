#pragma once

#include "common.h"
#include "siexception.h"

// anonymous namespace to prevent external linkage of the class
namespace
{
    class sistartwith
    {
        HANDLE hProcess;
        IMAGE_DOS_HEADER  m_dh;
        PIMAGE_DOS_HEADER m_pdhTarget;

        IMAGE_NT_HEADERS  m_nth;
        PIMAGE_NT_HEADERS m_pnthTarget;

        byte *m_pbIT;        unsigned long m_ulITSize;
        byte *m_pbITTarget;  unsigned long m_ulITSizeTarget;

        char *m_pszDll;

        // private methods
        void *padToPtr(void *address)
        {
            return (void*)(((DWORD)address + 7) & ~7u);
        }

        bool isImage(void* pvAddress)
        {
            // -- check for a valid DOS header
            memset(&m_dh,  0, sizeof(IMAGE_DOS_HEADER));
            m_pdhTarget = (PIMAGE_DOS_HEADER) pvAddress;

            // read in dos header from the given address
            if (!this->refreshDH())                              return false;
            // is this an executable?
            if (m_dh.e_magic != IMAGE_DOS_SIGNATURE)               return false;

            // -- check for a valid NT header
            memset(&m_nth, 0, sizeof(IMAGE_NT_HEADERS));
            m_pnthTarget = (PIMAGE_NT_HEADERS) ((byte*)pvAddress + m_dh.e_lfanew);

            // read in nt header from the given address
            if (!this->refreshNTH())                             return false;
            // is this an NT executable?
            if (m_nth.Signature != IMAGE_NT_SIGNATURE)             return false;

            // is this an executable or a dll image?
            if (m_nth.FileHeader.Characteristics & IMAGE_FILE_DLL) return false;


            return true;
        }

        void *findImage()
        {
            MEMORY_BASIC_INFORMATION mbi = {0};
            byte *pbAddress = 0;
            while (VirtualQueryEx(hProcess, (void*) pbAddress, &mbi, sizeof(mbi)))
            {
                if (mbi.State == MEM_COMMIT)
                    if (isImage((void*) pbAddress))
                        return (void*) pbAddress;
                pbAddress = (byte*) mbi.BaseAddress + mbi.RegionSize;
            }

            return 0;
        }

    public:
        sistartwith(HANDLE hProcess, const char *pszDll) : hProcess(hProcess),
                                                           m_pdhTarget(0), m_pnthTarget(0),
                                                           m_pbIT(0),       m_ulITSize(0),
                                                           m_pbITTarget(0), m_ulITSizeTarget(0)
        {
            m_pszDll = new char[strlen(pszDll) + 1];
            strcpy(m_pszDll, pszDll);

            m_pdhTarget = (PIMAGE_DOS_HEADER) findImage();
            if (!m_pdhTarget)
                throw siexception(std::wstring(L"Couldn't find an executable image in the process!"), 0, GetLastError());
#if   defined(WIN64)
            if (m_nth.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64)
                throw siexception(std::wstring(L"Incorrect machine type found on executable! Expected x64."), 0, 0);
#else
            if (m_nth.FileHeader.Machine != IMAGE_FILE_MACHINE_I386)
                throw siexception(std::wstring(L"Incorrect machine type found on executable! Expected i386."), 0, 0);
#endif

            if (!refreshIT())
                throw siexception(std::wstring(L"Couldn't find an IT for the executable image in the process!"), 0, GetLastError());

            if (!addDllToIT())
                throw siexception(std::wstring(L"Couldn't add module to the IT in the process!"), 0, GetLastError());
        }

        ~sistartwith()
        {
            if (m_pszDll) delete[] m_pszDll;
            if (m_pbIT)   delete[] m_pbIT;
        }


        bool refreshDH()
        {
            SIZE_T dwRead;
            if (!ReadProcessMemory(hProcess,  m_pdhTarget,  &m_dh, sizeof(IMAGE_DOS_HEADER), &dwRead))
                return false;

            TRACEX( L"Reading m_dh from: ", m_pdhTarget);
            return true;
        }

        bool refreshNTH()
        {
            SIZE_T dwRead;
            if (!ReadProcessMemory(hProcess, m_pnthTarget, &m_nth, sizeof(IMAGE_NT_HEADERS), &dwRead))
                return false;

            TRACEX( L"Reading m_nth from: ", m_pnthTarget);
            return true;
        }

        // make sure that both the m_nth and m_dh exist by now!
        bool refreshIT()
        {
            // sanity check;
            assert(m_pdhTarget); assert(m_pnthTarget);

            m_ulITSizeTarget = m_nth.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;
            m_pbITTarget     = (byte*) m_pdhTarget + m_nth.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;

            // means that the process doesn't have an import table; which could be a slight problem
            if ((void*)m_pbITTarget == (void*)m_pdhTarget)
            {
                m_pbITTarget = 0;
                return false;
            }

            m_ulITSize       = m_ulITSizeTarget;
            m_pbIT           = new byte[m_ulITSize];

            SIZE_T dwRead;
            if (!ReadProcessMemory(hProcess, m_pbITTarget, m_pbIT, m_ulITSizeTarget, &dwRead))
            {
                m_ulITSizeTarget = 0;
                m_pbITTarget     = 0;

                delete [] m_pbIT;
                m_ulITSize       = 0;
                m_pbIT           = 0;

                return false;
            }
            return true;
        }

        bool refreshHeaders()
        {
            if (!refreshDH())   return false;
            if (!refreshNTH())  return false;
            if (!refreshIT())  return false;

            return true;
        }

        bool dumpDH()
        {
            DWORD dwOldProtect, dwProtect;
            SIZE_T dwWritten;
            if (!VirtualProtectEx(hProcess, m_pdhTarget, sizeof(IMAGE_DOS_HEADER), PAGE_EXECUTE_READWRITE, &dwOldProtect))
                return false;
            if (!WriteProcessMemory(hProcess, m_pdhTarget, &m_dh, sizeof(IMAGE_DOS_HEADER), &dwWritten))
                return false;
            if (!VirtualProtectEx(hProcess, m_pdhTarget, sizeof(IMAGE_DOS_HEADER), dwOldProtect, &dwProtect))
                return false;
            return true;
        }

        bool dumpNTH()
        {
            DWORD dwOldProtect, dwProtect;
            SIZE_T dwWritten;
            if (!VirtualProtectEx(hProcess, m_pnthTarget, sizeof(IMAGE_NT_HEADERS), PAGE_EXECUTE_READWRITE, &dwOldProtect))
                return false;
            if (!WriteProcessMemory(hProcess, m_pnthTarget, &m_nth, sizeof(IMAGE_NT_HEADERS), &dwWritten))
                return false;
            if (!VirtualProtectEx(hProcess, m_pnthTarget, sizeof(IMAGE_NT_HEADERS), dwOldProtect, &dwProtect))
                return false;
            return true;
        }

        bool addDllToIT()
        {
            SIZE_T dwWritten;
            unsigned long ulITSizeOld  = m_ulITSize;

            // get new IT size
            m_ulITSize     += sizeof(IMAGE_IMPORT_DESCRIPTOR); m_ulITSize = (unsigned long) padToPtr((void*)m_ulITSize);

            // create a copy of the IT with enough space to hold the extra descriptor
            if (m_pbIT)
            {
                byte *pbITOld = m_pbIT;
                m_pbIT = new byte[m_ulITSize]; if (!m_pbIT) return false;
                memcpy(m_pbIT + sizeof(IMAGE_IMPORT_DESCRIPTOR), pbITOld, ulITSizeOld);
                delete pbITOld;
            }
            else
            {
                // add the size of an additional descriptor (null descriptor) to end the new table
                m_ulITSize += sizeof(IMAGE_IMPORT_DESCRIPTOR);
                m_pbIT = new byte[m_ulITSize];

                // set the last descriptor to 0
                memset(m_pbIT + sizeof(IMAGE_IMPORT_DESCRIPTOR), 0, sizeof(IMAGE_IMPORT_DESCRIPTOR));
            }

            // set permission to the old IT to exec/write; I have NO fucking
            // idea why we need to do this but shit doesn't work if we don't
            // if we even have an old IT
            if (m_pbITTarget)
            {
                DWORD dwOldProt;
                if (!VirtualProtectEx(hProcess, m_pbITTarget, m_ulITSizeTarget, PAGE_EXECUTE_READWRITE, &dwOldProt))
                {
                    printf("GLE = %d\n", GetLastError());
                    return false;
                }
            }

            // allocate space for the new IT in the target process
            m_ulITSizeTarget = m_ulITSize;
            unsigned long ulExtra = sizeof(IMAGE_THUNK_DATA) * 4; ulExtra += (unsigned long)strlen(m_pszDll) + 1;
            m_pbITTarget = (byte*) memoryAllocNearInProcess(hProcess, m_ulITSizeTarget + ulExtra, m_pdhTarget);
            if (!m_pbITTarget)
            {
                debugOutput(L"Couldn't allocate near base!");
                return false;
            }

            // Point to the space allocated for our thunk data array and dll name in the target process
            // which starts at the end of our new IAT (which is now of size m_ulITSizeTarget)
            byte *pbTarget = (byte*) m_pbITTarget + m_ulITSizeTarget;

            // create and fill out a image thunk data array (for the new import
            // descriptor thunk fields to point too + the ending zero thunks)
            IMAGE_THUNK_DATA td[4];
            td[0].u1.Ordinal = IMAGE_ORDINAL_FLAG + 1;    // thunk indicates "Ordinal 1"
            td[1].u1.Ordinal = 0;                         // zero thunk; indicating end of array
            td[2].u1.Ordinal = IMAGE_ORDINAL_FLAG + 1;    // thunk indicates "Ordinal 1"
            td[3].u1.Ordinal = 0;                         // zero thunk; indicating end of array

            

            // get location of thunks in the target process
            PIMAGE_THUNK_DATA ptdarrTarget = (PIMAGE_THUNK_DATA) pbTarget; pbTarget += (sizeof(IMAGE_THUNK_DATA) * 4);
            // write thunks to the target process
            if (!WriteProcessMemory(hProcess, ptdarrTarget, td, sizeof(IMAGE_THUNK_DATA) * 4, &dwWritten))
                return false;

            // get location of the dll name in the target process (target pointer was incremented to point to next free space)
            char *pszDllTarget = (char*) pbTarget;
            if (!WriteProcessMemory(hProcess, pszDllTarget , m_pszDll, strlen(m_pszDll) + 1, &dwWritten))
                return false;

            // set the new iid pointer to the empty space in front of the IT
            PIMAGE_IMPORT_DESCRIPTOR piid = (PIMAGE_IMPORT_DESCRIPTOR) m_pbIT;

            // fill up the new iid
            piid->OriginalFirstThunk = (DWORD) ((byte*) ptdarrTarget      - (byte*)m_pdhTarget);
            piid->FirstThunk         = (DWORD) ((byte*)(ptdarrTarget + 2) - (byte*)m_pdhTarget);
            piid->Name               = (DWORD) ((byte*) pszDllTarget      - (byte*)m_pdhTarget);
            piid->TimeDateStamp      = 0;               // leave this 0, it's of no use to us
            piid->ForwarderChain     = 0;               // no forwarder


            TRACEX( L"New IT is at: ", m_pbITTarget);

            // write piid out to the target process
            if (!WriteProcessMemory(hProcess, m_pbITTarget, m_pbIT, m_ulITSizeTarget, &dwWritten))
                return false;

            // now change the NT header to reflect the new IT address and dump it back to the target process
            m_nth.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress = (DWORD) (m_pbITTarget - (byte*)m_pdhTarget);
            m_nth.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size           = m_ulITSizeTarget;

            m_nth.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT].VirtualAddress = 0;
            m_nth.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT].Size           = 0;

            if (!dumpNTH()) return false;

            TRACEX( L"Done setting up Import Table. Base: ", this->m_pdhTarget);
            return true;
        }
    };
} // namespace

