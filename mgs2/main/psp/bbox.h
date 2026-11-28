/* psp/bbox.h - gl44 block A, H2 (2026-11-27; attic/gl44/state.md): the game thread's phase words, read once a second by the black box
   thread (psp/bbox.c).  The game thread is their only writer: a phase tag (two letters and a detail byte) at each boundary, and a count
   of the changes (it moves many times a step while the game thread runs).  Plain stores; nothing is read back on the game thread. */
#ifndef MGS2_BBOX_H
#define MGS2_BBOX_H
#include <stdint.h>
extern volatile uint32_t g_bb_ph, g_bb_n;
#define BB_PH(a, b, d) ((uint32_t)(a) | (uint32_t)(b) << 8 | ((uint32_t)(d) & 0xffu) << 16)
#define BB_ST BB_PH('S', 'T', 0)   /* the start, before the game (probes, the crash catcher's question) */
#define BB_LG BB_PH('L', 'G', 0)   /* the game's logic: no timed part open (runtime/host_time.h) */
#define BB_HK BB_PH('H', 'K', 0)   /* the vblank hook's timing part (main_psp.c host_vb_begin: the window line, the screen) */
#define BB_HS BB_PH('H', 'S', 0)   /* the hook between host_vb_begin and host_vb_end: the state hash when one is due (boot.c state_hash) */
#define BB_HE BB_PH('H', 'E', 0)   /* the hook's end (host_vb_end: the shots' turn, the picture's hand-over) */
#define BB_LC BB_PH('L', 'C', 0)   /* log commit (main_psp.c log_put: log.txt's open, write, close) */
#define BB_PT BB_PH('P', 'T', 0)   /* scePowerTick (the window's call, as gl43) */
#define BB_OV BB_PH('O', 'V', 0)   /* a stage overlay's load (main_psp.c stage_ovl_call: ovl_<stage>.bin read) */
#define BB_DR BB_PH('D', 'R', 0)   /* a disc pack read (rt/iop_host.h disc_pak_read; its sector and count in the record's d field) */
#define BB_SH BB_PH('S', 'H', 0)   /* a shot's write (psp/xl_psp.c gl_shot, gl_live: shot_ / zbuf_ / live_ files) */
#define BB_MW(why) BB_PH('M', 'W', why)   /* the main CPU waiting for the ME's loop (xl_psp.c mq_wait, mq_probe; detail: the wait's MW_ kind) */
#define BB_EN BB_PH('E', 'N', 0)   /* the run's end (main_psp.c finish: the summary, then the wait for X) */
#define BB_EX BB_PH('E', 'X', 0)   /* the program exits in order (gl1_glue.c's sceKernelExitGame wrap: the last record) */
/* the timed parts (runtime/host_time.h HOST_*): IO the IOP and sound, GS the GS walk, DP the draw pipeline, XL the translator, PI the
   picture's work, GW the wait for the GE */
#define BB_REG(k) ((k) == 0 ? BB_PH('I', 'O', 0) : (k) == 1 ? BB_PH('G', 'S', 0) : (k) == 2 ? BB_PH('D', 'P', 0) : (k) == 3 ? BB_PH('X', 'L', 0) \
                   : (k) == 4 ? BB_PH('P', 'I', 0) : (k) == 5 ? BB_PH('G', 'W', 0) : BB_PH('R', '?', k))
static inline uint32_t bb_set(uint32_t v) { uint32_t o = g_bb_ph; g_bb_ph = v; g_bb_n++; return o; }   /* the tag before, for its return */
/* psp/bbox.c */
int bb_rotate(const char *dir);                 /* H1: main() before any file is written */
void bb_rotate_report(void (*o)(const char *, ...));   /* H1's results into log.txt, once it is open */
void bb_start(const char *dir);                 /* H2: after mgs2.cfg is read (MGS2_BBOX): bbox.txt at its full size */
void bb_go(void);                               /* H2: the thread, at the first window (the picture's system memory taken by then) */
void bb_exit(void);                             /* the last record (gl1_glue.c's sceKernelExitGame wrap) */
void bb_report(void (*o)(const char *, ...));   /* the totals, in the end's summary */
uint64_t bb_disc_clock(void);                   /* rt/iop_host.h g_disc_clock: the pack read's start and end, as main_psp.c now() */
#endif
