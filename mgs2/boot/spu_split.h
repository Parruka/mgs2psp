/* spu_split.h - v26me: boot/spu_split.c (the recording side, every platform) and its platform's consumer of the commands (the PSP's
   Media Engine in psp/me_mix.c; the PC's at the end of spu_split.c). */
#ifndef SPU_SPLIT_H
#define SPU_SPLIT_H
#include <stdint.h>
#include "spu_split_b.h"
/* the platform */
SplitB *split_plat_b(void);                                     /* engine B's memory (spu_split.c sets its start state) */
int split_plat_start(char *why, int n);                         /* start the consumer; 1 = running (why says how), 0 = not (why) */
void split_plat_push(uint32_t w0, uint32_t w1);                 /* one command, in order */
void split_plat_barrier(void);                                  /* returns once every command pushed so far is consumed */
void split_plat_ram_written(uint32_t addr, uint32_t size);      /* SPU RAM [addr, addr + size) was written (before its RAMW) */
/* spu_split.c */
extern int g_split_on;
void split_ck_b(const SplitCk *ck, const char *extra);          /* B's checkpoint, in order: compared with A's, printed */
void split_report(const char *when);
void split_stop(const char *why);                               /* the consumer is gone: A goes on alone */
uint64_t split_cmds(void);
uint64_t split_frames(void);
#endif
