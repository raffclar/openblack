# usage: car_finish.sh <train number> <batch name> <short:branch> ...
# Finishes a hand-merged car: the branches are already merged by hand in prm-wtb<batch>. run_queue.sh calls this for
# a "finish" line, after it merges the main branch into the car.
set -u
source "$(dirname "$0")/common.sh"
N=$1; NAME=$2; shift 2
WT=$OB_WORKTREES/prm-wtb$NAME
cd "$WT" || { echo "STOP: no hand car at $WT"; exit 2; }
IN=("$@"); OUT=()
source "$HERE/car_checks.sh"
