// PspProf: dev-only execution profiler for the headless build (env PSPPROF_FLAG=<hex address of a u32 the game sets to 1 while profiling>,
// PSPPROF_OUT=<file>): per-PC instruction counts plus a simulated Allegrex I-cache / D-cache (16 KB, 2-way, 64-byte lines, LRU), so the
// misses each instruction causes can be attributed.  Dumped as text "pc count imiss dmiss" when the flag drops back to 0 (or at exit).
#pragma once
#include <cstdint>
#include <xmmintrin.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <atomic>
#include <unordered_map>
#include <array>
#include "Core/MemMap.h"
#include "Core/MIPS/MIPS.h"
namespace PspProf {
struct Cache {
	uint32_t tag[128][2]; uint8_t lru[128]; // lru: the way to replace next
	uint8_t dirty[128][2];                  // local change (MGS2): the D-cache's written lines (accessD)
	void reset() { for (int i = 0; i < 128; i++) { tag[i][0] = tag[i][1] = 0xffffffffu; lru[i] = 0; dirty[i][0] = dirty[i][1] = 0; } }
	// true on a miss
	inline bool access(uint32_t a) {
		uint32_t line = a >> 6, set = line & 127;
		if (tag[set][0] == line) { lru[set] = 1; return false; }
		if (tag[set][1] == line) { lru[set] = 0; return false; }
		int w = lru[set]; tag[set][w] = line; lru[set] = (uint8_t)(w ^ 1); return true;
	}
	// local change (MGS2): the D-cache as a write-back, write-allocate cache: a store marks its line written; a miss that replaces a written
	// line writes it back first (*wb), the bus traffic the GE shares (audit H2); true on a miss
	inline bool accessD(uint32_t a, bool st, bool *wb) {
		uint32_t line = a >> 6, set = line & 127; *wb = false;
		if (tag[set][0] == line) { lru[set] = 1; if (st) dirty[set][0] = 1; return false; }
		if (tag[set][1] == line) { lru[set] = 0; if (st) dirty[set][1] = 1; return false; }
		int w = lru[set]; *wb = tag[set][w] != 0xffffffffu && dirty[set][w]; tag[set][w] = line; dirty[set][w] = st; lru[set] = (uint8_t)(w ^ 1); return true;
	}
	// local change (MGS2): cache 0x19 / 0x1b (hit invalidate, hit writeback invalidate) drop the line; the freed way is replaced next;
	// true when 0x1b (wb) wrote a written line back
	inline bool inval(uint32_t a, bool wb = false) {
		uint32_t line = a >> 6, set = line & 127; bool w = false;
		if (tag[set][0] == line) { w = wb && dirty[set][0]; tag[set][0] = 0xffffffffu; dirty[set][0] = 0; lru[set] = 0; }
		else if (tag[set][1] == line) { w = wb && dirty[set][1]; tag[set][1] = 0xffffffffu; dirty[set][1] = 0; lru[set] = 1; }
		return w;
	}
};
// fully associative LRU I-cache of the same size (256 lines): the misses no layout can remove (PSPPROF_FA=1)
struct FACache {
	std::unordered_map<uint32_t, uint64_t> last; uint64_t clock = 0; std::vector<std::pair<uint64_t,uint32_t>> dummy;
	// LRU via a stamp map + size cap: on a miss with a full cache, evict the oldest stamp (linear scan of 256 entries)
	uint32_t lines[256]; uint64_t stamp[256]; int n = 0;
	void reset() { n = 0; clock = 0; }
	inline bool access(uint32_t a) {
		uint32_t line = a >> 6; clock++;
		for (int i = 0; i < n; i++) if (lines[i] == line) { stamp[i] = clock; return false; }
		if (n < 256) { lines[n] = line; stamp[n] = clock; n++; return true; }
		int o = 0; for (int i = 1; i < 256; i++) if (stamp[i] < stamp[o]) o = i;
		lines[o] = line; stamp[o] = clock; return true;
	}
};
struct State {
	bool inited = false, enabled = false, active = false; uint32_t flagAddr = 0; const char *out = nullptr;
	uint32_t tick = 0; uint64_t total = 0, imiss = 0, dmiss = 0, dacc = 0;
	std::vector<uint32_t> cnt, im, dm; Cache ic, dc; int dumps = 0; bool fa = false; FACache fic; uint64_t faimiss = 0; uint32_t faline = 0xffffffffu;
	std::unordered_map<uint64_t, uint32_t> calls;   // (call site pc << 32 | target) -> count
	bool sw = false;   // local change (MGS2): PSPPROF_SWITCH=1, see WantCore
	// local change (MGS2): PSPPROF_ITRACE=<file>: the counted instruction fetches as runs of sequential pcs (uint32 pc, uint32 length;
	// a run is cut at 255), for code-layout experiments (mgs2/tools/layout_sim.c)
	FILE *it = nullptr; uint32_t rstart = 0, rlen = 0, rnext = 0;
	// PSPPROF_ITRACE_ALL=1: the trace covers every profiled window (flushed at each dump; a window starts with the record {0xffffffff, 0});
	// PSPPROF_ITRACE_MARK=1: the paused fetches (flag 2: draw pipeline, GS walk) are traced too, with bit 31 of the length set
	bool itAll = false, itMark = false; uint32_t rmark = 0;
	std::vector<uint32_t> dh;   // local change (MGS2): PSPPROF_DHIST=1: D-misses per 1 KB of data address (0x08000000-0x0bffffff), <out>.dhist
	uint32_t dhShift = 10;      // PSPPROF_DHIST=64: per 64-byte line instead
	std::vector<uint32_t> da;   // local change (MGS2): PSPPROF_DACC=1: counted data accesses per 64-byte line (0x08000000-0x0bffffff), <out>.dacc
	// local change (MGS2): PSPPROF_DWATCH=<lo hex>:<hi hex>[,<lo>:<hi>...] (up to 4 ranges): counted data accesses into each range per pc, <out>.dwatch
	uint32_t dwLo[4] = {}, dwHi[4] = {}; int dwN = 0; std::unordered_map<uint64_t, uint32_t> dw;
	// local change (MGS2): PSPPROF_BUCKET=<hex address of a u32 the game sets>: the counted costs also go to the bucket named by that word's
	// value at each instruction (the harness's GEN_ACTOR_PROF build: the act function of the actor running, 0 outside), <out>.buckets
	uint32_t bucketAddr = 0, bkey = 0xffffffffu; uint64_t *bcur = nullptr; std::unordered_map<uint32_t, std::array<uint64_t, 3>> bk;
	bool bonly = false; uint32_t bonlyKey = 0;   // PSPPROF_BUCKET_ONLY=<hex key>: count nothing outside that bucket (one actor's own profile)
	// local change (MGS2): cache ops on the D-cache model (PPSSPP runs them as no-ops): 0x18 / 0x1c create dirty exclusive = the line without
	// a fill (no miss charged), 0x1e / 0x1f fill = the line fetched ahead (counted in pref, not in dmiss: its cost is a hardware question),
	// 0x19 / 0x1b drop the line.  PSPPROF_DSKIP=<lo hex>:<hi hex>[,...] (up to 8 ranges): data accesses there bypass the D-cache model (what
	// moving that data to the PSP scratchpad would do: a scratchpad load costs a cache hit, v8 probe); counted in dskip.
	uint64_t cde = 0, pref = 0, cinv = 0, dskip = 0; uint32_t dsLo[8] = {}, dsHi[8] = {}; int dsN = 0;
	// local change (MGS2): dst = the D-misses that were stores (in dmiss too), dwb = written lines written back (replaced, or cache 0x1b)
	uint64_t dst = 0, dwb = 0;
	void itFlush() { if (it && rlen) { uint32_t w[2] = { rstart, rlen | rmark }; fwrite(w, 4, 2, it); } rlen = 0; }
};
extern State g;
void Init();
void Dump();
static constexpr uint32_t BASE = 0x08800000u, SIZE = 0x04000000u;   // MGS2T: 51 MB image at 0x08804000 (was 24 MB for TT)
// local change: the interpreter computes in the PSP's FPU mode: FCR31 rounding mode on the host, FS as FTZ only (no DAZ)
inline void ApplyFcr31Host(uint32_t fcr31) {
	static const uint32_t rc[4] = {0, 3, 2, 1};   // MIPS RN RZ RP RM -> x86 RC
	uint32_t csr = _mm_getcsr() & ~(0x6000u | 0x8040u);
	csr |= rc[fcr31 & 3] << 13; if (fcr31 & 0x01000000u) csr |= 0x8000u;
	_mm_setcsr(csr);
}
inline uint32_t memAddr(MIPSState *mips, uint32_t op, bool *isMem) {
	uint32_t opc = op >> 26; *isMem = false;
	switch (opc) {
	case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2e:
	case 0x30: case 0x31: case 0x32: case 0x38: case 0x39: case 0x3a:
		*isMem = true; return mips->r[(op >> 21) & 31] + (uint32_t)(int32_t)(int16_t)(op & 0xffff);
	case 0x35: case 0x36: case 0x3d: case 0x3e:
		*isMem = true; return mips->r[(op >> 21) & 31] + (uint32_t)(int32_t)(int16_t)(op & 0xfffc);
	}
	return 0;
}
inline void Exec(MIPSState *mips, uint32_t pc, uint32_t op) {
	if (!g.inited) Init();
	if (!g.enabled) return;
	if ((++g.tick & 4095) == 0) {
		bool on = Memory::IsValidAddress(g.flagAddr) && Memory::ReadUnchecked_U32(g.flagAddr) != 0;
		if (on && !g.active) { g.active = true; g.ic.reset(); g.dc.reset(); g.fic.reset(); g.faline = 0xffffffffu;
			if (g.it && g.itAll) { g.itFlush(); uint32_t w[2] = { 0xffffffffu, 0 }; fwrite(w, 4, 2, g.it); } }
		else if (!on && g.active) { g.active = false; Dump(); }
	}
	if (!g.active) return;
	// local change (MGS2): flag 2 = paused (the game sets it around code the profile leaves out): the caches run on, nothing is counted
	const bool cnt = Memory::ReadUnchecked_U32(g.flagAddr) != 2 && (!g.bonly || Memory::ReadUnchecked_U32(g.bucketAddr) == g.bonlyKey);
	uint32_t k = (pc - BASE) >> 2; bool inRange = cnt && (pc - BASE) < SIZE;
	if (cnt) g.total++;
	if (inRange) g.cnt[k]++;
	if (cnt && g.bucketAddr) { uint32_t key = Memory::ReadUnchecked_U32(g.bucketAddr); if (key != g.bkey || !g.bcur) { g.bkey = key; g.bcur = g.bk[key].data(); } g.bcur[0]++; }
	if (g.ic.access(pc & 0x1fffffffu)) { if (cnt) { g.imiss++; if (g.bucketAddr) g.bcur[1]++; } if (inRange) g.im[k]++; }
	if (g.it && (cnt || g.itMark)) { uint32_t m = cnt ? 0 : 0x80000000u;
		if (pc == g.rnext && g.rlen < 255 && m == g.rmark) g.rlen++; else { g.itFlush(); g.rstart = pc; g.rlen = 1; g.rmark = m; } g.rnext = pc + 4; }
	if (g.fa) { uint32_t ln = (pc & 0x1fffffffu) >> 6; if (ln != g.faline) { g.faline = ln; if (g.fic.access(pc & 0x1fffffffu) && cnt) g.faimiss++; } }
	if (cnt && (op >> 26) == 3) g.calls[((uint64_t)pc << 32) | ((pc & 0xf0000000u) | ((op & 0x03ffffffu) << 2))]++;
	else if (cnt && (op >> 26) == 0 && (op & 0x3f) == 9) g.calls[((uint64_t)pc << 32) | mips->r[(op >> 21) & 31]]++;
	bool isMem; uint32_t a = memAddr(mips, op, &isMem);
	if ((op >> 26) == 0x2f) {
		uint32_t ca = mips->r[(op >> 21) & 31] + (uint32_t)(int32_t)(int16_t)(op & 0xffff), cpa = ca & 0x3fffffffu, fn = (op >> 16) & 31;
		if (!(ca & 0x40000000u) && cpa >= 0x08000000u && cpa < 0x0c000000u) {
			bool wb = false;
			if (fn == 0x18 || fn == 0x1c) { g.dc.accessD(cpa, true, &wb); if (cnt) g.cde++; }
			else if (fn == 0x1e || fn == 0x1f) { if (g.dc.accessD(cpa, false, &wb) && cnt) g.pref++; }
			else if (fn == 0x19 || fn == 0x1b) { wb = g.dc.inval(cpa, fn == 0x1b); if (cnt) g.cinv++; }
			if (wb && cnt) g.dwb++;
		}
	}
	if (isMem) {
		uint32_t pa = a & 0x3fffffffu;
		bool skip = false; for (int r = 0; r < g.dsN; r++) if (pa - g.dsLo[r] < g.dsHi[r] - g.dsLo[r]) skip = true;
		if (skip) { if (cnt) { g.dacc++; g.dskip++; } }
		else if (!(a & 0x40000000u) && pa >= 0x08000000u && pa < 0x0c000000u) { if (cnt) { g.dacc++; if (!g.da.empty()) g.da[(pa - 0x08000000u) >> 6]++; for (int r = 0; r < g.dwN; r++) if (pa - g.dwLo[r] < g.dwHi[r] - g.dwLo[r]) g.dw[((uint64_t)r << 32) | pc]++; }
			const uint32_t opc = op >> 26; const bool st = opc == 0x28 || opc == 0x29 || opc == 0x2a || opc == 0x2b || opc == 0x2e || opc == 0x38 || opc == 0x39 || opc == 0x3a || opc == 0x3d || opc == 0x3e;
			bool wb; const bool miss = g.dc.accessD(pa, st, &wb); if (wb && cnt) g.dwb++;
			if (miss) { if (cnt) { g.dmiss++; if (st) g.dst++; if (!g.dh.empty()) g.dh[(pa - 0x08000000u) >> g.dhShift]++; if (g.bucketAddr) g.bcur[2]++; } if (inRange) g.dm[k]++; } }
	}
}
// local change (MGS2): with PSPPROF_SWITCH=1 the JIT runs until the flag turns non-zero, the (profiled) interpreter while it is, the JIT again
// after it drops (the dump happens here then): 1 = want the interpreter, 0 = want the JIT, -1 = leave the core alone (MIPS.cpp RunLoopUntil)
inline int WantCore() {
	if (!g.inited) Init();
	if (!g.enabled || !g.sw || !Memory::IsValidAddress(g.flagAddr)) return -1;
	bool on = Memory::ReadUnchecked_U32(g.flagAddr) != 0;
	if (!on && g.active) { g.active = false; Dump(); }
	return on ? 1 : 0;
}
}
