---
name: sample_skill
description: >
  Sample skill showing the complete anatomy Anthropic recommends for a skill: frontmatter,
  a lean SKILL.md body, and bundled scripts/, references/, assets/ and evals/. Use it whenever
  you are writing, reviewing, or restructuring a skill in this project, even if the request
  only says "add a procedure" or "document how we do X". Never run it as a project task
  (P-04). Triggers: new skill, skill format, skill template, "what goes in a skill",
  "how do I write a skill", SKILL.md, skill review.
---

# sample_skill

This skill does nothing for the project. Copy its shape when you write a real skill. The
shape follows Anthropic's skill-authoring guidance: keep `SKILL.md` lean, push detail into
bundled files, and explain why each instruction matters rather than issuing bare commands.

## Anatomy

```
sample_skill/
├── SKILL.md                 required: frontmatter + instructions (keep under 500 lines)
├── scripts/                 executable code for deterministic or repetitive steps
│   └── check_frontmatter.sh
├── references/              documentation loaded into context only when needed
│   └── example-reference.md
├── assets/                  files used in the output: templates, icons, fonts
│   └── skill-template.md
└── evals/                   realistic test prompts used to check the skill works
    └── evals.json
```

Skills load in three levels. The frontmatter is always in context, so it carries everything
about *when* to use the skill. The body loads when the skill triggers, so it carries the
*how*. Bundled files load only when a step points at them, so they can be as long as they need
to be. Keeping each level to its job is what keeps a skill cheap to carry and reliable to run.

## Frontmatter

- `name` equals the folder name (R-12).
- `description` says what the skill does and when to use it, and errs on the side of
  triggering: models tend to under-use skills, so list the phrasings and situations that should
  bring it in, including ones that do not name the skill.
- Nothing else is required. Add `compatibility` only when the skill needs a tool or
  dependency that may be missing.

## Writing the body

1. Open with two or three lines on what the skill owns and why it exists.
2. State the invariants or preconditions, so the refusals below are predictable.
3. Write procedures as numbered steps in the imperative, one action per step. Explain the
   reason behind a step when it is not obvious; a model that understands the why handles the
   cases you did not anticipate.
4. Define any output with an exact template, and show at least one input/output example.
5. Point at bundled files by relative path and say when to read them, so they are not loaded
   needlessly.
6. End with a refusals table and a file list.

## Output format

When a skill produces a document, fix the template in the skill:

```markdown
# [Title]
## Summary
## Findings
## Next steps
```

## Example

**Input:** "write me a skill that renames photo files by their EXIF date"
**Output:** a folder `photo_rename/` with a `SKILL.md` whose description triggers on
"rename photos", "sort by date taken", and "EXIF"; a `scripts/rename.py` that does the
deterministic work; and `evals/evals.json` with three realistic prompts.

## Checking a skill

Run `scripts/check_frontmatter.sh <skill-folder>` before registering a skill. It confirms the
frontmatter has exactly `name` and `description` and that `name` matches the folder, which
is what discovery relies on (R-10, R-12).

## Refusals

| Condition | Message | What the human does |
|---|---|---|
| Asked to run sample_skill as a project task | `Refused: sample_skill is a reference example (P-04).` | Pick a real skill from `skills/`. |

## Files

- `scripts/check_frontmatter.sh` — validates a skill folder's frontmatter against R-12.
- `references/example-reference.md` — what reference files are for and how to link them.
- `assets/skill-template.md` — blank SKILL.md to copy when starting a new skill.
- `evals/evals.json` — the test-prompt format Anthropic's skill tooling reads.
