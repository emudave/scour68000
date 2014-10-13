
#include "Tds.h"

// Disa.c
#include "Disa.h"

// Ea.cpp
int EaCalc(int mask,int ea,int size, char *off = "off");
int EaRead(int ea,int size, char *off = "off", char *val = "val");
int EaCanRead(int ea,int size);
int EaWrite(int ea,int size, char *off = "off", char *val = "val");
int EaCanWrite(int ea);

// Main.cpp
extern int *ScopTable; // Function table
void ot(char *format, ...);
void MemHandler(int type, int size, char *off = "off", char *val = "val");

// OpAny.cpp
extern int Cycles; // Current cycles for opcode
extern int Ipos; // Instruction position in words
void OpAny(int op);
void OpUse(int op, int use);
void OpStart(int op);
void OpEnd();
int OpBase(int op);
int OpGetNZ(int size, int xbit = 0);
int OpGetNZVC(int size, int subtract, int xbit = 0);

//----------------------
// OpArith.cpp
int OpArith(int op);
int OpAddq(int op);
int OpArithReg(int op);
int OpMul(int op);
int OpAritha(int op);
int OpAddx(int op);
int OpCmpEor(int op);

// OpBranch.cpp
int OpLink(int op);
int OpUnlk(int op);
int Op4E70(int op);
int OpJsr(int op);
int OpDbra(int op);
int OpBranch(int op);

// OpLogic.cpp
int OpBtstReg(int op);
int OpBtstImm(int op);
int OpNeg(int op);
int OpSwap(int op);
int OpTst(int op);
int OpExt(int op);
int OpSet(int op);
int OpAsr(int op);

// OpMove.cpp
int OpMove(int op);
int OpLea(int op);
void OpValToStatus(int high);
int OpMoveSr(int op);
int OpArithSr(int op);
int OpMovem(int op);
int OpMoveUsp(int op);
int OpMoveq(int op);
