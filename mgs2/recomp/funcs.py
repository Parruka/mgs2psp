"""Function discovery for the stripped MGS2 code (the ELF has no symbols).
Starts: the entry point, jal / j targets, the stage image's hash exports, words in data that point at code right after a function end,
and code that follows a 'jr ra' + delay slot (or a tail 'j' + delay slot, if it opens a stack frame) without being reached from the
function before it.  End of a function = the next start.
Usage (as a module): funcs.discover(regions, data_ranges, extra_starts) -> sorted list of (start, end)."""
import struct
from recomp import W

JR_RA = 0x03e00008
def is_branch(ins):
    op = ins >> 26
    return (op == 1 and (ins >> 16) & 31 < 24) or op in (4, 5, 6, 7, 0x14, 0x15, 0x16, 0x17) or (op in (0x10, 0x11, 0x12) and (ins >> 21) & 31 == 8)
def s16(x): return x - 0x10000 if x & 0x8000 else x

VALID_OP = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c,
            0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x31, 0x33, 0x36, 0x37,
            0x39, 0x3e, 0x3f}
SPECIAL_BAD = {1, 5, 0xe, 0x15, 0x1c, 0x1d, 0x1e, 0x1f, 0x29, 0x35, 0x37, 0x39, 0x3d}
MMI_FN = {0, 1, 4, 8, 9, 0x10, 0x11, 0x12, 0x13, 0x18, 0x19, 0x1a, 0x1b, 0x20, 0x21, 0x28, 0x29, 0x30, 0x31, 0x34, 0x36, 0x37, 0x3c, 0x3e, 0x3f}
def valid(ins):
    """False for words that cannot be EE instructions (data inside a code region)"""
    op = ins >> 26; rs = (ins >> 21) & 31
    if op not in VALID_OP: return False
    if op == 0: return (ins & 63) not in SPECIAL_BAD
    if op == 0x11: return rs in (0, 2, 4, 6, 8, 16, 20)
    if op == 0x12: return rs in (1, 2, 5, 6, 8) or rs >= 16
    if op == 0x10: return rs in (0, 4, 8, 16)
    if op == 0x1c: return (ins & 63) in MMI_FN
    return True

def find_jt(addr, start):
    """switch tables: [sltiu n] ... lui hi ... [addiu lo] ... addu ... lw off(base) ... jr  -> (table, n)"""
    jreg = (W(addr) >> 21) & 31; n = None; lwoff = None; adds = {}; hi = None
    for a in range(addr - 4, max(start, addr - 160) - 4, -4):
        ins = W(a); op = ins >> 26; rs = (ins >> 21) & 31; rt = (ins >> 16) & 31
        if op == 0x0b and n is None: n = ins & 0xffff
        if op == 0x23 and lwoff is None and rt == jreg: lwoff = s16(ins & 0xffff)
        if op == 0x09 and rs == rt and rt not in adds and hi is None: adds[rt] = s16(ins & 0xffff)
        if op == 0x0f and hi is None and lwoff is not None: hi = (ins & 0xffff) << 16; hreg = rt
    if None in (n, hi, lwoff): return None
    t = (hi + adds.get(hreg, 0) + lwoff) & 0xffffffff
    if not all((W(t + 4 * k) & 3) == 0 and abs(W(t + 4 * k) - addr) < 0x10000 for k in range(n)): return None   # not a switch table
    return (t, n)

def extent(s, limit):
    """end of the code reachable from s without leaving [s, limit): fall-through, branches, switch tables; stops after returns and jumps"""
    seen = set(); todo = [s]; top = s
    while todo:
        a = todo.pop()
        while s <= a < limit and a not in seen:
            ins = W(a); op = ins >> 26
            if not valid(ins): break                                     # ran into data
            seen.add(a); top = max(top, a)
            if is_branch(ins):
                t = a + 4 + (s16(ins & 0xffff) << 2)
                if s <= t < limit: todo.append(t)
                seen.add(a + 4); top = max(top, a + 4)
                if op == 4 and (ins >> 16) & 0x3ff == 0: break          # b (beq $0, $0): unconditional
                a += 8; continue                                         # fall-through after the delay slot
            if op == 2:                                                  # j
                t = (a & 0xf0000000) | ((ins & 0x3ffffff) << 2)
                if s <= t < limit: todo.append(t)
                seen.add(a + 4); top = max(top, a + 4); break
            if op == 0 and ins & 63 == 8:                                # jr
                seen.add(a + 4); top = max(top, a + 4)
                if ins != JR_RA:
                    jt = find_jt(a, s)
                    if jt: todo.extend(W(jt[0] + 4 * k) for k in range(jt[1]))
                break
            a += 4
    return top + 4

def reach(s, limit):
    """addresses reachable from s inside [s, limit) (fall-through, branches, switch tables), plus the calls / tail jumps seen"""
    seen = set(); todo = [s]; calls = set(); tails = set()
    while todo:
        a = todo.pop()
        while s <= a < limit and a not in seen:
            ins = W(a); op = ins >> 26
            if not valid(ins): break
            seen.add(a)
            if op == 3: calls.add((a & 0xf0000000) | ((ins & 0x3ffffff) << 2))
            if is_branch(ins):
                t = a + 4 + (s16(ins & 0xffff) << 2)
                if s <= t < limit: todo.append(t)
                else: tails.add(t)
                seen.add(a + 4)
                if op == 4 and (ins >> 16) & 0x3ff == 0: break               # b: unconditional
                a += 8; continue
            if op == 2:
                t = (a & 0xf0000000) | ((ins & 0x3ffffff) << 2)
                if s <= t < limit: todo.append(t)
                else: tails.add(t)
                seen.add(a + 4); break
            if op == 0 and ins & 63 == 8:
                seen.add(a + 4)
                if ins != JR_RA:
                    jt = find_jt(a, s)
                    if jt: todo.extend(W(jt[0] + 4 * k) for k in range(jt[1]))
                break
            a += 4
    return seen, calls, tails

def discover(regions, data_ranges, extra_starts=()):
    """Recursive descent from the seeds (entry points, stage exports, code pointers in data and in lui/addiu pairs): only code that
    is reached contributes call targets, so data inside code regions is never read as calls.  Code reached only through pointers the
    scans missed is picked up afterwards: valid code that starts right after a reached function's return.
    regions: [(lo, hi)] of code; data_ranges: [(lo, hi)] scanned for code pointers"""
    inr = lambda a: any(lo <= a < hi for lo, hi in regions)
    rhi = lambda a: next(hi for lo, hi in regions if lo <= a < hi)
    after_end = set()
    for lo, hi in regions:
        for a in range(lo, hi - 8, 4):
            if W(a) == JR_RA:
                b = a + 8
                while b < hi and W(b) == 0: b += 4
                if b < hi: after_end.add(b)
    after_j = set()                              # after a tail jump (j + delay slot): a function start too, but only taken with a pointer to it
    for lo, hi in regions:
        for a in range(lo, hi - 8, 4):
            if W(a) >> 26 == 2:
                b = a + 8
                while b < hi and W(b) == 0: b += 4
                if b < hi: after_j.add(b)
    def codeish0(b):                             # valid code from b that reaches a return (a pointer after a tail jump can be data)
        try: seen, _, _ = reach(b, rhi(b))
        except Exception: return False
        return (len(seen) >= 2 and any(W(a) == JR_RA or W(a) >> 26 == 2 for a in seen)     # ends in a return or a tail jump
                and all(valid(W(a)) for a in seen) and W(b) >> 26 != 0x10)                  # (not opening with COP0: float data like 9.0f decodes as BC0F)
    ptr_ok = set(after_end); ptr_j = after_j - after_end
    ok_ptr = lambda v: v in ptr_ok or (v in ptr_j and codeish0(v))
    jtwords = set()
    for lo, hi in regions:
        for a in range(lo, hi, 4):
            ins = W(a)
            if ins >> 26 == 0 and ins & 63 == 8 and ins != JR_RA:
                jt = find_jt(a, max(lo, a - 160))
                if jt: jtwords.update(range(jt[0], jt[0] + 4 * jt[1], 4))
    cases = set(W(a) for a in jtwords)          # switch case labels: a data word equal to one is not a function pointer
    seeds = set(a for a in extra_starts if inr(a))
    for lo, hi in data_ranges:
        for a in range(lo, hi, 4):
            v = W(a)
            if a not in jtwords and v not in cases and ok_ptr(v): seeds.add(v)
    for lo, hi in regions:
        for a in range(lo, hi, 4):
            ins = W(a)
            if ins >> 26 != 0x0f: continue
            r = (ins >> 16) & 31; up = (ins & 0xffff) << 16
            for b in range(a + 4, min(a + 36, hi), 4):
                i2 = W(b); op2 = i2 >> 26
                if op2 in (0x09, 0x0d) and (i2 >> 21) & 31 == r:
                    v = (up + (s16(i2 & 0xffff) if op2 == 0x09 else i2 & 0xffff)) & 0xffffffff
                    if v not in cases and ok_ptr(v): seeds.add(v)
                    break
                if (op2 == 0 and (i2 >> 11) & 31 == r) or (op2 not in (0, 0x2b, 0x28, 0x29, 0x3f, 0x39, 0x1f, 1, 2, 3, 4, 5, 6, 7, 0x14, 0x15, 0x16, 0x17) and (i2 >> 16) & 31 == r): break
    starts = set(); covered = set(); todo = sorted(seeds)
    def run(todo):
        while todo:
            s = todo.pop()
            if s in starts or not inr(s) or not valid(W(s)): continue
            starts.add(s)
            seen, calls, tails = reach(s, rhi(s))
            covered.update(seen)
            todo.extend(t for t in calls | tails if inr(t) and t not in starts)
    run(todo)
    # functions reached only through pointers not found above: valid code right after a covered return, not covered itself
    def codeish(b):                              # the reachable code from b is valid and ends in a return
        seen, _, _ = reach(b, rhi(b))
        return len(seen) >= 2 and any(W(a) == JR_RA for a in seen)
    for _ in range(64):
        added = False
        for b in sorted(after_end):                # one at a time: a candidate may cover the next ones (switch cases after a return)
            if b not in covered and b not in starts and valid(W(b)) and valid(W(b + 4)) and codeish(b): run([b]); added = True
        if not added: break
    # the same after a tail jump (j + delay slot), when the code opens a stack frame (addiu sp, sp, -n): w00a calls ELF 0x105ad8, which
    # follows the tail jump of 0x105a60's function and is reached only through a handler pointer built at run time
    for _ in range(64):
        added = False
        for b in sorted(after_j - after_end):
            if (b not in covered and b not in starts and W(b) >> 16 == 0x27bd and W(b) & 0x8000 and valid(W(b + 4)) and codeish0(b)):
                run([b]); added = True
        if not added: break
    # boundaries: next start, reachable end; a jump out of those bounds is a tail jump, and its target a function start (fixpoint)
    for _ in range(16):
        ss = sorted(starts); out = []; new = []
        for i, s in enumerate(ss):
            e = min(ss[i + 1], rhi(s)) if i + 1 < len(ss) else rhi(s)
            seen, calls, tails = reach(s, e)
            new += [t for t in calls | tails if inr(t) and t not in starts]
            out.append((s, max(seen) + 4 if seen else s + 4))
        if not new: break
        run(new)
    return out
