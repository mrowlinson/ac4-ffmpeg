#!/usr/bin/env python3
"""B2 pin gate: staged FATE pins vs merged dec-lib output bytes.

Decodes each locally-available B2 sample with the dec-lib shim and
checks the staged REF/pin hashes + slot properties. Suite-resident
behavior (in-tree framemd5 goldens, wrapper stderr wording, demux
REMUX goldens) is SUITE-GATED and NOT asserted here.

Usage:
  python3 validate.py [--shim PATH] [--pack DIR] [--built DIR]
Prints one line per check, exits 0 iff all pass.
"""
import argparse, hashlib, os, struct, subprocess, sys

def sha(b):
    return hashlib.sha256(b).hexdigest()

def run(shim, inp, outp, tmp):
    e = os.path.join(tmp, "e.tmp")
    with open(e, "w") as ef:
        r = subprocess.run([shim, "decode", "--in", inp, "--out", outp],
                           stderr=ef, timeout=300)
    err = open(e).read() if os.path.exists(e) else ""
    return r.returncode, err

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--shim", default=os.path.join(os.path.dirname(__file__),
                    "..", "dec-lib", "build", "shim"))
    ap.add_argument("--pack", default="/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-samples-pack")
    ap.add_argument("--built", default="/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-mak-samples")
    ap.add_argument("--tmp", default="/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-B2-stage/out")
    a = ap.parse_args()
    os.makedirs(a.tmp, exist_ok=True)
    fails = []
    def check(name, ok, detail=""):
        print(("ok  " if ok else "FAIL") + f" {name}" + (f" ({detail})" if detail else ""))
        if not ok:
            fails.append(name)

    # --- PCM pins: shim output must equal staged REF bytes ---
    pins = [
        ("stereo",    "clip-stereo.ac4",    "clip-stereo-ref.pcm",    "pack",  "5fe8ef4c0748dc507c51ef1fb1d6f90bb65d6a12194c945224c09ac4201bb4ae"),
        ("51",        "clip-51-real.ac4",   "clip-51-real-ref.pcm",   "pack",  "354f6e0cfb6179ec4dd82d3986475fe1bfca34ac5e8d16edfd52ec3235ad12c6"),
        ("crc-bad",   "clip-crc-bad.ac4",   "clip-crc-bad-ref.pcm",   "pack",  "2ee1ade1b1bd4af89a3175225bee8d9aeb7834492a747d062e64b64f35ee0176"),
        ("stereo51",  "clip-stereo51.ac4",  "clip-stereo51-ref.pcm",  "built", "d91a4274aaae8a448568f7484f975a7f47c25cc5aa430ed9ba1867fdb80c2564"),
        ("impulse",   "clip-impulse.ac4",   "clip-impulse-ref.pcm",   "built", "bab2cb61d61283c61826503f95d30f85b8c5246ab63af2abb3acb67250517c68"),
    ]
    outs = {}
    for name, ac4, ref, src, want in pins:
        root = a.pack if src == "pack" else a.built
        outp = os.path.join(a.tmp, f"v-{name}.pcm")
        rc, err = run(a.shim, os.path.join(root, ac4), outp, a.tmp)
        outs[name] = (outp, err)
        ok = rc == 0 and os.path.exists(outp)
        check(f"{name} decodes rc=0", ok, f"rc={rc}")
        if not ok:
            continue
        got = sha(open(outp, "rb").read())
        check(f"{name} pin {want[:8]}", got == want, f"got {got[:8]}")
        refp = os.path.join(root, ref)
        same = (os.path.exists(refp) and
                open(outp, "rb").read() == open(refp, "rb").read())
        check(f"{name} byte-identical staged {ref}", same)

    # --- slot properties ---
    o51, e51 = outs.get("51", ("", ""))
    check("51 presents 6ch", "ch=6" in e51.split("\n")[0], e51.split("\n")[0][:60])
    ocb, ecb = outs.get("crc-bad", ("", ""))
    check("crc-bad survivor 1 frame + crc1 note",
          "frames=1" in ecb and "crc1" in ecb, ecb.split("\n")[0][:80])
    os51, e51b = outs.get("stereo51", ("", ""))
    if os.path.exists(os51):
        d = open(os51, "rb").read()
        n = len(d) // 12
        bad = sum(1 for i in range(n) for c in range(2, 6)
                  if struct.unpack_from("<h", d, i * 12 + c * 2)[0] != 0)
        c0 = sum(1 for i in range(n)
                 if struct.unpack_from("<h", d, i * 12)[0] != 0)
        check("stereo51 c2-c5 exact-zero + c0 content", bad == 0 and c0 > 0,
              f"c2-c5-nonzero={bad} c0nz={c0}")
    oi, ei = outs.get("impulse", ("", ""))
    if os.path.exists(oi):
        check("impulse 5 frames", "frames=5" in ei, ei.split("\n")[0][:60])
    # seek-slice identity: staged slice == stereo-ref 2nd half == crc-bad-ref
    st = os.path.join(a.pack, "clip-stereo-ref.pcm")
    sl = os.path.join(a.built, "clip-stereo-seek-0.05.pcm")
    cb = os.path.join(a.pack, "clip-crc-bad-ref.pcm")
    if all(os.path.exists(p) for p in (st, sl, cb)):
        d = open(st, "rb").read()
        half = d[len(d) // 2:]
        check("seek-slice == stereo-ref 2nd half",
              open(sl, "rb").read() == half)
        check("crc-bad-ref == stereo-ref 2nd half",
              open(cb, "rb").read() == half)

    # --- negative behavior (lib-level; wrapper asserts stay SUITE-GATED) ---
    for name, ac4, src in (("gap-trunc", "clip-gap-fail.ac4", "pack"),
                           ("garbage", "garbage.ac4", "built")):
        root = a.pack if src == "pack" else a.built
        outp = os.path.join(a.tmp, f"v-{name}.pcm")
        if os.path.exists(outp):
            os.remove(outp)
        rc, err = run(a.shim, os.path.join(root, ac4), outp, a.tmp)
        first = err.split("\n")[0] if err else ""
        check(f"{name} lib exit 3 + note=no-sync, no output",
              rc == 3 and "note=no-sync" in first
              and not os.path.exists(outp),
              f"rc={rc} {first[:60]}")

    print(f"== {'PASS' if not fails else 'FAIL: ' + ','.join(fails)} ==")
    return 1 if fails else 0

if __name__ == "__main__":
    sys.exit(main())