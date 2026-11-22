# patch_rainspr4.py (task #161, x86 check only): the rain's four sprite actors' calls by actor and by part of the route (before vb 16,000:
# the opening; 16,000-31,499: the deck; from 31,500: inside after the deck), to tell what each draws where.  After patch_rainspr3.py.
import hashlib
T = '/home/claude/sb17/'
def edit(rel, md5, pairs):
    p = T + rel; s = open(p).read()
    assert hashlib.md5(s.encode()).hexdigest().startswith(md5), (rel, hashlib.md5(s.encode()).hexdigest()[:8])
    for old, new in pairs:
        assert s.count(old) == 1, (rel, old[:60], s.count(old)); s = s.replace(old, new)
    open(p, 'w').write(s); print(rel, hashlib.md5(s.encode()).hexdigest()[:8])
edit('boot/xlive_chk.c', 'c2c22892', [
 ("  ts_own[w == 0 ? !rs : w == -2 ? 2 : 3][out]++;\n",
  "  ts_own[w == 0 ? !rs : w == -2 ? 2 : 3][out]++;\n"
  "  if (rs) { const int a = !strcmp(nm, \"body_sph.c\") ? 0 : !strcmp(nm, \"charaspread.c\") ? 1 : !strcmp(nm, \"line_sph.c\") ? 2 : 3;   /* patch_rainspr4 */\n"
  "    ts_reg[a][cur_vb < 16000 ? 0 : cur_vb < 31500 ? 1 : 2]++; if (cur_vb < ts_rvb[a][0] || !ts_rvb[a][0]) ts_rvb[a][0] = cur_vb;\n"
  "    if (cur_vb > ts_rvb[a][1]) ts_rvb[a][1] = cur_vb; }\n"),
 ("static long ts_nb_n, ts_nb_pool, ts_nb_vb0 = -1, ts_nb_vb1;",
  "static long ts_reg[4][3], ts_rvb[4][2];   /* patch_rainspr4: [body_sph, charaspread, line_sph, rain_gas_pers][opening, deck, inside]; first and last vb */\n"
  "static long ts_nb_n, ts_nb_pool, ts_nb_vb0 = -1, ts_nb_vb1;"),
 ('                           "vb %ld..%ld\\n", ts_nb_n, ts_nb_pool, ts_nb_vb0, ts_nb_vb1); }',
  '                           "vb %ld..%ld\\n", ts_nb_n, ts_nb_pool, ts_nb_vb0, ts_nb_vb1);\n'
  '      { static const char *an[4] = { "body_sph", "charaspread", "line_sph", "rain_gas_pers" };\n'
  '        for (int a = 0; a < 4; a++) if (ts_reg[a][0] + ts_reg[a][1] + ts_reg[a][2])\n'
  '          fprintf(stderr, "xlive gb: thin check sprites: %s calls: the opening (vb < 16,000) %ld, the deck (16,000-31,499) %ld, inside (31,500 on) "\n'
  '                  "%ld; vb %ld..%ld\\n", an[a], ts_reg[a][0], ts_reg[a][1], ts_reg[a][2], ts_rvb[a][0], ts_rvb[a][1]); } }'),
])
