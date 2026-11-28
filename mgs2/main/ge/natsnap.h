/* natsnap.h: the native renderer's snapshot, step 1 (native_renderer_design.md 6.1; native_step0_report.md): ship meshes (the 0x1214a0
   builder) only.  One record a group, self-contained (step 0 kept a 32-byte record a call beside a 32-byte one a group: the walk then
   needed both and a place for the header quadword; here the group carries its call's words, so the ME copies and reads one array and
   the header's four words stay where the translator may keep a pointer to them until the transfer ends).  The layout both CPUs share:
   guest addresses and words, no host pointers.  The main CPU (boot/nat_stub.h) writes the groups inside the pipeline call, one array per
   packet buffer; the walk (ge/xlme.c lw_walk; the x86 twin's runtime walk, boot/nat_x86.c) reads them while that buffer's transfers run.
   Game data passes through it: personal use only. */
#ifndef MGS2_NATSNAP_H
#define MGS2_NATSNAP_H
#include <stdint.h>

#define NAT_MAXGRP 2304          /* groups a picture (a pipeline call's packet buffer) holds (the routes from vb 24,000: 2,079 at most; the opening 2,572
                                    [native_step0_report]); a call that does not fit runs the builder (boot/nat_stub.h).  The same on the
                                    twin and the PSP: the stub's choices are part of the run */
#define NAT_PKT0 0x1b00000u      /* packet memory: two buffers of 2 MB, [NAT_PKT0, NAT_PKT1) */
#define NAT_PKTM 0x1d00000u
#define NAT_PKT1 0x1f00000u
/* the packet buffer of RAM offset a (within packet memory): 1 the lower one (as step 0's nat_s[] index) */
#define NAT_BUF(a) ((a) < NAT_PKTM)

enum { NATG_FULL = 1,            /* the full template: q0, the constant block's REF, first (the call's first two groups) */
       NATG_PKT = 2,             /* x86 shadow modes only: the builder ran too, the group's packet lies at key (the walk steps over it) */
       NATG_INPL = 4,            /* gl33 (boot/nat_stub.h's rule; MGS2_NAT_INPL): a static group, the P04+df0 entry with its three streams sent
                                    from outside packet memory: the main CPU's copy plan leaves the three streams in RAM, where the ME's
                                    walk reads them (ge/xlme.c xc_nat) */
       NATG_FAST = 8,            /* gl34 (MGS2_NAT_FAST, MGS2_NAT_FAST_CYCLE; boot/nat_stub.h flags every group of a pipeline call in a
                                    fast turn): the walk hands the group to the translator's fast front end (ge/xlate.h xl_p04_fast),
                                    which leaves the same translator state and makes the same converter call as xl_p04_core, or
                                    declines (then xl_p04_core takes it as always) */
       NATG_KEEP = 0xf0 };       /* gl35 (MGS2_NAT_KEEP, MGS2_NAT_KEEP_CYCLE; fast groups only): flags >> 4 = the keep bits of the turn the
                                    group was recorded in (ge/xlate.h xl_p04_fast: 1 drawn areas, 2 A+D packet, 4 / 8 texture) */
#define NAT_DF0_MSCAL 0x140001beu   /* gl33: MSCAL to P04+0xdf0 (pc 0x1be), the plain textured entry */
typedef struct {                 /* 48 B */
  uint32_t key;                  /* the order key: the RAM address (as the packet control holds it) of the next packet item at the call
                                    (stubbed: every group of a call has its call's), or the group's own first tag (x86 shadow modes) */
  uint32_t blk;                  /* SPR 0xc54 = mesh+0xFC: the constant block (packet memory), q0's ADDR (REF 24; NATG_FULL only) */
  uint32_t mat;                  /* desc+0x14: the material block, q1's ADDR (REF 8) */
  uint32_t s[3];                 /* streams A, B, C: the REF's ADDR (the mesh's cursor), 0: not sent (CNT 0 and a NOP) */
  uint8_t n[3];                  /* their quadwords: desc+0x0E, +0x0F, +0x10 (0: not sent) */
  uint8_t flags;                 /* NATG_* */
  uint32_t hdr[4];               /* q6, the header quadword: desc+0x0C the vertex count, 0, SPR 0xb90 the fog word, mesh+0xC8 & 1 */
  uint32_t mscal;                /* q7 x: 0x14000000 | the VU1 entry's halfword (SPR 0xb9c + 2 idx, sign-extended) */
} NatGrp;

/* a packet buffer's groups as the main CPU writes them (boot/nat_stub.h g_nat[2]); the host gives g: an array each, or the same one
   to both when it has room for one only (0: no memory: the stub's choices stay, nothing is drawn) */
typedef struct {
  NatGrp *g;                     /* NAT_MAXGRP entries, or 0 */
  uint32_t n;                    /* groups written in this picture (counted without g too) */
  uint32_t cur;                  /* groups the transfers so far have taken (the plan's or the direct walk's; the host's) */
  uint32_t draw;                 /* 1: the walk draws them (arm B); 0: none to draw */
  uint32_t seq;                  /* the pipeline call that wrote them (boot.c g_dg_seq) */
  uint32_t vq;                   /* quadwords the builder would have written for them (the virtual packet position) */
  uint32_t last_key;             /* the last group's key (RAM offset) */
  uint32_t ru;                   /* gl36: 1: the picture's runs are reused (MGS2_NAT_REUSE, _CYCLE; ge/xlate.h xl_nr_*), set as the array starts */
} NatBuf;
/* the stub's counts (boot/nat_stub.h g_nat_c): calls recorded (arm B, or the x86 shadow arms), their groups; calls stubbed with no record
   (arm C); calls with no group; arm B calls that ran the builder: the array full, a key below the last, the virtual position near the
   buffer's end, another reason (the destination outside the buffer, a count over its cap); calls recorded with no array; pictures
   (pipeline calls) with a record; the most groups in one; groups no transfer had taken when their array was written again, and the
   pictures that had such */
enum { NC_REC, NC_GRP, NC_STUBC, NC_EMPTY, NC_FB_FULL, NC_FB_ORDER, NC_FB_END, NC_FB_OTHER, NC_NOMEM, NC_PICS, NC_GRP_MAX, NC_LEFT, NC_LEFT_PICS, NC_N };
#endif
