# C-R2-VERIFY — independent re-verify of Series-C r5 re-cut

Seat `ff-C-r2-verify`, 2026-10-16. Mbox/cover exported from master
`2018c89`; during the run master moved to `ef93aa9` (B-post-final
merge, B files only — all 4 C files byte-identical across the move,
verified by diff). This branch is from `ef93aa9`; verdict covers both.

Method: re-ran every r5 gate from scratch on the TB peer
(MacBook-Pro.lan, 169.254.103.231, peer-local /tmp/ffcverify) +
local cross-checks. Scratch: `scratch/ff-C-r2-verify`
(mbox/cover `patches/`, NVME trees `ffsrc`/`ffsrcAC`, all peer logs
`logs/`). Out-of-tree builds, TMPDIR in scratch/peer-tmp. Nothing
pushed; merges parent-only.

## Gates (all green)

- `git am` pristine `3e6cd5e`: rc=0, 11/11, twice — local
  (`d56a179..b603e07`) and peer-local (tree-identical:
  `1a39b40a59aaacbbb6566d5fa15aef9fdae3033a` both).
- Files/patch: 7/28/7/2/4/1/1/1/2/10/1 (matches preflight).
- `git show --check`: zero on all 11. Added-line whitespace scan: 0.
- Applied tree == r5 tip `fd578ca` minus exactly A0's 6 files
  (735 deletions, 0 additions) — zero A0/C overlap, standalone proven.
- A+C assembly (peer-local): A 2/2 plain; C1/C2 `-3` auto; C3 hand
  (allcodecs dec+enc adjacency, byte-identical to combo-6 `e34ba44`
  region); C4–C11 plain. Zero markers (sole grep hit is upstream
  `tools/murge` by design). Single ac4.mak include.
- Build (peer-local out-of-tree, `--disable-doc --disable-debug`
  + `--disable-autodetect`, `make -j10`): rc=0, 5 warnings all
  upstream (nspdec/adpcm), **0 ac4**, **ZERO duplicate symbols**,
  0 errors. (`--disable-autodetect` is this seat's deviation from
  the preflight recipe — deterministic peer build; immaterial to
  link/fate gates. Warning count differs from preflight's 34 for
  the same reason + different SDK.)
- `nm` defined-globals intersect: `ac4_huffman_tables.o` (110) ∩
  `huffman_tab.o` (110) = EMPTY. L1 closed.
- Encoder fate (SAMPLES unset): 5/5 rc=0, 0 FAIL —
  `ac4-encode` REF `86828e84ccecb4171d775806616e0e93`,
  `ac4-encode-51` REF `53a44acaa21853f91cfef7dbdcccb67e`
  (exact combo-6 hashes), framecrc/tail/fold8 PASS.
  `#extradata` refs: stereo `0xb7a30a0b` x2, fold8 `0xbafa0ae9`.
- Neg gate `tests/ac4-neg-check.sh`: 2/2 PASS (exit 234 + message).
- Codec registers: `DEA.L. ac4`.
- r5-vs-r4 == combo-6 `e34ba44` delta EXACTLY (406/406 diff lines
  identical, 2 files 85/85). Numerics extracted from both files
  identical (table bytes); 17 string book names kept; 34 `AC4_`
  symbols per file, zero bare uses outside strings.
- 02/11..11/11 byte-identical to gated r4 `ff-submit-fix/C/0002..0011`
  (modulo From-SHA). 01/11 delta = r5 message note + 2 blob indices
  + `AC4_` renames only (diff after `AC4_`-strip shows nothing else).
- Subjects 1:1 cover↔mbox x11 (wraps identical). Signoffs 11/11
  owner. Authors 11/11 owner. Dates non-decreasing. ASCII-only
  (mbox + cover, 0 non-ASCII). Cover: 0 placeholders. Mbox sole
  XXX = `mktemp ac4neg.XXXXXX` (legitimate). Goldens in mbox 05/11
  refs (short+full x2) and cover (4 + extradata x2).

## Environment notes (no gate impact)

- Peer git over the NVME SMB mount cannot run `apply --index`
  (`does not match index` on correct bytes; worktree ops + object
  reads all correct). Peer `core.autocrlf=true` suspected then
  ruled out (fails with `false` too). Peer-local disk am passes
  rc=0 with tree-identical result. All peer gates therefore ran
  peer-local; logs copied to `scratch/ff-C-r2-verify/logs/`.
- Peer build tree kept at `/tmp/ffcverify` (MBP local); NVME trees
  `ffsrc` (C-only applied) + `ffsrcAC` (pristine) kept as evidence.

## Verdict

**POST-OK** — Series-C r5 re-cut independently re-verified; no gaps.
