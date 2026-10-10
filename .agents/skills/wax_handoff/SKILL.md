---
name: wax_handoff
description: >
  Read and write the project's handoff directory (handoffs/ under .agents/). This is the only
  permitted way to touch handoffs. Use when a session starts (pickup: validate HANDOFF.md, open
  the HEAD entry, restate where work stopped and what is open) and when a session ends
  (handoff: copy the template, fill it, write it under a new UTC timestamp name, advance HEAD
  and status in HANDOFF.md). Triggers: session start, session end, pickup, handoff, "where did
  we stop", "what is open", "write the handoff", "update HANDOFF.md", resume work.
---

# wax_handoff

Owns `handoffs/`. Nothing else reads or writes there (R-04). Two procedures: `pickup` at the
start of a session and `handoff` at the end. Both begin with the same validation. All paths
are relative to `.agents/`. Exact formats are in `references/formats.md`.

## Invariants

- HEAD is the lexically greatest filename in `handoffs/handoffs/` matching
  `^[0-9]{4}(-[0-9]{2}){5}\.md$`, or `none` when there is no such file (R-08).
- `Entries` in `handoffs/HANDOFF.md` equals the number of files matching that regex.
- The template `handoffs/handoffs/yyyy-mm-dd-hh-mm-ss.md` is excluded from the count and is
  never modified (R-07).
- Every index row in `HANDOFF.md` has a file, and every entry file has an index row.
- An entry file and the matching `HANDOFF.md` update are written as one operation (R-05).
- Existing entries are never changed (P-02).

## Validate (shared; run first in both procedures)

1. `handoffs/HANDOFF.md` exists and its `## Head` section has all seven keys: HEAD,
   HEAD timestamp, Entries, Last writer, Resume, Integrity, Status.
2. List files in `handoffs/handoffs/` matching the entry regex and sort them. The last one is
   the newest, or there are none.
3. HEAD equals `handoffs/handoffs/<newest>`, or both are `none`.
4. `Entries` equals the number of listed files.
5. Every stem in `## Index` has a file, and every listed file has an index row.
6. The template file exists and still contains the literal text `<yyyy-mm-dd-hh-mm-ss>`.
7. If HEAD is not `none`, the HEAD file contains headings `## 1.` through `## 10.` in order and
   section 9 contains the four keys `Resume at`, `State`, `First action`, `Blocked on`.

If any check fails: print `HANDOFF INCONSISTENT: check <n>, expected <x>, found <y>`, write
nothing, and stop. Tell the human and wait (R-09).

## Procedure: pickup

1. Run Validate.
2. If `Entries` is 0, report "Fresh project, no previous HEAD." and end. (A fresh copy normally
   has one entry, the shipped genesis entry; it is read like any other.)
3. Read the HEAD file.
4. Print verbatim, in this order: section 9 (Exit point), section 8 (Open threads), and from
   section 1 the Title and Session status.
5. Restate in two sentences to the human: where the work stopped and what the first action is.
6. Write nothing. Pickup is idempotent and may be run again at any time.

## Procedure: handoff

1. Run Validate.
2. Derive the stem with `date -u +%Y-%m-%d-%H-%M-%S`. If a file with that stem exists, or the
   stem is not greater than the current HEAD stem, wait one second and derive again (R-08).
3. Copy `handoffs/handoffs/yyyy-mm-dd-hh-mm-ss.md` to `handoffs/handoffs/<stem>.md`.
4. Fill all ten sections of the new file. Section 2 is the previous HEAD's section 9 copied
   verbatim, or `Fresh project, no previous HEAD.` Section 1 `Previous HEAD` is the old HEAD
   stem or `none`. Section 1 `Session` is this session's host and id, found as
   `references/formats.md` describes, or `unknown`. Section 10 has at least one row; label anything not run as unverified
   (P-07).
5. Self-check the new file: no `<` placeholder remains outside code spans and comments, all
   ten headings are present in order, section 9 has exactly the four keys, section 10 has at
   least one row. On failure delete the new file, report, and stop.
6. Rewrite `handoffs/HANDOFF.md` per `references/formats.md`: Updated line, HEAD, HEAD
   timestamp, Entries plus one, Last writer, Resume (the command that reopens this session,
   derived from the Session key, or `none`), Integrity, Status `closed`, and a new index row
   prepended (Title and Session status from section 1, First action from section 9).
7. Run Validate again. On failure report the mismatch and stop; do not delete the entry, the
   human decides (P-02, R-09).
8. Tell the human the new stem and the First action recorded in section 9.

## Refusals

| Condition | Message | What the human does |
|---|---|---|
| HANDOFF.md missing or missing keys | `HANDOFF INCONSISTENT: check 1 …` | Restore HANDOFF.md from the genesis block in references/formats.md, or from a backup. |
| HEAD is not the newest file | `HANDOFF INCONSISTENT: check 3 …` | Inspect the directory, set HEAD by hand, re-run pickup. |
| Entry count mismatch | `HANDOFF INCONSISTENT: check 4 …` | Count files, correct `Entries` by hand, re-run pickup. |
| Index and files disagree | `HANDOFF INCONSISTENT: check 5 …` | Add or remove the index row by hand, re-run pickup. |
| Template altered or missing | `HANDOFF INCONSISTENT: check 6 …` | Restore the template from the reference design. |
| HEAD entry malformed | `HANDOFF INCONSISTENT: check 7 …` | Fix the entry's headings or section 9 by hand, re-run pickup. |
| Asked to edit a past entry | `Refused: entries are immutable (P-02).` | Ask for a new entry that references the old one. |
| Asked to write handoffs/ outside this skill | `Refused: handoffs/ is accessed only through wax_handoff (R-04).` | Run the handoff procedure instead. |

## Files

- `references/formats.md` — exact field formats for HANDOFF.md, entry files, index rows, and
  the genesis state.
