#!/usr/bin/env python3
"""N5 pin gate: staged rate-semantics pins vs fixed dec-lib output.

Decodes the pinned vectors with the dec-lib shim and checks FMT rate
labels, Table-70 output counts (via byte size + PCM hash), the WAV
header rate, and the sf_mult contrast pair. Suite-resident behavior
(in-tree framemd5 goldens, wrapper asserts) is SUITE-GATED and NOT
asserted here.

Usage:
  python3 validate.py [--shim PATH] [--corpus DIR] [--synth DIR] [--tmp DIR]
Prints one line per check, exits 0 iff all pass.
"""
import argparse
import hashlib
import os
import struct
import subprocess
import sys

GPAC_SHA = "b82fabbad563c011538c78f6802319c3d517011bb75c4a9e36556f3d67270ce1"
GPAC_PCM_SHA = "8b41f7cf1239a1a4a256a5e6a5ae7a935763d1d0b251e998151400126c6b39f6"
GPAC_PCM_BYTES = 5727312  # 477276 samples x 6ch x 2B (298 x 1601.6)
SF4_SHA = "b3fdc1269e4c88b564cf3cfe812b1325ac2b70d33683747ebea1c9086bb7242a"
FS1_SHA = "c105d9bc9c4063a0958d5f1a7dab694e06e7ea1097524469957d33844bc80410"


def sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for blk in iter(lambda: f.read(1 << 20), b""):
            h.update(blk)
    return h.hexdigest()


def run(shim, args, tmp):
    e = os.path.join(tmp, "e.tmp")
    with open(e, "w") as ef:
        r = subprocess.run([shim] + args, stderr=ef, timeout=300)
    err = open(e).read() if os.path.exists(e) else ""
    return r.returncode, err


def wav_sr(path):
    d = open(path, "rb").read()
    i = d.find(b"fmt ")
    if i < 0:
        return None
    return struct.unpack("<I", d[i + 12:i + 16])[0]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--shim", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "dec-lib",
        "build", "shim"))
    ap.add_argument("--corpus", default="/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-corpus/streams")
    ap.add_argument("--synth", default="/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-a0-r2/synth")
    ap.add_argument("--tmp", default="/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-N5-fix-r2/pins-out")
    a = ap.parse_args()
    os.makedirs(a.tmp, exist_ok=True)
    fails = []

    def check(name, ok, detail=""):
        print(("ok  " if ok else "FAIL") + f" {name}" + (f" ({detail})" if detail else ""))
        if not ok:
            fails.append(name)

    gpac = os.path.join(a.corpus, "gpac-atsc3_test_audio.ac4")
    sf4 = os.path.join(a.synth, "g-fri2-sf4.ac4")
    fs1 = os.path.join(a.synth, "g-fri2-fs1.ac4")

    check("gpac sample sha", sha(gpac) == GPAC_SHA)
    check("g-fri2-sf4 sample sha", sha(sf4) == SF4_SHA)
    check("g-fri2-fs1 sample sha", sha(fs1) == FS1_SHA)

    # --- gpac: 48000 Hz, 298 x 1601/1602, pcm hash ---
    outp = os.path.join(a.tmp, "v-gpac.pcm")
    if os.path.exists(outp):
        os.remove(outp)
    rc, err = run(a.shim, ["decode", "--in", gpac, "--out", outp], a.tmp)
    line = err.split("\n")[0] if err else ""
    check("gpac FMT sr=48000", "sr=48000" in line, line[:90])
    check("gpac FMT frames=298 note=pcm",
          "frames=298" in line and "note=pcm" in line, line[:90])
    ok = rc == 0 and os.path.exists(outp)
    check("gpac decodes rc=0", ok, f"rc={rc}")
    if ok:
        n = os.path.getsize(outp)
        check("gpac 5727312 bytes", n == GPAC_PCM_BYTES, f"got {n}")
        got = sha(outp)
        check("gpac pcm pin 8b41f7cf", got == GPAC_PCM_SHA,
              f"got {got[:8]}")

    # --- gpac WAV header rate ---
    outw = os.path.join(a.tmp, "v-gpac.wav")
    if os.path.exists(outw):
        os.remove(outw)
    rc, _ = run(a.shim, ["decode", "--in", gpac, "--out", outw, "--wav"], a.tmp)
    ok = rc == 0 and os.path.exists(outw)
    check("gpac wav rc=0", ok, f"rc={rc}")
    if ok:
        check("gpac wav-sr 48000", wav_sr(outw) == 48000,
              f"got {wav_sr(outw)}")

    # --- sf_mult contrast pair (fail path carries the label) ---
    for name, vec, sr in (("g-fri2-sf4", sf4, "192000"),
                          ("g-fri2-fs1", fs1, "48000")):
        outx = os.path.join(a.tmp, f"v-{name}.pcm")
        if os.path.exists(outx):
            os.remove(outx)
        rc, err = run(a.shim, ["decode", "--in", vec, "--out", outx], a.tmp)
        line = err.split("\n")[0] if err else ""
        check(f"{name} FMT sr={sr}", f"sr={sr}" in line, line[:90])
        check(f"{name} exit 3 no-output note=lead-trunc",
              rc == 3 and not os.path.exists(outx) and "lead-trunc" in line,
              f"rc={rc}")

    print(f"== {'PASS' if not fails else 'FAIL'} "
          f"{14 - len(fails)}/14 ==")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
