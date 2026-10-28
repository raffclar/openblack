# usage: post_hand.sh <batch> <branch> <old:new>...
# Ends a hand merge after you fixed the conflicts by hand: renumbers the branch's vanilla-bugs rows (old:new), sorts
# the rows, and commits the merge.
set -u
source "$(dirname "$0")/common.sh"
B=$1; BR=$2; shift 2
cd "$OB_WORKTREES/prm-wtb$B" || exit 2
git add -u
for m in "$@"; do o=${m%%:*}; n=${m#*:}; sed -i -E "s/^\| $o \|/| $n |/" docs/vanilla-bugs.md; done
python "$HERE/sort_rows.py" docs/vanilla-bugs.md
git add docs/vanilla-bugs.md
git commit -q -m "Merge $BR into batch $B" -m "$OB_TRAIN_TRAILER" && git log -1 --format="%an | %s" && git status --porcelain | head -3
grep -oE '^\| [0-9]+ ' docs/vanilla-bugs.md | tail -4 | tr '\n' ' '
