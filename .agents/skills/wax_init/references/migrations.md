# wax_init — migrations

What changed between versions, and how `scripts/migrate.sh --merge` brings an older `.agents/`
up to the installer's version. The script works from any older layout straight to the current
one; this table is the human-readable account of each step. Older entries are never edited
(P-02); they may cite paths and rules from their own version.

## Ownership (every version)

| Group | Files | On merge |
|---|---|---|
| Reference-owned | `RULES.md`, the entry template, `skills/wax_init`, `skills/wax_handoff`, `skills/sample_skill` | replaced with the installer's copy; `sample_skill` stays absent if the project removed it (P-04) |
| Project-owned | every `handoffs/handoffs/<stamp>.md`; any skill folder not named above | carried over unchanged |
| Merged | `AGENTS.md` (installer's text, project name kept), `PREFERENCES.md` (project's text kept, new IDs appended under a marked heading), `handoffs/HANDOFF.md` (project's file, missing head keys added) | see each |
| Foreign | anything else at the top level, or everything when the folder is not WAX | moved to `project/`, which P-11 then lists |

## Version steps

| From | To | Change | What migrate.sh does |
|---|---|---|---|
| 1.0 | 1.1 | Skills moved from `skill/skillset/<name>/` to `skills/<name>/`; the router `skill/SKILL.md` was dropped (R-10, R-11, R-13). | Carries project skills into `skills/`; the old `skill/` folder stays only in `.agents.old-<stamp>`. |
| 1.0 | 1.1 | `HANDOFF.md` gained the `Resume` key; entries gained the `Session` key in section 1. | Adds `- **Resume:** none` after `Last writer` when absent. Old entries are left as they are; the template is replaced. |
| 1.0 | 1.1 | `wax_setup` renamed to `wax_init`. | Reference skills are replaced wholesale, so the old folder is simply not carried. |
| 1.1 | 1.2 | `P-11` lists tolerated extra top-level items; R-13 and `check.sh` honor it; R-04 allows `wax_init` to add missing `HANDOFF.md` keys during an upgrade. | Appends `P-11` to the project's `PREFERENCES.md`; sets it to `project/` when foreign content was moved there. |
| 1.2 | 1.3 | `.agents/README.md` removed; the repository root holds the only README. R-13 and R-14 list five fixed items; P-01 retired; P-05 drops its README clause. | The old `README.md` is simply not carried (it is reference-owned); it stays in `.agents.old-<stamp>`. |

## What is never done

- Deleting the old folder. It is moved to `<project>/.agents.old-<utc-stamp>`.
- Editing or dropping a handoff entry.
- Rewriting a preference the project changed. New IDs are appended; existing text is kept even when the installer's wording differs.
- Swapping in a merged tree that fails `check.sh`.
