#pragma once

#include <windows.h>

#include "sisystem.h"
#include "siheatdefines.h"
#include "instrtable64.h"

#define TWO_BYTE_OPCODE         0x0f
#define ADDRESS_SIZE_OPCODE     0x67
#define CALL_NEAR_OPCODE        0xe8
#define JUMP_NEAR_OPCODE        0xe9
#define JUMP_SHORT_OPCODE       0xeb
#define JUMP_COND_NEAR_OPCODE   0x80
#define JCXZ_OPCODE             0xe3
#define LOOP_OPCODE             0xe2
#define LOOPZ_OPCODE            0xe1
#define LOOPNZ_OPCODE           0xe0

IS BYTE readByte(PBYTE& src)
{
	return *(src++);
}

IS WORD readWord(PBYTE& src)
{
	WORD val=(WORD)readByte(src);
	val|=((WORD)readByte(src))<<8;
	return val;
}

IS DWORD readDWord(PBYTE& src)
{
	DWORD val=(DWORD)readWord(src);
	val|=((DWORD)readWord(src))<<16;
	return val;
}

IS void writeByte(PBYTE& dest,BYTE n)
{
	*(dest++)=n;
}

IS void writeWord(PBYTE& dest,WORD n)
{
	writeByte(dest,n&0xff);
	writeByte(dest,(n>>8)&0xff);
}

IS void writeDWord(PBYTE& dest,DWORD n)
{
	writeWord(dest,(WORD)(n&0xffff));
	writeWord(dest,(WORD)((n>>16)&0xffff));
}

IS void writeQWord(PBYTE& dest,DWORD64 n)
{
	writeDWord(dest,(DWORD)(n & 0xfffffffful));
	writeDWord(dest,(DWORD)((n>>32) & 0xfffffffful));
}

IS void copyByte(PBYTE& dest, PBYTE& src)
{
	writeByte(dest,readByte(src));
}

IS void copyWord(PBYTE& dest, PBYTE& src)
{
	writeWord(dest,readWord(src));
}

IS void copyDWord(PBYTE& dest, PBYTE& src)
{
	writeDWord(dest,readDWord(src));
}

// Handler to relocate near jump instructions
void JumpNear(PBYTE& dest, PBYTE& src, int addrSize);

// Handler to relocate short jump instructions
void JumpShort(PBYTE& dest, PBYTE& src, int addrSize);

// Handler to relocate conditional near jump instructions
void JumpCondNear(PBYTE& dest, PBYTE& src, int addrSize);

// Handler to relocate conditional short jump instructions
void JumpCondShort(PBYTE& dest, PBYTE& src, int addrSize);

// Handler to relocate near call instructions
void CallNear(PBYTE& dest, PBYTE& src, int addrSize);

FI void LoopingInstructionRelocate(PBYTE& dest, PBYTE& src, int addrSize, int opcode);

// Handler to relocate JCXZ instructions
void JCXZShort(PBYTE& dest, PBYTE& src, int addrSize);

// Handler to relocate LOOP instructions
void LoopShort(PBYTE& dest, PBYTE& src, int addrSize);

// Handler to relocate LOOPZ instructions
void LoopZShort(PBYTE& dest, PBYTE& src, int addrSize);

// Handler to relocate LOOPNZ instructions
void LoopNZShort(PBYTE& dest, PBYTE& src, int addrSize);

IS void copyOpSize(PBYTE& dest, PBYTE& src, int opSize)
{
	copyWord(dest,src);
    if (opSize > 16)
	    copyWord(dest,src);
    if (opSize > 32)
		copyDWord(dest,src);
}



IS void writeJump(byte *&pbDst, byte *pbTarget, bool isRIPAddress = false)
{
    if (isRIPAddress)  // we're using RIP relative addressing; use the target as a pointer, not a target
        pbTarget = *((byte**)pbTarget);

    long long ullDistance = pbTarget - (pbDst + 5);
    if (ullDistance < 127 && ullDistance > -128)           // then, use a short jump
    {
        writeByte(pbDst, JMP_IMM8);
        writeByte(pbDst, (byte) ullDistance);
    }
    else if (ullDistance < 0x7fffff && ullDistance > 0x80000000) // then, use a near jump
    {
        writeByte(pbDst, JMP);
       writeDWord(pbDst, (DWORD) ((long)ullDistance));
    }
    else                             // else, we're too far; use the ret trick
    {
        // first, make an extra space on the stack to store the address to jump to
        writeByte(pbDst, REX_W);
        writeByte(pbDst, ADDSUB); writeByte(pbDst, 0xec);             // sub rsp, imm8
                                  writeByte(pbDst, sizeof(void*));    // imm8 = sizeof an address/rexregister

        // save old RAX "after" that new space
        writeByte(pbDst, PUSH_EAX);    // save RAX

        // move target into RAX
        writeByte(pbDst, REX_W);
        writeByte(pbDst, MOV_EAX_Iv);
       writeQWord(pbDst, (DWORD64) pbTarget);

        // now move RSP back to where we had made that space
        writeByte(pbDst, REX_W);
        writeByte(pbDst, ADDSUB); writeByte(pbDst, 0xc4);                 // add rsp, imm8
                                  writeByte(pbDst, sizeof(void*) * 2);    // imm8 = (sizeof an address/rexregister) * 2
                                                                          // (one more to adjust for the push moving us forward)

        // push on the target into that space
        writeByte(pbDst, PUSH_EAX);

        // move RSP back to where our old RAX is stored
        writeByte(pbDst, REX_W);
        writeByte(pbDst, ADDSUB); writeByte(pbDst, 0xec);             // sub rsp, imm8
                                  writeByte(pbDst, sizeof(void*));    // imm8 = sizeof an address/rexregister

        // restore our old RAX; RSP will now point to where the address we need to jump to is stored
        writeByte(pbDst, POP_EAX);

        // use ret to jump to our target
        writeByte(pbDst, RET);
    }
}


IS void writeJumpConditional(byte *&pbDst, byte *pbTarget, unsigned long ulConditionCode)
{
    unsigned long long ullDistance = pbTarget - (pbDst + 6);    // since we have a two byte opcode
    if (ullDistance < 0x7fffff && ullDistance > 0x80000000) // then, use a near jump
    {
	    writeByte(pbDst, JEb1);
	    writeByte(pbDst, (byte)(JUMP_COND_NEAR_OPCODE | ulConditionCode));
       writeDWord(pbDst, (DWORD) ((long)ullDistance));
    }
    else                             // else, we're too far; use the ret trick
    {
	    writeByte(pbDst, JEb1);
	    writeByte(pbDst, (byte)(JUMP_COND_NEAR_OPCODE | ulConditionCode));
       writeDWord(pbDst, 5);        // jump to +5 from instruction start (where our real jump to target code is)
        
        writeByte(pbDst, JMP);

        byte *pbIfNotOffset = pbDst;
       writeDWord(pbDst, 0);        // write a placeholder 0 here, we'll update this later;

        writeJump(pbDst, pbTarget); // (if condition true) write code to jump to our target

        // update the jump earlier to jump to here, "after" this jump code to our target
        writeDWord(pbIfNotOffset, (DWORD)(pbDst - (pbIfNotOffset+4)));
    }
}


// Handler to relocate near jump instructions
IS void JumpNear(PBYTE& dest, PBYTE& src, int addrSize)
{
    byte *pbTarget = src + (long)readDWord(src);
    writeJump(dest, pbTarget);
}

// Handler to relocate short jump instructions
IS void JumpShort(PBYTE& dest, PBYTE& src, int addrSize)
{
    byte *pbTarget = src + (long)readByte(src);
    writeJump(dest, pbTarget);
}

// Handler to relocate conditional near jump instructions
IS void JumpCondNear(PBYTE& dest, PBYTE& src, int addrSize)
{
	int conditionCode=(*(src-1))&0xf;
    byte *pbTarget = src + (long)readDWord(src);

    writeJumpConditional(dest, pbTarget, conditionCode);
}

// Handler to relocate conditional short jump instructions
IS void JumpCondShort(PBYTE& dest, PBYTE& src, int addrSize)
{
	int conditionCode=(*(src-1))&0xf;
    byte *pbTarget = src + (long)readByte(src);

    writeJumpConditional(dest, pbTarget, conditionCode);
}

// Handler to relocate near call instructions
IS void CallNear(PBYTE& dest, PBYTE& src, int addrSize)
{
	unsigned long long offset = (long)readDWord(src);
    // if we're withing a 2gig limit; use a near call
    
    
    if (_abs64((long long)(src + offset) - (long long)dest) <= 0x7fffffff)
    {
	    PBYTE destAddr=src+offset;
	    // Output call near instruction
	    if (addrSize==16) writeByte(dest,ADDRESS_SIZE_OPCODE);
	    writeByte(dest, CALL);
	    writeDWord(dest, (DWORD)(destAddr-(dest+4)));
    }
    // else we need to make a 2gb+ branch, which can only be an indirect branch
    else
    {
        byte *pbTarget = src + offset;
        writeByte(dest, PUSH_EBX);

        writeByte(dest, REX_W);
        writeByte(dest, MOV_EBX_Iv);
       writeQWord(dest, (DWORD64) pbTarget);

        writeByte(dest, CALL_EBX1);
        writeByte(dest, CALL_EBX2);

        writeByte(dest, POP_EBX);
    }
}


// Handler to relocate a relative to RIP call instruction
IS void JumpRIP(PBYTE& dest, PBYTE& src, int addrSize)
{
    byte *pbTarget = src + (long)readDWord(src);
    writeJump(dest, pbTarget, true);
}


IS void LoopingInstructionRelocate(PBYTE& dest, PBYTE& src, int addrSize, int opcode)
{
	long offset=(long)((char)readByte(src));
	PBYTE destAddr=src+offset;
	// Ouput following code to simulate a near form of the looping instruction
	//   [Loop instr] JumpTaken
	//   JMP  JumpNotTaken  (short jump)
	// JumpTaken:
	//   JMP  [Actual JCXZ destination]  (near jump)
	// JumpNotTaken:
	writeByte(dest,opcode);
	writeByte(dest,2); // JumpTaken label is 2 bytes after loop instruction
	writeByte(dest,JUMP_SHORT_OPCODE);
	writeByte(dest,5); // Near jump is 5 bytes long
	writeByte(dest,JUMP_NEAR_OPCODE);
	writeDWord(dest, (DWORD)(destAddr-(dest+4)));
}

// Handler to relocate JCXZ instructions
IS void JCXZShort(PBYTE& dest, PBYTE& src, int addrSize)
{
	LoopingInstructionRelocate(dest,src,addrSize,JCXZ_OPCODE);
}

// Handler to relocate LOOP instructions
IS void LoopShort(PBYTE& dest, PBYTE& src, int addrSize)
{
	LoopingInstructionRelocate(dest,src,addrSize,LOOP_OPCODE);
}

// Handler to relocate LOOPZ instructions
IS void LoopZShort(PBYTE& dest, PBYTE& src, int addrSize)
{
	LoopingInstructionRelocate(dest,src,addrSize,LOOPZ_OPCODE);
}

// Handler to relocate LOOPNZ instructions
IS void LoopNZShort(PBYTE& dest, PBYTE& src, int addrSize)
{
	LoopingInstructionRelocate(dest,src,addrSize,LOOPNZ_OPCODE);
}


IS bool isRIPAddressing(byte bModRM)
{
    return ((bModRM & RIP_MASK) == RIP_MODRM);
}

IS PBYTE copyNextOpcode(PBYTE dest, PBYTE src, PBYTE* updatedSrcPtr, int addrSize=64, int opSize=32, BOOL twoByteOp=false, byte bRexFlag = 0)
{
	int opcode=readByte(src);
	if (updatedSrcPtr) *(updatedSrcPtr)=src;
	DWORD flags=twoByteOp ? twoByteOpcodeTable[opcode] : mainOpcodeTable[opcode];

	// Check for two byte opcode prefix, if not already found
	if ((!twoByteOp) && (opcode==TWO_BYTE_OPCODE))
		return copyNextOpcode(dest,src,updatedSrcPtr,addrSize,opSize,true, bRexFlag);

	// Check for relative displacement instructions first, and execute handler function
	// if needed
	if (flags&DISP8)
	{
		if (twoByteOp)
		{
			if (twoByteDispHandlerTable[opcode])
			{
				twoByteDispHandlerTable[opcode](dest,src,addrSize);
				if (updatedSrcPtr) *(updatedSrcPtr)=src;
				return dest;
			}
		}
		else
		{
			if (mainDispHandlerTable[opcode])
			{
				mainDispHandlerTable[opcode](dest,src,addrSize);
				if (updatedSrcPtr) *(updatedSrcPtr)=src;
				return dest;
			}
		}
		// Unhandled 8-bit relative displacement opcode
		writeByte(dest,opcode);
		copyByte(dest,src);
		if (updatedSrcPtr) *(updatedSrcPtr)=src;
		return dest;
	}
	else if (flags&DISP32)
	{
		if (twoByteOp)
		{
			if (twoByteDispHandlerTable[opcode])
			{
				twoByteDispHandlerTable[opcode](dest,src,addrSize);
				if (updatedSrcPtr) *(updatedSrcPtr)=src;
				return dest;
			}
		}
		else
		{
			if (mainDispHandlerTable[opcode])
			{
				mainDispHandlerTable[opcode](dest,src,addrSize);
				if (updatedSrcPtr) *(updatedSrcPtr)=src;
				return dest;
			}
		}
		// Unhandled 32-bit relative displacement opcode
		writeByte(dest,opcode);
		copyOpSize(dest,src,addrSize);
		if (updatedSrcPtr) *(updatedSrcPtr)=src;
		return dest;
	}
    // TODO: Add flags&DISP64

	// Check for prefixes
	if (flags & REX_PREFIX)
	{
		writeByte(dest,opcode);
		return copyNextOpcode(dest, src, updatedSrcPtr, addrSize, opSize, twoByteOp, opcode);
	}
	else if (flags & ADDR_SIZE_PREFIX)
	{
		writeByte(dest,opcode);
		addrSize= (addrSize == 64) ? 32 : 64;
		return copyNextOpcode(dest, src, updatedSrcPtr, addrSize, opSize, twoByteOp, bRexFlag);
	}
	else if (flags&OP_SIZE_PREFIX)
	{
		writeByte(dest,opcode);
		opSize=(opSize==32)?16:32;
		return copyNextOpcode(dest, src, updatedSrcPtr, addrSize, opSize, twoByteOp, bRexFlag);
	}
	else if (flags&PREFIX)
	{
		writeByte(dest,opcode);
		return copyNextOpcode(dest, src, updatedSrcPtr, addrSize, opSize, twoByteOp, bRexFlag);
	}

	// Check if we're checking Mod/RM for group 5 encoding
	if (flags & MODRMGROUP5)
    {
        byte bGroup5 = readByte(src);

        // change flags to be MODRM instead
        // in case we're not on a jmp or a call,
        // let regular modRM processing continue
        flags &= ~MODRMGROUP5; flags |= MODRM;
        if (opcode)
        {
            DispHandlerFuncPtr branchHandler = 0;
            switch (modRM32Group5Table[bGroup5])
            {
            case MODRM_CALL:
                if (!isRIPAddressing(bGroup5))
                    branchHandler = CallNear;
                break;
            case MODRM_JMP:
                if (!isRIPAddressing(bGroup5))
                    branchHandler = JumpNear;
                break;
            default:
                // continue with regular modRM processing
                break;
            }

            if (branchHandler)
            {
		        branchHandler(dest,src,addrSize);
		        if (updatedSrcPtr) *(updatedSrcPtr)=src;
		        return dest;
            }
            // else go to regular ModRM processing
            src--;
        }
    }


	// Output two byte prefix if needed, and then opcode
	if (twoByteOp)
		writeByte(dest, TWO_BYTE_OPCODE);
	writeByte(dest, opcode);

	// Check for Mod/RM byte
	if (flags & MODRM)
	{
		DWORD rmFlags;
		opcode = readByte(src);

		writeByte(dest,opcode);

        // for our purposes; no 16 bit modRM anymore
		rmFlags = modRM32Table[opcode];

		if (rmFlags&SIB)
			copyByte(dest,src);
		if (rmFlags&IMMED8)
			copyByte(dest,src);
		else if (rmFlags&IMMED16)
			copyWord(dest,src);
		else if (rmFlags&IMMED32)
        {
            if (isRIPAddressing(opcode))
            {
                long      lOffset     = readDWord(src);
                byte*     pbTarget    = src + lOffset;
                long long llNewOffset = pbTarget - (dest + 4);

                if (llNewOffset > INT_MAX || llNewOffset < INT_MIN)
                    DebugBreak();

                writeDWord(dest, (DWORD) llNewOffset);
            }
            else
			    copyDWord(dest,src);
        }
		else if ((rmFlags&SIB) && (((*(src-1))&7) == 5)) // special immed32 for mod=0 sib byte
			copyDWord(dest,src);
		if ((flags&IMMED8_ON_CASE0) && (((opcode>>3)&7) == 0))
			copyByte(dest,src);
		else if ((flags&IMMED32_ON_CASE0) && (((opcode>>3)&7) == 0))
			copyOpSize(dest,src,opSize);
	}


	// Check for immediate values
	if (flags&IMMED8)
		copyByte(dest,src);
	else if (flags&IMMED16)
		copyByte(dest,src);
	else if (flags&IMMED24)
	{
		copyWord(dest,src);
		copyByte(dest,src);
	}
	else if (flags&IMMED32)
		copyOpSize(dest,src,opSize);
	else if (flags&IMMED_REX)
    {
        if (bRexFlag & REX_W)
		    copyOpSize(dest,src,64);
        else
		    copyOpSize(dest,src,32);
    }
    else if (flags&NEAR_ADDR)
		copyOpSize(dest,src,addrSize);
	else if (flags&FAR_ADDR)
	{
		copyWord(dest,src);
		copyOpSize(dest,src,addrSize);
	}
	if (updatedSrcPtr)
		*(updatedSrcPtr)=src;
	return dest;
}

IS byte *relocInstruction(byte *dest, byte *src, byte **updatedSrcPtr)
{
	return copyNextOpcode(dest, src, updatedSrcPtr);
}


