#!/usr/bin/env sh
# wax_init: explore an existing .agents/ and migrate it (merge) or set it aside (discard).
# Usage: migrate.sh <target-project-dir> [--explore | --merge | --discard] [--consolidate] [--name <project-name>] [--source <reference-.agents>]
# --explore (default) is read-only and prints a report. --merge and --discard never delete anything:
# the old folder is moved to <target>/.agents.old-<utc-stamp>. --consolidate also moves a root CLAUDE.md
# and AGENTS.md into .agents/project/root/. Exit 1 on FAIL; nothing is swapped on failure.
set -eu
here=$(cd "$(dirname "$0")" && pwd -P)
source=$(cd "$here/../../.." && pwd)
mode=explore; consolidate=0; target=""; name=""
while [ $# -gt 0 ]; do
  case "$1" in
    --explore|--merge|--discard) mode=${1#--}; shift ;;
    --consolidate) consolidate=1; shift ;;
    --name) name=$2; shift 2 ;;
    --source) source=$(cd "$2" && pwd); shift 2 ;;
    -h|--help) sed -n '2,6p' "$0" | sed 's/^# //'; exit 0 ;;
    -*) echo "FAIL: unknown option $1"; exit 2 ;;
    *) target=$1; shift ;;
  esac
done
[ -n "$target" ] || target=$PWD
[ -d "$target" ] || { echo "FAIL: $target is not a directory"; exit 1; }
target=$(cd "$target" && pwd)
old="$target/.agents"
[ -f "$source/AGENTS.md" ] && [ -d "$source/skills/wax_init" ] && [ -d "$source/handoffs/handoffs" ] \
  || { echo "FAIL: $source is not a WAX reference (missing AGENTS.md, skills/wax_init or handoffs/handoffs)"; exit 1; }
case "$target/" in "$source"/*) echo "FAIL: target $target is inside the reference .agents"; exit 1 ;; esac
[ "$target" = "$(dirname "$source")" ] && { echo "FAIL: $target holds the reference .agents itself"; exit 1; }
newver=$(grep -oE 'WAX [0-9][0-9.]*' "$source/AGENTS.md" | head -1)
# The source must ship the genesis state: exactly one entry, titled GENESIS. A development tree with its own
# entries would hand its history to the project (the problem WAX exists to prevent).
src_entries=$(ls "$source/handoffs/handoffs" | grep -cE '^[0-9]{4}(-[0-9]{2}){5}\.md$' || true)
src_genesis=$(grep -l '^- \*\*Title:\*\* GENESIS' "$source"/handoffs/handoffs/[0-9]*.md 2>/dev/null | wc -l | tr -d ' ')
src_ok=0; [ "$src_entries" = 1 ] && [ "$src_genesis" = 1 ] && src_ok=1
stamp=$(date -u +%Y-%m-%d-%H-%M-%S)
REF_SKILLS="wax_init wax_setup wax_handoff sample_skill xaw_setup xaw_handoff xewe_setup"
FIXED="AGENTS.md PREFERENCES.md README.md RULES.md handoffs skill skills"
is_ref_skill() { for r in $REF_SKILLS; do [ "$1" = "$r" ] && return 0; done; return 1; }
in_list() { for r in $2; do [ "$1" = "$r" ] && return 0; done; return 1; }
version_le() { [ "$(printf '%s\n%s\n' "$1" "$2" | sort -V | head -1)" = "$1" ]; }

# ---------- explore (always; read-only) ----------
oldver=""
if [ ! -d "$old" ]; then kind=missing
else
  oldver=$(grep -oE 'WAX [0-9][0-9.]*' "$old/AGENTS.md" 2>/dev/null | head -1 || true)
  if [ -z "$oldver" ]; then kind=foreign
  elif [ "$oldver" = "$newver" ]; then kind=same
  elif version_le "${oldver#WAX }" "${newver#WAX }"; then kind=wax
  else kind=newer; fi
fi
oldname=""
[ -f "$old/AGENTS.md" ] && oldname=$(sed -n 's/^# AGENTS.md — //p' "$old/AGENTS.md" | head -1 || true)
[ "$oldname" = "<project>" ] && oldname=""
[ -n "$name" ] || name=${oldname:-$(basename "$target")}

echo "installer:  $newver ($source)"
[ "$src_ok" = 1 ] || echo "warn:       source holds $src_entries handoff entries, not the single genesis entry; merge and discard will refuse it"
echo "target:     $old"
echo "kind:       $kind${oldver:+ ($oldver)}"
echo "name:       $name"
if [ "$kind" = missing ]; then
  echo "next:       nothing to migrate; run setup.sh for a fresh install"
  [ "$mode" = explore ] && exit 0
  echo "FAIL: no .agents to $mode; run setup.sh instead"; exit 1
fi

project_skills=""; old_layout=""
for d in "$old"/skills/*/ "$old"/skill/skillset/*/; do
  [ -d "$d" ] || continue
  n=$(basename "$d"); is_ref_skill "$n" && continue
  project_skills="$project_skills $n"
done
[ -d "$old/skill/skillset" ] && old_layout="skill/skillset (pre-1.1)"
entries=$(ls "$old/handoffs/handoffs" 2>/dev/null | grep -cE '^[0-9]{4}(-[0-9]{2}){5}\.md$' || true)
head_missing=""
if [ -f "$old/handoffs/HANDOFF.md" ]; then
  for k in HEAD "HEAD timestamp" Entries "Last writer" Resume Integrity Status; do
    grep -q "^- \*\*$k:\*\*" "$old/handoffs/HANDOFF.md" || head_missing="$head_missing '$k'"
  done
fi
pref_add=""
if [ -f "$old/PREFERENCES.md" ]; then
  for id in $(grep -oE '^- \*\*P-[0-9]{2}' "$source/PREFERENCES.md" | grep -oE 'P-[0-9]{2}'); do
    grep -q "^- \*\*$id " "$old/PREFERENCES.md" || pref_add="$pref_add $id"
  done
fi
# Items the old P-11 tolerates are carried as they are; everything else outside FIXED is foreign.
extras=""
[ -f "$old/PREFERENCES.md" ] && extras=$(sed -n 's/^- \*\*P-11 .*tolerates exactly these: \([^.]*\)\..*/\1/p' "$old/PREFERENCES.md" | grep -oE '`[^`]+`' | tr -d '`/' | tr '\n' ' ')
foreign=""
for item in "$old"/* "$old"/.[!.]*; do
  [ -e "$item" ] || continue
  n=$(basename "$item"); [ "$n" = ".git" ] && continue
  if [ "$kind" = foreign ] || { ! in_list "$n" "$FIXED" && ! in_list "$n" "$extras"; }; then foreign="$foreign $n"; fi
done
root_files=""
for f in CLAUDE.md AGENTS.md; do [ -f "$target/$f" ] && root_files="$root_files $f"; done
agents_diff="n/a"
if [ "$kind" = wax ] || [ "$kind" = same ]; then
  agents_diff=$(sed "s|<project>|$name|g" "$source/AGENTS.md" | diff - "$old/AGENTS.md" | grep -c '^[<>]' || true)
fi
gitstate="not a git repository"
if git -C "$target" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  dirty=$(git -C "$target" status --short -- .agents 2>/dev/null | wc -l | tr -d ' ')
  gitstate="git repository; $dirty uncommitted path(s) under .agents"
fi
# The old copy is judged by its own check.sh, which knows its own layout and format.
checkres="n/a"
if [ "$kind" = wax ] || [ "$kind" = same ]; then
  oldcheck=""
  for c in "$old/skills/wax_init/scripts/check.sh" "$old/skill/skillset/wax_init/scripts/check.sh" "$old/skill/skillset/wax_setup/scripts/check.sh" "$old/skill/skillset/xaw_setup/scripts/check.sh"; do
    [ -f "$c" ] && { oldcheck=$c; break; }
  done
  if [ -n "$oldcheck" ]; then checkres=$(sh "$oldcheck" "$old" 2>/dev/null | tail -1 || true)
  else checkres="no check.sh in the old copy (gate skipped)"; fi
fi

echo "skills:     project skills:${project_skills:- none}${old_layout:+; layout: $old_layout}"
echo "handoffs:   $entries entries; HANDOFF.md missing keys:${head_missing:- none}"
echo "prefs:      IDs the upgrade would append:${pref_add:- none}"
echo "agents_md:  $agents_diff line(s) differ from the installer's AGENTS.md (edits stay in the old copy)"
echo "extras:     ${extras:-none}(P-11, carried as they are)"
echo "foreign:    ${foreign:-none}"
echo "root:       ${root_files:-no CLAUDE.md or AGENTS.md at the project root}"
echo "git:        $gitstate"
echo "check:      $checkres"
echo "merge would: keep $entries entries; carry project skills${project_skills:+ ($project_skills)}; keep PREFERENCES.md and append${pref_add:- nothing}; replace RULES.md, AGENTS.md (name kept), template, reference skills${old_layout:+; move project skills to skills/ and drop skill/SKILL.md}${head_missing:+; add missing HANDOFF.md keys}${foreign:+; move foreign items to project/ and list project/ in P-11}"
echo "discard would: move .agents to .agents.old-<stamp>, install $newver fresh with the genesis entry"
[ "$mode" = explore ] && exit 0

# ---------- guards ----------
[ "$kind" = same ] && { echo "FAIL: $old is already $newver; run check.sh instead"; exit 1; }
[ "$kind" = newer ] && { echo "FAIL: $old is $oldver, newer than this installer ($newver); update the installer first"; exit 1; }
if [ "$mode" = merge ] && [ "$kind" = wax ]; then
  case "$checkres" in PASS*|"no check.sh"*) ;; *) echo "FAIL: the old copy's check.sh does not pass on $old; repair it first or choose discard (R-09)"; exit 1 ;; esac
fi
[ -e "$target/.agents.old-$stamp" ] && { echo "FAIL: $target/.agents.old-$stamp already exists"; exit 1; }
[ "$src_ok" = 1 ] || { echo "FAIL: $source holds $src_entries handoff entries, not the single genesis entry; run from a clone of the public repository"; exit 1; }

# ---------- discard ----------
if [ "$mode" = discard ]; then
  mv "$old" "$target/.agents.old-$stamp"
  echo "moved:      .agents -> .agents.old-$stamp (nothing deleted)"
  echo "---"
  sh "$here/setup.sh" "$target" --name "$name" --source "$source"
  if [ "$consolidate" = 1 ] && [ -n "$root_files" ]; then
    mkdir -p "$old/project/root"
    for f in $root_files; do mv "$target/$f" "$old/project/root/$f"; echo "moved:      $f -> .agents/project/root/$f"; done
    sed -i 's/tolerates exactly these: none\./tolerates exactly these: `project\/`./' "$old/PREFERENCES.md"
    echo "---"; sh "$here/check.sh" "$old" | tail -1
  fi
  exit 0
fi

# ---------- merge ----------
new="$target/.agents.new-$stamp"
cp -R "$source" "$new"; rm -rf "$new/.git"
for f in "$new/AGENTS.md" "$new"/handoffs/handoffs/[0-9]*.md; do
  [ -f "$f" ] && sed "s|<project>|$name|g" "$f" > "$f.tmp" && mv "$f.tmp" "$f"
done
if [ "$kind" = wax ]; then
  # 1. Handoffs: all entries and HANDOFF.md carried over; only missing head keys are added (R-04 exception).
  rm -f "$new"/handoffs/handoffs/[0-9]*.md
  cp "$old"/handoffs/handoffs/[0-9]*.md "$new/handoffs/handoffs/" 2>/dev/null || true
  cp "$old/handoffs/HANDOFF.md" "$new/handoffs/HANDOFF.md"
  if ! grep -q '^- \*\*Resume:\*\*' "$new/handoffs/HANDOFF.md"; then
    sed -i '/^- \*\*Last writer:\*\*/a - **Resume:** none' "$new/handoffs/HANDOFF.md"
    echo "handoffs:   added '- **Resume:** none' to HANDOFF.md"
  fi
  # 2. Preferences: the project's file, plus the IDs this version adds, under a marked heading.
  cp "$old/PREFERENCES.md" "$new/PREFERENCES.md"
  if [ -n "$pref_add" ]; then
    printf '\n## Added by the upgrade to %s (review: keep, edit, or delete each)\n\n' "$newver" >> "$new/PREFERENCES.md"
    for id in $pref_add; do
      awk -v id="$id" '$0 ~ "^- \\*\\*" id " " {p=1; print; next} p && /^(- \*\*|## )/ {p=0} p {print}' "$source/PREFERENCES.md" >> "$new/PREFERENCES.md"
    done
    echo "prefs:      appended$pref_add"
  fi
  # 3. Project skills carried; sample_skill stays deleted if the project deleted it (P-04).
  for n in $project_skills; do
    src=""; [ -d "$old/skills/$n" ] && src="$old/skills/$n"; [ -d "$old/skill/skillset/$n" ] && src="$old/skill/skillset/$n"
    cp -R "$src" "$new/skills/$n"; echo "skills:     carried $n"
  done
  [ -d "$old/skills/sample_skill" ] || [ -d "$old/skill/skillset/sample_skill" ] || { rm -rf "$new/skills/sample_skill"; echo "skills:     sample_skill stays removed (P-04)"; }
fi
# 4. P-11 extras carried as they are; foreign content -> project/, tolerated through P-11.
for n in $extras; do [ -e "$old/$n" ] && { cp -R "$old/$n" "$new/$n"; echo "extras:     carried $n"; }; done
if [ -n "$foreign" ]; then
  mkdir -p "$new/project"
  for n in $foreign; do cp -R "$old/$n" "$new/project/$n"; echo "foreign:    $n -> .agents/project/$n"; done
fi
# 5. Root CLAUDE.md / AGENTS.md -> project/root/, on request.
if [ "$consolidate" = 1 ] && [ -n "$root_files" ]; then
  mkdir -p "$new/project/root"
  for f in $root_files; do cp "$target/$f" "$new/project/root/$f"; done
fi
if [ -d "$new/project" ]; then
  grep -q 'tolerates exactly these: `project/`' "$new/PREFERENCES.md" \
    || sed -i 's/tolerates exactly these: none\./tolerates exactly these: `project\/`./' "$new/PREFERENCES.md"
  grep -q 'tolerates exactly these: `project/`' "$new/PREFERENCES.md" || echo "warn:       P-11 not found in PREFERENCES.md; add 'project/' to it by hand"
fi
# 6. Verify the new tree before swapping; nothing is swapped on failure.
if ! sh "$here/check.sh" "$new" | tail -1 | grep -q '^PASS'; then
  sh "$here/check.sh" "$new" | grep '^FAIL'
  rm -rf "$new"
  echo "FAIL: the merged tree did not pass check.sh; $old is untouched"; exit 1
fi
# 7. Swap.
mv "$old" "$target/.agents.old-$stamp"
mv "$new" "$old"
if [ "$consolidate" = 1 ] && [ -n "$root_files" ]; then
  for f in $root_files; do rm -f "$target/$f"; echo "moved:      $f -> .agents/project/root/$f"; done
fi
echo "moved:      .agents -> .agents.old-$stamp (nothing deleted)"
echo "---"
sh "$here/check.sh" "$old" | tail -1
echo "---"
echo "Done. Review $old/PREFERENCES.md (appended section, if any)."
[ "$kind" = wax ] && echo "Record the upgrade: run the handoff procedure of wax_handoff in the project, titled 'Upgraded $oldver to $newver'."
exit 0
