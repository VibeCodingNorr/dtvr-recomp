#!/usr/bin/env python3
"""Disassemble NE code segments with objdump (i8086 mode) and annotate fixups.

Usage: ne_disasm.py EXE OUTDIR [--dll-dir DIR ...]

Writes OUTDIR/segNN.asm per code segment, OUTDIR/data.bin (automatic data
segment) and OUTDIR/strings.txt (DS offsets of C strings). Import ordinals are
named from export tables of DLLs found in --dll-dir (e.g. the game's own
DLLs and Wine's *.dll16 / *.exe16 builtins). Output contains disassembled
game code: keep it out of Git (use build/ or local/).
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ne import NE, NEError, find_embedded_ne  # noqa: E402


def load_exports(dirs):
    exports = {}
    for d in dirs:
        for p in sorted(Path(d).iterdir()):
            if p.suffix.lower() not in (".dll", ".exe", ".drv", ".dll16", ".exe16", ".drv16"):
                continue
            try:
                ne = find_embedded_ne(p.read_bytes())
            except (NEError, OSError):
                continue
            module = p.name.split(".")[0].upper()
            if module == "KRNL386":
                module = "KERNEL"
            if module in exports and len(exports[module]) >= len(ne.names):
                continue
            exports[module] = ne.names
    return exports


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("exe", type=Path)
    ap.add_argument("out", type=Path)
    ap.add_argument("--dll-dir", action="append", default=[])
    ap.add_argument("--use32", action="append", type=int, default=[], metavar="SEG",
                    help="disassemble segment SEG as 32-bit code (e.g. after a DPMI "
                         "descriptor switch, as seen in rally.exe segment 9)")
    args = ap.parse_args()
    ne = NE(args.exe.read_bytes())
    exports = load_exports(args.dll_dir)
    args.out.mkdir(parents=True, exist_ok=True)

    data_seg = next(s for s in ne.segments if s["data"])
    ds = ne.segment_bytes(data_seg["number"])
    (args.out / "data.bin").write_bytes(ds)
    with open(args.out / "strings.txt", "w", encoding="utf-8") as f:
        for m in re.finditer(rb"[\x20-\x7e\xa0-\xff\t\r\n]{4,}", ds):
            f.write(f"{m.start():04x} {m.group().decode('latin-1')!r}\n")

    def describe(reloc):
        kind, a, b = reloc["target"]
        if kind == "internal":
            if a == 0xFF:
                return f"entry#{b}"
            # source type 2 patches only the selector; the offset in the
            # instruction is the real target offset.
            return f"seg{a}" if reloc["src"] == 2 else f"seg{a}:{b:04x}"
        if kind == "ordinal":
            return f"{a}.{exports.get(a, {}).get(b, b)}"
        return f"{a}.{b}"

    summary = []
    for seg in ne.segments:
        if seg["data"] or not seg["size"]:
            continue
        n = seg["number"]
        raw = args.out / f"seg{n:02d}.bin"
        raw.write_bytes(ne.segment_bytes(n))
        notes = {}
        for site, r in ne.fixup_sites(n):
            notes.setdefault(site, describe(r))
        asm = subprocess.run(["objdump", "-D", "-b", "binary", "-m",
                              "i386" if n in args.use32 else "i8086", "-M", "intel",
                              str(raw)], check=True, capture_output=True, text=True).stdout
        lines = []
        for line in asm.splitlines():
            m = re.match(r"\s*([0-9a-f]+):\t((?:[0-9a-f]{2} )+)", line)
            if m:
                start = int(m.group(1), 16)
                length = len(m.group(2).split())
                hits = [notes[s] for s in range(start, start + length) if s in notes]
                if hits:
                    line += "    ; " + ", ".join(hits)
            lines.append(line)
        (args.out / f"seg{n:02d}.asm").write_text("\n".join(lines) + "\n")
        summary.append(f"seg{n}: {seg['size']} bytes, {len(notes)} fixup sites")
    print(f"entry seg{ne.entry[0]}:{ne.entry[1]:04x}; modules {ne.modules}")
    print(f"export tables: {', '.join(f'{k}({len(v)})' for k, v in sorted(exports.items()))}")
    print("\n".join(summary))


if __name__ == "__main__":
    main()
