# B2-POST-READY — B2/post-gate readiness on master fc5573a

Seat `ff-B2-post-ready`. Base master `fc5573a` (own clone
`scratch/ff-B2-post-ready/wt`, branch `ff/doc-B2-post-ready` holds only
this file). Parent repo untouched, nothing pushed, nothing sent. All builds
out-of-tree (`build/ff-B2-post-ready`), TMPDIR in scratch. Venue:
MBP-TB `169.254.103.231` unreachable (ssh timeout) → LOCAL-ONLY
pure-FFmpeg; zero Dolby-binary invocation; Dolby-dependent checks UNRUN
(none in scope below).

## Gate table

| # | Gate | Verdict |
|---|---|---|
| a | Suite +24h clock ≥ 2026-10-17T22:23:09Z | **READY** (expired first-hand) |
| b | Live suite mirror 12/12 | **READY** (live rsync + 12/12 OK) |
| c1 | A0 trio 3/3 on B-tip | **READY** (rc=0) |
| c2 | B2 mak pins (fate-ac4 16/16 x2) on B-tip | **READY** (rc=0, deterministic) |
| c3 | Full-stack am 23/23 | **READY** (rc=0, first-hand) |
| d | Hygiene: markers/pushes/bytes | **READY** (0/0/explained) |
| — | TB send link 169.254.103.231 | OWNER-AT-POST (absent here, owner link) |
| — | Full-assembly fate 29/29 | CARRIED (R2, byte-identical mboxes) |

## a. Clock — READY

`date -u` first-hand `2026-10-18T00:4x–00:55Z` (3 reads, HB 1/4/5),
all ≥ gate `2026-10-17T22:23:09Z`. Expired ~2.5h at first read. No park.

## b. Suite — READY (live 12/12)

- Live `rsync rsync://fate-suite.ffmpeg.org/fate-suite/ac4/` (exact
  B-PREFLIGHT recipe): rc=0 to `suite-live/ac4/` (16 files live).
- Oracle `EXPECTED.sha256` (12 pins): two independent seat copies
  (`ff-assembly-debts/suite-live`, `ff-B-714-recut/tmp`) `cmp`-identical.
- `sha256sum -c`: **12/12 OK**, zero non-OK (`suite-12.log`).

## c. Assembly on fc5573a bytes — READY

- Base: pristine `3e6cd5e` (verified log + clean; local clone `ffasm`).
- B-tip am A0+A+B `--whitespace=error`: **5/5 rc=0** (`am-Btip.log`).
- Build (`--samples=suite-live --disable-doc`, out-of-tree): configure
  rc=0, `make -j18` rc=0, **34 warnings / 0 ac4**; decoder row
  `A..X.D ac4 ATSC A/344 Dolby AC-4`, no encoder row.
- `make fate-ac4`: **rc=0, 16 TEST**, zero fail/error; rerun rc=0 16 TEST
  (deterministic). Enc-neg correctly absent pre-C.
- A0 trio `fate-ac4-demux fate-ac4-demux-51 fate-seek-extra-ac4`:
  **rc=0, 3/3** (`fate-A0.log`).
- Full-stack am A0→A→B→C→E→D: **23/23 rc=0** (`am-full.log`, `fffull`).
- Full-assembly fate 29/29 NOT re-run: carried from ASSEMBLY-DEBTS-R2,
  valid — mboxes byte-identical `20bb917..fc5573a` (verified EMPTY diff).

## d. Hygiene — READY

- SENT markers: `^SENT:` 0; `Message-ID.*@` 1 hit = self-referential
  prose in A0-PREFLIGHT L57 only. Zero `*sent*` files. SEND-SITTING
  Owner-at-post: all 10 boxes unchecked.
- Nothing pushed: branch `ff/doc-B2-post-ready` local-only; origin is
  local path `wt-merge`; wt-merge holds no such branch; no push ran.
- Mbox bytes (fc5573a) vs SEND-SITTING (2def950):

| Mbox | SEND-SITTING | fc5573a | State |
|---|---|---|---|
| A | 507373 | 507373 | match |
| A0 | 43263 | 43263 | match |
| B | 40708 | 40729 | R2 debt 5 (enc-neg `-strict -2` x2) |
| C | 650974 | 650794 | R2 debt 1 (context refresh, +26/−35) |
| E | 12693 | 12774 | R2 debt 2 (GEN strict + msg) |
| D | 162643 | 163076 | R2 debts 3+4+6 (−toc block, T3 strict, 7 re-pins) |

  Deltas owned by `20bb917` (MUST-final fixed mboxes); new sizes +
  numstats (+26/−35 C, +386/−718 D, D `ac4_toc.h` refs 2, D bare
  `-c:a ac4` 0) verified first-hand EXACT vs ASSEMBLY-DEBTS-R2 table.
- SEND-SITTING step-1 re-checks: non-ASCII 0 (mboxes + covers);
  From counts 2/1/2/10/2/6 exact; COVER-B L33 `incl. fate-source` present.

## Notes for parent

- Master moved during run: `fc5573a → 8c9ba74` (C/E/D reverify HOLD-lift
  merges, doc-only). `fc5573a..8c9ba74` mbox diff EMPTY → all byte-gates
  above carry to 8c9ba74 unchanged.
- TB-link + send steps remain owner-at-post per SEND-SITTING.

## Logs

`HEARTBEAT.md` HB 1–15. `suite-12.log`, `am-Btip.log`, `am-full.log`,
`configure-Btip.log`, `make-Btip.log`, `fate-Btip.log`,
`fate-Btip-rerun.log`, `fate-A0.log`. Trees: `ffasm` (B-tip), `fffull`
(23-stack). Build: `build/ff-B2-post-ready`.
