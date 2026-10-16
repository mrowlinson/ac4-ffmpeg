# A-SENDSTEPS-NITFIX -- VERIFY-A-R3 mailsplit doc wart fix

Source: VERIFY-A-R3 T+7 (`ff-A-post-r3-verify/VERIFY-A-R3.md`):
send-steps `git mailsplit -o send/ ...` fails verbatim on this git
(needs `-odir` + mkdir).

Fix (this branch `ff/doc-A-sendsteps-fix`, from master `2436f47`):
`doc/post/A-OWNER-SEND-STEPS.md:30`:
`git mailsplit -o send/ doc/post/series-A-decoder.mbox`
-> `mkdir -p send && git mailsplit -osend doc/post/series-A-decoder.mbox`.
One line, no other content touched.

Evidence (git 2.56.0, scratch copy of master's
`doc/post/series-A-decoder.mbox`, 606385 B):
old verbatim `git mailsplit -o send/ ...` -> `fatal: unknown option: -o`,
rc=128, no output. `git mailsplit -osend ...` without dir ->
`fatal: unable to create 'send/0001'`, rc=128. New verbatim
`mkdir -p send && git mailsplit -osend ...` -> rc=0, `send/0001`
(572260 B, `[PATCH 1/2] avcodec: add native AC-4 decoder core`) +
`send/0002` (34125 B, `[PATCH 2/2] avcodec: wire ff_ac4_decoder`).
Both halves of the fix required; verified verbatim.

Sweep: same `-o send/` wart also present (NOT fixed here, out of
scope) in `doc/post/B-OWNER-SEND-STEPS.md:28` and
`doc/post/E-OWNER-SEND-STEPS.md:34`. Runbook mentions are prose
shape refs, no flag text, correct as-is.

Re-verify at re-cut per VERIFY-A-R3.
