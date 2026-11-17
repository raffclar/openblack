#!/bin/bash
# usage: net_plain_check.sh <worktree> <bin folder> <out folder>
# Post-merge check, once for each batch on the new main branch: net_check on mpm_2p_1, 2 clients, 2000 turns, the store
# inputs of the worktree with the camera of each input file, and no --hash-parts or --hash-fields. With these options
# the clients hash as the live clients do: beside the draw, after the frame effects. The server hashes at the end of
# the turn. So a frame step that changes the synced world parts this run, as it parts a live game (Sentry OPENBLACK-N).
# A run with --hash-parts or --hash-fields hashes in the turn and does not find it. Exit 0 when every expect is met,
# both slots are the same and no client resumes; exit 16 otherwise.
WT=$1; BIN=$2; O=$3
HI=$WT/tools/sim/hand_inputs
rm -rf "$O"; mkdir -p "$O"
A=(--clients 2)
names=("" --input --input2)
for n in 1 2; do
  f=$HI/mpm_2p_1_slot$((n-1))_store.json
  A+=(${names[$n]} "$f" --camera$n "$(python -c "import json,sys; print(json.dumps(json.load(open(sys.argv[1]))['camera']))" "$f")")
done
cd "$WT" || exit 16
python tools/sim/net_check.py --bin "$BIN" --out "$O" --turns 2000 --land "Online Maps/mpm_2p_1.map" \
  --input2-delay-frames 300 --camera-again-after 700 "${A[@]}" > "$O/stdout.txt" 2>&1 </dev/null
rc=$?
rm -rf "$O"/bin "$O"/*/bin
grep -h "commands \[" "$O/net_check.json" 2>/dev/null | sed 's/.*Network game: //' | cut -c1-220
python -c "import json,sys; d=json.load(open(sys.argv[1])); print('missing', d['missing_deeds'])" "$O/net_check.json" 2>/dev/null
if [ $rc != 0 ]; then echo "net_check exit $rc"; exit 16; fi
exit 0
