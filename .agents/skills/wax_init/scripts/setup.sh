#!/usr/bin/env sh
# wax_init: create a fresh .agents/ in a project from this reference design.
# Usage: setup.sh [target-project-dir] [--name <project-name>] [--source <reference-.agents>]
# Defaults: target = current directory, name = basename of target, source = the .agents this script lives in.
set -eu
here=$(cd "$(dirname "$0")" && pwd -P)
source=$(cd "$here/../../.." && pwd)
target=""; name=""
while [ $# -gt 0 ]; do
  case "$1" in
    --name) name=$2; shift 2 ;;
    --source) source=$(cd "$2" && pwd); shift 2 ;;
    -h|--help) sed -n '2,4p' "$0" | sed 's/^# //'; exit 0 ;;
    -*) echo "FAIL: unknown option $1"; exit 2 ;;
    *) target=$1; shift ;;
  esac
done
[ -n "$target" ] || target=$PWD
[ -d "$target" ] || { echo "FAIL: $target is not a directory"; exit 1; }
target=$(cd "$target" && pwd)
[ -n "$name" ] || name=$(basename "$target")

# Refusals: wrong source, target inside the reference, target already set up.
[ -f "$source/AGENTS.md" ] && [ -f "$source/RULES.md" ] && [ -d "$source/handoffs/handoffs" ] \
  || { echo "FAIL: $source is not an .agents reference (missing AGENTS.md, RULES.md or handoffs/handoffs)"; exit 1; }
case "$target/" in "$source"/*) echo "FAIL: target $target is inside the reference .agents"; exit 1 ;; esac
[ "$target" = "$(dirname "$source")" ] && { echo "FAIL: $target already holds the reference .agents itself"; exit 1; }
[ -e "$target/.agents" ] && { echo "FAIL: $target/.agents already exists; run check.sh on it instead (nothing overwritten)"; exit 1; }
# The source must ship the genesis state: exactly one entry, titled GENESIS. A development tree with its own
# entries would hand its history to the project (the problem WAX exists to prevent).
src_entries=$(ls "$source/handoffs/handoffs" | grep -cE '^[0-9]{4}(-[0-9]{2}){5}\.md$' || true)
src_genesis=$(grep -l '^- \*\*Title:\*\* GENESIS' "$source"/handoffs/handoffs/[0-9]*.md 2>/dev/null | wc -l | tr -d ' ')
[ "$src_entries" = 1 ] && [ "$src_genesis" = 1 ] || { echo "FAIL: $source holds $src_entries handoff entries, not the single genesis entry; install from a clone of the public repository"; exit 1; }

echo "source: $source"
echo "target: $target/.agents"
echo "name:   $name"

# 1. Copy the folder, without any git metadata.
cp -R "$source" "$target/.agents"
rm -rf "$target/.agents/.git"
dest="$target/.agents"

# 2. Handoffs come as shipped: the template plus the genesis entry (see wax_handoff references/formats.md).
#    Run this from a clone of the public repository, never from a tree that holds its own entries.

# 3. Fill the project name in AGENTS.md and in the genesis entry.
for f in "$dest/AGENTS.md" "$dest"/handoffs/handoffs/[0-9]*.md; do
  [ -f "$f" ] && sed "s|<project>|$name|g" "$f" > "$f.tmp" && mv "$f.tmp" "$f"
done

# 4. Verify the result.
echo "---"
sh "$here/check.sh" "$dest"
echo "---"
echo "Done. Review $dest/PREFERENCES.md now: these defaults are yours to change (R-15). RULES.md stays as shipped."
echo "Point the agent at $dest/AGENTS.md. Its first action is the pickup procedure of wax_handoff, which quotes the genesis entry: explore the project first."
