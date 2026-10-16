#!/usr/bin/env python3
"""Parity gate: lib bare frames vs c-ac4enc oracle raw payloads.
Compares byte-exact frames; records neutral divergences separately.
"""
import csv
import hashlib
import math
import os
import re
import struct
import subprocess
import sys
import wave

# Paths: overridable via env for lanes/scratches; defaults are the
# v1 refactor scratch (vectors live there; binaries + outputs go to S).
S = os.environ.get("ENC_PARITY_S",
                   "/Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-enc-librefactor")
R = os.environ.get(
    "ENC_PARITY_R", "/Users/mrowlinson/Projects/dolby-ac4-cleanroom")
ORACLE = os.environ.get("ENC_PARITY_ORACLE",
                        os.path.join(R, "c-ac4enc"))
HARNESS = os.environ.get("ENC_PARITY_HARNESS",
                         os.path.join(S, "encharness"))
VEC = os.environ.get("ENC_PARITY_VEC",
                     os.path.join(S, "vectors"))
OUT = os.environ.get("ENC_PARITY_OUT",
                     os.path.join(S, "parity-out"))
TIMEOUT = 180
GOLDP = os.environ.get(
    "ENC_PARITY_GOLDENS",
    os.path.join(os.path.dirname(os.path.abspath(__file__)),
                 "..", "parity", "s5fu_goldens.txt"))

# S5FU-STABLE: stereo cases whose bytes are gain-invariant
# (coded silence: no spectrum quantized, so x2.0 vs x3.0
# never materializes). They keep expect="PASS".
S5FU_STABLE = frozenset([
    "st_silence_2049.wav",
    "st_silence_24000.wav",
    "st_silence_240000.wav",
    "m8_silence_2049.wav_8stereo",
])


# F4: read-once cache; the goldens file is fixed for the
# run, so memoize instead of re-reading per S5FU-FIXED case.
_GOLD_CACHE = None


def load_goldens():
    global _GOLD_CACHE
    if _GOLD_CACHE is not None:
        return _GOLD_CACHE
    gold = {}
    try:
        for line in open(GOLDP):
            p = line.split()
            if len(p) == 2 and not p[0].startswith("#"):
                gold[p[0]] = p[1]
    except OSError:
        pass
    _GOLD_CACHE = gold
    return gold


def iframe_expect(nframes, interval, forced):
    mark = [0] * nframes
    if nframes > 0:
        mark[0] = 1
    if nframes > 1:
        mark[1] = 1
    if interval in (None, "1sec"):
        n = 1
        while True:
            j = 1 + (n * 48000) // 2048
            if j >= nframes:
                break
            mark[j] = 1
            n += 1
    else:
        j = 1
        while True:
            j += int(interval)
            if j >= nframes:
                break
            mark[j] = 1
    for f in forced or []:
        if 0 <= f < nframes:
            mark[f] = 1
    return mark


def strip_oracle(path):
    d = open(path, "rb").read()
    pos, frames = 0, []
    while pos < len(d):
        if pos + 6 > len(d):
            raise ValueError("truncated sync frame")
        if d[pos] != 0xAC or d[pos + 1] != 0x41:
            raise ValueError("bad sync at %d" % pos)
        ln = (d[pos + 2] << 8) | d[pos + 3]
        if pos + 4 + ln + 2 > len(d):
            raise ValueError("truncated payload")
        frames.append(d[pos + 4:pos + 4 + ln])
        pos += 4 + ln + 2
    return frames


def read_meta(path):
    meta = {"frames": []}
    for line in open(path):
        p = line.split()
        if p[0] == "measure":
            meta["measure"] = line.strip()
        elif p[0] == "frame":
            meta["frames"].append((int(p[3]), int(p[5])))
        elif p[0] == "frames":
            meta["nframes"] = int(p[1])
    return meta


def hotimp_path():
    """R3 genuine-overflow vector (self-generated, deterministic):
    stereo 8192, bed 440/880 Hz amp 1000 + impulses amp 20000
    at samples 0/4096. Quiet bed + positive correction gain
    bakes the impulses past s16 (dense: f~0.23, so the frozen
    oracle can only trip at its peak gate, same cause class
    as the lib's EBAKE_PEAK; pyref asserts (5.41 dB, 37281))."""
    p = os.path.join(OUT, "r3_hotimp_8192.wav")
    if os.path.exists(p):
        return p
    ns = 8192
    L = [int(round(1000 * math.sin(2 * math.pi * 440 * k /
                                   48000)))
         for k in range(ns)]
    R = [int(round(1000 * math.sin(2 * math.pi * 880 * k /
                                   48000)))
         for k in range(ns)]
    L[0] = R[0] = L[4096] = R[4096] = 20000
    w = wave.open(p, "wb")
    w.setnchannels(2)
    w.setsampwidth(2)
    w.setframerate(48000)
    frames = bytearray()
    for k in range(ns):
        frames += struct.pack("<h", L[k])
        frames += struct.pack("<h", R[k])
    w.writeframes(bytes(frames))
    w.close()
    return p


def meta_dense(fmeta):
    """True iff the lib's pass-1 measure is NOT sparse-flagged
    (dense => the frozen oracle's silent refusal can only be
    its peak gate: same cause class as EBAKE_PEAK)."""
    try:
        for line in open(fmeta):
            if line.startswith("measure m="):
                p = dict(kv.split("=")
                         for kv in line.split()[1:])
                f, tf = float(p["f"]), float(p["tf"])
                return not (tf < 0.6 and f > 0.5)
    except (OSError, ValueError, KeyError):
        pass
    return False


# m8: BOTH-FAIL requires cause equality. Classes stay coarse
# because the frozen oracle refuses silently (exit 1, empty
# stderr, progress on stdout). Unknown on either side fails
# loud (CAUSE-MISMATCH), never a quiet pass.
def _rc_class(rc):
    if rc == -5:  # -EIO: encode refusal
        return "encode-refusal"
    if rc == -22:  # -EINVAL: bad config
        return "config-error"
    if rc == -12:  # -ENOMEM
        return "resource-error"
    return "unknown-lib"


def lib_fail_class(hrc, herr):
    e = herr or ""
    if ("(bake overflow/" in e or
            "(baked peak overflow/" in e or
            "(encode refusal/" in e):
        return "encode-refusal"
    for pat in (r"ac4enc_open failed rc=(-?\d+)",
                r"spectral lines rc=(-?\d+)",
                r"measure failed (-?\d+)"):
        m = re.search(pat, e)
        if m:
            return _rc_class(int(m.group(1)))
    if hrc == 2:
        return "config-error"  # usage/io/pre-encode failure
    return "unknown-lib"


def oracle_fail_class(orc, out, err):
    text = (out or "") + "\n" + (err or "")
    if orc is not None and orc < 0:
        return "crash"
    if "ERROR:" in text or "Summary of errors" in text:
        return "config-error"
    if "Step 2/2" in text:
        return "encode-refusal"  # silent exit-1 refusal
    return "unknown-oracle"


def build_cases():
    files = sorted(os.listdir(VEC))
    cases = []
    for fn in files:
        p = os.path.join(VEC, fn)
        if fn.startswith("st_") and fn.endswith(".wav"):
            cases.append(dict(tag=fn, f=p, fmt="wav", ch=2,
                              layout="stereo", rate=64))
        elif fn.startswith("mc_") and fn.endswith(".wav"):
            cases.append(dict(tag=fn, f=p, fmt="wav", ch=6,
                              layout="5.1", rate=128))
        elif fn.endswith(".s16"):
            ch = 6 if fn.startswith("mc_") else 2
            cases.append(dict(tag=fn, f=p, fmt="pcm_s16le",
                              ch=ch,
                              layout="5.1" if ch == 6 else "stereo",
                              rate=128 if ch == 6 else 64))
        elif fn.endswith(".s24"):
            cases.append(dict(tag=fn, f=p, fmt="pcm_s24le",
                              ch=2, layout="stereo", rate=64))
        elif fn.endswith(".f32"):
            ch = 6 if fn.startswith("mc_") else 2
            cases.append(dict(tag=fn, f=p, fmt="pcm_f32le",
                              ch=ch,
                              layout="5.1" if ch == 6 else "stereo",
                              rate=128 if ch == 6 else 64))
        elif fn.startswith("m8_"):
            cases.append(dict(tag=fn + "_8stereo", f=p,
                              fmt="wav", ch=8, layout="stereo",
                              rate=64, fold="8stereo"))
            cases.append(dict(tag=fn + "_851", f=p, fmt="wav",
                              ch=8, layout="5.1", rate=128,
                              fold="851"))
    # S1 class: sparse-flagged (f=0.537, tf=0.5) short noise.
    # The frozen oracle refuses these (silent exit 1); the lib
    # encodes, byte-identical to the Python reference (PARITY.md).
    for c in cases:
        if c["tag"] in ("st_noise_2049.s16", "st_noise_2049.wav"):
            c["expect"] = "LIB-AHEAD"
    # 6ch -> stereo fold parity on 3 existing 5.1 wavs.
    for fn in ("mc_sine440_24000.wav", "mc_noise_96000.wav",
               "mc_sweep_2049.wav"):
        cases.append(dict(tag=fn + "_51stereo",
                          f=os.path.join(VEC, fn), fmt="wav",
                          ch=6, layout="stereo", rate=64,
                          fold="51stereo"))
    # Option variants (reuse files).
    V = lambda fn: os.path.join(VEC, fn)
    cases += [
        dict(tag="var:t0-st", f=V("st_sweep_24000.wav"),
             fmt="wav", ch=2, layout="stereo", rate=64,
             target="0"),
        dict(tag="var:t0-mc", f=V("mc_noise_24000.wav"),
             fmt="wav", ch=6, layout="5.1", rate=128,
             target="0"),
        dict(tag="var:t0-long", f=V("st_noise_240000.wav"),
             fmt="wav", ch=2, layout="stereo", rate=64,
             target="0"),
        dict(tag="var:t0-noise2049", f=V("st_noise_2049.wav"),
             fmt="wav", ch=2, layout="stereo", rate=64,
             target="0"),
        dict(tag="var:shorts0", f=V("st_clicks_24000.wav"),
             fmt="wav", ch=2, layout="stereo", rate=64,
             shorts="0"),
        dict(tag="var:shorts0-mc", f=V("mc_clicks_24000.wav"),
             fmt="wav", ch=6, layout="5.1", rate=128,
             shorts="0"),
        dict(tag="var:jshort", f=V("st_clicks_24000.wav"),
             fmt="wav", ch=2, layout="stereo", rate=64,
             joint="1"),
        dict(tag="var:jshort-noise",
             f=V("st_noise_24000.wav"), fmt="wav", ch=2,
             layout="stereo", rate=64, joint="1"),
        dict(tag="var:bake+3", f=V("st_sine1k_24000.wav"),
             fmt="wav", ch=2, layout="stereo", rate=64,
             bake="3.0"),
        dict(tag="var:bake-6", f=V("st_dual_24000.wav"),
             fmt="wav", ch=2, layout="stereo", rate=64,
             bake="-6.0"),
        # m8 pin: both sides refuse (baked peak overflow) with
        # equal causes -> BOTH-FAIL, never CAUSE-MISMATCH.
        dict(tag="var:refuse-bake60",
             f=V("st_sine1k_24000.wav"), fmt="wav", ch=2,
             layout="stereo", rate=64, bake="60"),
        dict(tag="var:rate256", f=V("st_sweep_24000.wav"),
             fmt="wav", ch=2, layout="stereo", rate=256),
        dict(tag="var:rate320mc", f=V("mc_sweep_24000.wav"),
             fmt="wav", ch=6, layout="5.1", rate=320),
        dict(tag="var:forced", f=V("st_noise_240000.wav"),
             fmt="wav", ch=2, layout="stereo", rate=64,
             forced=[3, 7, 50]),
        dict(tag="var:forced-mc", f=V("mc_noise_96000.wav"),
             fmt="wav", ch=6, layout="5.1", rate=128,
             forced=[2, 5]),
        dict(tag="var:iv48", f=V("st_sweep_240000.wav"),
             fmt="wav", ch=2, layout="stereo", rate=64,
             interval="48"),
        dict(tag="var:iv11mc", f=V("mc_sweep_96000.wav"),
             fmt="wav", ch=6, layout="5.1", rate=128,
             interval="11"),
        dict(tag="var:preset-ebu", f=V("st_sine1k_24000.wav"),
             fmt="wav", ch=2, layout="stereo", rate=64,
             preset="ebu_r128"),
        dict(tag="var:preset-atsc", f=V("mc_sine440_24000.wav"),
             fmt="wav", ch=6, layout="5.1", rate=128,
             preset="atsc_a85"),
        dict(tag="var:ffloat-st", f=V("st_sweep_24000.wav"),
             fmt="wav", ch=2, layout="stereo", rate=64,
             force_float=True),
        dict(tag="var:ffloat-mc", f=V("mc_clicks_24000.wav"),
             fmt="wav", ch=6, layout="5.1", rate=128,
             force_float=True),
        dict(tag="var:ffloat-edge", f=V("st_sine1k_2049.wav"),
             fmt="wav", ch=2, layout="stereo", rate=64,
             force_float=True),
        dict(tag="var:ffloat-raw",
             f=V("st_sweep_48000.s16"), fmt="pcm_s16le",
             ch=2, layout="stereo", rate=64, force_float=True),
        # R3 pinned genuine peak-gate refusals (policy (a):
        # keep the pyref-mandated refusal, loud). Both sides
        # must refuse; the lib must name EBAKE_PEAK and the
        # vector must be dense (same cause class as the
        # frozen oracle's silent trip).
        dict(tag="r3:hotimp-20x", f=hotimp_path(),
             fmt="wav", ch=2, layout="stereo", rate=128,
             expect="BOTH-FAIL"),
        dict(tag="var:bake+12", f=V("st_sine1k_24000.wav"),
             fmt="wav", ch=2, layout="stereo", rate=64,
             bake="12.0", expect="BOTH-FAIL"),
    ]
    # S5FU: the fixed lib (stereo x2.0 DEE staging) must
    # DIFFER from the frozen x3.0 CLI on stereo spectral
    # content (identical CBR lengths, clean iframes, golden
    # lib bytes). Gain-invariant silence keeps PASS.
    for c in cases:
        if c["tag"] == "var:refuse-bake60":
            continue  # m8 fallback BOTH-FAIL pin, not S5FU
        if c["layout"] == "stereo" and not c.get("neutral") \
                and "expect" not in c:
            c["expect"] = "S5FU-FIXED"
    for c in cases:
        if c["tag"] in S5FU_STABLE:
            c["expect"] = "PASS"
    # Neutral divergence docs (not parity).
    for fn, ch, lay, rate in (
            ("st_sweep_24000.wav", 2, "stereo", 64),
            ("st_sparse_24000.wav", 2, "stereo", 64),
            ("mc_noise_24000.wav", 6, "5.1", 128),
            ("st_silence_24000.wav", 2, "stereo", 64),
            ("st_clicks_24000.wav", 2, "stereo", 64),
            ("mc_sweep_96000.wav", 6, "5.1", 128)):
        cases.append(dict(tag="neutral:" + fn, f=V(fn),
                          fmt="wav", ch=ch, layout=lay,
                          rate=rate, neutral=True,
                          expect="DIVERGE-neutral"))
    return cases


def run_case(i, c):
    tag = "%03d_%s" % (i, c["tag"].replace("/", "_").replace(":", "_"))
    opath = os.path.join(OUT, "o_" + tag + ".ac4")
    mpath = os.path.join(OUT, "o_" + tag + ".json")
    fbin = os.path.join(OUT, "l_" + tag + ".bin")
    fmeta = os.path.join(OUT, "l_" + tag + ".meta")
    fmt = c["fmt"]
    if fmt == "wav":
        fmtstr = "wav"
    else:
        fmtstr = "%s:sample_rate=48000:channels=%d" % (fmt,
                                                       c["ch"])
    ocmd = [ORACLE, "--temp-dir", S, "-i", c["f"],
            "--input-format", fmtstr, "-c", c["layout"],
            "--data-rate", str(c["rate"])]
    if c.get("target") is not None:
        ocmd += ["--target-db", c["target"]]
    if c.get("bake") is not None:
        ocmd += ["--bake-db", c["bake"]]
    if c.get("shorts") is not None:
        ocmd += ["--shorts", c["shorts"]]
    if c.get("joint") is not None:
        ocmd += ["--joint-shorts", c["joint"]]
    if c.get("interval") is not None:
        ocmd += ["--encoder",
                 "iframe_interval=" + c["interval"]]
    if c.get("preset") is not None:
        ocmd += ["--loudness-management",
                 "measure_and_correct:preset=" + c["preset"]]
    if c.get("forced") is not None:
        fcsv = os.path.join(OUT, "f_" + tag + ".csv")
        with open(fcsv, "w") as f:
            for idx in c["forced"]:
                f.write("%d\n" % idx)
        ocmd += ["--forced-iframes", "csvfile:path=" + fcsv]
    ocmd += ["-o", opath, "--output-manifest", mpath,
             "--overwrite", "1"]
    hcmd = [HARNESS, "--in", c["f"], "--fmt", fmt, "--ch",
            str(c["ch"]), "--layout", c["layout"], "--rate",
            str(c["rate"])]
    if c.get("target") is not None:
        hcmd += ["--target-db", c["target"]]
    if c.get("bake") is not None:
        hcmd += ["--bake-db", c["bake"]]
    if c.get("shorts") is not None:
        hcmd += ["--shorts", c["shorts"]]
    if c.get("joint") is not None:
        hcmd += ["--joint-shorts", c["joint"]]
    hcmd += ["--interval", c.get("interval") or "1sec"]
    if c.get("preset") is not None:
        hcmd += ["--preset", c["preset"]]
    if c.get("forced") is not None:
        # CLI CSV holds user indices; core (and harness) take the
        # +1-shifted internal indices (enc_cli.c k+1 quirk).
        hcmd += ["--forced",
                 ",".join(str(x + 1) for x in c["forced"])]
    if c.get("fold"):
        hcmd += ["--fold", c["fold"]]
    if c.get("force_float"):
        hcmd += ["--force-float"]
    if c.get("neutral"):
        hcmd += ["--neutral"]
    hcmd += ["--frames-out", fbin, "--meta-out", fmeta]
    env = dict(os.environ, TMPDIR=S)
    try:
        o = subprocess.run(ocmd, capture_output=True, text=True,
                           timeout=TIMEOUT, cwd=S, env=env)
        orc = o.returncode
    except subprocess.TimeoutExpired:
        return (tag, "TIMEOUT-oracle", "", "")
    try:
        h = subprocess.run(hcmd, capture_output=True, text=True,
                           timeout=TIMEOUT, cwd=S, env=env)
        hrc = h.returncode
    except subprocess.TimeoutExpired:
        return (tag, "TIMEOUT-lib", "", "")
    if c.get("expect") == "LIB-AHEAD":
        # Frozen oracle still refuses (known S1 sparse-stamp
        # bug); the lib must encode. Byte-proof vs the Python
        # reference is recorded in parity/PARITY.md.
        if orc != 0 and hrc == 0:
            try:
                meta = read_meta(fmeta)
                nfr = meta.get("nframes", 0)
            except (OSError, ValueError, IndexError):
                return (tag, "FAIL",
                        "LIB-AHEAD unreadable meta", "")
            if nfr > 0:
                return (tag, "LIB-AHEAD",
                        "o=%d h=%d frames=%d" % (orc, hrc, nfr),
                        meta.get("measure", ""))
        return (tag, "FAIL",
                "LIB-AHEAD unmet o=%d h=%d" % (orc, hrc), "")
    if c.get("expect") == "BOTH-FAIL":
        # Pinned genuine refusal (R3): both sides must refuse,
        # the lib must name the baked-peak gate, and the vector
        # must be dense (rules out the oracle's stamp gate, so
        # its silent trip is the same cause class).
        herr = h.stderr or ""
        if orc != 0 and hrc != 0 and \
                "baked peak overflow" in herr and \
                meta_dense(fmeta):
            return (tag, "BOTH-FAIL",
                    "o=%d h=%d pinned dense" % (orc, hrc), "")
        return (tag, "FAIL",
                "BOTH-FAIL unmet o=%d h=%d dense=%s %s" %
                (orc, hrc, meta_dense(fmeta), herr[:120]), "")
    if orc != 0 and hrc != 0:
        # m8: consistent refusal needs equal causes.
        lc = lib_fail_class(hrc, h.stderr)
        oc = oracle_fail_class(orc, o.stdout, o.stderr)
        if lc == oc and not lc.startswith("unknown"):
            return (tag, "BOTH-FAIL",
                    "o=%d h=%d cause=%s %s" %
                    (orc, hrc, lc, (h.stderr or "")[:100]),
                    "")
        return (tag, "CAUSE-MISMATCH",
                "o=%d h=%d lib=%s oracle=%s %s" %
                (orc, hrc, lc, oc, (h.stderr or "")[:100]),
                "")
    if orc != 0 or hrc != 0:
        return (tag, "RC-FAIL o=%d h=%d" % (orc, hrc),
                (h.stderr or "")[:200], "")
    try:
        oframes = strip_oracle(opath)
    except ValueError as e:
        return (tag, "ORACLE-SPLIT-FAIL", str(e), "")
    lib = open(fbin, "rb").read()
    meta = read_meta(fmeta)
    ob = b"".join(oframes)
    detail = "frames=%d/%d bytes=%d/%d" % (len(meta["frames"]),
                                           len(oframes),
                                           len(lib), len(ob))
    # iframe cross-check (independent python recompute).
    forced_core = ([x + 1 for x in c["forced"]]
                   if c.get("forced") is not None else None)
    exp_if = iframe_expect(len(oframes), c.get("interval"),
                           forced_core)
    got_if = [f[1] for f in meta["frames"]]
    ifmeta = "iframe-ok" if got_if == exp_if else "IFRAME-MISMATCH"
    if c.get("expect", "PASS") == "PASS":
        if lib == ob and ifmeta == "iframe-ok":
            return (tag, "PASS", detail, meta["measure"])
        if lib != ob:
            nd = sum(1 for a, b in zip(lib, ob) if a != b)
            off = next((i for i, (a, b) in
                        enumerate(zip(lib, ob)) if a != b), -1)
            return (tag, "FAIL", "%s ndiff=%d first=%d %s" %
                    (detail, nd, off, ifmeta), "")
        return (tag, "FAIL", detail + " " + ifmeta, "")
    if c.get("expect") == "S5FU-FIXED":
        # Fixed stereo staging: identical CBR lengths, bytes
        # must differ (x2.0 refs vs frozen x3.0), iframes
        # clean, lib bytes match the committed golden.
        if len(lib) != len(ob):
            return (tag, "FAIL",
                    "S5FU-FIXED len %s" % detail, "")
        if lib == ob:
            return (tag, "FAIL",
                    "S5FU-FIXED unmet (identical) %s %s" %
                    (detail, ifmeta), "")
        if ifmeta != "iframe-ok":
            return (tag, "FAIL",
                    "S5FU-FIXED %s %s" % (detail, ifmeta),
                    "")
        nd = sum(1 for a, b in zip(lib, ob) if a != b)
        gold = load_goldens().get(c["tag"])
        lh = hashlib.sha256(lib).hexdigest()
        if gold is None:
            return (tag, "FAIL",
                    "S5FU-FIXED golden-missing %s ndiff=%d" %
                    (detail, nd), "")
        if lh != gold:
            return (tag, "FAIL",
                    "S5FU-FIXED golden-mismatch %s ndiff=%d" %
                    (detail, nd), "")
        return (tag, "S5FU-FIXED",
                "%s ndiff=%d sha=%s" % (detail, nd,
                                        lh[:16]), "")
    # Divergence-doc case: record stats, never FAIL on bytes.
    if len(lib) == len(ob):
        nd = sum(1 for a, b in zip(lib, ob) if a != b)
        nfr = sum(1 for i, f in enumerate(meta["frames"])
                  if lib[sum(x[0] for x in meta["frames"][:i]):
                          sum(x[0] for x in meta["frames"][:i + 1])]
                  != oframes[i]) if len(meta["frames"]) == len(oframes) else -1
        return (tag, c["expect"], "%s ndiff=%d/%d nframes-diff=%s" %
                (detail, nd, len(ob), nfr), "")
    return (tag, c["expect"] + "-LEN", detail, "")


def main():
    os.makedirs(OUT, exist_ok=True)
    cases = build_cases()
    print("cases: %d" % len(cases), flush=True)
    rows = []
    for i, c in enumerate(cases):
        rows.append(run_case(i, c))
        if (i + 1) % 25 == 0:
            print("heartbeat: %d/%d" % (i + 1, len(cases)),
                  flush=True)
    npass = sum(1 for r in rows if r[1] == "PASS")
    nboth = sum(1 for r in rows if r[1] == "BOTH-FAIL")
    nahead = sum(1 for r in rows if r[1] == "LIB-AHEAD")
    ns5fu = sum(1 for r in rows if r[1] == "S5FU-FIXED")
    nfail = sum(1 for r in rows if r[1] not in
                ("PASS", "BOTH-FAIL", "LIB-AHEAD",
                 "S5FU-FIXED",
                 "DIVERGE-neutral",
                 "DIVERGE-neutral-LEN"))
    print("BOTH-FAIL (consistent refusal): %d" % nboth)
    print("CAUSE-MISMATCH (refusal cause differs): %d" %
          sum(1 for r in rows if r[1] == "CAUSE-MISMATCH"))
    print("LIB-AHEAD (oracle S1 refusal, lib encodes): %d" %
          nahead)
    print("S5FU-FIXED (stereo x2.0, golden lib bytes): %d" %
          ns5fu)
    print("PASS %d/%d, non-parity %d" %
          (npass, sum(1 for r in rows
                      if not r[1].startswith("DIVERGE")), nfail))
    for r in rows:
        if r[1] != "PASS":
            print("  %-45s %s %s" % (r[0], r[1], r[2]),
                  flush=True)
    with open(os.path.join(S, "parity_results.csv"), "w",
              newline="") as f:
        w = csv.writer(f)
        w.writerow(["tag", "result", "detail", "measure"])
        w.writerows(rows)


if __name__ == "__main__":
    main()
