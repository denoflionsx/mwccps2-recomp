"""Which hooked functions can reach a negative control's code (for portcheck.py's targeted runs).

usage: python tools/ctlreach.py CONTROL [CONTROL ...]   prints each control's hook addresses

A control's #ifdef/#ifndef block sits inside some function of src. If that function has a
HOOK, the control only runs when that hook's replacement runs. If not (a helper), its callers are
followed by name, across all sources, until hooked functions are reached. A case that reaches none
of those hooks runs the control build exactly as the normal build, so it cannot fail.

Anything uncertain gives None (portcheck then runs the full suite): a control outside any function,
or a name on the way that is used outside a function body (a macro, a table, a function pointer
initializer).
"""
import glob
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import common  # noqa: E402
from common import BUILD, DATA, ORIG_EXE, ROOT  # noqa: E402
SRC = ROOT / "src"
IDENT = re.compile(r"[A-Za-z_]\w*")
KEYWORDS = {"if", "while", "for", "switch", "return", "sizeof", "do", "else"}


def strip(text):
    """Comments and string/char literals blanked (newlines kept), so braces and names are real code."""
    out = re.sub(r"/\*.*?\*/|//[^\n]*|\"(?:\\.|[^\"\\\n])*\"|'(?:\\.|[^'\\\n])*'",
                 lambda m: re.sub(r"[^\n]", " ", m.group(0)), text, flags=re.S)
    return out


def active(s):
    """The normal build's code: preprocessor lines blanked, and so are the lines of inactive
    conditional branches (#ifdef: nothing is defined; #if: true unless #if 0)."""
    stack, out = [], []
    for line in s.split("\n"):
        t = line.strip()
        if t.startswith("#"):
            d = t[1:].strip()
            if d.startswith("ifdef"):
                stack.append(False)
            elif d.startswith("ifndef"):
                stack.append(True)
            elif d.startswith("if"):
                stack.append(not re.match(r"if\s+0\b", d))
            elif d.startswith("elif") and stack:
                stack[-1] = not stack[-1] and not re.match(r"elif\s+0\b", d)
            elif d.startswith("else") and stack:
                stack[-1] = not stack[-1]
            elif d.startswith("endif") and stack:
                stack.pop()
            out.append(" " * len(line))
        else:
            out.append(line if all(stack) else " " * len(line))
    return "\n".join(out)


def parse(path):
    """(functions, outside): functions = [(name, start, end)] by offset in the stripped text;
    outside = the stripped text with function bodies blanked."""
    raw = path.read_text(encoding="utf-8")
    s = strip(raw)
    code = active(s)
    funcs, depth, i, n = [], 0, 0, len(code)
    while i < n:
        c = code[i]
        if c == "{" and depth == 0:
            head = code[max(0, code.rfind(";", 0, i), code.rfind("}", 0, i)) + 1: i]
            m = re.search(r"([A-Za-z_]\w*)\s*\([^;{}=]*\)\s*$", head)
            is_func = m and m.group(1) not in KEYWORDS and "=" not in head.split(m.group(1))[0][-3:] \
                and not re.search(r"\b(struct|union|enum)\b[^()]*$", head)
            start, d = i, 0
            while i < n:
                if code[i] == "{":
                    d += 1
                elif code[i] == "}":
                    d -= 1
                    if d == 0:
                        break
                i += 1
            if is_func:
                funcs.append((m.group(1), start, i))
            i += 1
            continue
        i += 1
    outside = list(s)
    for _, a, b in funcs:
        for k in range(a, min(b + 1, len(outside))):
            if outside[k] != "\n":
                outside[k] = " "
    return raw, s, funcs, "".join(outside)


def load():
    files = {}
    hooks = {}  # function name -> set of hex addresses
    for f in sorted(str(p) for p in SRC.rglob("*.c")):
        p = Path(f)
        raw, s, funcs, outside = parse(p)
        files[p.name] = (raw, s, funcs, outside)
        for addr, fn in re.findall(r"^HOOK\(0x([0-9a-fA-F]+),\s*(\w+)\)", raw, re.M):
            hooks.setdefault(fn, set()).add(addr.lower())
    return files, hooks


def reach(ctl, files=None, hooks=None):
    if files is None:
        files, hooks = load()
    tag = re.compile(r"^[ \t]*#if(?:n?def)?\b[^\n]*\bRECOMP_NEGATIVE_CONTROL_%s\b" % re.escape(ctl), re.M)
    seeds = []
    for name, (raw, s, funcs, _) in files.items():
        for m in tag.finditer(raw):
            fn = next((f for f, a, b in funcs if a <= m.start() <= b), None)
            if fn is None:
                return None  # file-scope control
            seeds.append(fn)
    if not seeds:
        return None
    out, seen, todo = set(), set(), list(seeds)
    while todo:
        fn = todo.pop()
        if fn in seen:
            continue
        seen.add(fn)
        hooked = fn in hooks
        if hooked:
            out |= hooks[fn]
            # its entry trap sees calls through the original's jmp and direct calls, but clang may
            # inline it into a ported caller (Peephole_Block into Peephole_Run): keep following
            # callers, so those callers' hooks count too
        word = re.compile(r"\b%s\b" % re.escape(fn))
        callers = 0
        for name, (raw, s, funcs, outside) in files.items():
            # a use outside function bodies (other than its own declaration line) may hide a call
            for m in word.finditer(outside):
                line = outside[outside.rfind("\n", 0, m.start()) + 1: outside.find("\n", m.end())]
                if re.match(r"^\s*HOOK\(", line):
                    continue
                if re.search(r"\b%s\s*\([^;]*\)\s*;\s*$" % re.escape(fn), line) or \
                        re.search(r"\b%s\s*\([^;]*$" % re.escape(fn), line):
                    continue  # its prototype or definition header
                if hooked:
                    continue  # e.g. a hooked function's address in a table: its own hook covers it
                return None
            for f, a, b in funcs:
                if f != fn and word.search(s, a, b + 1):
                    todo.append(f)
                    callers += 1
        if not callers and not hooked:
            return None  # an unhooked function nothing calls by name: unknown entry
    return out


def main():
    files, hooks = load()
    for c in sys.argv[1:]:
        r = reach(c, files, hooks)
        print(c, "FULL SUITE" if r is None else " ".join(sorted(r)))


if __name__ == "__main__":
    main()
