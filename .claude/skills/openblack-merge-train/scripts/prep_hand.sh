# usage: prep_hand.sh <batch> <branch> <old:new>...
# Starts a hand car: makes prm-wtb<batch> from the main branch, merges the branch, resolves the vanilla-bugs and
# testbed conflicts, renumbers the branch's vanilla-bugs rows (old:new) and commits. Any other conflict stops it.
set -u
source "$(dirname "$0")/common.sh"
B=$1; BR=$2; shift 2
WT=$OB_WORKTREES/prm-wtb$B
cd "$OB_MAIN_CHECKOUT" && git fetch -q $R && git worktree add -q -b batch-$B "$WT" $R/$M || exit 2
cd "$WT"
git -c merge.conflictStyle=diff3 merge --no-ff --no-commit $R/$BR >/dev/null 2>&1
for f in $(git diff --name-only --diff-filter=U); do
  case $f in
    docs/vanilla-bugs.md) python "$HERE/union_conflicts.py" $f ;;
    src/Debug/TestbedScenario*) python "$HERE/union_conflicts.py" $f && "$OB_CLANG_FORMAT" -i $f && git add $f ;;
    *) echo "CONFLICT $f"; exit 3 ;;
  esac
done
bash "$HERE/post_hand.sh" "$B" "$BR" "$@"
