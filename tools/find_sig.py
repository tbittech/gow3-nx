"""86a: find [spu-interp] page signatures (16 bytes hex) inside SPU images.
usage: python find_sig.py <log> [root ...]  -> for each 'sig=' line prints files/offsets that contain it."""
import os, re, sys

log = sys.argv[1]
roots = sys.argv[2:] or ["port/spu", "analysis"]
sigs = {}
for ln in open(log, errors="replace"):
    m = re.search(r"img=0x([0-9A-F]+) ls=0x([0-9A-F]+)\s+(\d+)\s+sig=([0-9A-F]{32})", ln)
    if m:
        k = (m.group(1), m.group(2), m.group(4))
        sigs[k] = sigs.get(k, 0) + int(m.group(3))
files = []
for r in roots:
    for d, _, fs in os.walk(r):
        for f in fs:
            if f.endswith((".bin", ".elf", ".self", ".spu")) or f == "raw.bin":
                files.append(os.path.join(d, f))
blobs = {}
for f in files:
    try:
        if os.path.getsize(f) < 64 << 20:
            blobs[f] = open(f, "rb").read()
    except OSError:
        pass
for (img, ls, sig), n in sorted(sigs.items(), key=lambda x: -x[1])[:16]:
    pat = bytes.fromhex(sig)
    hits = []
    for f, b in blobs.items():
        i = b.find(pat)
        while i >= 0 and len(hits) < 6:
            hits.append(f"{f}@0x{i:X}")
            i = b.find(pat, i + 1)
    zero = " (zeros)" if pat == bytes(16) else ""
    print(f"img={img} ls=0x{ls} steps={n} sig={sig}{zero}: " + (", ".join(hits) or "NO MATCH"))
