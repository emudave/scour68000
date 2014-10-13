
#include "stdafx.h"
#include "app.h"

// --------------------- Opcodes 0x0000+ ---------------------
// Emit an Ori/And/Sub/Add/Eor/Cmp Immediate opcode, 0000ttt0 00aaaaaa
int OpArith(int op)
{
	int type=0,size=0;
	int sea=0,tea=0;
	int use=0;

	// Get source and target EA
	type=(op>>9)&7; if (type==4 || type>=7) return 1;

	size=(op>>6)&3; if (size>=3) return 1;
	sea=   0x003c;
	tea=op&0x003f;

	// See if we can do this opcode:
	if (EaCanRead(tea,size)==0) return 1;
	if (type!=6 && EaCanWrite(tea)==0) return 1;

	use=OpBase(op);
	if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

	OpStart(op); Cycles=4;
	ot("	u32 a = 0, b = 0;\n\n");

	EaCalc(0x0000, sea, size, "off");
	EaRead(        sea, size, "off", "b");

	EaCalc(0x003f, tea, size, "off");
	EaRead(        tea, size, "off", "a");

	ot("	// Do arithmetic:\n");

	if (type == 0) { ot("	res = a | b;\n"); OpGetNZ  (size); }
	if (type == 1) { ot("	res = a & b;\n"); OpGetNZ  (size); }
	if (type == 2) { ot("	res = a - b;\n"); OpGetNZVC(size, 1, 1); } // X-bit
	if (type == 3) { ot("	res = a + b;\n"); OpGetNZVC(size, 0, 1); } // X-bit
	if (type == 5) { ot("	res = a ^ b;\n"); OpGetNZ  (size); }
	if (type == 6) { ot("	res = a - b;\n"); OpGetNZVC(size, 1); }

	if (type!=6)
	{
		EaWrite(tea, size, "off", "res");
	}

	// Correct cycles:
	if (type==6)
	{
		if (size>=2 && tea<0x10) Cycles+=2;
	}
	else
	{
		if (size>=2) Cycles+=4;
		if (tea>=0x10) Cycles+=4;
	}

	OpEnd();

	return 0;
}

// --------------------- Opcodes 0x5000+ ---------------------
int OpAddq(int op)
{
	// 0101nnnt xxeeeeee (nnn=#8,1-7 t=addq/subq xx=size, eeeeee=EA)
	int num=0,type=0,size=0,ea=0;
	int use=0;

	num =(op>>9)&7; if (num==0) num=8;
	type=(op>>8)&1;
	size=(op>>6)&3; if (size>=3) return 1;
	ea  = op&0x3f;

	// See if we can do this opcode:
	if (EaCanRead (ea,size)==0) return 1;
	if (EaCanWrite(ea     )==0) return 1;

	use=op; if (ea<0x38) use&=~7;
	if ((ea&0x38)==0x08) { size=2; use&=~0xc0; } // Every addq #n,An is 32-bit

	if (num!=8) use|=0x0e00; // If num is not 8, use same handler
	if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

	OpStart(op);
	Cycles=ea<8?4:8;
	if (size>=2 && ea!=8) Cycles+=4;

	ot("	u32 a = 0, b = 0;\n\n");

	EaCalc(0x003f, ea, size, "off");
	EaRead(        ea, size, "off", "a");

	if (num!=8)
	{
		ot("	b = (o.pc[0] >> 9) & 7; // Get quick value\n");
	}

	if (num==8) ot("	b = 8;\n");
	
	if (type==0) ot("	res = a + b;\n");
	if (type==1) ot("	res = a - b;\n");

	if ((ea&0x38)!=0x08) OpGetNZVC(size, type, 1);
	ot("\n");

	EaWrite(ea, size, "off", "res");

	OpEnd();

	return 0;
}

// --------------------- Opcodes 0x8000+ ---------------------
// 1t0tnnnd xxeeeeee (tt=type:or/sub/and/add xx=size, eeeeee=EA)
int OpArithReg(int op)
{
	int use=0;
	int type=0,size=0,dir=0,rea=0,ea=0;

	type=(op>>12)&5;
	rea =(op>> 9)&7;
	dir =(op>> 8)&1;
	size=(op>> 6)&3; if (size>=3) return 1;

	ea  = op&0x3f;

	if (dir && ea<0x10) return 1; // addx/subx opcode

	// See if we can do this opcode:
	if (dir==0 && EaCanWrite(rea)==0) return 1;
	if (dir    && EaCanWrite( ea)==0) return 1;
	if (dir==0 && EaCanRead( ea, size)==0) return 1;
	if (dir    && EaCanRead(rea, size)==0) return 1;

	use=OpBase(op);
	use&=~0x0e00; // Use same opcode for Dn
	if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

	OpStart(op); Cycles=4;

	ot("	u32 a = 0, roff = 0, b = 0;\n\n");

	ot("	// Get off = EA val = EA value:\n");
	EaCalc(0x003f,  ea, size, "off");
	EaRead(         ea, size, "off",  dir ? "a" : "b");
	ot("	// Get roff = Register rval = Register value:\n");
	EaCalc(0x0e00, rea, size, "roff");
	EaRead(        rea, size, "roff", dir ? "b" : "a");

	ot("	// Do arithmetic:\n");
	if (type == 0) ot("	res = a | b;\n");
	if (type == 1) ot("	res = a - b;\n");
	if (type == 4) ot("	res = a & b;\n");
	if (type == 5) ot("	res = a + b;\n");
	
	if (type == 0) OpGetNZ  (size);
	if (type == 1) OpGetNZVC(size, 1, 1);
	if (type == 4) OpGetNZ  (size);
	if (type == 5) OpGetNZVC(size, 0, 1);
	ot("\n");

	ot("	// Save result:\n");
	if (dir) EaWrite( ea,size, "off",  "res");
	else     EaWrite(rea,size, "roff", "res");

	if (size==1 && ea>=0x10) Cycles+=4;
	if (size>=2) { if (ea<0x10) Cycles+=4; else Cycles+=2; }

	OpEnd();

	return 0;
}

// --------------------- Opcodes 0x80c0+ ---------------------
int OpMul(int op)
{
	// Div/Mul: 1m00nnns 11eeeeee (m=Mul, nnn=Register Dn, s=signed, eeeeee=EA)
	int type=0,rea=0,sign=0,ea=0;
	int use=0;

	type=(op>>14)&1; // div/mul
	rea =(op>> 9)&7;
	sign=(op>> 8)&1;

	ea  = op&0x3f;

	// See if we can do this opcode:
	if (EaCanRead(ea,1)==0) return 1;

	use=OpBase(op);
	use&=~0x0e00; // Use same for all registers
	if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

	OpStart(op); Cycles=type?70:133;

	ot("	u32 a = 0, b = 0;\n");
	if (type == 0) ot("	u32 rm = 0;\n");
	ot("\n");

	EaCalc(0x003f, ea, 1, "off");
	EaRead(        ea, 1, "off", "a");

	EaCalc(0x0e00,rea, 2, "off");
	EaRead(       rea, 2, "off", "b");

	if (type == 0)
	{
		// Divide:
		if (sign)
		{
			ot("	res = (u16)( (s32)b / (s16)a );\n");
			OpGetNZ(1);
			ot("	rm = (s32)b %% (s16)a; // Get remainder\n");
			ot("	res |= ((u16)rm) << 16;\n");
		}
		else
		{
			ot("	res = (u16)(b / a);\n");
			OpGetNZ(1);
			ot("	rm = b %% a; // Get remainder\n");
			ot("	res |= ((u16)rm) << 16;\n");
		}
	}

	if (type==1)
	{
		// Multiply:
		if (sign)
			ot("	res = (s16)a * (s32)b;\n");
		else
			ot("	res = a * b;\n");
		OpGetNZ(2);
	}

	ot("\n");

	EaWrite(rea, 2, "off", "res");

	OpEnd();

	return 0;
}


// --------------------- Opcodes 0x90c0+ ---------------------
// Suba/Cmpa/Adda 1tt1nnnx 11eeeeee (tt=type, x=size, eeeeee=Source EA)
int OpAritha(int op)
{
	int use=0;
	int type=0,size=0,sea=0,dea=0;

	// Suba/Cmpa/Adda/(invalid):
	type=(op>>13)&3; if (type>=3) return 1;

	size=(op>>8)&1; size++;
	dea=(op>>9)&7; dea|=8; // Dest=An
	sea=op&0x003f; // Source

	// See if we can do this opcode:
	if (EaCanRead(sea,size)==0) return 1;

	use=OpBase(op);
	use&=~0x0e00; // Use same opcode for An
	if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

	OpStart(op); Cycles=4;

	ot("	u32 a = 0, b = 0;\n\n");

	EaCalc(0x003f, sea, size, "off");
	EaRead(        sea, size, "off", "b");

	EaCalc(0x0e00, dea, 2, "off");
	EaRead(        dea, 2, "off", "a");

	if (type==0) ot("	res = a - b;\n");
	if (type==1) ot("	res = a - b;\n");
	if (type==1) OpGetNZVC(2, 1, 0); // Get Cmp flags
	if (type==2) ot("	res = a + b;\n");
	ot("\n");

	if (type!=1) EaWrite(dea, 2, "off", "res");

	if (size>=2) { if (sea<0x10) Cycles+=4; else Cycles+=2; }

	OpEnd();

	return 0;
}

// --------------------- Opcodes 0x9100+ ---------------------
// Emit a Subx/Addx opcode, 1t01ddd1 zz000sss addx.z Ds,Dd
int OpAddx(int op)
{
	int use=0;
	int type=0,size=0,dea=0,sea=0;

	type=(op>>12)&5;
	dea =(op>> 9)&7;
	size=(op>> 6)&3; if (size>=3) return 1;
	sea = op&0x3f;

	// See if we can do this opcode:
	if (EaCanRead(sea,size)==0) return 1;
	if (EaCanWrite(dea)==0) return 1;

	use=OpBase(op);
	use&=~0x0e00; // Use same opcode for Dn
	if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

	OpStart(op); Cycles=8;

	ot("	u32 a = 0, b = 0, carry = 0;\n\n");
	EaCalc(0x003f, sea, size, "off");
	EaRead(        sea, size, "off", "b");

	EaCalc(0x0e00, dea, size, "off");
	EaRead(        dea, size, "off", "a");

	ot("	// Do arithmetic:\n");
	ot("	carry = (o.sr >> 4) & 1; \n");

	if (type == 1) ot("	res = a - b + 1 - carry;\n");
	if (type == 5) ot("	res = a + b + carry;\n");

	OpGetNZVC(size, type == 1, 1);
	ot("\n");

	ot("	// Save result:\n");
	EaWrite(dea, size, "off", "a");

	ot("	eprintf(\"EaWrite(%%8x, %%8x)\\n\", off, a);\n"); //djh

	OpEnd();

	return 0;
}



// --------------------- Opcodes 0xb000+ ---------------------
// Emit a Cmp/Eor opcode, 1011rrrt xxeeeeee (rrr=Dn, t=cmp/eor, xx=size extension, eeeeee=ea)
int OpCmpEor(int op)
{
	int rea=0,eor=0;
	int size=0,ea=0,use=0;

	// Get EA and register EA
	rea=(op>>9)&7;
	eor=(op>>8)&1;
	size=(op>>6)&3; if (size>=3) return 1;
	ea=op&0x3f;
	if (eor && (ea&0x38)==0x08) return 1; // This is a cmpm opcode

	// See if we can do this opcode:
	if (EaCanRead(ea,size)==0) return 1;
	if (eor && EaCanWrite(ea)==0) return 1;

	use=OpBase(op);
	use&=~0x0e00; // Use 1 handler for register d0-7
	if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

	OpStart(op); Cycles=eor?8:4;

	ot("	u32 a = 0, b = 0;\n\n");

	ot("	// Get register operand into a:\n");
	EaCalc (0x0e00, rea, size, "off");
	EaRead (        rea, size, "off", "a");

	ot("	// Get EA into off and value into b:\n");
	EaCalc (0x003f,  ea, size, "off");
	EaRead (         ea, size, "off", "b");

	ot("	// Do arithmetic:\n");
	if (eor == 0)
	{
		ot("	res = a - b;\n");
		OpGetNZVC(size, 1);
	}
	if (eor)
	{
		ot("	res = a ^ b;\n");
		OpGetNZ(size);
	}

	ot("\n");

	if (size>=2) Cycles += 4; // Correct?
	if (ea==0x3c) Cycles -= 4;

	if (eor) EaWrite(ea, size, "off", "res");

	OpEnd();
	return 0;
}

