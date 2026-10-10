---
name: wax_init
description: >
  Install or upgrade the WAX Agentic Workspace (.agents/) in a project. With no .agents present it
  copies the reference in, fills the project name, and verifies. With one present it explores the
  folder, reports what is there, and lets the human choose: merge (keep handoff history,
  preferences, and project skills; move foreign files into project/) or discard (set the old folder
  aside, install fresh). Nothing is ever deleted. Also checks an existing .agents for structural
  problems. Use whenever someone wants to "add .agents", "install wax", "set up the agent
  workspace", "upgrade .agents", "migrate .agents", or asks whether a project's .agents is set up
  correctly, even if they do not name this skill. Triggers: init, setup, bootstrap, install,
  upgrade, migrate, merge, new project, check .agents, verify setup.
---

# wax_init

Installs, upgrades, and checks `.agents/`. Two procedures: `init` and `check`. The deterministic
work lives in `scripts/`: `setup.sh` makes a fresh copy, `migrate.sh` explores an existing folder
and merges or sets it aside, `check.sh` verifies. What each version changed, and how a merge
handles it, is in `references/migrations.md`. All paths below are relative to `.agents/` unless
stated.

## Invariants

- Nothing is deleted. An existing `.agents/` is moved to `<project>/.agents.old-<utc-stamp>`,
  by merge and by discard alike. The human removes it later, or never.
- The human chooses merge or discard. The agent explains both and never picks.
- Handoff entries are carried over unchanged (P-02). `migrate.sh` touches
  `handoffs/HANDOFF.md` only to add head keys the current format requires, and never writes an
  entry (R-04 exception).
- Preferences are the project's. A merge keeps the old `PREFERENCES.md` and appends, under a
  marked heading, only the IDs the new version adds. The human reviews them (R-15).
- Project skills, any folder under `skills/` (or the old `skill/skillset/`) that is not a
  reference skill, are carried over.
- Foreign content, anything that is not part of WAX, moves into `project/` inside the new
  `.agents/`, and `P-11` lists `project/` so R-13 and `check.sh` tolerate it. Nothing else may
  be added at the top level.
- A merged tree is verified with `check.sh` before it replaces the old one. On failure the old
  folder is untouched.
- Setup never overwrites: with `.agents/` present, `setup.sh` refuses and `init` goes through
  `migrate.sh`. `check` is read-only; it reports and never repairs (R-09).

## Procedure: init

1. Determine the target project root. If the human gave a path, use it; otherwise the
   repository you are working in, not a subfolder. Say which one before running anything.
2. Run `scripts/migrate.sh <target> --explore`. It is read-only and prints a report:
   `kind` (`missing`, `wax (<version>)`, `same`, `newer`, `foreign`), project skills and
   layout, entry count and missing `HANDOFF.md` keys, preference IDs an upgrade would append,
   how far `AGENTS.md` differs from the installer's, foreign items, a root `CLAUDE.md` or
   `AGENTS.md`, the git state of `.agents`, the `check.sh` result, and one line each for what
   merge and discard would do.
3. `kind: missing`: run `scripts/setup.sh <target> --name <name>` (the human's wording for the
   project, else the folder's basename). Continue at step 7.
4. `kind: same`: say the project already has this version and run `check`. Stop.
5. `kind: newer`: say the project's copy is newer than this installer and the installer must be
   updated first. Stop.
6. `kind: wax` or `kind: foreign`: restate the report to the human in plain terms, then ask one
   question with two answers, each with its consequences from the report's `merge would` and
   `discard would` lines:
   - **merge**: keep the handoff history, `PREFERENCES.md`, and project skills; move foreign
     files into `project/`; bring everything else to the new version.
   - **discard**: set the old folder aside and install fresh, starting from the genesis entry.
   Say that the old folder is kept as `.agents.old-<stamp>` either way. If the report lists a
   root `CLAUDE.md` or `AGENTS.md`, also suggest consolidating them into `.agents/project/root/`
   so that all agentic material lives in `.agents/`; note that without a root file, sessions
   must be started with the pickup procedure of `wax_handoff` by hand. Wait for the answers.
   Then run `scripts/migrate.sh <target> --merge` or `--discard`, adding `--consolidate` on a
   yes and `--name <name>` if the human named the project. Report the output verbatim. A `FAIL`
   line means nothing was swapped (P-08); stop and show it.
7. Walk `PREFERENCES.md` in the new copy with the human (R-15): every preference after a fresh
   install or a discard; only the appended section after a merge. Apply the answers to the
   copy's `PREFERENCES.md` only: edit or delete text, keep the remaining IDs. Never touch
   `RULES.md`.
8. After a merge of a WAX install, run the `handoff` procedure of `wax_handoff` in the project,
   titled `Upgraded <old version> to <new version>`, with the script's output summarized under
   "Work done" and the appended preferences under "Open threads". This records the upgrade in
   the project's own chain and leaves `HANDOFF.md` in the current format. After a fresh
   install, a discard, or a foreign merge, write nothing: the genesis entry is HEAD.
9. Tell the human where the folder is, where the old one went, and that the next session starts
   with the pickup procedure of `wax_handoff`.

## Procedure: check

1. Run `scripts/check.sh <project-or-.agents-path>`. With no argument it checks the current
   directory.
2. Report the `ok`, `warn`, and `FAIL` lines. On any `FAIL`, do not fix anything; the human
   decides what to repair (R-09). The most common repairs are in the refusals table below.

## What merge keeps, replaces, and moves

| Item | Kept from the project | Replaced from the installer | Moved |
|---|---|---|---|
| `handoffs/handoffs/<stamp>.md` | all | — | — |
| `handoffs/HANDOFF.md` | yes, plus missing head keys added | — | — |
| `PREFERENCES.md` | yes, plus new IDs appended | — | — |
| `AGENTS.md` | project name | text | — |
| `RULES.md`, template, `skills/wax_init`, `skills/wax_handoff` | — | yes | — |
| `skills/sample_skill` | absent stays absent (P-04) | present is replaced | — |
| project skills | yes | — | from `skill/skillset/` to `skills/` when needed |
| foreign items | — | — | to `project/`; `P-11` lists `project/` |
| root `CLAUDE.md`, `AGENTS.md` | — | — | to `project/root/` only with `--consolidate` |
| old `.agents/` | — | — | to `.agents.old-<stamp>` |

## Refusals

| Condition | Message | What the human does |
|---|---|---|
| `.agents/` is already the installer's version | `FAIL: … is already WAX x.y; run check.sh instead` | Run `check`. |
| `.agents/` is newer than the installer | `FAIL: … is WAX x.y, newer than this installer` | Update the installed skills (re-run the install prompt). |
| merge requested but `check.sh` fails on the old folder | `FAIL: check.sh does not pass on …; repair it first or choose discard (R-09)` | Repair by hand and retry, or choose discard. |
| merged tree fails `check.sh` | `FAIL: the merged tree did not pass check.sh; … is untouched` | Read the `FAIL` lines above it; report to the maintainer. |
| `setup.sh` run with `.agents/` present | `FAIL: … already exists; run check.sh on it instead` | Use `init`, which goes through `migrate.sh`. |
| Target is inside the reference `.agents` | `FAIL: target … is inside the reference .agents` | Give a project path outside this folder. |
| Source is not a WAX reference | `FAIL: … is not a WAX reference` | Run from a clone of the reference, or pass `--source`. |
| `check` reports a skill with bad frontmatter | `FAIL  <name> frontmatter keys are …` | Make the frontmatter exactly `name` and `description` (R-12). |

## Files

- `scripts/setup.sh` — fresh copy: copies, names, then checks. Usage: `setup.sh [target] [--name N] [--source S]`.
- `scripts/migrate.sh` — existing folder: explore (read-only), merge, or discard; never deletes. Usage: `migrate.sh <target> [--explore|--merge|--discard] [--consolidate] [--name N] [--source S]`.
- `scripts/check.sh` — read-only structural and handoff checks; honors `P-11`. Usage: `check.sh [path]`. Exit 1 on failure.
- `references/migrations.md` — ownership of every file and the per-version changes a merge applies.
