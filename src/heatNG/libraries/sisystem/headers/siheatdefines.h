#pragma once

// Register defines (for compatibility between 32 and 64 bit)
#if   defined(WIN64)
#define EAX         Rax
#define EBX         Rbx
#define ECX         Rcx
#define EDX         Rdx
#define ESI         Rsi
#define EDI         Rdi
#define EIP         Rip
#define ESP         Rsp
#define EBP         Rbp

#define R8          R8
#define R9          R9
#define R10         R10
#define R11         R11
#define R12         R12
#define R13         R13
#define R14         R14
#define R15         R15

#elif defined(WIN32)
#define EAX         Eax
#define EBX         Ebx
#define ECX         Ecx
#define EDX         Edx
#define ESI         Esi
#define EDI         Edi
#define EIP         Eip
#define ESP         Esp
#define EBP         Ebp
#else
#error Unsupported platform!
#endif

#define RIP_MASK    0xc7            // 11000111
#define RIP_MODRM   0x05            // 00xxx101


// Instruction op-code defines

#define REX_W       0x48
#define REX_WB      0x49
#define REX_WRXB    0x4f

#define MOV_EAX_Iv  0xb8
#define MOV_EBX_Iv  0xbb
#define MOV_ECX_Iv  0xb9
#define MOV_EDX_Iv  0xba

#define MOV_ESP_Iv  0xbc
#define MOV_EBP_Iv  0xbd
#define MOV_ESI_Iv  0xbe
#define MOV_EDI_Iv  0xbf

#define MOV_R8_Iv   0xb8
#define MOV_R9_Iv   0xb9
#define MOV_R10_Iv  0xba
#define MOV_R11_Iv  0xbb
#define MOV_R12_Iv  0xbc
#define MOV_R13_Iv  0xbd
#define MOV_R14_Iv  0xbe
#define MOV_R15_Iv  0xbf

#define MOV_GvEv    0x8b
#define MOV_EvGv    0x89

#define PUSH_EAX    0x50
#define PUSH_EBX    0x53
#define PUSH_ECX    0x51
#define PUSH_EDX    0x52

#define PUSH_EBP    0x55

#define PUSH_IMM32  0x68
#define PUSH_IND_1  0xff
#define PUSH_IND_2  0x35

#define POP_EAX     0x58
#define POP_EBX     0x5b
#define POP_ECX     0x59
#define POP_EDX     0x5a

#define POP_EBP     0x5d


#define PUSHAD      0x60
#define POPAD       0x61

#define PUSHFD      0x9c
#define POPFD       0x9d


#define CALL        0xe8
#define JMP         0xe9

#define CALL_EAX1   0xff
#define CALL_EAX2   0xd0

#define CALL_EBX1   0xff
#define CALL_EBX2   0xd3

#define JMP_EAX1    0xff
#define JMP_EAX2    0xe0

#define JEb1        0x0f
#define JEb2        0x84

#define JMP_IMM8    0xeb
#define JE_IMM8     0x74
#define JNE_IMM8    0x75

#define RET         0xc3
#define RET_XX      0xc2

#define ADDSUB      0x83

#define ADD_EAX     0x05
#define SUB_EAX     0x2d


#define TEST        0x85

