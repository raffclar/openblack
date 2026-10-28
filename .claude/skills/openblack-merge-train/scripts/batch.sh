# usage: batch.sh <train number> <batch name> <short:branch> ...
# One car for many branches: from the current main branch, merge each branch in turn (test/CMakeLists.txt, the docs
# tables, the testbed and include-only conflicts are resolved automatically). A branch with another conflict is left
# out for the next batch. Then car_checks.sh builds and tests the car once, opens one PR and merges it.
set -u
source "$(dirname "$0")/common.sh"
N=$1; NAME=$2; shift 2
WT=$OB_WORKTREES/prm-wtb$NAME
cd "$OB_MAIN_CHECKOUT" && git fetch -q $R
# A queue branch that is missing on the remote would build an empty batch: stop before any worktree or build
for car in "$@"; do
  branch=${car#*:}
  [ -n "$(git ls-remote $R "refs/heads/$branch" </dev/null)" ] || { echo "STOP: branch $branch (${car%%:*}) is not on the $R remote: push it first (nothing was built)"; exit 16; }
done
git worktree add -q -b batch-$NAME "$WT" $R/$M || exit 2
cd "$WT"
IN=(); OUT=()
for car in "$@"; do
  short=${car%%:*}; branch=${car#*:}
  MB=$(git merge-base HEAD $R/$branch)
  if git merge-base --is-ancestor $R/$branch HEAD; then IN+=("$short:$branch"); continue; fi
  if ! git -c merge.conflictStyle=diff3 merge --no-commit --no-ff $R/$branch >/dev/null 2>&1; then
    ok=1
    for f in $(git diff --name-only --diff-filter=U); do
      case "$f" in
        test/CMakeLists.txt) python "$HERE/resolve_cmake.py" "$WT" $MB $R/$branch HEAD >/dev/null && git add "$f" || ok=0 ;;
        docs/deviations.md|docs/vanilla-bugs.md) python "$HERE/resolve_deviations.py" "$WT" $MB $R/$branch HEAD "$f" && git add "$f" || ok=0 ;;
        src/Debug/TestbedScenario*) python "$HERE/union_conflicts.py" "$f" && "$OB_CLANG_FORMAT" -i "$f" && git add "$f" || ok=0 ;;
        *.cpp|*.h) if python "$HERE/includes_only.py" "$f"; then python "$HERE/union_conflicts.py" "$f" && "$OB_CLANG_FORMAT" -i "$f" && git add "$f" || ok=0; else ok=0; fi ;;
        *) ok=0 ;;
      esac
    done
    if [ $ok = 0 ] || [ -n "$(git diff --name-only --diff-filter=U)" ]; then
      echo "LEFT OUT $short ($branch): $(git diff --name-only --diff-filter=U | tr '\n' ' ')"
      git merge --abort; OUT+=("$short:$branch"); continue
    fi
  fi
  git commit -q -m "Merge $branch into batch $NAME" -m "$OB_TRAIN_TRAILER"
  IN+=("$short:$branch"); echo "IN $short"
done
source "$HERE/car_checks.sh"
