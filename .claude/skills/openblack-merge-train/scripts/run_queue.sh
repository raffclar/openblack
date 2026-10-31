# usage: run_queue.sh <queue file>
# Runs merge cars strictly one after another. Each queue line: "<train> <batch> <short:branch>..." or
# "finish <train> <batch> <short:branch>..." for a hand-merged car in prm-wtb<batch>. Stops at the first car that
# does not merge, so nothing later runs on a wrong base.
set -u
source "$(dirname "$0")/common.sh"
while read -r line; do
	[ -z "$line" ] && continue
	set -- $line
	if [ "$1" = finish ]; then
		shift; n=$1; name=$2
		# A hand car takes in whatever merged before it
		git -C "$OB_MAIN_CHECKOUT" fetch -q $R
		git -C "$OB_WORKTREES/prm-wtb$name" merge -q --no-edit $R/$M || { echo "STOPPED at train $n ($name): $M does not merge into the hand car"; exit 1; }
		bash "$HERE/car_finish.sh" "$@" > "$P/carlogs/queue-$name.log" 2>&1
	else
		n=$1; name=$2
		bash "$HERE/batch.sh" "$@" > "$P/carlogs/queue-$name.log" 2>&1
	fi
	if grep -q "^MERGED $n " "$P/carlogs/queue-$name.log" && ! grep -q "LEFT OUT" "$P/carlogs/queue-$name.log"; then
		grep "^MERGED $n " "$P/carlogs/queue-$name.log"
	else
		echo "STOPPED at train $n ($name):"
		grep -E "LEFT OUT|STOP|MERGED" "$P/carlogs/queue-$name.log"
		exit 1
	fi
done < "$1"
echo "QUEUE DONE"
