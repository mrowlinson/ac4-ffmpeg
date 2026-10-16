#!/usr/bin/env python3
"""Generate parity vectors (s16 wav / raw s16/s24/f32, 48k). Pure stdlib."""
import math
import os
import struct
import sys
import wave

SR = 48000


def lcg(n):
    x = 123456789
    out = []
    for _ in range(n):
        x = (1103515245 * x + 12345) & 0x7FFFFFFF
        out.append(x / 1073741823.5 - 1.0)
    return out


NOISE = lcg(240000)


def content(name, n, ch=0):
    ph = ch * 0.7
    if name == "silence":
        return [0.0] * n
    if name == "sine440":
        return [0.5 * math.sin(2 * math.pi * 440 * i / SR + ph)
                for i in range(n)]
    if name == "sine1k":
        return [0.5 * math.sin(2 * math.pi * 1000 * i / SR + ph)
                for i in range(n)]
    if name == "sine10k":
        return [0.3 * math.sin(2 * math.pi * 10000 * i / SR + ph)
                for i in range(n)]
    if name == "sweep":
        out = []
        for i in range(n):
            f = 100.0 * (100.0 ** (i / max(n - 1, 1)))
            out.append(0.5 * math.sin(2 * math.pi * f * i / SR + ph))
        return out
    if name == "dual":
        return [0.3 * math.sin(2 * math.pi * 440 * i / SR + ph) +
                0.2 * math.sin(2 * math.pi * 1000 * i / SR)
                for i in range(n)]
    if name == "clicks":
        out = [0.1 * math.sin(2 * math.pi * 220 * i / SR)
               for i in range(n)]
        for i in range(0, n, 4800):
            out[i] = 0.9 if (i // 4800) % 2 == 0 else -0.9
        return out
    if name == "noise":
        return [0.3 * NOISE[(i + ch * 7919) % len(NOISE)]
                for i in range(n)]
    if name == "speechish":
        return [0.4 * (0.5 + 0.5 * math.sin(2 * math.pi * 4 * i / SR)) *
                (0.5 * math.sin(2 * math.pi * 110 * i / SR + ph) +
                 0.3 * math.sin(2 * math.pi * 220 * i / SR) +
                 0.2 * math.sin(2 * math.pi * 330 * i / SR))
                for i in range(n)]
    if name == "dc":
        return [0.1 + 0.4 * math.sin(2 * math.pi * 500 * i / SR + ph)
                for i in range(n)]
    if name == "fullscale":
        return [0.99 * math.sin(2 * math.pi * 700 * i / SR + ph)
                for i in range(n)]
    if name == "sparse":
        out = [0.0] * n
        for s in range(0, n, 24000):
            for i in range(min(200, n - s)):
                out[s + i] = 0.9 * math.sin(2 * math.pi * 2000 * i / SR)
        return out
    if name == "alt":
        return [(0.5 if i % 2 == 0 else -0.5) for i in range(n)]
    if name == "ramp":
        return [((i + ch * 131) % 480) / 480.0 - 0.5 for i in range(n)]
    if name == "frac":
        return [0.123456 * math.sin(2 * math.pi * 333 * i / SR + ph) +
                0.037777 * math.sin(2 * math.pi * 1234 * i / SR)
                for i in range(n)]
    if name == "clip":
        return [1.5 * math.sin(2 * math.pi * 250 * i / SR + ph)
                for i in range(n)]
    raise ValueError(name)


def to_s16(x):
    v = int(math.floor(x * 32767.0 + 0.5))
    return max(-32768, min(32767, v))


def write_wav(path, rows):
    nch = len(rows)
    ns = len(rows[0])
    with wave.open(path, "wb") as w:
        w.setnchannels(nch)
        w.setsampwidth(2)
        w.setframerate(SR)
        frames = bytearray()
        for i in range(ns):
            for c in range(nch):
                frames += struct.pack("<h", to_s16(rows[c][i]))
        w.writeframes(bytes(frames))


def write_raw_s16(path, rows):
    with open(path, "wb") as f:
        nch = len(rows)
        ns = len(rows[0])
        for i in range(ns):
            for c in range(nch):
                f.write(struct.pack("<h", to_s16(rows[c][i])))


def write_raw_s24(path, rows):
    with open(path, "wb") as f:
        nch = len(rows)
        ns = len(rows[0])
        for i in range(ns):
            for c in range(nch):
                v = int(math.floor(rows[c][i] * 8388607.0 + 0.5))
                v = max(-8388608, min(8388607, v))
                if v < 0:
                    v += 0x1000000
                f.write(bytes((v & 0xFF, (v >> 8) & 0xFF,
                               (v >> 16) & 0xFF)))


def write_raw_f32(path, rows):
    with open(path, "wb") as f:
        nch = len(rows)
        ns = len(rows[0])
        for i in range(ns):
            for c in range(nch):
                f.write(struct.pack("<f", rows[c][i]))


def stereo(name, n):
    return [content(name, n, 0), content(name, n, 1)]


def multi(names, n, nch):
    if isinstance(names, str):
        names = [names]
    out = []
    for c in range(nch):
        out.append(content(names[c % len(names)], n, c))
    return out


def main():
    d = sys.argv[1] if len(sys.argv) > 1 else "vectors"
    os.makedirs(d, exist_ok=True)
    files = []

    def wav(name, rows):
        p = os.path.join(d, name + ".wav")
        write_wav(p, rows)
        files.append(p)

    def raw(name, rows, kind):
        p = os.path.join(d, name + "." + kind)
        {"s16": write_raw_s16, "s24": write_raw_s24,
         "f32": write_raw_f32}[kind](p, rows)
        files.append(p)

    st10 = ["silence", "sine440", "sine1k", "sine10k", "sweep",
            "dual", "clicks", "noise", "speechish", "sparse"]
    for nm in st10:
        for ln in (2049, 24000, 240000):
            wav("st_%s_%d" % (nm, ln), stereo(nm, ln))
    for ln in (1, 100, 1000, 2047, 2048, 4096, 48000, 96000):
        wav("st_sine1k_%d" % ln, stereo("sine1k", ln))
    for nm in ("dc", "fullscale", "alt", "ramp"):
        wav("st_%s_24000" % nm, stereo(nm, 24000))
    mc6 = ["silence", "sine440", "sweep", "clicks", "noise",
           "sparse"]
    for nm in mc6:
        for ln in (2049, 24000, 96000):
            wav("mc_%s_%d" % (nm, ln),
                multi(nm, ln, 6))
    raw("st_sine1k_24000", stereo("sine1k", 24000), "s16")
    raw("st_noise_2049", stereo("noise", 2049), "s16")
    raw("st_sweep_48000", stereo("sweep", 48000), "s16")
    raw("st_sine1k_24000", stereo("sine1k", 24000), "s24")
    raw("st_clicks_24000", stereo("clicks", 24000), "s24")
    raw("st_frac_24000", stereo("frac", 24000), "f32")
    raw("st_frac_2049", stereo("frac", 2049), "f32")
    raw("st_clip_48000", stereo("clip", 48000), "f32")
    raw("mc_sweep_24000", multi("sweep", 24000, 6), "f32")
    wav("m8_mix_24000",
        multi(["sine440", "sine1k", "sweep", "noise"], 24000, 8))
    wav("m8_sweep_24000", multi("sweep", 24000, 8))
    wav("m8_silence_2049", multi("silence", 2049, 8))
    wav("m8_noise_96000", multi("noise", 96000, 8))
    print("wrote %d files to %s" % (len(files), d))


if __name__ == "__main__":
    main()
