
#include "app.h"

// ---------------------------------------------------------------------------
// Gets the offset of a register for an ea, and puts it in 'off'
// Shifted left by 'shift'
static int EaCalcReg(int ea, int mask, int forceor, char *off)
{
	int i=0,low=0,needor=0;

	for (i=mask|0x8000; (i&1)==0; i>>=1) low++; // Find out how high up the EA mask is
	mask&=0xf<<low; // This is the max we can do

	if (ea>=8) needor=1; // Need to OR to access A0-7

	if ((mask>>low)&8) if (ea&8) needor=0; // Ah - no we don't actually need to or, since the bit is high in r8

	if (forceor) needor=1; // Special case for 0x30-0x38 EAs ;)

	if (low)
	{
		// Need to shift right:
		ot("	%s = (o.pc[0] & 0x%.4x) >> %d;\n", off, mask, low);
	}
	else
	{
		ot("	%s = o.pc[0] & 0x%.4x;\n", off, mask);
	}

	if (needor) ot("	%s |= 8; // A0-7\n", off);
	return 0;
}

int EaCalc(int mask,int ea,int size, char *off)
{
	char text[32] = {0};

	DisaPc = 2; DisaGetEa(text, ea, size); // Get text version of the effective address
	// EaCalc : Get Effective Address into val:

	if (ea == 0x3c) return 0; // Don't need to do anything for immediate values:

	if (ea < 0x10)
	{
		ot("	// EaCalc : Get register index into '%s':\n", off);

		EaCalcReg(ea, mask, 0, off);
		return 0;
	}

	ot("	// EaCalc : Get '%s':\n", text);

	// (An), (An)+, -(An):
	if (ea<0x28)
	{
		int step=1<<size;

		if ((ea&7)==7 && step<2) step=2; // move.b (a7)+ or -(a7) steps by 2 not 1

		EaCalcReg(ea, mask, 0, off);

		if ((ea&0x38)==0x20) ot("	sc->d[off] -= %d; // Pre-decrement An\n", step);
		if ((ea&0x38)==0x18) ot("	sc->d[off] += %d; // Post-increment An\n", step);

		if ((ea&0x38)==0x18)
			ot("	%s = sc->d[%s] - %d;\n", off, off, step);
		else
			ot("	%s = sc->d[%s];\n", off, off);

		if ((ea&0x38)==0x20) Cycles+=size<2 ? 6:10; // -(An) Extra cycles
		else                 Cycles+=size<2 ? 4:8;  // (An),(An)+ Extra cycles
		return 0;
	}

	if (ea<0x30) // ($nn,An) (di)
	{
		EaCalcReg(8, mask, 0, off);
		ot("	%s = sc->d[%s] + (s16)o.pc[%d]; // Add on offset\n", off, off, Ipos);
		Ipos ++;
		Cycles+=size<2 ? 8:12; // Extra cycles
		return 0;
	}

	if (ea<0x38) // ($nn,An,Rn)
	{
		ot("	{\n");
		ot("		u32 ext = o.pc[%d]; // Get extension word\n", Ipos);
		ot("		s32 rn = 0;\n");
		EaCalcReg(8, mask, 1, off);
		ot("		%s = sc->d[%s];\n", off, off);

		ot("		rn = sc->d[(ext >> 12) & 0xf]; // Rn\n", off);
		ot("		if ((ext & 0x800) == 0) rn = (s16)rn; // Rn.w\n");
		ot("		%s += (s8)ext + rn; // 8-bit signed offset + Rn\n", off);
		ot("	}\n");
		Ipos++;
		Cycles+=size<2 ? 10:14; // Extra cycles
		return 0;
	}

	if (ea == 0x38)
	{
		ot("	// Fetch Absolute Short address:\n");
		ot("	%s = (s16)o.pc[%d];\n", off, Ipos);
		Ipos++;
	    Cycles += size<2 ? 8:12; // Extra cycles
		return 0;
	}

	if (ea == 0x39)
	{
		ot("	// Fetch Absolute Long address:\n");
		ot("	%s = (o.pc[%d] << 16) | o.pc[%d];\n", off, Ipos, Ipos + 1);
		Ipos += 2;
		Cycles += size < 2 ? 12:16; // Extra cycles
		return 0;
	}

	if (ea == 0x3a)
	{
		ot("	%s = (o.pc + %d - sc->membase) << 1; // Get PC\n", off, Ipos);
		ot("	%s += (s16)o.pc[%d]; // Add on offset\n", off, Ipos);
		Ipos++;
		Cycles += size < 2 ? 8:12; // Extra cycles
		return 0;
	}

	if (ea == 0x3b) // ($nn,pc,Rn)
	{
		ot("	{\n");
		ot("		u32 ext = o.pc[%d]; // Get extension word\n", Ipos);
		ot("		%s = sc->d[(ext >> 12) & 0xf]; // Rn\n", off);
		ot("		if ((ext & 0x800) == 0) %s = (s16)%s; // Rn.w\n", off, off);
		ot("		%s += (s8)ext; // 8-bit signed offset\n", off);
		ot("		%s += (o.pc + %d - sc->membase) << 1; // Plus PC\n", off, Ipos);
		ot("	}\n");
		Ipos++;
		Cycles+=size<2 ? 10:14; // Extra cycles
		return 0;
	}

	return 1;
}

int EaRead(int ea, int size, char *off, char *val)
{
	char text[32] = {0};

	DisaPc = 2; DisaGetEa(text, ea, size); // Get text version of the effective address

	if (ea<0x10)
	{
		char *cast = "";

		if (size == 0) cast = "(s8)";
		if (size == 1) cast = "(s16)";
		ot("	// EaRead : Read register[%s] into %s:\n", off, val);
		ot("	%s = %ssc->d[%s];\n", val, cast, off);
		ot("\n"); return 0;
	}

	ot("	// EaRead : Read '%s' into %s:\n", text, val);

	if (ea==0x3c)
	{
		if (size == 0)
		{
			ot("	%s = (s8)o.pc[%d]; // Fetch immediate value\n", val, Ipos);
			Ipos++;
		}
		else if (size == 1)
		{
			ot("	%s = (s16)o.pc[%d]; // Fetch immediate value\n", val, Ipos);
			Ipos++;
		}
		else
		{
			ot("	%s = (o.pc[%d] << 16) | o.pc[%d]; // Fetch 32-bit value\n", val, Ipos, Ipos + 1);
			Ipos += 2;
		}

		ot("\n"); return 0;
	}

	if (ea >= 0x3a && ea <= 0x3b) MemHandler(2, size, off, val); // Fetch
	else                          MemHandler(0, size, off, val); // Read

	ot("\n"); return 0;
}

// Return 1 if we can read this ea
int EaCanRead(int ea, int)
{
	if (ea <= 0x3c) return 1;

	return 0;
}

// ---------------------------------------------------------------------------
int EaWrite(int ea, int size, char *off, char *val)
{
	char text[32] = {0};

	DisaPc = 2; DisaGetEa(text, ea, size); // Get text version of the effective address

	if (ea < 0x10)
	{
		ot("	// EaWrite : Write %s into register[%s]:\n", val, off);
		
		if (size == 0)
		{
			ot("	sc->d[%s] &= ~0xff;\n", off);
			ot("	sc->d[%s] |= %s & 0xff;\n", off, val);
		}
		else if (size == 1)
		{
			ot("	sc->d[%s] &= ~0xffff;\n", off);
			ot("	sc->d[%s] |= %s & 0xffff;\n", off, val);
		}
		if (size == 2)
		{
			ot("	sc->d[%s] = %s;\n", off, val);
		}
		ot("\n"); return 0;
	}

	ot("	// EaWrite: Write %s into '%s' (address in %s):\n", val, text, off);
	MemHandler(1, size, off, val); // Call write handler

	ot("\n"); return 0;
}

// Return 1 if we can write this ea
int EaCanWrite(int ea)
{
	if (ea <= 0x3b) return 1;

	return 0;
}
