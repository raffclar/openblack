#!/bin/bash
# Lists unmerged queue_all cars that are not in priority_order or lost_ignore (run before you pick the next car).
source "$(dirname "$0")/common.sh"
cd "$OB_MAIN_CHECKOUT" || exit 1
git fetch -q $R
touch "$P/priority_order.txt" "$P/lost_ignore.txt"
grep -oE '^[0-9]+ [a-z]+ [a-z0-9]+:[a-z0-9.-]+' "$P/queue_all.txt" | while read n b s; do
  grep -q "^$n " "$P/priority_order.txt" && continue
  grep -q "^$n " "$P/lost_ignore.txt" && continue
  h=$(git rev-parse -q --verify "$R/${s#*:}") || { echo "$n $s NO-BRANCH"; continue; }
  git merge-base --is-ancestor "$h" $R/$M || echo "NOT-IN-PRIORITY $n $b $s"
done
