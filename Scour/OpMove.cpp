
#include "app.h"

// --------------------- Opcodes 0x1000+ ---------------------
// Emit a Move opcode, 00xxdddd ddssssss
int OpMove(int op)
{
	int sea=0,tea=0;
	int size=0,use=0;
	int movea=0;

	// Get source and target EA
	sea = op&0x003f;
	tea =(op&0x01c0)>>3;
	tea|=(op&0x0e00)>>9;

	if (tea>=8 && tea<0x10) movea=1;

	// Find size extension
	switch (op&0x3000)
	{
		default: return 1;
		case 0x1000: size=0; break;
		case 0x3000: size=1; break;
		case 0x2000: size=2; break;
	}

	if (movea && size<1) return 1; // movea.b is invalid

	// See if we can do this opcode:
	if (EaCanRead (sea,size)==0) return 1;
	if (EaCanWrite(tea     )==0) return 1;

	use=OpBase(op);
	if (tea<0x38) use&=~0x0e00; // Use same handler for register ?0-7

	if (tea>=0x18 && tea<0x28 && (tea&7)==7) use|=0x0e00; // Specific handler for (a7)+ and -(a7)

	if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

	OpStart(op); Cycles=4;

	EaCalc(0x003f,sea,size);
	EaRead(       sea,size);

	if (!movea)
	{
		ot("	res = val;\n");
		OpGetNZ(size);
		ot("\n");
	}

	if (movea) size=2; // movea always expands to 32-bits

	EaCalc (0x0e00,tea,size);
	EaWrite(       tea,size);

	OpEnd();
	return 0;
}

// --------------------- Opcodes 0x41c0+ ---------------------
// Emit an Lea opcode, 0100nnn1 11aaaaaa
int OpLea(int op)
{
	int use=0;
	int sea=0,tea=0;

	sea= op&0x003f;
	if (sea < 0x10) return 1; // No address, so doesn't make sense
	tea=(op&0x0e00)>>9; tea|=8;

	if (EaCanRead(sea,-1)==0) return 1; // See if we can do this opcode:

	use=OpBase(op);
	use&=~0x0e00; // Also use 1 handler for target ?0-7
	if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

	OpStart(op); Cycles=4;

	EaCalc (0x003f, sea, 0, "val"); // Lea
	EaCalc (0x0e00, tea, 2);
	EaWrite(        tea, 2);

	OpEnd();

	return 0;
}

void OpValToStatus(int high)
{
	if (high)
		ot("	o.sr = val;\n");
	else
		ot("	o.sr = (o.sr & ~0xff) | (val & 0xff);\n");
}

static void OpStatusToVal(int high)
{
	if (high)
		ot("	val = o.sr;\n");
	else
		ot("	val = o.sr & 0xff;\n");
}


// Move SR opcode, 01000tt0 11aaaaaa move to SR
int OpMoveSr(int op)
{
	int type = (op>>9)&3; // from SR, from CCR, to CCR, to SR
	if (type == 1) return 1; // No such opcode on 68000

	int ea = op&0x3f;
	if ((ea & 0x38) == 0x08) return 1; // can't use An

	int size = 1;

	// See if we can do this opcode:
	switch(type)
	{
		case 0:
			if (EaCanWrite(ea)==0) return 1; 
		break;

		case 2: case 3:
			if (EaCanRead(ea,size)==0) return 1; // See if we can do this opcode:
		break;
	}

	int use = OpBase(op);
	if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

	OpStart(op);
	     if (type==0) Cycles=8;
	else if (type==1) Cycles=6;
	else Cycles=12;

	// todo - SuperCheck

	if (type==0 || type==1)
	{
		OpStatusToVal(type==0);
		EaCalc (0x003f, ea, size);
		EaWrite(        ea, size);
	}

	if (type==2 || type==3)
	{
		EaCalc(0x003f, ea, size);
		EaRead(        ea, size);
		OpValToStatus(type==3);
		// todo if (type==3) CheckInterrupt();
	}
	ot("\n");

	OpEnd();

	//todo if (size>0) if (type==0 || type==3) SuperEnd(op);

	return 0;
}

// Ori/Andi/Eori $nnnn,sr 0000t0t0 01111100
int OpArithSr(int op)
{
	int type=0,ea=0;
	int use=0,size=0;

	type=(op>>9)&5; if (type==4) return 1;
	size=(op>>6)&1;
	ea=0x3c;

	use=OpBase(op);
	if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

	OpStart(op); Cycles=16;

	// Changing CCR is allowed in any mode, but SR is only allowed in supervisor mode:
	// todo if (size>0) SuperCheck(op);

	EaCalc(0x003f, ea, size, "off");
	EaRead(        ea, size, "off", "res");

	OpStatusToVal(size);
	if (type==0) ot("	val |= res;\n");
	if (type==1) ot("	val &= res;\n");
	if (type==5) ot("	val ^= res;\n");
	OpValToStatus(size);
	// todo if (size) CheckInterrupt();

	OpEnd();
	//todo if (size>0) SuperEnd(op);

	return 0;
}



// --------------------- Opcodes 0x4880+ ---------------------
// Emit a Movem opcode, 01001d00 1xeeeeee regmask
int OpMovem(int op)
{
	int size=0,ea=0,cea=0,dir=0;
	int use=0,decr=0,change=0;

	size=((op>>6)&1)+1;
	ea=op&0x003f;
	dir=(op>>10)&1; // Direction

	if (ea < 0x10 || ea >= 0x28) return 1; // Invalid EA
	if ((ea&0x38)==0x18 || (ea&0x38)==0x20) change=1; // Address register changes
	if ((ea&0x38)==0x20) decr=1; // -(An), bitfield is decr

	// See if we can do this opcode:
	if (EaCanWrite(ea)==0) return 1;

	cea=ea; if (change) cea=0x10;

	use=OpBase(op);
	if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

	OpStart(op);
	ot("	u32 mask = o.pc[%d]; // register mask\n", Ipos++);
	ot("	u32 addr = 0; // Address value\n");
	ot("	int i = 0;\n");
	ot("\n");

	ot("	// Get the address into addr:\n");
	EaCalc (0x0007, 8|(ea&7), 2, "off");
	EaRead(         8|(ea&7), 2, "off", "addr");

	ot("	for (i = 0; i < 16; i++)\n");
	ot("	{\n");

	ot("		if (((mask >> i) & 1) == 0) continue;\n");

	if (decr) ot("		addr -= %d; // Pre-decrement address\n", 1<<size);

	if (dir)
	{
		ot("	// Copy memory to register:\n", 1<<size);
		EaRead(ea, size, "addr", "val");
		ot("		sc->d[i] = val;\n");
	}
	else
	{
		ot("		val = sc->d[15 - i];\n");
		EaWrite(ea, size, "addr", "val");
	}

	if (decr == 0) ot("		addr += %d; // Post-increment address\n", 1<<size);

	ot("		o.cycles -= %d; // Take some cycles\n", 2<<size);

	ot("	}\n");
	ot("\n");

	if (change)
	{
		ot("	// Write back address:\n");
		EaWrite(        8|(ea&7), 2, "off", "addr");
	}

	OpEnd();

	return 0;
}

// --------------------- Opcodes 0x4e60+ ---------------------
// Emit a Move USP opcode, 01001110 0110dnnn move An to/from USP
int OpMoveUsp(int op)
{
	int use=0,dir=0;

	dir=(op>>3)&1; // Direction
	use=op&~0x0007; // Use same opcode for all An

	if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

	OpStart(op); Cycles=4;

	// todo check we are in supervisor mode

	if (dir)
	{
		EaCalc (0x0007,8,2);
		ot("	val = sc->osp; // Get from USP\n\n");
		EaWrite(       8,2);
	}
	else
	{
		EaCalc (0x0007,8,2);
		EaRead (       8,2);
		ot("	sc->osp = val; // Put in USP\n\n");
	}

	OpEnd();

	return 0;
}

// --------------------- Opcodes 0x7000+ ---------------------
// Emit a Move Quick opcode, 0111nnn0 dddddddd  moveq #dd,Dn
int OpMoveq(int op)
{
	int use=0;

	use=op&0xf100; // Use same opcode for all values
	if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

	OpStart(op); Cycles=4;

	ot("	val = o.pc[0]; // Get opcode\n");
	ot("	off = (val & 0x0e00) >> 9; // Get target register\n");
	ot("	val = ((val + 0x80) & 0xff) - 0x80; // Sign-extend\n\n");
	ot("	sc->d[off] = val; // Store into Dn\n\n");

	ot("	res = val;\n");
	OpGetNZ(2);

	OpEnd();

	return 0;
}

