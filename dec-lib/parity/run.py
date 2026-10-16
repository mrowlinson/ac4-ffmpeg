#!/usr/bin/env python3
"""Parity gate: shim vs prebuilt c-ac4dec oracle, byte-exact PCM (cap 100)."""
import os, subprocess, sys, glob

ORACLE = "/Users/mrowlinson/Projects/dolby-ac4-cleanroom/c-ac4dec"
SHIM = "/Users/mrowlinson/Projects/ac4-ffmpeg/dec-lib/build/shim"
TMP = "/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-dec-librefactor/parity"
os.makedirs(TMP, exist_ok=True)

cands = []
cands += sorted(glob.glob("/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-corpus/streams/*"))
cands += sorted(glob.glob("/Volumes/NVME/dolby-ac4/scratch/vectors/*"))
cands += sorted(glob.glob("/Users/mrowlinson/Projects/dolby-ac4-cleanroom/vectors/*"))
# Recursive extras: decodable media only (.raw = reference PCM dumps,
# not inputs). Base files first, then extras by path; cap below.
MEDIA = (".ac4", ".ts", ".mp4", ".m4s", ".mpa", ".ec3", ".mpd")
for f in sorted(glob.glob("/Volumes/NVME/dolby-ac4/scratch/vectors/**/*",
                           recursive=True)):
    if f.endswith(MEDIA):
        cands.append(f)
seen, files = set(), []
for f in cands:
    b = os.path.basename(f)
    if not os.path.isfile(f) or b in seen:
        continue
    if b.endswith((".ini", ".md", ".txt", ".json", ".py", ".pcm", ".wav",
                   ".err", ".log")):
        continue
    seen.add(b)
    files.append(f)
files = files[:100]
print(f"vectors: {len(files)}")

def run(exe, inp, outp):
    e = os.path.join(TMP, "e.tmp")
    try:
        r = subprocess.run([exe, "decode", "--in", inp, "--out", outp],
                           stderr=open(e, "w"), timeout=300)
        err = open(e).read()
    except subprocess.TimeoutExpired:
        return ("TIMEOUT", "")
    first = err.split("\n")[0] if err else ""
    return (r.returncode, first)

ident = diff = 0
rows = []
for f in files:
    b = os.path.basename(f)
    o1, o2 = os.path.join(TMP, "a.pcm"), os.path.join(TMP, "b.pcm")
    for p in (o1, o2):
        if os.path.exists(p):
            os.remove(p)
    r1, e1 = run(ORACLE, f, o1)
    r2, e2 = run(SHIM, f, o2)
    pcm = "IDENT"
    if r1 == "TIMEOUT" or r2 == "TIMEOUT":
        pcm = "TIMEOUT"
    elif r1 != r2:
        pcm = f"RC-DIFF({r1}/{r2})"
    elif r1 != 0:
        n1 = e1.split("note=")[-1] if "note=" in e1 else ""
        n2 = e2.split("note=")[-1] if "note=" in e2 else ""
        if n1 == n2 and (n1 or (not e1 and not e2)):
            pcm = f"both-rc{r1}-agree"
        else:
            pcm = f"both-rc{r1}/{r2}" 
    elif not (os.path.exists(o1) and os.path.exists(o2)):
        if not os.path.exists(o1) and not os.path.exists(o2) and e1 == e2 and e1:
            pcm = "both-noout-agree"
        else:
            pcm = "MISSING-OUT"
    else:
        a = open(o1, "rb").read()
        c = open(o2, "rb").read()
        if a != c:
            pcm = f"BYTES-DIFF({len(a)}/{len(c)})"
    f1 = e1.replace("FMT ", "") if e1.startswith("FMT") else e1[:100]
    f2 = e2.replace("FMT ", "") if e2.startswith("FMT") else e2[:100]
    fmtd = "" if f1 == f2 else f" | FMT1:{f1} / FMT2:{f2}"
    ok = (pcm == "IDENT" or pcm.startswith("both-rc")) and not fmtd
    if pcm.startswith("both-") and pcm.endswith("-agree"):
        ok = True
        fmtd = " (agree)"
    if ok:
        ident += 1
    else:
        diff += 1
    rows.append((ok, b, pcm, fmtd))

for ok, b, pcm, fmtd in rows:
    mark = "ok " if ok else "DIFF"
    print(f"{mark} {b} {pcm}{fmtd}")
print(f"== IDENT {ident}/{len(files)}  DIFF {diff} ==")
