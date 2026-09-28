#!/usr/bin/env python3
"""Generate stubs_auto.S for symbols the harness link cannot resolve.
Functions (called via PLT32) return 0; data symbols get 64 KiB of zeros."""
import re, subprocess, sys, glob
build = sys.argv[1]
log = open(f"{build}/link.log").read()
syms = sorted(set(re.findall(r"undefined reference to `([^']+)'", log)))
kind = {}
for o in glob.glob(f"{build}/CMakeFiles/apps.dir/**/*.o", recursive=True) + glob.glob(f"{build}/CMakeFiles/apps_ui_harness.dir/*.o"):
    out = subprocess.run(["readelf", "-rW", o], capture_output=True, text=True).stdout
    for line in out.splitlines():
        p = line.split()
        if len(p) >= 5 and p[2].startswith("R_X86_64"):
            s = p[4]
            if s in syms:
                if p[2] in ("R_X86_64_PLT32",): kind[s] = "f"
                else: kind.setdefault(s, "d")
existing = open(f"{build}/stubs_auto.S").read()
old = set(re.findall(r"^\.globl (\S+)$", existing, re.M))
with open(f"{build}/stubs_auto.S", "a") as f:
    for s in syms:
        if s in old: continue
        isdata = kind.get(s, "f") == "d" and not re.match(r"(controls_|radio_|voice_|vol_|mfk_|dialog_|on_|btn_)", s)
        if not isdata:
            f.write(f'.text\n.globl {s}\n.type {s},@function\n{s}:\n  xor %eax,%eax\n  pxor %xmm0,%xmm0\n  ret\n')
        else:
            f.write(f'.bss\n.globl {s}\n.type {s},@object\n.align 64\n{s}:\n  .zero 65536\n')
print(len(syms), "stubbed", sum(1 for s in syms if kind.get(s,'f')=='d'), "data")
