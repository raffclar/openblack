#!/usr/bin/env bash
# The desync soak: long network games of computer gods (tools/sim/ai_soak.py, which runs net_check.py) and server_check
# runs of the same lands, with one command. Every observer and the game must stay in step with the server at every turn.
#
#   bash tools/sim/desync_soak.sh mac   [options]   from the PC: copy a commit to the Mac build box, build it there, soak
#                                                   there, and fetch the results to the PC
#   bash tools/sim/desync_soak.sh local [options]   soak on this computer (the PC or the Mac) with a build that exists
#
# Options (all optional):
#   --commit REF        mac: the commit to soak (default HEAD of this tree)
#   --name N            mac: the name of the tree and build on the Mac (default desyncsoak)
#   --bin DIR           local: the folder of openblack and openblack-server (default the build of this tree)
#   --hours H           start no new game after H hours (default 8)
#   --lands L           the lands, divided by commas (default TwoGods,mpm_2p_1,mpm_3p_1,mpm_4p_1)
#   --seeds S           the seeds of the world (default 1,2,3)
#   --ai-seeds A        the AI seeds (default 1,2; 0 is the built-in personalities)
#   --observers O       the desktop observers of each game (default 2)
#   --turns T           the turns of each game (default 20000)
#   --server-turns T    the turns of each server_check run (default 5000; 0 for no server_check)
#   --shuffle-seed N    the order of the games (default the day of the year, so that each night covers other games)
#   --out DIR           the results (default <runs>/desync-soak; the Mac uses ~/projects/openblack/sim-runs)
#
# Results: <out>/<date>/ai-soak (a folder for each game and summary.jsonl), <out>/<date>/server-check/<land>-s<seed>-ai<n>
# and <out>/<date>/report.md (the table of every game and every server_check). The mac mode copies the report, the
# summaries and the logs (not the programs) to E:/openblack/sim-runs/desync-soak/<name>/<date> on the PC.
#
# The load rules: ai_soak.py starts a game only when the computer allows it (the PC: processor under 75 %, 16 GB free
# memory, 150 GB free disk; the Mac: processor under 75 %, 6 GB free memory, 30 GB free disk, at most 4 game programs of
# all lanes with the new game's). Only one soak runs on a computer at a time (a lock file with the process id). The script
# stops only the processes that it started, by their process ids. Every game runs with --background.
#
# Exit codes: 0 every game and every server_check stayed in step; 1 a game parted or failed; 2 a wrong command line or a
# missing build; 3 another soak runs.
set -u
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
TREE=$(cd "$HERE/../.." && pwd)
MODE=${1:-}
shift || true

COMMIT=HEAD
NAME=desyncsoak
BIN=""
HOURS=8
LANDS=TwoGods,mpm_2p_1,mpm_3p_1,mpm_4p_1
SEEDS=1,2,3
AI_SEEDS=1,2
OBSERVERS=2
TURNS=20000
SERVER_TURNS=5000
SHUFFLE=$((10#$(date +%j)))
OUT=""
while [ $# -gt 0 ]; do
	case "$1" in
	--commit) COMMIT=$2; shift 2 ;;
	--name) NAME=$2; shift 2 ;;
	--bin) BIN=$2; shift 2 ;;
	--hours) HOURS=$2; shift 2 ;;
	--lands) LANDS=$2; shift 2 ;;
	--seeds) SEEDS=$2; shift 2 ;;
	--ai-seeds) AI_SEEDS=$2; shift 2 ;;
	--observers) OBSERVERS=$2; shift 2 ;;
	--turns) TURNS=$2; shift 2 ;;
	--server-turns) SERVER_TURNS=$2; shift 2 ;;
	--shuffle-seed) SHUFFLE=$2; shift 2 ;;
	--out) OUT=$2; shift 2 ;;
	*) echo "desync_soak: unknown option $1"; exit 2 ;;
	esac
done
# The options that the mac mode gives to the local mode on the Mac
PASS=(--hours "$HOURS" --lands "$LANDS" --seeds "$SEEDS" --ai-seeds "$AI_SEEDS" --observers "$OBSERVERS" --turns "$TURNS"
	--server-turns "$SERVER_TURNS" --shuffle-seed "$SHUFFLE")
DATE=$(date +%Y-%m-%d)
MINUTES=$(awk -v h="$HOURS" 'BEGIN{printf "%d", h*60}')

# The land arguments of server_check for a land of ai_soak.py
server_land() {
	case "$1" in
	ring32) echo "--gods-ring-map" ;;
	TwoGods | ThreeGods | FourGods | Firestorm) echo "--land Playgrounds/$1.txt" ;;
	IslandWars) echo "--land Playgrounds/Island\ Wars.txt" ;;
	mpm_*) echo "--land Online\ Maps/$1.map" ;;
	*) return 1 ;;
	esac
}

run_local() {
	case "$(uname -s)" in
	Darwin)
		PY=${OB_SIM_PYTHON:-$HOME/projects/openblack/cache/simvenv/bin/python}
		EXE=""
		[ -n "$BIN" ] || BIN="$HOME/projects/openblack/builds/$(basename "$TREE")/RelWithDebInfo/bin"
		[ -n "$OUT" ] || OUT="$HOME/projects/openblack/sim-runs/desync-soak"
		alive() { kill -0 "$1" 2>/dev/null; }
		;;
	*)
		PY=${OB_SIM_PYTHON:-python}
		EXE=".exe"
		[ -n "$BIN" ] || BIN="E:/openblack/builds/$(basename "$TREE")/bin/RelWithDebInfo"
		[ -n "$OUT" ] || OUT="E:/openblack/sim-runs/desync-soak"
		alive() { powershell -NoProfile -Command "if (Get-Process -Id $1 -ErrorAction SilentlyContinue) { exit 0 } else { exit 1 }" >/dev/null 2>&1; }
		;;
	esac
	[ -x "$PY" ] || command -v "$PY" >/dev/null || { echo "desync_soak: no Python $PY (the Mac needs cache/simvenv)"; exit 2; }
	[ -f "$BIN/openblack$EXE" ] && [ -f "$BIN/openblack-server$EXE" ] || { echo "desync_soak: no openblack and openblack-server in $BIN"; exit 2; }
	mkdir -p "$OUT"
	local lock="$OUT/soak.lock"
	if [ -f "$lock" ] && alive "$(cat "$lock")"; then
		echo "desync_soak: another soak runs (pid $(cat "$lock"))"
		exit 3
	fi
	echo $$ >"$lock"
	trap 'rm -f "$lock"' EXIT
	local day="$OUT/$DATE"
	mkdir -p "$day/server-check"
	echo "desync_soak: tree $TREE at $(git -C "$TREE" rev-parse --short HEAD 2>/dev/null), programs $BIN, results $day"
	local code=0
	"$PY" "$TREE/tools/sim/ai_soak.py" --lands "$LANDS" --seeds "$SEEDS" --ai-seeds "$AI_SEEDS" --modes all-computer \
		--observers "$OBSERVERS" --turns "$TURNS" --shuffle-seed "$SHUFFLE" --budget-minutes "$MINUTES" \
		--bin "$BIN" --out "$day" --date ai-soak --nightly "$day/ai-soak.jsonl" >"$day/ai-soak.log" 2>&1 || code=1
	# server_check of each land with computer gods: the game alone in the fast mode, then the server at its own pace
	local sc_rows="$day/server-check.tsv"
	: >"$sc_rows"
	if [ "$SERVER_TURNS" -gt 0 ]; then
		local land seed ai args
		for land in ${LANDS//,/ }; do
			args=$(server_land "$land") || continue
			seed=${SEEDS%%,*}
			ai=${AI_SEEDS%%,*}
			local sc="$day/server-check/$land-s$seed-ai$ai"
			eval "\"$PY\" \"$TREE/tools/sim/server_check.py\" $args --seed $seed --all-computer-gods --ai-seed $ai \
				--turns $SERVER_TURNS --bin \"$BIN\" --out \"$sc\" --hash-parts" >"$sc.log" 2>&1
			local rc=$?
			local verdict
			verdict=$("$PY" -c 'import json,sys
try:
    s = json.load(open(sys.argv[1]))
except Exception:
    print("no result (see the log)"); sys.exit()
first = s["different"][0] if s.get("different") else None
compared, until = s.get("compared"), s.get("same_until")
print(("same" if s.get("same") else "PART") + f", compared {compared}, same until {until}"
      + (f", first different turn {first}" if first is not None else ""))' "$sc/server_check.json")
			[ $rc = 0 ] || code=1
			printf '%s\t%s\t%s\t%s\t%s\t%s\n' "$land" "$seed" "$ai" "$SERVER_TURNS" "$rc" "$verdict" >>"$sc_rows"
		done
	fi
	{
		echo "# Desync soak $DATE"
		echo
		echo "Tree $(git -C "$TREE" rev-parse HEAD 2>/dev/null), programs $BIN, $(uname -s)."
		echo
		echo "## Network games (ai_soak.py, all computer gods, $OBSERVERS observers)"
		echo
		"$PY" "$TREE/tools/sim/ai_soak.py" --table "$day/ai-soak.jsonl"
		echo
		echo "## server_check (game against the server, all computer gods)"
		echo
		echo "| Land | Seed | AI seed | Turns | Exit | Result |"
		echo "|---|---|---|---|---|---|"
		while IFS=$'\t' read -r l s a t r v; do echo "| $l | $s | $a | $t | $r | $v |"; done <"$sc_rows"
	} >"$day/report.md"
	cat "$day/report.md"
	exit $code
}

run_mac() {
	local tool=${OB_MAC_TOOL:-C:/projects/obtools/mac/ob-mac.sh}
	local key=${OB_MAC_KEY:-E:/openblack/mac/ssh/id_ed25519} known=${OB_MAC_KNOWN:-E:/openblack/mac/ssh/known_hosts}
	local host=${OB_MAC_HOST:-gavxn@192.168.1.64}
	local ssh_opts=(-i "$key" -o "UserKnownHostsFile=$known" -o IdentitiesOnly=yes -o BatchMode=yes -o ServerAliveInterval=30)
	export OB_SRC="$TREE"
	bash "$tool" sync "$COMMIT" --name "$NAME" || { echo "desync_soak: the sync to the Mac failed"; exit 2; }
	bash "$tool" build --name "$NAME" openblack openblack-server || { echo "desync_soak: the Mac build failed"; exit 2; }
	local remote_out='$HOME/projects/openblack/sim-runs/desync-soak/'"$NAME"
	local quoted
	quoted=$(printf '%q ' "${PASS[@]}")
	# The soak runs on the Mac with nohup, so that a lost connection does not stop it; the script waits for its done file
	ssh "${ssh_opts[@]}" "$host" "bash -s" <<EOF || { echo "desync_soak: the soak did not start on the Mac"; exit 2; }
set -e
T=\$HOME/projects/openblack/trees/$NAME
O=$remote_out
mkdir -p \$O/$DATE
rm -f \$O/$DATE/done
nohup bash -c "bash \$T/tools/sim/desync_soak.sh local --out \$O $quoted > \$O/$DATE/soak.log 2>&1; echo \\\$? > \$O/$DATE/done" >/dev/null 2>&1 </dev/null &
echo "desync_soak: the soak runs on the Mac (pid \$!), log \$O/$DATE/soak.log"
EOF
	local limit=$(( $(awk -v h="$HOURS" 'BEGIN{printf "%d", h*3600}') + 4 * 3600 )) waited=0
	while ! ssh "${ssh_opts[@]}" "$host" "test -f $remote_out/$DATE/done" 2>/dev/null; do
		[ $waited -ge $limit ] && { echo "desync_soak: no result after $waited s; the soak may still run on the Mac"; exit 1; }
		sleep 300
		waited=$((waited + 300))
	done
	local local_out=${OUT:-E:/openblack/sim-runs/desync-soak/$NAME}
	mkdir -p "$local_out/$DATE"
	# The report, the summaries and the logs only: rsync is not on every PC, so tar on the Mac without the programs
	ssh "${ssh_opts[@]}" "$host" "cd $remote_out && tar czf - --exclude='*/bin' --exclude='*.jsonl.gz' $DATE" |
		tar xzf - -C "$local_out"
	cat "$local_out/$DATE/report.md" 2>/dev/null
	return "$(cat "$local_out/$DATE/done" 2>/dev/null || echo 1)"
}

case "$MODE" in
local) run_local ;;
mac) run_mac ;;
*) sed -n 2,30p "$0"; exit 2 ;;
esac
