
// Dave's Disa 68000 Disassembler

#ifdef __cplusplus
extern "C" {
#endif

#include "Tds.h"

#define CPU_CALL

extern u32 DisaPc;
extern char *DisaText; // Text buffer to write in

extern u16 (CPU_CALL *DisaWord)(u32 a);
int DisaGetEa(char *t,int ea,int size);

int DisaGet();

#ifdef __cplusplus
} // End of extern "C"
#endif
