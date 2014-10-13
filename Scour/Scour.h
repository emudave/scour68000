
// Scour 68000 Emulator - Header File

// Copyright (c) 2014 Dave

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifdef __cplusplus
extern "C" {
#endif

#include "Tds.h"

extern int ScourVer; // Version number of library

// Parts of the CPU which needs to be accessed fast (hopefully passed in registers)
struct Sco
{
	u16 *pc; // Memory Base + PC
	int sr; // Status Register, including CCR
 	int cycles;
};

// The entire CPU state:
struct Scour
{
	u32 d[16]; // D0-7 and A0-7
	struct Sco o;
	int irq; // Interrupt level
	u32 osp; // Other Stack Pointer (USP/SSP)
	u16 *membase; // Memory Base (Host address minus 68000 address)
	u16 *(*checkpc)(u16 *pc);
	u8  (*read8  )(u32 a);
	u16 (*read16 )(u32 a);
	u32   (*read32 )(u32 a);
	void (*write8 )(u32 a,u8  d);
	void (*write16)(u32 a,u16 d);
	void (*write32)(u32 a,u32   d);
	u8  (*fetch8 )(u32 a);
	u16 (*fetch16)(u32 a);
	u32   (*fetch32)(u32 a);
};

typedef struct Sco (* ScopFunc)(struct Sco, struct Scour *);
extern ScopFunc ScopTable[0x10000];

void ScourRun(struct Scour *sc);

// Application defined:
#ifdef _MSC_VER
#define MY_CDECL __cdecl
#else
#define MY_CDECL
#endif
int MY_CDECL eprintf(char *format, ...);

#ifdef __cplusplus
} // End of extern "C"
#endif

