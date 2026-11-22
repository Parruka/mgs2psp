#include <cstring>
#include "Core/MIPS/PspProf.h"
namespace PspProf {
State g;
static void atExit() { if (g.active) Dump(); }
void Init() {
	g.inited = true;
	const char *f = getenv("PSPPROF_FLAG"); g.out = getenv("PSPPROF_OUT");
	if (!f || !g.out) return;
	g.flagAddr = (uint32_t)strtoul(f, nullptr, 16); g.enabled = true; { const char *fa = getenv("PSPPROF_FA"); g.fa = fa && *fa; } { const char *sw = getenv("PSPPROF_SWITCH"); g.sw = sw && *sw == '1'; }
	{ const char *it = getenv("PSPPROF_ITRACE"); if (it && *it) g.it = fopen(it, "wb"); }
	{ const char *ia = getenv("PSPPROF_ITRACE_ALL"); g.itAll = ia && *ia == '1'; const char *im = getenv("PSPPROF_ITRACE_MARK"); g.itMark = im && *im == '1'; }
	{ const char *dh = getenv("PSPPROF_DHIST"); if (dh && !strcmp(dh, "64")) g.dhShift = 6; if (dh && (*dh == '1' || g.dhShift == 6)) g.dh.assign(0x04000000u >> g.dhShift, 0); }
	{ const char *da = getenv("PSPPROF_DACC"); if (da && *da == '1') g.da.assign(0x04000000u >> 6, 0); }
	{ const char *dw = getenv("PSPPROF_DWATCH"); while (dw && *dw && g.dwN < 4) { unsigned lo, hi; if (sscanf(dw, "%x:%x", &lo, &hi) != 2) break;
	    g.dwLo[g.dwN] = lo & 0x3fffffffu; g.dwHi[g.dwN] = hi & 0x3fffffffu; g.dwN++; dw = strchr(dw, ','); if (dw) dw++; } }
	{ const char *ds = getenv("PSPPROF_DSKIP"); while (ds && *ds && g.dsN < 8) { unsigned lo, hi; if (sscanf(ds, "%x:%x", &lo, &hi) != 2) break;
	    g.dsLo[g.dsN] = lo & 0x3fffffffu; g.dsHi[g.dsN] = hi & 0x3fffffffu; g.dsN++; ds = strchr(ds, ','); if (ds) ds++; } }
	{ const char *bk = getenv("PSPPROF_BUCKET"); if (bk && *bk) g.bucketAddr = (uint32_t)strtoul(bk, nullptr, 16); }
	{ const char *bo = getenv("PSPPROF_BUCKET_ONLY"); if (bo && *bo && g.bucketAddr) { g.bonly = true; g.bonlyKey = (uint32_t)strtoul(bo, nullptr, 16); } }
	g.cnt.assign(SIZE / 4, 0); g.im.assign(SIZE / 4, 0); g.dm.assign(SIZE / 4, 0); g.ic.reset(); g.dc.reset();
	atexit(atExit);
	fprintf(stderr, "PspProf: flag at %08x, output %s\n", g.flagAddr, g.out);
}
void Dump() {
	if (g.it) { g.itFlush(); if (g.itAll) fflush(g.it); else { fclose(g.it); g.it = nullptr; } }   // the first profiled window only, unless PSPPROF_ITRACE_ALL
	char name[512]; snprintf(name, sizeof name, g.dumps ? "%s.%d" : "%s", g.out, g.dumps); g.dumps++;
	FILE *fp = fopen(name, "w"); if (!fp) return;
	fprintf(fp, "# total %llu imiss %llu dmiss %llu dacc %llu fa_imiss %llu cde %llu pref %llu cinv %llu dskip %llu dst %llu dwb %llu\n", (unsigned long long)g.total, (unsigned long long)g.imiss, (unsigned long long)g.dmiss, (unsigned long long)g.dacc, (unsigned long long)g.faimiss,
	        (unsigned long long)g.cde, (unsigned long long)g.pref, (unsigned long long)g.cinv, (unsigned long long)g.dskip, (unsigned long long)g.dst, (unsigned long long)g.dwb);
	for (uint32_t k = 0; k < SIZE / 4; k++) if (g.cnt[k] || g.im[k] || g.dm[k]) fprintf(fp, "%08x %u %u %u\n", BASE + k * 4, g.cnt[k], g.im[k], g.dm[k]);
	fclose(fp);
	{ char cn[600]; snprintf(cn, sizeof cn, "%s.calls", name); FILE *cf = fopen(cn, "w");
	  if (cf) { for (auto &kv : g.calls) fprintf(cf, "%08x %08x %u\n", (uint32_t)(kv.first >> 32), (uint32_t)kv.first, kv.second); fclose(cf); } g.calls.clear(); }
	if (!g.dh.empty()) { char dn[600]; snprintf(dn, sizeof dn, "%s.dhist", name); FILE *df = fopen(dn, "w");
	  if (df) { for (uint32_t i = 0; i < g.dh.size(); i++) if (g.dh[i]) fprintf(df, "%08x %u\n", 0x08000000u + (i << g.dhShift), g.dh[i]); fclose(df); }
	  std::fill(g.dh.begin(), g.dh.end(), 0); }
	if (!g.da.empty()) { char dn[600]; snprintf(dn, sizeof dn, "%s.dacc", name); FILE *df = fopen(dn, "w");
	  if (df) { for (uint32_t i = 0; i < g.da.size(); i++) if (g.da[i]) fprintf(df, "%08x %u\n", 0x08000000u + (i << 6), g.da[i]); fclose(df); }
	  std::fill(g.da.begin(), g.da.end(), 0); }
	if (g.dwN) { char dn[600]; snprintf(dn, sizeof dn, "%s.dwatch", name); FILE *df = fopen(dn, "w");
	  if (df) { for (auto &kv : g.dw) fprintf(df, "%u %08x %u\n", (unsigned)(kv.first >> 32), (uint32_t)kv.first, kv.second); fclose(df); } g.dw.clear(); }
	if (g.bucketAddr) { char bn[600]; snprintf(bn, sizeof bn, "%s.buckets", name); FILE *bf = fopen(bn, "w");
	  if (bf) { for (auto &kv : g.bk) fprintf(bf, "%08x %llu %llu %llu\n", kv.first, (unsigned long long)kv.second[0], (unsigned long long)kv.second[1], (unsigned long long)kv.second[2]); fclose(bf); }
	  g.bk.clear(); g.bcur = nullptr; g.bkey = 0xffffffffu; }
	fprintf(stderr, "PspProf: dumped %s (%llu instructions)\n", name, (unsigned long long)g.total);
	std::fill(g.cnt.begin(), g.cnt.end(), 0); std::fill(g.im.begin(), g.im.end(), 0); std::fill(g.dm.begin(), g.dm.end(), 0);
	g.total = g.imiss = g.dmiss = g.dacc = g.faimiss = 0; g.cde = g.pref = g.cinv = g.dskip = 0; g.dst = g.dwb = 0;
}
}
