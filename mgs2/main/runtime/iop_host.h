/* The IOP side of the x86 development host: the PS2RC runtime instance, a disc made of the game's files, and the RPC servers that
   have no IOP module here (loadfile).  The game's own IOP modules (jsifman, cdbios, iop_main) are the PS2RC translations of the
   files in MODULE.IPK, built at fixed load addresses:
     python3 -m ps2rc.iop gen --base 0x10000 --sym mod_jsifman  -o jsifman.c  jsifman.irx
     python3 -m ps2rc.iop gen --base 0x12000 --sym mod_cdbios   -o cdbios.c   cdbios.irx
     python3 -m ps2rc.iop gen --base 0x20000 --sym mod_iop_main -o iop_main.c iop_main.irx
   Needs sif.h first. */
#ifndef IOP_HOST_H
#define IOP_HOST_H
#include <dirent.h>
#include <strings.h>
#include <stdarg.h>
#include "iop_rt_ucontext.h"
#include "spu2_lite.h"                 /* third_party/ps2rc/audio: the SPU2 voices (NAX / ENVX / ENDX / IRQ) the sound driver polls */

extern const IopModule mod_jsifman, mod_cdbios, mod_iop_main;

/* ---- the disc: the files under a host directory laid out as on the DVD (MODULES\..., MGS2\..., SLES_503.83), placed one after
   another from LSN 0x400 in path order.  The real disc's sector numbers are not known here: reads by LSN are logged. ---- */
typedef struct { char path[96]; char host[256]; uint32_t lsn, size; } DiscFile;
static DiscFile disc_f[64]; static int disc_n; static uint32_t disc_next = 0x400;
static int disc_cmp(const void *a, const void *b) { return strcasecmp(((const DiscFile *)a)->path, ((const DiscFile *)b)->path); }
static void disc_scan(const char *root, const char *rel)
{
  char dir[512]; DIR *d; struct dirent *e;
  snprintf(dir, sizeof dir, "%s%s", root, rel);
  if (!(d = opendir(dir))) return;
  while ((e = readdir(d))) {
    char host[512], r[160]; FILE *f; long sz;
    if (e->d_name[0] == '.') continue;
    snprintf(host, sizeof host, "%s/%s", dir, e->d_name); snprintf(r, sizeof r, "%s\\%s", rel, e->d_name);
    { DIR *sub = opendir(host); if (sub) { closedir(sub); { char rr[160]; snprintf(rr, sizeof rr, "%s/%s", rel, e->d_name); disc_scan(root, rr); } continue; } }
    if (!(f = fopen(host, "rb")) || disc_n >= 64) { if (f) fclose(f); continue; }
    fseek(f, 0, SEEK_END); sz = ftell(f); fclose(f);
    { DiscFile *df = &disc_f[disc_n++]; char *q; snprintf(df->path, sizeof df->path, "%s", r); for (q = df->path; *q; q++) if (*q == '/') *q = '\\';
      snprintf(df->host, sizeof df->host, "%s", host); df->size = (uint32_t)sz; }
  }
  closedir(d);
}
/* the disc as one pack file (tools/make_pak.py: the table and only the sectors one scripted run reads; the rest reads as zeros), chosen
   by a disc root ending in ".pak": the PSP harness */
static FILE *disc_pak; static uint32_t (*disc_pak_r)[3], disc_pak_nr, disc_pak_data;
static int disc_pak_open(const char *path)
{
  char m[8]; uint32_t nf, i; FILE *f = fopen(path, "rb");
  if (!f || fread(m, 1, 8, f) != 8 || memcmp(m, "MGS2PAK1", 8) || fread(&nf, 4, 1, f) != 1 || fread(&disc_pak_nr, 4, 1, f) != 1 || nf > 64) {
    fprintf(stderr, "disc: %s is not a disc pack\n", path); if (f) fclose(f); return -1; }
  for (i = 0; i < nf; i++) { char p[64]; uint32_t v[2]; if (fread(p, 1, 64, f) != 64 || fread(v, 4, 2, f) != 2) return -1;
    snprintf(disc_f[i].path, sizeof disc_f[i].path, "%.63s", p); disc_f[i].host[0] = 0; disc_f[i].lsn = v[0]; disc_f[i].size = v[1];
    fprintf(stderr, "disc: %-28s lsn %6x size %u (pack)\n", disc_f[i].path, disc_f[i].lsn, disc_f[i].size); }
  disc_n = nf; disc_pak_r = malloc(disc_pak_nr * 12);
  if (!disc_pak_r) { fprintf(stderr, "disc: no memory for the pack's %u ranges\n", disc_pak_nr); return -1; }
  if (fread(disc_pak_r, 12, disc_pak_nr, f) != disc_pak_nr) return -1;
  disc_pak_data = (uint32_t)((ftell(f) + 2047) / 2048); disc_pak = f; return 0;
}
/* the pack reads, for the PSP harness's per-step counts (audit 2026-10-23 M3: are the deck's IOP / sound spikes memory-stick reads in the
   game thread?); g_disc_clock (host microseconds) is set by the PSP harness only */
uint32_t g_disc_reads, g_disc_sectors; uint64_t g_disc_us; uint64_t (*g_disc_clock)(void);
static void disc_pak_read(uint32_t lsn, uint32_t k, uint8_t *o)   /* k sectors from lsn; o is zeroed */
{
  uint64_t t0 = g_disc_clock ? g_disc_clock() : 0; g_disc_reads++; g_disc_sectors += k;
  while (k) {
    uint32_t lo = 0, hi = disc_pak_nr, n = 1;
    while (lo < hi) { uint32_t mid = (lo + hi) / 2; if (disc_pak_r[mid][0] + disc_pak_r[mid][1] <= lsn) lo = mid + 1; else hi = mid; }
    if (lo < disc_pak_nr && disc_pak_r[lo][0] <= lsn) {
      n = disc_pak_r[lo][0] + disc_pak_r[lo][1] - lsn; if (n > k) n = k;
      fseek(disc_pak, (long)(disc_pak_data + disc_pak_r[lo][2] + (lsn - disc_pak_r[lo][0])) * 2048, SEEK_SET);
      if (fread(o, 1, n * 2048, disc_pak) != n * 2048) fprintf(stderr, "disc: pack read short at lsn %x\n", lsn);
    } else if (lo < disc_pak_nr && disc_pak_r[lo][0] - lsn < k) n = disc_pak_r[lo][0] - lsn; else n = k;
    o += n * 2048; lsn += n; k -= n;
  }
  if (g_disc_clock) g_disc_us += g_disc_clock() - t0;
}
static void disc_file_read(const DiscFile *df, uint32_t pos, uint8_t *dst, uint32_t n)   /* bytes of a disc file (fileio, iopheap) */
{
  FILE *f;
  if (!n) return;
  if (disc_pak) {   /* gl36: the whole sectors straight into dst (zeroed first, as disc_pak_read wants), the partial first and last
                       sectors through one sector of memory: the same bytes as the whole-read buffer before, without it */
    uint32_t s = df->lsn + pos / 2048, o = pos % 2048; uint8_t *t = 0;
    while (n) {
      uint32_t k;
      if (o || n < 2048) {
        if (!t && !(t = malloc(2048))) { fprintf(stderr, "disc: no memory for a sector (%u bytes of %s)\n", n, df->path); exit(7); }
        memset(t, 0, 2048); disc_pak_read(s, 1, t); k = 2048 - o; if (k > n) k = n; memcpy(dst, t + o, k); o = 0; s++;
      } else { uint32_t ns = n / 2048; k = ns * 2048; memset(dst, 0, k); disc_pak_read(s, ns, dst); s += ns; }
      dst += k; n -= k;
    }
    free(t); return; }
  if ((f = fopen(df->host, "rb"))) { fseek(f, pos, SEEK_SET); if (fread(dst, 1, n, f)) {} fclose(f); }
}
static void disc_open(const char *root)
{
  int i;
  { size_t l = strlen(root); if (l > 4 && !strcmp(root + l - 4, ".pak")) { if (disc_pak_open(root)) exit(9); return; } }
  disc_scan(root, ""); qsort(disc_f, disc_n, sizeof *disc_f, disc_cmp);
  for (i = 0; i < disc_n; i++) { disc_f[i].lsn = disc_next; disc_next += (disc_f[i].size + 2047) / 2048 + 16; fprintf(stderr, "disc: %-28s lsn %6x size %u\n", disc_f[i].path, disc_f[i].lsn, disc_f[i].size); }
}
static int disc_lookup_name(const char *path, uint32_t *lsn, uint32_t *size)
{
  char p[128]; int i, n = 0; const char *s = path, *c = strchr(path, ':');
  if (c && c - path < 10) s = c + 1;
  if (*s != '\\' && *s != '/') p[n++] = '\\';
  for (; *s && *s != ';' && n < 120; s++) p[n++] = *s == '/' ? '\\' : *s;
  p[n] = 0;
  for (i = 0; i < disc_n; i++) if (!strcasecmp(disc_f[i].path, p)) { *lsn = disc_f[i].lsn; *size = disc_f[i].size; return 0; }
  fprintf(stderr, "disc: %s not found\n", path); return -1;
}
static int disc_lookup_cb(void *u, const char *path, uint32_t *lsn, uint32_t *size) { (void)u; return disc_lookup_name(path, lsn, size); }
static int disc_read_cb(void *u, uint32_t lsn, uint32_t nsect, void *dst)
{
  uint8_t *o = (uint8_t *)dst; (void)u;
  while (nsect) {
    int i, hit = 0;
    for (i = 0; i < disc_n; i++) {
      uint32_t ns = (disc_f[i].size + 2047) / 2048;
      if (lsn >= disc_f[i].lsn && lsn < disc_f[i].lsn + ns) {
        uint32_t k = disc_f[i].lsn + ns - lsn; FILE *f = disc_pak ? 0 : fopen(disc_f[i].host, "rb");
        if (k > nsect) k = nsect;
        memset(o, 0, k * 2048);
        if (disc_pak) disc_pak_read(lsn, k, o);
        if (f) { fseek(f, (long)(lsn - disc_f[i].lsn) * 2048, SEEK_SET); if (fread(o, 1, k * 2048, f)) {} fclose(f); }
        if (getenv("MGS2_DISC_LOG")) fprintf(stderr, "disc: vblank %ld read %s sector %u x %u\n", k_vblanks, disc_f[i].path, lsn - disc_f[i].lsn, k);
        if ((getenv("MGS2_SKIP_MOVIES") && strstr(disc_f[i].path, "MOVIE.DAT")) ||        /* dev: every MOVIE.DAT (VOX.DAT, DEMO.DAT) */
            (getenv("MGS2_SKIP_VOX") && strstr(disc_f[i].path, "VOX.DAT")) ||             /* read is end-of-stream chunks */
            (getenv("MGS2_SKIP_DEMO") && strstr(disc_f[i].path, "DEMO.DAT"))) {
          uint32_t z, e[4] = { 0xf0, 0x10, 0, 0 };            /* { type 0xf0 end, size 16 } (the chunk walker at 0x101c08 stops there) */
          for (z = 0; z < k * 2048; z += 16) memcpy(o + z, e, 16);
          fprintf(stderr, "disc: %s sector %u x %u served as end of stream (dev skip)\n", disc_f[i].path, lsn - disc_f[i].lsn, k);
          f = 0;
        }
#ifndef __psp__
        if (!disc_pak) { char rp[4096]; if (!realpath(disc_f[i].host, rp)) rp[0] = 0;
        if (strstr(rp, "/absent/")) {       /* a file copied from the disc only in parts (a sparse stand-in): all-zero reads are missing data */
          uint32_t z; for (z = 0; z < k * 2048 && !o[z]; z++) {}
          if (z == k * 2048) fprintf(stderr, "disc: read of %s sector %u x %u: not copied yet, zeros\n", disc_f[i].path, lsn - disc_f[i].lsn, k);
        } }
#endif
        o += k * 2048; lsn += k; nsect -= k; hit = 1; break;
      }
    }
    if (!hit) { fprintf(stderr, "disc: read of lsn %x outside the files\n", lsn); memset(o, 0, 2048); o += 2048; lsn++; nsect--; }
  }
  return 0;
}

/* ---- loadfile (sid 0x80000006): sceSifLoadModule.  LF_F_MOD_LOAD (0) packet { arg_len / result, modres, path[252], args[252] }
   (ps2sdk ee/kernel/src/loadfile.c, compatible with Sony's).  PLD.IRX is the game's loader of MODULE.IPK: here the modules it would
   load come up directly (the Sony ones as built-in HLE, the KCEJ ones as their translations). ---- */
static int iop_mod_id = 1;
static int lf_server(uint32_t fno, uint32_t buf, uint32_t ssize, uint32_t *rep)
{
  const char *path = (const char *)iop_p(buf + 8), *args = (const char *)iop_p(buf + 8 + 252);
  int alen = (int)rd32(g_iop, buf);
  (void)ssize;
  fprintf(stderr, "loadfile: fno %u path %.252s args %.*s\n", fno, path, alen > 0 && alen < 252 ? alen : 0, args);
  *rep = buf;
  if (fno == 0) {
    int r = -1;
    if (strstr(path, "PLD.IRX") || strstr(path, "pld.irx")) {
      const char *av[1];
      av[0] = "jsifman"; r = iop_rt_load(g_iop, &mod_jsifman, 1, av, 0x20);
      av[0] = "cdbios"; r = iop_rt_load(g_iop, &mod_cdbios, 1, av, 0x20);
      av[0] = "iop_main"; r = iop_rt_load(g_iop, &mod_iop_main, 1, av, 0x20);
      fprintf(stderr, "loadfile: KCEJ modules loaded (iop_main start: %d)\n", r);
      r = iop_mod_id++;
    } else if (strstr(path, "rom0:")) r = iop_mod_id++;                       /* ROM modules: built-in HLE */
    else fprintf(stderr, "loadfile: no module for %s\n", path);
    wr32(g_iop, buf, (uint32_t)r); wr32(g_iop, buf + 4, 0);
    return 0;
  }
  if (fno == 0xff) { memcpy(iop_p(buf), "2300", 4); return 0; }              /* LF_F_GET_VERSION: the game's EE library accepts "2300" (IOPRP23) */
  wr32(g_iop, buf, (uint32_t)-1); return 0;
}

/* ---- cdvdfsv: the EE's libcdvd RPC servers (packets: ps2sdk common/include/libcdvd-rpc.h; behaviour: iop/cdvd/cdvdfsv) ---- */
static int cd_srv_init(uint32_t fno, uint32_t buf, uint32_t ssize, uint32_t *rep)          /* 0x80000592 sceCdInit */
{
  (void)fno; (void)ssize; fprintf(stderr, "cdvd: sceCdInit mode %d\n", (int)rd32(g_iop, buf));
  wr32(g_iop, buf, 1); wr32(g_iop, buf + 4, 0x0101); wr32(g_iop, buf + 8, 0x0101); wr32(g_iop, buf + 12, 0); *rep = buf; return 0;
}
static int cd_srv_ready(uint32_t fno, uint32_t buf, uint32_t ssize, uint32_t *rep)         /* 0x8000059A / 0x8000059C sceCdDiskReady */
{
  (void)fno; (void)ssize; wr32(g_iop, buf, 2); *rep = buf; return 0;                         /* SCECdComplete */
}
static int cd_srv_search(uint32_t fno, uint32_t buf, uint32_t ssize, uint32_t *rep)        /* 0x80000597 sceCdSearchFile */
{
  /* sceCdlFILE { lsn, size, name[16], date[8] }, then (packet sizes 0x12c / 0x128) attributes, path[256], ee dest (, layer) */
  uint32_t lsn = 0, size = 0, pathoff = ssize == 0x124 ? 32 : 36, eedest = rd32(g_iop, buf + pathoff + 256), n = ssize == 0x124 ? 32 : 36;
  const char *path = (const char *)iop_p(buf + pathoff); int ok = !disc_lookup_name(path, &lsn, &size); uint32_t i;
  (void)fno;
  fprintf(stderr, "cdvd: sceCdSearchFile %s -> %s lsn %x size %u\n", path, ok ? "found" : "not found", lsn, size);
  if (ok) { const char *b = strrchr(path, '\\'); wr32(g_iop, buf, lsn); wr32(g_iop, buf + 4, size); memset(iop_p(buf + 8), 0, 24); if (b) strncpy((char *)iop_p(buf + 8), b + 1, 15); }
  for (i = 0; i < n; i++) WR8(eedest + i, *iop_p(buf + i));
  wr32(g_iop, buf + 0x200, ok); *rep = buf + 0x200; return 0;
}
static uint8_t cd_sec[2048 * 64];
static int cd_srv_ncmd(uint32_t fno, uint32_t buf, uint32_t ssize, uint32_t *rep)          /* 0x80000595 N commands */
{
  uint32_t r = 1; (void)ssize;
  if (fno == 1 || fno == 19) {                     /* read to EE memory: { lbn, sectors, ee addr, mode, ... } */
    uint32_t lbn = rd32(g_iop, buf), n = rd32(g_iop, buf + 4), ee = rd32(g_iop, buf + 8);
    fprintf(stderr, "cdvd: read lsn %x x %u -> EE %08x\n", lbn, n, ee);
    while (n) { uint32_t k = n > 64 ? 64 : n, i; disc_read_cb(0, lbn, k, cd_sec); for (i = 0; i < k * 2048; i++) WR8(ee + i, cd_sec[i]); lbn += k; ee += k * 2048; n -= k; }
  } else fprintf(stderr, "cdvd: N command %u\n", fno);
  wr32(g_iop, buf + 0x100, r); wr32(g_iop, buf + 0x104, 1); *rep = buf + 0x100; return 0;
}
static int cd_srv_scmd(uint32_t fno, uint32_t buf, uint32_t ssize, uint32_t *rep)          /* 0x80000593 S commands */
{
  (void)ssize; fprintf(stderr, "cdvd: S command %u\n", fno);
  memset(iop_p(buf + 0x100), 0, 0x40); wr32(g_iop, buf + 0x100, 1); *rep = buf + 0x100; return 0;
}

/* ---- fileio (sid 0x80000001), the protocol of the IOPRP23 FILEIO 2.3 module as the game's EE library uses it (read from the ELF,
   0x1773b0..0x17a000; no ps2sdk equivalent).  fno 0xff { EE reply area } answers "2300".  Every other call is { sema, result ptr,
   result size, args... } and is answered twice: the RPC reply (4 bytes, nonzero = accepted), then the reply area in EE memory
   { sema, fno, result ptr, result size, result..., } and SIF command 0x80000011, whose EE handler (0x176f60) copies the result and
   signals the semaphore.  Reads go straight to their EE destination, so the read reply's unaligned pieces (size1 / size2) are 0. ---- */
typedef struct { int used; DiscFile *f; uint32_t pos; } FioFd;
static FioFd fio_fd[32];
static uint32_t fio_eebuf, fio_rep[48];
static DiscFile *disc_find(const char *path)
{
  uint32_t lsn, size; int i;
  if (disc_lookup_name(path, &lsn, &size)) return 0;
  for (i = 0; i < disc_n; i++) if (disc_f[i].lsn == lsn) return &disc_f[i];
  return 0;
}
static void fio_done(void)
{
  uint32_t hdr[4] = { 0 };
  sif_push(0, fio_eebuf, (uint8_t *)fio_rep, sizeof fio_rep);
  sif_send_to_ee(0x80000011u, (uint8_t *)hdr, 16, 0, 0, 0);
}
static int fio_server(uint32_t fno, uint32_t buf, uint32_t ssize, uint32_t *rep)
{
  uint32_t out = buf + 0x1000, w[8]; int32_t r = -5; int i;
  for (i = 0; i < 8; i++) w[i] = rd32(g_iop, buf + 4 * i);
  *rep = out; wr32(g_iop, out, 1);
  if (fno == 0xff) { fio_eebuf = w[0]; memcpy(iop_p(out), "2300", 4); fprintf(stderr, "fileio: init, EE reply area %08x\n", w[0]); return 0; }
  memset(fio_rep, 0, sizeof fio_rep);
  switch (fno) {
  case 0: {                                                      /* open { hdr, flags, mode, name[0x400], slot } */
    const char *name = (const char *)iop_p(buf + 0x14); DiscFile *f = disc_find(name);
    if (f) for (i = 3; i < 32; i++) if (!fio_fd[i].used) { fio_fd[i].used = 1; fio_fd[i].f = f; fio_fd[i].pos = 0; r = i; break; }
    if (!f) r = -2;                                              /* ENOENT */
    fprintf(stderr, "fileio: open %s flags %x -> %d\n", name, w[3], r); break; }
  case 1: if (w[3] < 32 && fio_fd[w[3]].used) { fio_fd[w[3]].used = 0; r = 0; } else r = -9; break;   /* close { hdr, fd, slot } */
  case 2: {                                                      /* read { hdr, fd, buffer, size, -, slot } */
    FioFd *d = w[3] < 32 && fio_fd[w[3]].used ? &fio_fd[w[3]] : 0;
    if (d) {
      uint32_t n = d->pos >= d->f->size ? 0 : d->f->size - d->pos; uint8_t *tmp; FILE *f;
      if (n > w[5]) n = w[5];
      tmp = malloc(n + 1);
      if (!tmp) { fprintf(stderr, "fileio: no memory for a read of %u bytes\n", n); exit(7); }
      disc_file_read(d->f, d->pos, tmp, n); (void)f;
      if (n) sif_push_own(0, w[4], tmp, n); else free(tmp);   /* gl36: the sif keeps tmp (no second copy of the read) */
      d->pos += n; r = (int32_t)n;
    } else r = -9;
    if (sif_log > 1) fprintf(stderr, "fileio: read fd %u %u bytes -> EE %08x (%d)\n", w[3], w[5], w[4], r);
    fio_rep[7] = w[4]; break; }
  case 4: {                                                      /* lseek { hdr, fd, offset, whence, slot } */
    FioFd *d = w[3] < 32 && fio_fd[w[3]].used ? &fio_fd[w[3]] : 0;
    if (d) { int64_t p = (w[5] == 0 ? 0 : w[5] == 1 ? d->pos : d->f->size) + (int32_t)w[4]; if (p >= 0) { d->pos = (uint32_t)p; r = (int32_t)p; } else r = -22; }
    else r = -9;
    if (sif_log > 1) fprintf(stderr, "fileio: lseek fd %u %d whence %u -> %d\n", w[3], (int32_t)w[4], w[5], r);
    break; }
  default: fprintf(stderr, "fileio: function %u (size %u: %08x %08x %08x %08x %08x) not handled\n", fno, ssize, w[3], w[4], w[5], w[6], w[7]);
  }
  fio_rep[0] = w[0]; fio_rep[1] = fno; fio_rep[2] = w[1]; fio_rep[3] = w[2]; fio_rep[4] = (uint32_t)r;
  sif_after_end = fio_done; return 0;
}
/* ---- iopheap (sid 0x80000003): sceSifAllocIopHeap (1), sceSifFreeIopHeap (2), sceSifLoadIopHeap (3 { addr, path[] }) ---- */
static int heap_server(uint32_t fno, uint32_t buf, uint32_t ssize, uint32_t *rep)
{
  uint32_t out = buf + 0x100, a0 = rd32(g_iop, buf); int32_t r = -1; (void)ssize;
  if (fno == 1) r = (int32_t)iop_rt_alloc(g_iop, a0, 0, 0);
  else if (fno == 2) r = 0;
  else if (fno == 3) {
    DiscFile *f = disc_find((const char *)iop_p(buf + 4)); FILE *h;
    if (f) { disc_file_read(f, 0, iop_p(a0), f->size); r = 0; } (void)h;
  }
  fprintf(stderr, "iopheap: fno %u arg %08x -> %08x\n", fno, a0, r);
  wr32(g_iop, out, (uint32_t)r); *rep = out; return 0;
}

/* ---- cdvdman functions the runtime does not have (cdbios imports 24 28 37 39 75).  A read completes at once in the runtime; its
   sceCdCallback (cdbios 0x1229c, sets the event flag the reader waits on) runs afterwards from the host loop, as the CD interrupt
   would. ---- */
static uint32_t cdv_cb, cdv_cb_gp, cdv_cb_pending; static IopFn cdv_read_rt;
static void cdv_Callback(IopCpu *c) { uint32_t old = cdv_cb; cdv_cb = iop_arg(c, 0); cdv_cb_gp = c->r[28]; iop_ret(c, old); }
static void cdv_Read(IopCpu *c)
{
  if (sif_log > 1) fprintf(stderr, "cdvd: IOP read lsn %x x %u -> %08x\n", iop_arg(c, 0), iop_arg(c, 1), iop_arg(c, 2));
  cdv_read_rt(c); if (c->r[2] && cdv_cb) cdv_cb_pending = 1;
}
static void cdv_tick(void)
{
  if (cdv_cb_pending && cdv_cb) { cdv_cb_pending = 0; iop_rt_call_handler(g_iop, cdv_cb, cdv_cb_gp, 1, 0, 0); }   /* SCECdFuncRead */
}
static void cdv_ReadClock(IopCpu *c)                     /* sceCdCLOCK { stat, second, minute, hour, pad, day, month, year } in BCD; fixed, for reproducible runs */
{
  static const uint8_t clk[8] = { 0, 0x00, 0x00, 0x12, 0, 0x18, 0x10, 0x26 };
  memcpy(iop_p(iop_arg(c, 0)), clk, 8); iop_ret(c, 1);
}
static void cdv_Status(IopCpu *c) { iop_ret(c, 0x0a); } /* SCECdStatPause: spinning, idle */
static void cdv_Break(IopCpu *c) { cdv_cb_pending = 0; iop_ret(c, 1); }
static void cdv_one(IopCpu *c) { iop_ret(c, 1); }

/* ---- padman (sid 0x80000100, extension 0x80000101): the "new" padman protocol of the game's libpad (ps2sdk iop/input/padman,
   ee/rpc/pad).  One DualShock 2 on port 0 slot 0, past its configuration: state STABLE, task UPDATE_PAD, the controller's answers
   to the config queries (model 3, 2 modes [0x0004 digital, 0x0007 DualShock], 2 actuators, 1 combination) as a real DS2 gives them.
   Every vblank the 128-byte pad record goes to the EE pad area (double buffered on the frame count), as padman's pdTransfer.
   Buttons: none pressed for now (input comes later). ---- */
typedef struct { int open; uint32_t area, frame; uint8_t mode, lock, curid, curoffs, req, btninfo; } HostPad;
static HostPad hpad[2]; static uint32_t pad_stat_ee, pad_stat_frame; static uint16_t pad_buttons = 0xffff;
/* scripted input for unattended runs: MGS2_PAD="<vblank>:<mask>,..." holds the buttons of <mask> (hex, 1 = pressed, the pad's own bit
   order: 0x0800 START, 0x0100 SELECT, 0x1000 UP, 0x4000 DOWN, 0x0040 CROSS, 0x0020 CIRCLE, ...) from that EE vblank on */
static void pad_script(void)
{
  static const char *next; static int init;
  if (!init) { init = 1; next = getenv("MGS2_PAD"); }
  while (next && *next) {
    char *e; long vb = strtol(next, &e, 10); unsigned long m;
    if (*e != ':' || vb > k_vblanks) return;
    m = strtoul(e + 1, &e, 16); pad_buttons = (uint16_t)~m;
    fprintf(stderr, "pad: vblank %ld buttons %04lx\n", vb, m);
    next = *e == ',' ? e + 1 : 0;
  }
}
static void pad_vblank(void)
{
  int port;
  pad_script();
  if (pad_stat_ee) {                                   /* open slots, for padGetConnection: { frame, openSlots[2] } */
    uint32_t sb[32] = { 0 };
    sb[0] = ++pad_stat_frame; sb[1] = hpad[0].open; sb[2] = hpad[1].open;
    sif_push(0, pad_stat_ee + ((sb[0] & 1) ? 0 : 128), (uint8_t *)sb, 128);
  }
  for (port = 0; port < 2; port++) {
    HostPad *p = &hpad[port]; uint8_t d[128] = { 0 }; uint32_t v;
    if (!p->open || !p->area) continue;
    d[0] = 0; d[1] = p->curid; d[2] = pad_buttons >> 8; d[3] = pad_buttons & 0xff; d[4] = d[5] = d[6] = d[7] = 0x80;   /* data[32] */
    if (p->curid == 0x79) {                            /* pressure bytes 8..19: RIGHT LEFT UP DOWN TRIANGLE CIRCLE CROSS SQUARE L1 R1 L2 R2 */
      static const uint16_t pb[12] = { 0x2000, 0x8000, 0x1000, 0x4000, 0x0010, 0x0020, 0x0040, 0x0080, 0x0004, 0x0008, 0x0001, 0x0002 };
      int k; for (k = 0; k < 12; k++) d[8 + k] = (pad_buttons & pb[k]) ? 0 : 0xff;
    }
    v = 0x0a020100u; memcpy(d + 48, &v, 4); v = 0x14010101u; memcpy(d + 52, &v, 4);   /* actData: { 01 02 00 0a } { 01 01 01 14 } */
    v = 0x00010002u; memcpy(d + 64, &v, 4);                                         /* combData[0]: { 02 00 01 00 } */
    v = 0x00070004u; memcpy(d + 80, &v, 4);                                         /* modeTable: 0x0004, 0x0007 */
    v = p->frame; memcpy(d + 88, &v, 4); v = 0; memcpy(d + 92, &v, 4); v = 32; memcpy(d + 96, &v, 4);   /* frame, findPadRetries, length */
    d[100] = 2; d[101] = p->curid; d[102] = 3; d[103] = 1; d[104] = 2; d[105] = p->curoffs; d[106] = 2; d[107] = 1;   /* modeConfig .. numActComb */
    d[108] = 0; d[109] = p->mode; d[110] = p->lock; d[111] = 0; d[112] = 6; d[113] = p->req; d[114] = 1; d[115] = 0; d[116] = 0;
    sif_push(0, p->area + ((p->frame & 1) ? 128 : 0), d, 128);
    p->frame++; p->req = 0;                            /* a mode request completes within the frame */
  }
}
static int pad_server(uint32_t fno, uint32_t buf, uint32_t ssize, uint32_t *rep)
{
  uint32_t w[8], cmd; int i; HostPad *p;
  for (i = 0; i < 8; i++) w[i] = rd32(g_iop, buf + 4 * i);
  cmd = w[0]; p = &hpad[w[1] & 1]; *rep = buf; (void)fno; (void)ssize;
  switch (cmd) {
  case 0x10: pad_stat_ee = w[4]; wr32(g_iop, buf + 12, 1); break;                                     /* INIT { statBuf } */
  case 0x12: wr32(g_iop, buf + 12, 0x0402); break;                                                     /* GET_MODVER: v4.02 */
  case 0x01:                                                                                           /* OPEN { port, slot, -, padArea } */
    if (w[1] == 0 && w[2] == 0) { memset(p, 0, sizeof *p); p->open = 1; p->area = w[4]; p->curid = 0x41; p->frame = 1; wr32(g_iop, buf + 12, 1); }
    else wr32(g_iop, buf + 12, 0);
    wr32(g_iop, buf + 20, 0); break;
  case 0x0e: p->open = 0; wr32(g_iop, buf + 12, 1); break;                                            /* CLOSE */
  case 0x0f: pad_stat_ee = 0; wr32(g_iop, buf + 12, 1); break;                                        /* END */
  case 0x06:                                                                                           /* SET_MMODE { port, slot, mode, lock } */
    if (w[3] < 2) { p->mode = w[3]; p->lock = w[4]; p->curoffs = w[3]; p->curid = w[3] ? (p->btninfo ? 0x79 : 0x73) : 0x41; p->req = 2; wr32(g_iop, buf + 20, 1); }
    else wr32(g_iop, buf + 20, 0);
    break;
  case 0x07: case 0x08: wr32(g_iop, buf + 20, 1); break;                                               /* SET_ACTDIR / SET_ACTALIGN */
  case 0x09: wr32(g_iop, buf + 12, 0x3ffff); break;                                                    /* GET_BTNMASK: pressure-sensitive */
  case 0x0a: p->btninfo = w[3] != 0; if (p->curoffs) p->curid = p->btninfo ? 0x79 : 0x73; p->req = 2; wr32(g_iop, buf + 16, 1); break;   /* SET_BTNINFO */
  case 0x0b: wr32(g_iop, buf + 28, 1); break;                                                          /* SET_VREF */
  case 0x0c: wr32(g_iop, buf + 12, 2); break;                                                          /* GET_PORTMAX */
  case 0x0d: wr32(g_iop, buf + 12, 1); break;                                                          /* GET_SLOTMAX */
  case 0x03: case 0x04: case 0x05: wr32(g_iop, buf + 20, 0); break;                                    /* INFO_*: read from the pad record by libpad */
  default: fprintf(stderr, "padman: command %x not handled\n", cmd);
  }
  if (cmd != 0x07) fprintf(stderr, "padman: command %x (%x %x %x %x)\n", cmd, w[1], w[2], w[3], w[4]);
  return 0;
}
static int pad_server_ext(uint32_t fno, uint32_t buf, uint32_t ssize, uint32_t *rep) { (void)fno; (void)ssize; *rep = buf; return 0; }

/* ---- mcserv (sid 0x80000400): memory cards.  Port 0 slot 0 holds a formatted 8 MB PS2 card backed by a host directory (env
   MGS2_MC0, default "memcard0"); port 1 is empty.  Commands of both libmc generations are taken (0x70.. MCSERV, 0x01.. / 0xFE
   XMCSERV), with the packets and replies of ps2sdk ee/rpc/memorycard and iop/memorycard/mcserv: the RPC reply is { result,
   mcserv version 0x20e, mcman version 0x21a }; info / read side data goes to the EE "param" area.  Reads go straight to their
   EE destination (the unaligned pieces in the param area are empty).  Time stamps are fixed, for reproducible runs. ---- */
#include <sys/stat.h>
#include <fnmatch.h>
typedef struct { FILE *f; int used; } McFd;
static McFd mc_fd[16]; static char mc_root[256] = "memcard0", mc_cwd[2][256] = { "/", "/" }; static int mc_seen[2];
static void mc_path(int port, const char *name, char *out, size_t n)          /* card path -> normalized absolute card path */
{
  char t[1024], *seg, *save; char res[1024] = "";
  if (name[0] == '/') snprintf(t, sizeof t, "%s", name); else snprintf(t, sizeof t, "%s/%s", mc_cwd[port & 1], name);
  for (seg = strtok_r(t, "/", &save); seg; seg = strtok_r(0, "/", &save)) {
    if (!strcmp(seg, ".")) continue;
    if (!strcmp(seg, "..")) { char *q = strrchr(res, '/'); if (q) *q = 0; continue; }
    strncat(res, "/", sizeof res - strlen(res) - 1); strncat(res, seg, sizeof res - strlen(res) - 1);
  }
  snprintf(out, n, "%s", res[0] ? res : "/");
}
static void mc_host(const char *cpath, char *out, size_t n) { snprintf(out, n, "%s%s", mc_root, cpath); }
static long mc_used_kb(const char *dir)
{
  DIR *d = opendir(dir); struct dirent *e; long kb = 0;
  if (!d) return 0;
  while ((e = readdir(d))) {
    char p[1024]; struct stat st;
    if (e->d_name[0] == '.') continue;
    snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
    if (stat(p, &st)) continue;
    kb += S_ISDIR(st.st_mode) ? 1 + mc_used_kb(p) : (st.st_size + 1023) / 1024 + 1;
  }
  closedir(d); return kb;
}
static void mc_ent(uint8_t *e, const char *name, int dir, uint32_t size)        /* sceMcTblGetDir, 64 bytes */
{
  static const uint8_t date[8] = { 0, 0, 0, 12, 18, 10, 0xea, 0x07 };            /* 2026-10-18 12:00:00 */
  uint16_t attr = dir ? 0x8427 : 0x8497;
  memset(e, 0, 64); memcpy(e, date, 8); memcpy(e + 8, date, 8); memcpy(e + 16, &size, 4); memcpy(e + 20, &attr, 2);
  strncpy((char *)e + 32, name, 31);
}
static int mc_server(uint32_t fno, uint32_t buf, uint32_t ssize, uint32_t *rep)
{
  /* mcDescParam_t { fd, port, slot, size, offset, origin, buffer, param, data[16] }
     mcNameParam_t { port, slot, flags, maxent, table / curdir, name[1024] } */
  uint32_t out = buf + 0x500, w[8]; int32_t r = -5; int i, port, slot; char cp[1024], hp[1300];
  const char *name = (const char *)iop_p(buf + 20);
  for (i = 0; i < 8; i++) w[i] = rd32(g_iop, buf + 4 * i);
  *rep = out; (void)ssize;
  switch (fno) {
  case 0x70: case 0xfe: r = 0; break;                                                        /* init */
  case 0x78: case 0x01: {                                                                    /* getinfo { -, port, slot, size, offset, origin, -, param } */
    uint32_t ep[48] = { 0 }; int newi = fno == 0x01;
    port = w[1]; slot = w[2];
    if (port == 0 && slot == 0) {
      mkdir(mc_root, 0755);
      r = mc_seen[0] ? 0 : -1; mc_seen[0] = 1;                                               /* sceMcResChangedCard the first time */
      if (newi ? w[5] : w[3]) ep[0] = 2;                                                     /* type: PS2 card */
      if (w[4]) ep[1] = (uint32_t)(8000 - mc_used_kb(mc_root));                              /* free (KB) */
      if (newi && w[3]) ep[36] = 1;                                                          /* formatted */
    } else r = -13;                                                                           /* sceMcResFailDetect2: no card */
    sif_push(0, w[7], (uint8_t *)ep, newi ? 192 : 64);
    fprintf(stderr, "mc: getinfo port %d slot %d -> %d (free %u)\n", port, slot, r, ep[1]); break; }
  case 0x71: case 0x02: {                                                                    /* open { port, slot, flags, -, -, name } */
    int fl = w[2]; port = w[0];
    if (port != 0) { r = -13; break; }
    mc_path(port, name, cp, sizeof cp); mc_host(cp, hp, sizeof hp);
    if (fl & 0x40) { r = mkdir(hp, 0755) ? -4 : 0; fprintf(stderr, "mc: mkdir %s -> %d\n", cp, r); break; }
    { struct stat st; int ex = !stat(hp, &st);
      if (ex && S_ISDIR(st.st_mode)) { r = -5; break; }
      if (!ex && !(fl & 0x200)) { r = -4; fprintf(stderr, "mc: open %s flags %x -> %d\n", cp, fl, r); break; }
      for (i = 0; i < 16; i++) if (!mc_fd[i].used) break;
      if (i == 16) { r = -7; break; }
      mc_fd[i].f = fopen(hp, !ex || (fl & 0x400) ? "w+b" : ((fl & 3) == 1 ? "rb" : "r+b"));
      if (!mc_fd[i].f) { r = -4; break; }
      mc_fd[i].used = 1; r = i; }
    fprintf(stderr, "mc: open %s flags %x -> %d\n", cp, fl, r); break; }
  case 0x72: case 0x03: if (w[0] < 16 && mc_fd[w[0]].used) { fclose(mc_fd[w[0]].f); mc_fd[w[0]].used = 0; r = 0; } else r = -5; break;   /* close { fd } */
  case 0x75: case 0x04: if (w[0] < 16 && mc_fd[w[0]].used) { fseek(mc_fd[w[0]].f, (int32_t)w[4], w[5]); r = (int32_t)ftell(mc_fd[w[0]].f); } else r = -5; break;   /* seek */
  case 0x73: case 0x05: {                                                                    /* read { fd, -, -, size, -, -, buffer, param } */
    uint32_t ep[48] = { 0 };
    if (w[0] < 16 && mc_fd[w[0]].used) {
      uint8_t *t = malloc(w[3] + 1); size_t n;
      if (!t) { fprintf(stderr, "mc: no memory for a read of %u bytes\n", w[3]); exit(7); }
      n = fread(t, 1, w[3], mc_fd[w[0]].f);
      if (n) sif_push(0, w[6], t, (uint32_t)n);
      free(t); r = (int32_t)n;
    } else r = -5;
    sif_push(0, w[7], (uint8_t *)ep, fno == 0x05 ? 192 : 64);
    fprintf(stderr, "mc: read fd %u %u bytes -> %d\n", w[0], w[3], r); break; }
  case 0x74: case 0x06: {                                                                    /* write { fd, -, -, size, -, head, buffer, -, data[16] } */
    if (w[0] < 16 && mc_fd[w[0]].used) {
      uint8_t *t = malloc(w[3] + 16);
      if (!t) { fprintf(stderr, "mc: no memory for a write of %u bytes\n", w[3]); exit(7); }
      memcpy(t, iop_p(buf + 32), w[5]); ee_copy_from(t + w[5], w[6], w[3]);
      r = (int32_t)fwrite(t, 1, w[5] + w[3], mc_fd[w[0]].f); free(t);
    } else r = -5;
    fprintf(stderr, "mc: write fd %u %u bytes -> %d\n", w[0], w[5] + w[3], r); break; }
  case 0x7a: case 0x0a: if ((int32_t)w[0] >= 0 && w[0] < 16 && mc_fd[w[0]].used) { fflush(mc_fd[w[0]].f); r = 0; } else r = -5; break;   /* flush */
  case 0x76: case 0x0d: {                                                                    /* getdir { port, slot, flags, maxent, table, name } */
    char dir[1024], pat[256], *sl; DIR *d; struct dirent *e; int n = 0; uint32_t tab = w[4]; uint8_t ent[64];
    port = w[0]; if (port != 0) { r = -13; break; }
    mc_path(port, name, cp, sizeof cp); sl = strrchr(cp, '/');
    snprintf(pat, sizeof pat, "%s", sl + 1); *sl = 0; snprintf(dir, sizeof dir, "%s", cp[0] ? cp : "/");
    if (!pat[0]) snprintf(pat, sizeof pat, "*");
    mc_host(dir, hp, sizeof hp);
    if (!(d = opendir(hp))) { r = -4; fprintf(stderr, "mc: getdir %s -> %d\n", name, r); break; }
    if ((w[2] & 0xffff) == 0) for (i = 0; i < 2 && n < (int)w[3]; i++) if (!fnmatch(pat, i ? ".." : ".", 0)) { mc_ent(ent, i ? ".." : ".", 1, 0); sif_push(0, tab, ent, 64); tab += 64; n++; }
    while ((e = readdir(d)) && n < (int)w[3]) {
      char p[1600]; struct stat st;
      if (e->d_name[0] == '.' || fnmatch(pat, e->d_name, 0)) continue;
      snprintf(p, sizeof p, "%s/%s", hp, e->d_name); if (stat(p, &st)) continue;
      mc_ent(ent, e->d_name, S_ISDIR(st.st_mode), (uint32_t)st.st_size); sif_push(0, tab, ent, 64); tab += 64; n++;
    }
    closedir(d); r = n; fprintf(stderr, "mc: getdir %s -> %d\n", name, r); break; }
  case 0x7b: case 0x0c: {                                                                    /* chdir { port, slot, -, -, curdir, name } */
    struct stat st; port = w[0];
    sif_push(0, w[4], (const uint8_t *)mc_cwd[port & 1], (uint32_t)strlen(mc_cwd[port & 1]) + 1);
    mc_path(port, name, cp, sizeof cp); mc_host(cp, hp, sizeof hp);
    if (!stat(hp, &st) && S_ISDIR(st.st_mode)) { snprintf(mc_cwd[port & 1], sizeof mc_cwd[0], "%s", cp); r = 0; } else r = -4;
    fprintf(stderr, "mc: chdir %s -> %d\n", cp, r); break; }
  case 0x79: case 0x0f: {                                                                    /* delete { port, slot, flags, -, -, name } */
    mc_path(w[0], name, cp, sizeof cp); mc_host(cp, hp, sizeof hp);
    r = (!remove(hp)) ? 0 : -4; fprintf(stderr, "mc: delete %s -> %d\n", cp, r); break; }
  case 0x7c: case 0x0e: r = 0; break;                                                        /* setfileinfo: attributes / dates are not kept */
  case 0x77: case 0x10: mkdir(mc_root, 0755); r = 0; break;                                  /* format */
  case 0x12: r = 0; break;                                                                   /* getentspace */
  case 0x15: r = 1; break;     /* slot max { -, port }: the disc's mcman export 26 -> sio2man 59, 1 when no multitap hook (no mtapman) */
  default: fprintf(stderr, "mc: function %x not handled\n", fno);
  }
  wr32(g_iop, out, (uint32_t)r); wr32(g_iop, out + 4, 0x20e); wr32(g_iop, out + 8, 0x21a);
  return 0;
}

static void iop_logf(void *u, int level, const char *fmt, ...)
{
  va_list ap; (void)u;
  if (level > 2) return;
  va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
}
/* SPU2: spu2_lite runs on the IOP clock, 48 samples per ms, in state-only mode (voice positions, envelopes, loop flags and the
   IRQ as the driver sees them; no samples mixed yet) */
static Spu2 g_spu; static uint64_t spu_frac;
/* sceSdBlockTrans in loop mode (SD_TRANS_MODE_WRITE | SD_BLOCK_LOOP = 0x10): the core's AutoDMA input reads the IOP buffer in a
   loop, 1024-byte blocks (256 samples left, then right) at 48 kHz = 192 bytes per ms.  The driver's demo-audio engine (iop_main
   0x583f0) polls sceSdBlockTransStatus for the DMA address (low 24 bits) to see which half of its 4 KB buffer is playing and
   refills the other one.  Address steps of 512 bytes (one channel's half of a block): an assumption, not measured.  The samples
   are not mixed into the output yet. */
static struct { int on; uint32_t base, size; uint64_t bytes, frac; } sd_bt[2];
static IopFn sd_bt_trans_rt, sd_bt_status_rt;
static void sd_bt_trans(IopCpu *c)
{
  uint32_t ch = iop_arg(c, 0) & 1, mode = iop_arg(c, 1);
  if ((mode & 3) == 2) { sd_bt[ch].on = 0; iop_ret(c, 0); return; }                      /* SD_TRANS_MODE_STOP */
  if ((mode & 3) == 0 && (mode & 0x10)) {
    sd_bt[ch].on = 1; sd_bt[ch].base = iop_arg(c, 2); sd_bt[ch].size = iop_arg(c, 3); sd_bt[ch].bytes = sd_bt[ch].frac = 0;
    if (getenv("MGS2_SD_LOG")) fprintf(stderr, "sd: vb %ld block trans loop core %u buffer %06x size %x\n", k_vblanks, ch, sd_bt[ch].base, sd_bt[ch].size);
    iop_ret(c, 0); return;
  }
  sd_bt_trans_rt(c);                                                                         /* one-shot: the runtime's */
}
static void sd_bt_status(IopCpu *c)
{
  uint32_t ch = iop_arg(c, 0) & 1;
  if (!sd_bt[ch].on) { sd_bt_status_rt(c); return; }
  iop_ret(c, (sd_bt[ch].base + (uint32_t)(sd_bt[ch].bytes % (sd_bt[ch].size ? sd_bt[ch].size : 1)) / 512 * 512) & 0xffffff);
}
static void spu_step(uint64_t ns)
{
  uint32_t n, k; spu_frac += ns * 48u; n = (uint32_t)(spu_frac / 1000000u); spu_frac -= (uint64_t)n * 1000000u;
  if (n) spu2_render48(&g_spu, 0, n);
  for (k = 0; k < 2; k++) if (sd_bt[k].on) { uint64_t b; sd_bt[k].frac += ns * 192u; b = sd_bt[k].frac / 1000000u; sd_bt[k].frac -= b * 1000000u; sd_bt[k].bytes += b; }
  if (spu2_irq_take(&g_spu)) iop_rt_spu_irq(g_iop, 1);
}
/* MGS2_SD_LOG=1: the sound driver's voice register writes and SPU RAM transfers, with the vblank (dev) */
static Spu2Ops spu_so; static int spu_log;
static void spl_set_param(void *u, unsigned v, unsigned p, uint16_t x) { if (spu_log && p == 2) fprintf(stderr, "sd: vb %ld voice %u pitch %04x\n", k_vblanks, v, x); spu_so.set_param(u, v, p, x); }
static void spl_set_addr(void *u, unsigned v, unsigned w, uint32_t a) { if (spu_log) fprintf(stderr, "sd: vb %ld voice %u %s %05x\n", k_vblanks, v, w ? "lsa" : "ssa", a); spu_so.set_addr(u, v, w, a); }
static void spl_key_on(void *u, unsigned c, uint32_t m) { if (spu_log) fprintf(stderr, "sd: vb %ld key on core %u %06x\n", k_vblanks, c, m); spu_so.key_on(u, c, m); }
static void spl_key_off(void *u, unsigned c, uint32_t m) { if (spu_log) fprintf(stderr, "sd: vb %ld key off core %u %06x\n", k_vblanks, c, m); spu_so.key_off(u, c, m); }
static uint32_t spl_get_nax(void *u, unsigned v) { uint32_t a = spu_so.get_nax(u, v); if (spu_log > 1) fprintf(stderr, "sd: vb %ld voice %u nax %05x\n", k_vblanks, v, a); return a; }
static void spl_dma_done(void *u, uint32_t a, uint32_t n) { (void)u; if (spu_log) fprintf(stderr, "sd: vb %ld spu ram write %05x + %x\n", k_vblanks, a, n); }
#ifndef IOP_HSTACK
#define IOP_HSTACK (256 * 1024)                  /* host stack per IOP thread */
#endif
static IopCoroOps stk_iop_ops; static struct { void *lo; size_t n; } stk_iop[64]; static int stk_iop_n;
static void *stk_iop_create(void *user, size_t stack, void (*entry)(void))   /* dev: MGS2_STACK_MARK on the IOP threads' stacks */
{
#ifdef HOST_STACK_POOL   /* as PS2RC's uc_create (iop_rt_ucontext.c), with the stack from the pool (kernel.h) */
  ucontext_t *uc = (ucontext_t *)calloc(1, sizeof *uc); void *st = host_stack_alloc(stack); (void)user;
  if (!uc || !st) { free(uc); return 0; }
  getcontext(uc); uc->uc_stack.ss_sp = st; uc->uc_stack.ss_size = stack; uc->uc_link = 0; makecontext(uc, entry, 0);
#else
  ucontext_t *uc = (ucontext_t *)stk_iop_ops.create(user, stack, entry);
#endif
  if (uc && stk_iop_n < 64) { stk_mark(uc->uc_stack.ss_sp, uc->uc_stack.ss_size); stk_iop[stk_iop_n].lo = uc->uc_stack.ss_sp; stk_iop[stk_iop_n++].n = uc->uc_stack.ss_size; }
  return uc; }
static void stack_report(void)
{
  int i; if (!getenv("MGS2_STACK_MARK")) return;
  for (i = 1; i < K_THREADS; i++) if (k_th[i].hstack) fprintf(stderr, "stack: guest thread %d uses %u of %u bytes\n", i, (unsigned)stk_used(k_th[i].hstack, K_HSTACK), (unsigned)K_HSTACK);
  for (i = 0; i < stk_iop_n; i++) fprintf(stderr, "stack: IOP thread %d uses %u of %u bytes\n", i, (unsigned)stk_used(stk_iop[i].lo, stk_iop[i].n), (unsigned)stk_iop[i].n);
}
static void iop_host_init(const char *disc_root)
{
  IopRtConfig cfg; memset(&cfg, 0, sizeof cfg);
#ifdef HOST_BIG_ALLOC   /* PSP: from the volatile block when granted (psp/main_psp.c) */
  void *host_big_alloc(size_t n); uint8_t *spu_ram = (uint8_t *)host_big_alloc(SPU2_RAM_SIZE); Spu2Ops so;
#else
  static uint8_t spu_ram[SPU2_RAM_SIZE]; Spu2Ops so;
#endif
  iop_rt_coro_ucontext(&cfg.coro); cfg.host_stack = IOP_HSTACK; stk_iop_ops = cfg.coro; cfg.coro.create = stk_iop_create; cfg.log = iop_logf;
  spu2_init(&g_spu, spu_ram); g_spu.x_state_only = !getenv("MGS2_SPU_MIX"); spu2_bind_ops(&g_spu, &so, 0, 0, 0);
  cfg.sd.user = so.user; cfg.sd.ram = spu_ram; cfg.sd.ram_size = SPU2_RAM_SIZE;
  cfg.sd.set_param = so.set_param; cfg.sd.set_addr = so.set_addr; cfg.sd.get_envx = so.get_envx; cfg.sd.get_nax = so.get_nax;
  cfg.sd.key_on = so.key_on; cfg.sd.key_off = so.key_off; cfg.sd.set_irq = so.set_irq; sif_step_host = spu_step;
  if (getenv("MGS2_SD_LOG")) {
    spu_log = atoi(getenv("MGS2_SD_LOG")); spu_so = so;
    cfg.sd.set_param = spl_set_param; cfg.sd.set_addr = spl_set_addr; cfg.sd.key_on = spl_key_on; cfg.sd.key_off = spl_key_off;
    cfg.sd.get_nax = spl_get_nax; cfg.sd.dma_done = spl_dma_done;
  }
  cfg.disc.lookup = disc_lookup_cb; cfg.disc.read = disc_read_cb;
  disc_open(disc_root);
  g_iop = iop_rt_create(&cfg);
  if (!g_iop) { fprintf(stderr, "iop: runtime not created\n"); exit(8); }
  sif_init();
  sd_bt_trans_rt = iop_rt_builtin("libsd", 18, 0); sd_bt_status_rt = iop_rt_builtin("libsd", 20, 0);
  iop_rt_bind(g_iop, "libsd", 18, sd_bt_trans); iop_rt_bind(g_iop, "libsd", 20, sd_bt_status);
  cdv_read_rt = iop_rt_builtin("cdvdman", 6, 0); iop_rt_bind(g_iop, "cdvdman", 6, cdv_Read);
  iop_rt_bind(g_iop, "cdvdman", 24, cdv_ReadClock); iop_rt_bind(g_iop, "cdvdman", 28, cdv_Status); iop_rt_bind(g_iop, "cdvdman", 37, cdv_Callback);
  iop_rt_bind(g_iop, "cdvdman", 39, cdv_Break); iop_rt_bind(g_iop, "cdvdman", 75, cdv_one); iop_rt_bind(g_iop, "intrman", 6, sif_ok); sif_iop_tick = cdv_tick;
  sif_host_server(0x80000006u, lf_server, 0x200, "loadfile");
  if (getenv("MGS2_MC0")) snprintf(mc_root, sizeof mc_root, "%s", getenv("MGS2_MC0"));
  sif_host_server(0x80000400u, mc_server, 0x500, "mcserv");
  sif_host_server(0x80000100u, pad_server, 0x80, "padman"); sif_host_server(0x80000101u, pad_server_ext, 0x80, "padman ext"); sif_vblank_host = pad_vblank;
  sif_host_server(0x80000001u, fio_server, 0x1010, "fileio"); sif_host_server(0x80000003u, heap_server, 0x140, "iopheap");
  sif_host_server(0x80000592u, cd_srv_init, 0x40, "cdvd init"); sif_host_server(0x8000059Au, cd_srv_ready, 0x40, "cdvd ready");
  sif_host_server(0x8000059Cu, cd_srv_ready, 0x40, "cdvd ready 2"); sif_host_server(0x80000593u, cd_srv_scmd, 0x200, "cdvd S");
  sif_host_server(0x80000595u, cd_srv_ncmd, 0x200, "cdvd N"); sif_host_server(0x80000597u, cd_srv_search, 0x240, "cdvd search");
}
#endif
