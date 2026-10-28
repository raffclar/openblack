#!/bin/bash
# usage: nc3_check.sh <worktree> <build dir>
# Car check (lead-approved 2026-10-27, after the car 904 slot 2 part at turn 1): net_check on mpm_3p_1, 3 clients, 2000
# turns, the store inputs of the worktree with the camera of each input file. Exit 0 when every expect is met, all
# slots are the same and no client resumes; exit 15 otherwise.
WT=$1; BD=$2
HI=$WT/tools/sim/hand_inputs
O=$BD/car-nc3
rm -rf "$O"; mkdir -p "$O"
A=(--clients 3)
names=("" --input --input2 --input3)
for n in 1 2 3; do
  f=$HI/mpm_3p_1_slot$((n-1))_store.json
  A+=(${names[$n]} $f --camera$n "$(python -c "import json,sys; print(json.dumps(json.load(open(sys.argv[1]))['camera']))" "$f")")
done
cd "$WT"
python tools/sim/net_check.py --bin "$BD/bin/RelWithDebInfo" --hash-fields --out "$O" --turns 2000 --land "Online Maps/mpm_3p_1.map" \
  --min-gods 3 --input2-delay-frames 300 --camera-again-after 700 "${A[@]}" > "$O/stdout.txt" 2>&1 </dev/null
rc=$?
rm -rf "$O"/bin "$O"/*/bin
grep -h "commands \[" "$O/net_check.json" 2>/dev/null | sed 's/.*Network game: //' | cut -c1-220
if [ $rc != 0 ]; then echo "net_check exit $rc"; exit 15; fi
exit 0
