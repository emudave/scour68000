#include "stdafx.h"
#include "app.h"

static void CheckPc()
{
	ot("	o.pc = sc->checkpc(o.pc);\n");
}

// Push 32-bit value in 'val'
static void OpPush32(char *val)
{
	ot("	// Push %s onto stack\n", val);
	ot("	sc->d[0xf] -= 4; // Predecrement A7\n");
	MemHandler(1, 2, "sc->d[0xf]", val);
}

// Pop 32-bit value into 'val'
static void OpPop(char *val, int size)
{
	MemHandler(0, size, "sc->d[0xf]", val);
	ot("	sc->d[0xf] += %d; // Postincrement A7\n", 1 << size);
}

static void PushPc()
{
	ot("	// Push PC:\n");
	ot("	{\n");
	ot("	u32 tmp = 0;\n");
	ot("	tmp = (o.pc + %d - sc->membase) << 1;\n", Ipos);
	OpPush32("tmp");
	ot("	}\n");
}

static void PopPc()
{
	ot("	// Pop PC:\n");
	ot("	{\n");
	ot("	u32 tmp = 0;\n");
	OpPop("tmp", 2);
	ot("	o.pc = sc->membase + (tmp >> 1);\n");
	ot("	}\n");
}

static void PopSr(int high)
{
	ot("	// Pop SR\n");
	OpPop("val", 1);
	OpValToStatus(high);
}



// --------------------- Opcodes 0x4e50+ ---------------------
int OpLink(int op)
{
	int use=0;

	use=op&~7;
	if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

	OpStart(use);

	ot("	// Get An\n");
	EaCalc(7, 8, 2);
	EaRead(   8, 2);

	ot("	off = (sc->d[0xf] -= 4); // A7 -= 4\n");

	ot("	// Write An to Stack\n");
	MemHandler(1, 2);
	ot("\n");

	ot("	// Save to An:\n");
	ot("	val = off;\n");
	EaCalc(7, 8, 2);
	EaWrite(  8, 2);

	ot("	// Get offset:\n");
	EaCalc(0, 0x3c, 1);
	EaRead(   0x3c, 1);

	ot("	sc->d[0xf] += val; // Add offset to A7\n");

	Cycles=16;
	OpEnd();
	return 0;
}

// --------------------- Opcodes 0x4e58+ ---------------------
int OpUnlk(int op)
{
	int use=0;

	use=op&~7;
	if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

	OpStart(use);
	ot("	// Get An\n");
	EaCalc(7, 8, 2);
	EaRead(   8, 2);

	ot("	// Pop An from stack:\n");
	MemHandler(0, 2);
	ot("	\n");
	ot("	sc->d[0xf] = off + 4; // A7\n");
	
	ot("	// An = value from stack:\n");
	EaWrite(8, 2);

	Cycles=12;
	OpEnd();
	return 0;
}

// --------------------- Opcodes 0x4e70+ ---------------------
int Op4E70(int op)
{
  int type=0;

	type=op&7;
	// 01001110 01110ttt, reset/nop/stop/rte/rtd/rts/trapv/rtr
	if (type==1) { OpStart(op); Cycles=4; OpEnd(); return 0; } // nop

	if (type==3 || type==7) // rte/rtr
	{
		OpStart(op); Cycles=20;
		PopSr(type==3);
		PopPc();
		// todo if (type==3) CheckInterrupt();
		Ipos = 0; // Don't advance PC
		OpEnd();
		return 0;
	}

	if (type==5) // rts
	{
		OpStart(op); Cycles=16;
		PopPc();
		Ipos = 0; // Don't advance PC
		OpEnd();
		return 0;
	}

	return 1;
}

// --------------------- Opcodes 0x4e80+ ---------------------
// Emit a Jsr/Jmp opcode, 01001110 1mEEEeee
int OpJsr(int op)
{
	int use=0, sea=0;

	sea=op&0x003f;

	if (sea < 0x10) return 1; // jmp Dn wouldn't work
	// See if we can do this opcode:
	if (EaCanRead(sea,-1)==0) return 1;

	use=OpBase(op);
	if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

	OpStart(op); Cycles=14; // Correct?

	EaCalc(0x003f, sea, 0);
	ot("\n");

	if ((op&0x40) == 0)
	{
		ot("	// Jsr:\n");
		PushPc();
		Cycles+=8;
	}

	ot("	// Get new PC from 'off'\n");
	ot("	o.pc = sc->membase + (off >> 1);\n");
	ot("\n");

	CheckPc();

	Ipos = 0; // Don't advance PC
	OpEnd();

	return 0;
}

// --------------------- Opcodes 0x50c8+ ---------------------

// Emit a Dbra opcode, 0101cccc 11001nnn vv
int OpDbra(int op)
{
	int use=0;
	int cc=0;

	use=op&~7; // Use same handler
	cc=(op>>8)&15;

	if (cc != 1) return 1; //todo

	if (op!=use) { OpUse(op,use); return 0; } // Use existing handler
	OpStart(op);

	ot("	int end = 0;\n\n");

/*
	if (cc>=2)
	{
		ot("	// Is the condition true?\n");
		if ((cc&~1)==2) ot("  eor r9,r9,#0x20000000 ;@ Invert carry for hi/ls\n");
		ot("  msr cpsr_flg,r9 ;@ ARM flags = 68000 flags\n");
		if ((cc&~1)==2) ot("  eor r9,r9,#0x20000000\n");
		ot(";@ If so, don't dbra\n");
		ot("  b%s DbraEnd%.4x\n",Cond[cc],op);
		ot("\n");
	}
*/

	ot("	// Decrement Dn.w:\n");
	ot("	off = o.pc[0] & 7;\n");
	ot("	val = (sc->d[off] - 1) & 0xffff;\n");
	ot("	if (val == 0xffff) end = 1;\n");
	ot("	sc->d[off] &= ~0xffff;\n");
	ot("	sc->d[off] |= val;\n");

	ot("\n");

	ot("	if (end)\n");
	ot("	{\n");
	ot("		o.cycles -= 12;\n");
	ot("		o.pc += 2;\n");
	ot("	}\n");
	ot("	else\n");
	ot("	{\n");
	ot("		// Get Branch offset:\n");
	ot("		val = (s16)o.pc[1];\n");
	ot("		o.pc += 1 + (val >> 1);\n");
	ot("		o.cycles -= 10;\n");
	ot("	}\n");
	ot("\n");

	Ipos = 0; // Don't change the PC afterwards
	OpEnd();

	return 0;
}


// --------------------- Opcodes 0x6000+ ---------------------
// Emit a Branch opcode 0110cccc nn  (cccc=condition)
int OpBranch(int op)
{
	int size=0,use=0;
	int offset=0;
	int cc=0;

	offset=(s8)(op&0xff);
	cc=(op>>8)&15;

	// Special offsets:
	if (offset==0)  size=1;
	if (offset==-1) size=2;

	if (size) use=op; // 16-bit or 32-bit
	else use=(op&0xff00)+1; // Use same opcode for all 8-bit branches

	if (op!=use) { OpUse(op,use); return 0; } // Use existing handler
	OpStart(op); Cycles=8; // Assume branch not taken

	ot("	// Get Branch offset:\n");
	if (size)
	{
		EaCalc(0,0x3c, size);
		EaRead(  0x3c, size);
	}
	else
	{
		ot("	val = ((o.pc[0] + 0x80) & 0xff) - 0x80;\n\n");
	}

	if (cc == 0)
	{
		// Unconditional branch:
		ot("	// Branch, add on val to PC:\n");
		ot("	o.pc += 1 + (val >> 1);\n");
	}

	if (cc == 1)
	{
		// Branch to subroutine
		ot("	// Bsr:\n");
		PushPc();

		ot("	// Add on val to PC:\n");
		ot("	o.pc += 1 + (val >> 1);\n");
		Cycles+=8;
	}

	if (cc>=2)
	{
		ot("	// Is the condition true? (XNZVC)\n");
		
		// XNZVC
		if (cc ==  2) ot("	if ((o.sr & 5) == 0) // bhs: ~C and ~Z\n");
		if (cc ==  3) ot("	if ( o.sr & 5)       // bls:  C or Z\n");

		if (cc ==  4) ot("	if ((o.sr & 1) == 0) // bcc: ~C\n");
		if (cc ==  5) ot("	if ( o.sr & 1)       // bcs:  C\n");

		if (cc ==  6) ot("	if ((o.sr & 4) == 0) // bne: ~Z\n");
		if (cc ==  7) ot("	if ( o.sr & 4      ) // beq:  Z\n");

		if (cc ==  8) ot("	if ((o.sr & 2) == 0) // bvc: ~V\n");
		if (cc ==  9) ot("	if ( o.sr & 2      ) // bvs:  V\n");

		if (cc == 10) ot("	if ((o.sr & 8) == 0) // bpl: ~N\n");
		if (cc == 11) ot("	if ( o.sr & 8      ) // bmi:  N\n");

		if (cc == 12) ot("	if ((o.sr & 0xa) == 0x0 || (o.sr & 0xa) == 0xa) // bge: ~N ~V or N  V\n");
		if (cc == 13) ot("	if ((o.sr & 0xa) == 0x2 || (o.sr & 0xa) == 0x8) // blt: ~N  V or N ~V\n");

		if (cc == 14) ot("	if ((o.sr & 0xe) == 0x0 || (o.sr & 0xe) == 0xa) // bgt: ~N ~Z ~V or N ~Z  V\n");
		if (cc == 15) ot("	if ((o.sr & 0xa) == 0x2 || (o.sr & 0xa) == 0x8 || (o.sr & 4)) // ble: ~N  V or N ~V or Z\n");

		ot("	{\n");
		ot("		// Take branch, add on val to PC:\n");
		ot("		o.pc += 1 + (val >> 1);\n");
		ot("		o.cycles -= 2;\n");
		ot("	}\n");
		if (Ipos)
		{
			ot("	else\n");
			ot("	{\n");
			ot("		// Don't branch, just increment PC normally:\n");
			ot("		o.pc += %d;\n", Ipos);
			ot("	}\n");
		}
	}

	Ipos = 0; // Don't change the PC afterwards

	ot("\n");

	if (offset==0 || offset==-1)
	{
		//ot("	// Branch is quite far, so may be a good idea to check Memory Base+pc\n");
		CheckPc();
	}

	OpEnd();

	return 0;
}
