# MBP-ORACLE-R1 — licensed-MBP Dolby legs (DRP decode oracle)

Date: 2026-10-18. Lane: ff/doc-mbp-oracle-r1 (base bb78fb0).
Venue rule: Dolby apps ONLY on MBP via ssh; ZERO Dolby exec on Studio.

## (a) Venue proof

- Host: `MacBook-Pro.lan` (= `hostname` output), macOS 27.0.1 (26A434), arm64, python3 3.14.6.
- DRP: `/Applications/Dolby/Dolby Reference Player.app`, CFBundle 4.3 / 4.3.0,
  CLI `drp --help` exit 0 headless, self-reports `4.3.0.19350`.
  Headless decode flags confirmed: `--audio-out-file`, `--print-info`,
  `--out-ch-config`, `--ac4dec-drc-enabled false`, `--ac4dec-limiter-enabled false`,
  `--ac4dec-dap-enabled false`, `--oar-limiter-enable false`.
- DME: `/Applications/Dolby/Dolby Media Encoder/Dolby Media Encoder.app`,
  CFBundle 4.0.0 / 4.0.0.2165, Encoding Engine 6.5.4-dme+b56bc97e.
  CLI prints `--help` headless BUT process did not exit (SIGTERM after 8s).
  No DME encode run attempted (none queued; see UNRUN).
- Remote work dir: `/tmp/mbp-oracle-r1/` (created fresh, DELETED at end).
  NOTE: `~/mnt-nvme-tb` EXISTS on MBP (brief said NVME not mounted) —
  lane still used scp-only per orders; discrepancy flagged, not exploited.

## Staged inputs (scp Studio -> MBP, sha16)

From `/Volumes/NVME/temp-ac4/dolby-ac4-remote` (@ stage dir):

| file | sha16 |
|---|---|
| tests/test_b81_c10.py | 379f07726a9b2c5d |
| tests/drp_decode.sh | 7fc27ab0225a4d15 |
| tools/compand_census.py | 257bf6067b0f8635 |
| tools/ts_extract.py | f7e3e88a15db26c7 |
| ac4dec (Oct-06, arm64, libSystem-only) | 3cfc89f7e23b3cf8 |
| vectors-staged-G8.ac4 (162939 B) | 6cfb1758573c2ac4 |
| vectors-staged-KDAF.ts (16211052 B) | (pulled as-is) |
| c-ac4dec (Oct-16, added run2) | — |

DRP backend on MBP: `DRP_BIN=/Applications/.../drp` (no standalone
oracle in tree; direct Mach-O exec over ssh, no GUI activation observed).

## (b) b81 live-drp legs ON MBP

Run1 (staged ac4dec): PASS=14 FAIL=2 SKIP=0 — live legs EXECUTED + green,
census legs FAIL rc=1.

```
PASS canned-fire/frames, canned-fire/status, canned-fire/hist, canned-off/hist
PASS canned-info/{Object coded : False, Immersive stereo, 0xAC41}
FAIL local-g8-rc0 rc=1 / FAIL local-kdaf-rc0 rc=1
PASS live-g8-20-rc0, live-g8-20-size, live-g8-20-sha16
PASS live-g8-info-rc0, live-g8-info/{Object coded : False, Immersive stereo, 0xAC41}
```

Root cause (first-hand): staged `ac4dec` rejects `--wav`
(`ac4dec: unknown flag: --wav`) — stale binary vs compand_census argv.
Fix: shipped `c-ac4dec` (strings-proved `--wav`, no Studio exec of it
needed for the proof), reran with `AC4DEC_BIN=.../c-ac4dec`.

Run2: **PASS=24 FAIL=0 SKIP=0** — full green. DRP G8 @2.0 pins
RECONFIRMED live: 983132 B RF64, sha16 `e355495e4ff0426b`
(full: `e355495e4ff0426bd3b2de2aacd432f9abec0038c0d68cccf6fbe15aef13e3b0`).

## (c) Drift-audit: DRP oracle vs FF dec-lib/shim (local pure-data diff)

DRP decodes on MBP (harness truth flags, exit 0 both; harness logs
empty = quiet-on-success, exit codes captured over ssh):

| vec | route | wav bytes | sha32 |
|---|---|---|---|
| G8-ims51 | direct ES | 983132 | e355495e4ff0426bd3b2de2aacd432f9 |
| KDAF.ts | ES pid 0x32 (prog 3, AC-4 eng) | 5778664 | 3b840fba46328c0c0d78e016cba9931e |

G8 DRP wav deterministic x3 (2x b81 + 1x fresh, all 983132 B).

FF side (Studio, OUR code only): dec-lib/shim built out-of-tree from
this wt; KDAF ES extracted locally via ts_extract.py
(900 frames, gaps=0 -> 724107 B stream.ac4).

| vec | DRP wav (2.0, s16) | shim pcm (6ch s16) | verdict |
|---|---|---|---|
| G8 | data 983040 B = 120x2048 stereo, sha16 27266628b7175d07, rms 464.4 | 2899968 B = 118x2048x6ch, sha16 e01aa9f69ee1cc05, rms 270.8 | DIVERGE (expected) |
| KDAF | data 5778572 B = 1444643 spl/ch, sha16 a815cf4266d897cf, rms 701.5 | 17297280 B = 1441440 spl/ch (900f), sha16 c6f8a40679d4583b, rms 306.4 | DIVERGE (expected) |

Sample-level (DRP L/R vs shim ch0/1, common length):
G8 maxabs 1195/1602; KDAF maxabs 9679/9765 (both peak @spl 748010). No match.

Expected-divergence reasons (same as b81 docstring pins):
ch 6 vs 2.0, companding unapplied in C, DRP hotter (rms 1.7-2.3x).
NEW shape observation: DRP renders +2 frames on BOTH vectors —
G8 120 vs 118; KDAF 1444643-1441440=3203 spl/ch = 2x1601.6 (29.97fps).
Cause unexamined; shape pinned, not explained.

## UNRUN + why

- DME encode legs: none queued; DME CLI help needed SIGTERM (non-exiting),
  so no encode attempted — would need a watchdog + owner ack first.
- G8 mp4 variant (`G8-ims51-5s-multi-mp4.mp4`): not queued; ES leg covers pins.
- DRP `--metadata-directory`/5.1/7.1 out-ch: out of scope for C9 battery.
- b81 with staged (Oct-06) ac4dec green: impossible, binary lacks --wav;
  superseded by c-ac4dec run2 (24/0/0).

## Compliance

- Zero Dolby execution on Studio ( Venue rule). Local execs: OUR shim,
  OUR ts_extract.py, OUR compare script — pure-data diff only.
- `/tmp/mbp-oracle-r1` on MBP removed at end (verify `ls` = absent).
- Heavy bytes: NVME scratch `ff-mbp-oracle-r1/` only (stage/, pulled/,
  shimbuild/). Repo commit = this doc + HO only.
