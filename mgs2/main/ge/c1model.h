/* c1model.h (sb11, task #134; x86 only): C1 for the codec faces sized before any GE code (c1model.c) */
#ifndef C1MODEL_H
#define C1MODEL_H
#include <stdint.h>
#include <stdio.h>
#include "vu1conv.h"
extern int c1m_on;   /* MGS2_C1_MODEL=1 */
/* one converted P07 call: vm and top as the converter read them, sp the slot's streams (A, B, C, D, E, F) or 0 for the VU memory's
   vertices, n its vertices, L its record, v1 pass 1's vertices, v3 pass 3's (0 when not drawn) */
void c1m_call(const uint8_t *vm, uint32_t top, const void *const *sp, int n, const VcLit *L, const void *v1, const void *v3);
void c1m_report(FILE *f);
#endif
