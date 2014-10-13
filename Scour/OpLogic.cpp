
#include "stdafx.h"
#include "app.h"

// --------------------- Opcodes 0x0100+ ---------------------
// Emit a Btst (Register) opcode 0000nnn1 00aaaaaa
int OpBtstReg(int op)
{
	int use=0;
	int type=0,sea=0,tea=0;
	int size=0;

	type=(op>>6)&3;
	// Get source and target EA
	sea=(op>>9)&7;
	tea=op&0x003f;
	if (tea<0x10) size=2; // For registers, 32-bits

	if ((tea&0x38)==0x08) return 1; // movep

	// See if we can do this opcode:
	if (EaCanRead(tea,0)==0) return 1;
	if (type>0)
	{
		if (EaCanWrite(tea)==0) return 1;
	}

	use=OpBase(op);
	use&=~0x0e00; // Use same handler for all registers
	if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

	OpStart(op); Cycles=4;
	if (tea<0x10) Cycles+=2;
	if (type>0) Cycles+=4;

	ot("	u32 bit = 0;\n\n");

	EaCalc (0x0e00,sea,0);
	EaRead (       sea,0);

	ot("	bit = 1 << (val & %d); // Make bit mask\n", (8 << size) - 1);
	ot("\n");

	EaCalc(0x003f,tea,size);
	EaRead(       tea,size);
	ot("	o.sr &= ~4;\n");
	ot("	if ((val & bit) == 0) o.sr |= 4;  // Do btst and get Z flag\n");
	ot("\n");

	if (type>0)
	{
		if (type==1) ot("	val ^= bit; // Toggle bit\n");
		if (type==2) ot("	val &= ~bit; // Clear bit\n");
		if (type==3) ot("	val |= bit; // Set bit\n");
		ot("\n");
		EaWrite(tea, size);
	}

	OpEnd();


	return 0;
}


// --------------------- Opcodes 0x0800+ ---------------------
// Emit a Btst/Bchg/Bclr/Bset (Immediate) opcode 00001000 ttaaaaaa nn
int OpBtstImm(int op)
{
	int type=0,sea=0,tea=0;
	int use=0;
	int size=0;

	type=(op>>6)&3;
	// Get source and target EA
	sea=   0x003c;
	tea=op&0x003f;
	if (tea<0x10) size=2; // For registers, 32-bits

	// See if we can do this opcode:
	if (EaCanRead(tea,0)==0) return 1;
	if (type>0)
	{
		if (EaCanWrite(tea)==0) return 1;
	}

	use=OpBase(op);
	if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

	OpStart(op); Cycles=4;

	ot("	u32 bit = 0;\n\n");

	if (type<3 && tea<0x10) Cycles+=2;
	if (type>0) Cycles+=4;

	EaCalc (0x0000,sea,0);
	EaRead (       sea,0);
	
	ot("	bit = 1 << (val & %d); // Make bit mask\n", (8 << size) - 1);
	ot("\n");

	EaCalc (0x003f,tea,size);
	EaRead (       tea,size);
	ot("	o.sr &= ~4;\n");
	ot("	if ((val & bit) == 0) o.sr |= 4;  // Do btst and get Z flag\n");
	ot("\n");

	if (type>0)
	{
		if (type==1) ot("	val ^= bit; // Toggle bit\n");
		if (type==2) ot("	val &= ~bit; // Clear bit\n");
		if (type==3) ot("	val |= bit; // Set bit\n");
		ot("\n");
		EaWrite(tea, size);
	}

	OpEnd();

	return 0;
}

// --------------------- Opcodes 0x4000+ ---------------------
int OpNeg(int op)
{
	// 01000tt0 xxeeeeee (tt=negx/clr/neg/not, xx=size, eeeeee=EA)
	int type=0,size=0,ea=0,use=0;

	type=(op>>9)&3;
	ea  =op&0x003f;
	size=(op>>6)&3; if (size>=3) return 1;

	switch (type)
	{
		case 1: case 2: case 3: break;
		default: return 1; // todo
	}

	// See if we can do this opcode:
	if (EaCanRead (ea,size)==0) return 1;
	if (EaCanWrite(ea     )==0) return 1;

	use=OpBase(op);
	if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

	OpStart(op); Cycles=size<2?4:6;

	EaCalc(0x003f, ea, size);

	if (type!=1) EaRead(ea, size); // Don't need to read for 'clr'
	if (type==1) ot("\n");

	if (type==1)
	{
		ot("	// Clear:\n");
		ot("	o.sr &= ~0xf;\n");
		ot("	o.sr |=  0x4; // NZVC=0100\n");
		ot("\n");
	}

	if (type==2)
	{
		ot("	// Neg:\n");
		ot("	{\n");
		ot("		u32 a = 0, b = val;\n");
		ot("		res = a - b;\n");
		OpGetNZVC(size, 1, 1);
		ot("	}\n");
		ot("\n");
	}

	if (type==3)
	{
		ot("	// Not:\n");
		ot("	res = ~val;\n");
		OpGetNZ(size);
		ot("\n");
	}

	EaWrite(ea, size, "off", "res");

	OpEnd();

	return 0;
}

// --------------------- Opcodes 0x4840+ ---------------------
// Swap, 01001000 01000nnn swap Dn
int OpSwap(int op)
{
	int ea=0,use=0;

	ea=op&7;
	use=op&~0x0007; // Use same opcode for all An
	int size = 2;

	if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

	OpStart(op); Cycles=4;

	EaCalc (0x0007, ea, size);
	EaRead (        ea, size);

	ot("	res  = (val & 0x0000ffff) << 16;\n");
	ot("	res |= (val & 0xffff0000) >> 16;\n");
	OpGetNZ(size);

	EaWrite( ea, size, "off", "res");

	OpEnd();

	return 0;
}

// --------------------- Opcodes 0x4a00+ ---------------------
// Emit a Tst opcode, 01001010 xxeeeeee
int OpTst(int op)
{
	int sea=0;
	int size=0,use=0;

	sea=op&0x003f;
	size=(op>>6)&3; if (size>=3) return 1;

	// See if we can do this opcode:
	if (EaCanRead(sea, size) == 0) return 1;

	use=OpBase(op);
	if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

	OpStart(op); Cycles=4;

	EaCalc (0x003f,sea,size);
	EaRead (       sea,size);

	ot("	res = val;\n");
	OpGetNZ(size);

	OpEnd();
	return 0;
}

// --------------------- Opcodes 0x4880+ ---------------------
// Emit an Ext opcode, 01001000 1x000nnn
int OpExt(int op)
{
	int ea=0;
	int size=0,use=0;

	ea=op&0x0007;
	size=(op>>6)&1;

	use=OpBase(op);
	if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

	OpStart(op); Cycles=4;

	EaCalc (0x0007,ea,size);
	EaRead (       ea,size);

	if (size == 0) ot("	res = (int)(s8)val;\n");
	else           ot("	res = (int)(s16)val;\n");
	OpGetNZ(size + 1);

	EaWrite(ea, size + 1, "off", "res");

	OpEnd();
	return 0;
}

// --------------------- Opcodes 0x50c0+ ---------------------
// Emit a Set cc opcode, 0101cccc 11eeeeee
int OpSet(int op)
{
	int cc=0,ea=0;
	int size=0,use=0;

	cc=(op>>8)&15;
	ea=op&0x003f;

	if ((ea&0x38)==0x08) return 1; // dbra, not scc

	// See if we can do this opcode:
	if (EaCanWrite(ea)==0) return 1;

	use=OpBase(op);
	if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

	OpStart(op); Cycles=8;

	if (ea<0x10) Cycles=4;

	//ot("  mov r1,#0\n");

	if (cc != 1)
	{
		ot("	// Is the condition true?\n");
		if (cc ==  2) ot("	if ((o.sr & 5) == 0) // hs: ~C and ~Z\n");
		if (cc ==  3) ot("	if ( o.sr & 5)       // ls:  C or Z\n");
		if (cc ==  4) ot("	if ((o.sr & 1) == 0) // cc: ~C\n");
		if (cc ==  5) ot("	if ( o.sr & 1)       // cs:  C\n");
		if (cc ==  6) ot("	if ((o.sr & 4) == 0) // ne: ~Z\n");
		if (cc ==  7) ot("	if ( o.sr & 4      ) // eq:  Z\n");
		if (cc ==  8) ot("	if ((o.sr & 2) == 0) // vc: ~V\n");
		if (cc ==  9) ot("	if ( o.sr & 2      ) // vs:  V\n");
		if (cc == 10) ot("	if ((o.sr & 8) == 0) // pl: ~N\n");
		if (cc == 11) ot("	if ( o.sr & 8      ) // mi:  N\n");
		if (cc == 12) ot("	if ((o.sr & 0xa) == 0x0 || (o.sr & 0xa) == 0xa) // ge: ~N ~V or N  V\n");
		if (cc == 13) ot("	if ((o.sr & 0xa) == 0x2 || (o.sr & 0xa) == 0x8) // lt: ~N  V or N ~V\n");
		if (cc == 14) ot("	if ((o.sr & 0xe) == 0x0 || (o.sr & 0xe) == 0xa) // gt: ~N ~Z ~V or N ~Z  V\n");
		if (cc == 15) ot("	if ((o.sr & 0xa) == 0x2 || (o.sr & 0xa) == 0x8 || (o.sr & 4)) // le: ~N  V or N ~V or Z\n");

		ot("	{\n");
		ot("		res = 0xffffffff;\n");
		if (ea<0x10) ot("		o.cycles -= 2; // Extra cycles\n");
		ot("	}\n");
		ot("\n");
	}

	EaCalc (0x003f, ea, size, "off");
	EaWrite(        ea, size, "off", "res");

	OpEnd();
	return 0;
}


// Emit a Asr/Lsr/Roxr/Ror opcode
static int EmitAsr(int, int type, int dir, int count, int size, int usereg)
{
	char pct[8] = {0}; // count
	int top = 8 << size;
	int topbit = 1 << (top - 1);
	//  int shift=32-(8<<size);

	if (count>=0) sprintf(pct, "%d", count); // Fixed count

	char *cast = "u32";
	if (size == 0) cast = "u8";
	if (size == 1) cast = "u16";

	if (usereg)
	{
		ot("	{\n");
		ot("		// Use Dn for count:\n");
		ot("		u32 n = (o.pc[0] & 0xe00) >> 9;\n");
		ot("		count = sc->d[n] & 63;\n");
		ot("	}\n\n");
		strcpy(pct, "count");
	}
	else if (count<0)
	{
		ot("	count = (o.pc[0] & 0xe00) >> 9;\n");
		strcpy(pct, "count");
	}

	if (count < 0)
		ot("	o.cycles -= (%s << 1); // Take 2*n cycles\n\n", pct);
	else
		Cycles += count << 1;

	// todo get carry bit for asl/lsl
	// --------------------------------------
	if (type == 0)
	{
		// Asr/r
		ot("	// Arithmetic Shift register:\n");

		if (dir)
			ot("	res = val << %s;\n", pct);
		else
		{
			ot("	res = (s32)val >> %s;\n", pct);
		}
		ot("\n");
	}
	// --------------------------------------
	if (type == 1)
	{
		// Lsr/l
		ot("	// Shift register:\n");

		if (dir)
			ot("	res = val << %s;\n", pct);
		else
			ot("	res = ((%s)val) >> %s;\n", cast, pct);
		ot("\n");
	}

	if (type <= 1)
	{
		OpGetNZ(size);

		ot("	if ((val ^ res) & 0x%x) o.sr |= 2; // V\n", topbit);

		if (dir == 0) ot("	o.sr |= (val >> (%s - 1)) & 1; // C\n", pct);
		else          ot("	o.sr |= (val >> (%d - %s)) & 1; // C\n", top, pct);
	}
	// --------------------------------------
	if (type == 2 || type == 3)
	{
		// Ror/l
		ot("	// Rotate register:\n");

		ot("	{\n");
		ot("		%s t = (%s)val;\n", cast, cast);

		if (count == 8 && top == 8)
		{
			ot("		res = t; // (Rotate ends up doing nothing...)\n");
		}
		else
		{
			if (dir)
				ot("		res = (t >> (%d - %s)) | (t << %s);\n", top, pct, pct);
			else
				ot("		res = (t << (%d - %s)) | (t >> %s);\n", top, pct, pct);
		}

		ot("	}\n");
		ot("\n");

		OpGetNZ(size);
		if (dir) ot("	o.sr |= res & 1;\n");
	}
	// --------------------------------------

	return 0;
}

// Emit a Asr/Lsr/Roxr/Ror opcode - 1110cccd xxuttnnn
// (ccc=count, d=direction(r,l) xx=size extension, u=use reg for count, tt=type, nnn=register Dn)
int OpAsr(int op)
{
	int ea=0,use=0;
	int count=0,dir=0;
	int size=0,usereg=0,type=0;

	ea=0;
	count =(op>>9)&7;
	dir   =(op>>8)&1;
	size  =(op>>6)&3;
	if (size>=3) return 1; // use OpAsrEa()
	usereg=(op>>5)&1;
	type  =(op>>3)&3;

	//djh if (type == 2) return 1; // todo

	if (usereg==0) count=((count-1)&7)+1; // because ccc=000 means 8

	// Use the same opcode for target registers:
	use=op&~0x0007;

	// As long as count is not 8, use the same opcode for all shift counts::
	if (usereg==0 && count!=8 && !(count==1&&type==2)) { use|=0x0e00; count=-1; }
	if (usereg) { use&=~0x0e00; count=-1; } // Use same opcode for all Dn

	if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

	OpStart(op); Cycles=size<2?6:8;
	ot("	int count = 0;\n\n");

	EaCalc( 0x0007, ea,size);
	EaRead(         ea,size);

	EmitAsr(op, type, dir, count, size, usereg);

	EaWrite(        ea,size, "off", "res");

	ot("	(void)count;\n");
	OpEnd();

	return 0;
}
