
#include "stdafx.h"
#include "app.h"

int Cycles = 0; // Current cycles for opcode
int Ipos = 0; // Instruction position in words

// Bytes for the disassembler:
static u8 OpData[16] = {0};

static u16 OpRead16(u32 a)
{
  return (u16)( (OpData[a & 15]<<8) | OpData[(a + 1) & 15] );
}

// For opcode 'op' use handler 'use'
void OpUse(int op, int use)
{
	char text[64] = {0};
	ScopTable[op] = use;

	if (op != use) return;

	// Disassemble opcode
	DisaPc = 0;
	DisaText = text;
	DisaWord = OpRead16;

	DisaGet();
	ot("// ---------- [%.4x] %s ----------\n", op, text);
}

void OpStart(int op)
{
	Cycles = 0;
	Ipos = 1;
	OpUse(op, op); // This opcode obviously uses this handler

	ot("static struct Sco Op%.4x(struct Sco o, struct Scour *sc)\n", op);
	ot("{\n");
	ot("	u32 off = 0, val = 0, res = 0;\n\n");
}

void OpEnd()
{
	if (Ipos) ot("	o.pc += %d;\n", Ipos);

	if (Cycles) ot("	o.cycles -= %d;\n", Cycles);
	ot("	(void)sc; (void)off; (void)val; (void)res;\n\n"); // To stop compiler warnings
	ot("	return o;\n");
	ot("}\n\n");
}

int OpBase(int op)
{
  int ea=op&0x3f; // Get Effective Address
  if (ea<0x10) return op&~0xf; // Use 1 handler for d0-d7 and a0-a7
  if (ea>=0x18 && ea<0x28 && (ea&7)==7) return op; // Specific handler for (a7)+ and -(a7)
  if (ea<0x38) return op&~7;   // Use 1 handler for (a0)-(a7), etc...
  return op;
}

// Get NV flags
int OpGetNZ(int size, int xbit)
{
	if (size == 0) ot("	res &= 0xff;\n");
	if (size == 1) ot("	res &= 0xffff;\n");

	if (xbit)
		ot("	o.sr &= ~0x1f; // Clear XNZVC\n");
	else
		ot("	o.sr &= ~0xf; // Clear NZVC\n");

	ot("	o.sr |= (res >> %d) & 8; // N\n", (8 << size) - 4);
	ot("	if (res == 0) o.sr |= 4; // Z\n");
	return 0;
}

// Get NZVC flags and also clips res to the size of the register

int OpGetNZVC(int size, int subtract, int xbit)
{
	OpGetNZ(size, xbit);
	int topbit = 1 << ((8 << size) - 1);

	char *cast = "";
	if (size == 0) cast = "(u8)";
	if (size == 1) cast = "(u16)";

	char* carry = xbit ? "0x11; // X + C" : "1; // C";
	if (subtract)
	{
		ot("	if ((a ^ b) & (a ^ res) & 0x%x) o.sr |= 2; // V\n", topbit);
		ot("	if (%sres > %sa) o.sr |= %s\n", cast, cast, carry);
	}
	else
	{
		ot("	if ((a ^ res) & (b ^ res) & 0x%x) o.sr |= 2; // V\n", topbit);
		ot("	if (%sres < %sa) o.sr |= %s\n", cast, cast, carry);
	}

	return 0;
}


// -----------------------------------------------------------------

void OpAny(int op)
{
	memset(OpData, 0x33, sizeof(OpData));
	OpData[0] = (u8)(op >> 8);
	OpData[1] = (u8)op;

	if ((op & 0xf100) == 0x0000) OpArith(op);
	if ((op & 0xc000) == 0x0000) OpMove(op);
	if ((op & 0xf5bf) == 0x003c) OpArithSr(op); // Ori/Andi/Eori $nnnn,sr
	if ((op & 0xf100) == 0x0100) OpBtstReg(op);
	if ((op & 0xff00) == 0x0800) OpBtstImm(op);
	if ((op & 0xf900) == 0x4000) OpNeg(op);
	if ((op & 0xf1c0) == 0x41c0) OpLea(op);
	if ((op & 0xf9c0) == 0x40c0) OpMoveSr(op);
	if ((op & 0xfff8) == 0x4840) OpSwap(op);
	if ((op & 0xff00) == 0x4a00) OpTst(op);
	if ((op & 0xfff8) == 0x4e50) OpLink(op);
	if ((op & 0xfff8) == 0x4e58) OpUnlk(op);
	if ((op & 0xfff0) == 0x4e60) OpMoveUsp(op);
	if ((op & 0xfff8) == 0x4e70) Op4E70(op); // Reset/Rts etc
	if ((op & 0xff80) == 0x4e80) OpJsr(op);
	if ((op & 0xffb8) == 0x4880) OpExt(op);
	if ((op & 0xfb80) == 0x4880) OpMovem(op);
	if ((op & 0xf000) == 0x5000) OpAddq(op);
	if ((op & 0xf0c0) == 0x50c0) OpSet(op);
	if ((op & 0xf0f8) == 0x50c8) OpDbra(op);
	if ((op & 0xf000) == 0x6000) OpBranch(op);
	if ((op & 0xf100) == 0x7000) OpMoveq(op);
	if ((op & 0xa000) == 0x8000) OpArithReg(op); // Or/Sub/And/Add
	if ((op & 0xb0c0) == 0x80c0) OpMul(op);
	if ((op & 0x90c0) == 0x90c0) OpAritha(op);
	if ((op & 0xb138) == 0x9100) OpAddx(op);
	if ((op & 0xf000) == 0xb000) OpCmpEor(op);
	if ((op & 0xf000) == 0xe000) OpAsr(op); // Asr/l/Ror/l etc
}
