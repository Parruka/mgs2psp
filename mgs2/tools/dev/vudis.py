#!/usr/bin/env python3
# vudis.py: VU micro code disassembler for gsref traces (tools/gsref.c GSREF_TRACE=<call>: lines "t <pc> hi <upper> lo <lower> ...").
# Prints each executed address once, in first-run order (-all: every executed line). Usage: vudis.py [-all] < trace.txt
# Upper ops follow gsref vu_upper tables; lower ops the VU encoding. Prints instructions only (no game data).
BC = "xyzw"
def dest(d): return "".join(c for i, c in enumerate("xyzw") if d & (8 >> i))
N1 = {0x1c:"MULq",0x1d:"MAXi",0x1e:"MULi",0x1f:"MINIi",0x20:"ADDq",0x21:"MADDq",0x22:"ADDi",0x23:"MADDi",0x24:"SUBq",0x25:"MSUBq",0x26:"SUBi",0x27:"MSUBi",0x28:"ADD",0x29:"MADD",0x2a:"MUL",0x2b:"MAX",0x2c:"SUB",0x2d:"MSUB",0x2e:"OPMSUB",0x2f:"MINI"}
N0 = ["ADD","SUB","MADD","MSUB","MAX","MINI","MUL"]
NS = {0:["ADDA%s","ADDA%s","ADDA%s","ADDA%s"],1:["SUBA%s"]*4,2:["MADDA%s"]*4,3:["MSUBA%s"]*4,4:["ITOF0","ITOF4","ITOF12","ITOF15"],5:["FTOI0","FTOI4","FTOI12","FTOI15"],6:["MULA%s"]*4,7:["MULAq","ABS","MULAi","CLIP"],8:["ADDAq","MADDAq","ADDAi","MADDAi"],9:["SUBAq","MSUBAq","SUBAi","MSUBAi"],10:["ADDA","MADDA","MULA","NOP"],11:["SUBA","MSUBA","OPMULA","NOP"]}
def dis(w):
    w &= 0x07ffffff
    op = w & 0x3f; d = (w >> 21) & 15; ft = (w >> 16) & 31; fs = (w >> 11) & 31; fd = (w >> 6) & 31; sel = op & 3
    if op < 0x1c: return "%s%s.%s vf%d, vf%d, vf%d%s" % (N0[op >> 2], BC[sel], dest(d), fd, fs, ft, BC[sel])
    if op < 0x30: return "%s.%s vf%d, vf%d, vf%d" % (N1[op], dest(d), fd, fs, ft)
    if op >= 0x3c:
        idx = fd
        if idx >= 12: return "?"
        nm = NS[idx][sel]
        if "%s" in nm: nm = nm % BC[sel]
        if nm == "NOP": return "NOP"
        if nm.startswith(("ITOF", "FTOI", "ABS")): return "%s.%s vf%d, vf%d" % (nm, dest(d), ft, fs)
        if nm == "CLIP": return "CLIPw.xyz vf%d, vf%d" % (fs, ft)
        return "%s.%s ACC, vf%d, vf%d" % (nm, dest(d), fs, ft)
    return "?%02x" % op
import sys, re
BC = "xyzw"
def dest(d): return "".join(c for i, c in enumerate("xyzw") if d & (8 >> i))
def s11(x): x &= 0x7ff; return x - 0x800 if x & 0x400 else x
EXT = {0x30:"MOVE",0x31:"MR32",0x34:"LQI",0x35:"SQI",0x36:"LQD",0x37:"SQD",0x38:"DIV",0x39:"SQRT",0x3a:"RSQRT",0x3b:"WAITQ",0x3c:"MTIR",0x3d:"MFIR",0x3e:"ILWR",0x3f:"ISWR",0x40:"RNEXT",0x41:"RGET",0x42:"RINIT",0x43:"RXOR",0x64:"MFP",0x68:"XTOP",0x69:"XITOP",0x6c:"XGKICK",0x70:"ESADD",0x71:"ERSADD",0x72:"ELENG",0x73:"ERLENG",0x74:"EATANxy",0x75:"EATANxz",0x76:"ESUM",0x78:"ESQRT",0x79:"ERSQRT",0x7a:"ERCPR",0x7b:"WAITP",0x7c:"ESIN",0x7d:"EATAN",0x7e:"EEXP"}
def lodis(lo, pc):
    op = lo >> 25; d = (lo >> 21) & 15; it = (lo >> 16) & 31; is_ = (lo >> 11) & 31; id_ = (lo >> 6) & 31; im = s11(lo)
    if op == 0x00: return "LQ.%s vf%d, %d(vi%d)" % (dest(d), it, im, is_)
    if op == 0x01: return "SQ.%s vf%d, %d(vi%d)" % (dest(d), is_, im, it)
    if op == 0x04: return "ILW.%s vi%d, %d(vi%d)" % (dest(d), it, im, is_)
    if op == 0x05: return "ISW.%s vi%d, %d(vi%d)" % (dest(d), it, im, is_)
    if op in (0x08, 0x09):
        i15 = ((lo >> 10) & 0x7800) | (lo & 0x7ff)
        return "%s vi%d, vi%d, 0x%x" % ("IADDIU" if op == 8 else "ISUBIU", it, is_, i15)
    if op in (0x10, 0x11, 0x12, 0x13): return "%s vi1, 0x%06x" % (["FCEQ","FCSET","FCAND","FCOR"][op - 0x10], lo & 0xffffff)
    if op in (0x14, 0x15, 0x16, 0x17): return "%s vi%d, 0x%03x" % (["FSEQ","FSSET","FSAND","FSOR"][op - 0x14], it, ((lo >> 10) & 0x800) | (lo & 0x7ff))
    if op in (0x18, 0x1a, 0x1b): return "%s vi%d, vi%d" % ({0x18:"FMEQ",0x1a:"FMAND",0x1b:"FMOR"}[op], it, is_)
    if op == 0x1c: return "FCGET vi%d" % it
    if op == 0x20: return "B 0x%04x" % (pc + 1 + im)
    if op == 0x21: return "BAL vi%d, 0x%04x" % (it, pc + 1 + im)
    if op == 0x24: return "JR vi%d" % is_
    if op == 0x25: return "JALR vi%d, vi%d" % (it, is_)
    if op in (0x28, 0x29): return "%s vi%d, vi%d, 0x%04x" % ("IBEQ" if op == 0x28 else "IBNE", it, is_, pc + 1 + im)
    if op in (0x2c, 0x2d, 0x2e, 0x2f): return "%s vi%d, 0x%04x" % (["IBLTZ","IBGTZ","IBLEZ","IBGEZ"][op - 0x2c], is_, pc + 1 + im)
    if op == 0x40:
        f = lo & 0x3f
        if f == 0x30: return "IADD vi%d, vi%d, vi%d" % (id_, is_, it)
        if f == 0x31: return "ISUB vi%d, vi%d, vi%d" % (id_, is_, it)
        if f == 0x32: i5 = id_ - 32 if id_ & 16 else id_; return "IADDI vi%d, vi%d, %d" % (it, is_, i5)
        if f == 0x34: return "IAND vi%d, vi%d, vi%d" % (id_, is_, it)
        if f == 0x35: return "IOR vi%d, vi%d, vi%d" % (id_, is_, it)
        if f >= 0x3c:
            f2 = (((lo >> 6) & 31) << 2) | (lo & 3); nm = EXT.get(f2, "?%x" % f2)
            fsf = (lo >> 21) & 3; ftf = (lo >> 23) & 3
            if nm in ("DIV",): return "DIV Q, vf%d%s, vf%d%s" % (is_, BC[fsf], it, BC[ftf])
            if nm in ("SQRT", "RSQRT"): return "%s Q, vf%d%s, vf%d%s" % (nm, is_, BC[fsf], it, BC[ftf])
            if nm in ("MOVE", "MR32"): return "%s.%s vf%d, vf%d" % (nm, dest(d), it, is_)
            if nm in ("LQI", "LQD"): return "%s.%s vf%d, (vi%d%s)" % (nm, dest(d), it, is_, "++" if nm == "LQI" else "--")
            if nm in ("SQI", "SQD"): return "%s.%s vf%d, (vi%d%s)" % (nm, dest(d), is_, it, "++" if nm == "SQI" else "--")
            if nm == "MTIR": return "MTIR vi%d, vf%d%s" % (it, is_, BC[fsf])
            if nm == "MFIR": return "MFIR.%s vf%d, vi%d" % (dest(d), it, is_)
            if nm in ("ILWR", "ISWR"): return "%s.%s vi%d, (vi%d)" % (nm, dest(d), it, is_)
            if nm == "XGKICK": return "XGKICK vi%d" % is_
            if nm in ("XTOP", "XITOP"): return "%s vi%d" % (nm, it)
            if nm == "MFP": return "MFP.%s vf%d, P" % (dest(d), it)
            if nm in ("ERSQRT", "ESQRT", "ERCPR", "ESIN", "EATAN", "EEXP"): return "%s P, vf%d%s" % (nm, is_, BC[fsf])
            if nm in ("ESADD", "ERSADD", "ELENG", "ERLENG", "ESUM", "EATANxy", "EATANxz"): return "%s P, vf%d" % (nm, is_)
            return nm
        return "?special %x" % f
    return "?op %x" % op
def full(pc, hi, lo):
    u = dis(hi)
    flags = "".join(f for b, f in ((31, "I"), (30, "E"), (29, "M"), (28, "D"), (27, "T")) if hi >> b & 1)
    if hi >> 31 & 1: l = "LOI 0x%08x (%g)" % (lo, __import__("struct").unpack("<f", lo.to_bytes(4, "little"))[0])
    else: l = lodis(lo, pc)
    return "%04x %-34s %-34s %s" % (pc, u, l, flags)
if __name__ == "__main__":
    seen = set()
    for line in sys.stdin:
        m = re.match(r"t T? ?([0-9a-f]{4}) hi ([0-9a-f]{8}) lo ([0-9a-f]{8})", line)
        if not m: continue
        pc = int(m.group(1), 16)
        if pc in seen and "-all" not in sys.argv: continue
        seen.add(pc)
        print(full(pc, int(m.group(2), 16), int(m.group(3), 16)))
