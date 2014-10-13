
#include "stdafx.h"
#include "app.h"

static FILE *OutFile = NULL;
static int ScourVer = 0x0005; // Version number of library
int *ScopTable = 0; // Function table

void ot(char *format, ...)
{
	va_list valist = NULL;
	va_start(valist, format);
	if (OutFile) vfprintf(OutFile, format, valist);
	va_end(valist);
}

static void PrintHeader()
{
	ot("\n// Dave's Scour 68000 Emulator v%x.%.3x - C Output\n\n", ScourVer>>12, ScourVer&0xfff);

	ot("#include \"Scour.h\"\n\n");

	ot("ScopFunc ScopTable[];\n\n");

	ot("static struct Sco CheckInterrupt(struct Sco o, struct Scour *sc)\n");
	ot("{\n");
	ot("	int mask = 0;\n");
	ot("	u32 tmp = 0;\n");
	ot("\n");
	ot("	mask = (o.sr >> 8) & 7; // Get interrupt mask\n");
	ot("	if (sc->irq < 7 && sc->irq <= mask) return o; // This interrupt level is not allowed\n");
	ot("\n");
	ot("	// Get PC:\n");
	ot("	tmp = (o.pc - sc->membase) << 1;\n");
	ot("	// Push PC\n");
	ot("	sc->d[0xf] -= 4;\n");
	ot("	sc->write32(sc->d[0xf], tmp);\n");
	ot("\n");
	ot("	// Push SR\n");
	ot("	sc->d[0xf] -= 2;\n");
	ot("	sc->write16(sc->d[0xf], (u16)o.sr);\n");
	ot("\n");
	ot("	// Get IRQ vector address:\n");
	ot("	tmp = sc->read32(0x60 + (sc->irq << 2));\n");
	ot("	o.pc = sc->membase + (tmp >> 1);\n");
	ot("	o.sr = 0x2000 | (sc->irq << 8); // Supervisor mode + IRQ number\n");
	ot("	sc->irq = 0; // Clear IRQ todo - is this right?\n");
	ot("	o.cycles -= 46; // Take some cycles\n");
	ot("	o.pc = sc->checkpc(o.pc);\n");
	ot("	return o;\n");
	ot("}\n\n");

	ot("void ScourRun(struct Scour *sc)\n");
	ot("{\n");
	ot("	struct Sco o = sc->o;\n");

	ot("	if (sc->irq)\n");
	ot("	{\n");
	ot("		o = CheckInterrupt(o, sc);\n");
	ot("		// Check for interrupt using up all the cycles:\n");
	ot("		if (o.cycles < 0) { sc->o = o; return; }\n");
	ot("	}\n");

	ot("	do { o = ScopTable[*o.pc] (o, sc); } while (o.cycles > 0);\n");
	ot("	sc->o = o;\n");
	ot("}\n\n");
}

void MemHandler(int type, int size, char *off, char *val)
{
	char *cast = "", *scast = "";

	if (size == 0) { cast = "(u8)";  scast = "(s8)";  }
	if (size == 1) { cast = "(u16)"; scast = "(s16)"; }

	if (type == 0)
	{
		ot("	%s = %ssc->read%d(%s);\n", val, scast, 8 << size, off);
	}
	else if (type == 1)
	{
		ot("	sc->write%d(%s, %s%s);\n", 8 << size, off, cast, val);
	}
	else
	{
		ot("	%s = %ssc->fetch%d(%s);\n", val, scast, 8 << size, off);
	}
}

static void PrintOpcodes()
{
	int op = 0;

	printf("Creating Opcodes: [");

	ot("// ---------------------------- Opcodes ---------------------------\n");

	// Emit null opcode:
	ot("// Called if an opcode is not recognised:\n");
	ot("static struct Sco Op____(struct Sco o, struct Scour *sc) { (void)sc; o.cycles = -256; return o; }\n\n");

	for (op = 0; op < 0x10000; op++)
	{
		if ((op & 0xfff) == 0) { printf("%x", op >> 12); fflush(stdout); } // Update progress

		OpAny(op);
	}

	ot("\n");
	printf("]\n");
}

static void PrintFuncTable()
{
	int i = 0, len = 0x10000;
	ot("ScopFunc ScopTable[0x10000] = {\n");

	for (i = 0; i < len; i++)
	{
		int op = ScopTable[i];
		
		if ((i & 15) == 0) ot("\t");
		if (op < 0) ot("Op____"); else ot("Op%.4x", op);
		
		if (i + 1 < len) ot(","); else ot(" ");
		if ((i & 15) == 15) ot(" // %.4x\n", i - 15);
	}

	ot("};\n");
}

static void PrintFooter()
{
	ot("// --------------------------- Footer --------------------------\n\n");

}

static int ScourMake()
{
	char *name = "Scour.c";

	OutFile = fopen(name, "wt"); if (OutFile == NULL) return 1;

	printf("Making %s...\n",name);

	ScopTable = new int[0x10000];
	if (ScopTable == 0) return 1;
	memset(ScopTable, 0xff, 0x10000 * sizeof(int) ); // Init to -1

	PrintHeader();
	PrintOpcodes();
	PrintFuncTable();
	PrintFooter();

	fclose(OutFile); OutFile = NULL;

	return 0;
}

int main()
{
	printf("\n  Dave's Scour 68000 Emulator v%x.%.3x - Core Creator\n\n", ScourVer>>12, ScourVer&0xfff);

	ScourMake();

	return 0;
}
