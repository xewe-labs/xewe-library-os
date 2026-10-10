#!/usr/bin/env sh
# wax_init: verify an existing .agents/ folder (read-only).
# Usage: check.sh [project-dir | .agents-dir]   (default: current directory)
# Exits 1 if any check fails.
set -u
root=${1:-$PWD}
[ -d "$root/.agents" ] && root="$root/.agents"
root=$(cd "$root" 2>/dev/null && pwd) || { echo "FAIL: $1 is not a directory"; exit 1; }
fails=0
ok()   { echo "ok    $1"; }
fail() { echo "FAIL  $1"; fails=$((fails+1)); }

# Required files (R-13, R-14)
for f in AGENTS.md RULES.md PREFERENCES.md handoffs/HANDOFF.md handoffs/handoffs/yyyy-mm-dd-hh-mm-ss.md; do
  [ -f "$root/$f" ] && ok "$f exists" || fail "$f missing"
done

# Exact top level (R-13), plus the extra items P-11 tolerates
extras=$(sed -n 's/^- \*\*P-11 .*tolerates exactly these: \([^.]*\)\..*/\1/p' "$root/PREFERENCES.md" 2>/dev/null | grep -oE '`[^`]+`' | tr -d '`/' | tr '\n' ' ')
top=$(ls -A "$root" | grep -v '^\.git$' | while read -r n; do skip=0; for e in $extras; do [ "$n" = "$e" ] && skip=1; done; [ "$skip" = 0 ] && echo "$n"; done | LC_ALL=C sort | tr '\n' ' ')
[ "$top" = "AGENTS.md PREFERENCES.md RULES.md handoffs skills " ] && ok "top level is the five items${extras:+ plus P-11 extras: $extras}" || fail "top level is '$top'"

# Template intact (R-07); project name filled
grep -q '<yyyy-mm-dd-hh-mm-ss>' "$root/handoffs/handoffs/yyyy-mm-dd-hh-mm-ss.md" 2>/dev/null && ok "template still holds its placeholder" || fail "template altered or missing"
grep -q '<project>' "$root/AGENTS.md" 2>/dev/null && echo "warn  AGENTS.md still has the <project> placeholder (expected only in the reference itself)" || ok "AGENTS.md has a project name"

# Skills: frontmatter shape (R-10, R-11, R-12)
for d in "$root"/skills/*/; do
  n=$(basename "$d")
  if [ ! -f "$d/SKILL.md" ]; then fail "$n has no SKILL.md"; continue; fi
  keys=$(sed -n '2,/^---$/p' "$d/SKILL.md" | grep -E '^[a-z_]+:' | cut -d: -f1 | sort | tr '\n' ' ')
  [ "$keys" = "description name " ] && ok "$n frontmatter has exactly name and description" || fail "$n frontmatter keys are '$keys'"
  [ "$(sed -n 's/^name: *//p' "$d/SKILL.md")" = "$n" ] && ok "$n name matches folder" || fail "$n name does not match folder"
done

# Handoffs: the same checks wax_handoff runs before every read or write (R-05, R-08, R-09)
H="$root/handoffs/HANDOFF.md"
for k in HEAD "HEAD timestamp" Entries "Last writer" Resume Integrity Status; do
  grep -q "^- \*\*$k:\*\*" "$H" 2>/dev/null || fail "HANDOFF.md missing key '$k'"
done
head_val=$(sed -n 's/^- \*\*HEAD:\*\* *//p' "$H" 2>/dev/null)
entries=$(sed -n 's/^- \*\*Entries:\*\* *//p' "$H" 2>/dev/null)
files=$(ls "$root/handoffs/handoffs" 2>/dev/null | grep -E '^[0-9]{4}(-[0-9]{2}){5}\.md$' | sort)
count=$(printf '%s\n' "$files" | grep -c . || true)
newest=$(printf '%s\n' "$files" | tail -1)
if [ "$count" -eq 0 ]; then
  [ "$head_val" = "none" ] && ok "HEAD is none with no entries" || fail "HEAD is '$head_val' but there are no entries"
else
  [ "$head_val" = "handoffs/handoffs/$newest" ] && ok "HEAD is the newest entry" || fail "HEAD is '$head_val', newest file is $newest"
fi
[ "$entries" = "$count" ] && ok "Entries ($entries) equals entry files ($count)" || fail "Entries is '$entries', found $count files"
for f in $files; do
  s=${f%.md}
  grep -q "^- \*\*$s\*\*" "$H" && ok "index row for $s" || fail "no index row for $s"
  grep -qE '^- \*\*(Resume at|State|First action|Blocked on):\*\*' "$root/handoffs/handoffs/$f" || fail "$f lacks section 9 keys"
done
for s in $(grep -oE '^- \*\*[0-9]{4}(-[0-9]{2}){5}\*\*' "$H" 2>/dev/null | sed 's/^- \*\*//; s/\*\*$//'); do
  [ -f "$root/handoffs/handoffs/$s.md" ] || fail "index row $s has no file"
done

# Rule and preference IDs cited exist (R-15 keeps them stable). Historical entries are skipped:
# they are immutable (P-02) and may cite IDs from before a renumbering.
for id in $(grep -rohE '\b[RP]-[0-9]{2}\b' "$root" --include='*.md' --exclude-dir=handoffs | sort -u); do
  case "$id" in
    R-*) grep -q "^- \*\*$id " "$root/RULES.md" || fail "$id is cited but not defined in RULES.md" ;;
    P-*) grep -q "^- \*\*$id " "$root/PREFERENCES.md" || fail "$id is cited but not defined in PREFERENCES.md" ;;
  esac
done
ok "every cited rule and preference ID is defined"

echo "---"
if [ "$fails" -eq 0 ]; then echo "PASS: $root"; exit 0; else echo "FAIL: $fails check(s) failed in $root"; exit 1; fi
