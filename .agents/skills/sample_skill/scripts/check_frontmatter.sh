#!/usr/bin/env sh
# Validate a skill folder against R-12: SKILL.md exists, frontmatter has exactly
# `name` and `description`, and `name` equals the folder name.
# Usage: check_frontmatter.sh <skill-folder>
set -eu
dir=${1:?usage: check_frontmatter.sh <skill-folder>}
file="$dir/SKILL.md"
[ -f "$file" ] || { echo "FAIL: $file not found"; exit 1; }
[ "$(sed -n '1p' "$file")" = "---" ] || { echo "FAIL: line 1 is not ---"; exit 1; }
keys=$(sed -n '2,/^---$/p' "$file" | grep -E '^[a-z_]+:' | cut -d: -f1 | sort | tr '\n' ' ')
[ "$keys" = "description name " ] || { echo "FAIL: frontmatter keys are '$keys', expected 'description name '"; exit 1; }
name=$(sed -n 's/^name: *//p' "$file")
folder=$(basename "$(cd "$dir" && pwd)")
[ "$name" = "$folder" ] || { echo "FAIL: name '$name' does not match folder '$folder'"; exit 1; }
echo "OK: $folder"
