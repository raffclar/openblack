#!/usr/bin/env bash
# The desync soak: long network games of computer gods (tools/sim/ai_soak.py, which runs net_check.py) and server_check
# runs of the same lands, with one command. Every observer and the game must stay in step with the server at every turn.
#
#   bash tools/sim/desync_soak.sh mac   [options]   from the PC: copy a commit to the Mac build box, build it there, soak
#                                                   there, and fetch the results to the PC
#   bash tools/sim/desync_soak.sh local [options]   soak on this computer (the PC or the Mac) with a build that exists
#   bash tools/sim/desync_soak.sh maps  [options]   the soak of every map for the merge train: as mac, with every land
#                                                   of ai_soak.py (--maps), all computer gods, 2 observers, 20000 turns,
#                                                   seed 1, AI seed 1, the last observer parts on purpose at a random
#                                                   turn and comes back, no server_check; it prints one line for each
#                                                   land at the end (same or PART, turns, first different turn, parts)
#   bash tools/sim/desync_soak.sh cross --out DAY --pc-bin DIR
#                                                   play the server_checks of a fetched Mac soak (DAY is its date
#                                                   folder on the PC) again on the PC and compare the checksums
#
# Options (all optional):
#   --commit REF        mac: the commit to soak (default HEAD of this tree)
#   --name N            mac: the name of the tree and build on the Mac (default desyncsoak)
#   --bin DIR           local: the folder of openblack and openblack-server (default the build of this tree)
#   --hours H           start no new game after H hours (default 8)
#   --lands L           the lands, divided by commas (default TwoGods,mpm_2p_1,mpm_3p_1,mpm_4p_1)
#   --seeds S           the seeds of the world (default 1,2,3)
#   --ai-seeds A        the AI seeds (default 1,2; 0 is the built-in personalities)
#   --modes M           all-computer, all-but-one or both, divided by commas (default all-computer; all-but-one has one
#                       human client with a still hand)
#   --part-observer     the world of the last observer of each game parts on purpose at a random turn (from the shuffle
#                       seed) and must come back in step from the server's world
#   --pc-bin DIR        mac: after the soak, play each server_check of the Mac again on the PC with these programs and
#                       compare the server checksums of the two computers turn by turn (checksum_compare.py)
#   --observers O       the desktop observers of each game (default 2)
#   --turns T           the turns of each game (default 20000)
#   --server-turns T    the turns of each server_check run (default 5000; 0 for no server_check)
#   --shuffle-seed N    the order of the games (default the day of the year, so that each night covers other games)
#   --date D            the name of the folder of the results in --out (default the date of today)
#   --out DIR           the results (default <runs>/desync-soak; the Mac uses ~/projects/openblack/sim-runs)
#
# Results: <out>/<date>/ai-soak (a folder for each game and summary.jsonl), <out>/<date>/server-check/<land>-s<seed>-ai<n>
# and <out>/<date>/report.md (the table of every game and every server_check). The mac mode copies the report, the
# summaries and the logs (not the programs) to E:/openblack/sim-runs/desync-soak/<name>/<date> on the PC.
#
# The load rules: ai_soak.py starts a game only when the computer allows it (the PC: processor under 75 %, 16 GB free
# memory, 150 GB free disk; the Mac: processor under 75 %, 6 GB free memory, 30 GB free disk, at most 4 game programs of
# all lanes with the new game's). Only one soak runs on a computer at a time (a lock file with the process id). The script
# stops only the processes that it started, by their process ids. Every game runs with --background and with no sound (ALSOFT_CONF, the null driver of OpenAL).
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
MODES=all-computer
PC_BIN=""
OBSERVERS=2
TURNS=20000
SERVER_TURNS=5000
SHUFFLE=$((10#$(date +%j)))
OUT=""
DATE=$(date +%Y-%m-%d)
PART_OBSERVER=""
LANDS_OPTION=(--lands "$LANDS")
if [ "$MODE" = maps ]; then
	# The soak of every map: one game of each land, long, with a part on purpose of an observer
	NAME=mpsoakmaps
	HOURS=24
	SEEDS=1
	AI_SEEDS=1
	SERVER_TURNS=0
	PART_OBSERVER=1
	LANDS_OPTION=(--maps)
fi
while [ $# -gt 0 ]; do
	case "$1" in
	--commit) COMMIT=$2; shift 2 ;;
	--name) NAME=$2; shift 2 ;;
	--bin) BIN=$2; shift 2 ;;
	--hours) HOURS=$2; shift 2 ;;
	--lands) LANDS=$2; LANDS_OPTION=(--lands "$LANDS"); shift 2 ;;
	--part-observer) PART_OBSERVER=1; shift ;;
	--seeds) SEEDS=$2; shift 2 ;;
	--ai-seeds) AI_SEEDS=$2; shift 2 ;;
	--modes) MODES=$2; shift 2 ;;
	--pc-bin) PC_BIN=$2; shift 2 ;;
	--observers) OBSERVERS=$2; shift 2 ;;
	--turns) TURNS=$2; shift 2 ;;
	--server-turns) SERVER_TURNS=$2; shift 2 ;;
	--shuffle-seed) SHUFFLE=$2; shift 2 ;;
	--out) OUT=$2; shift 2 ;;
	--date) DATE=$2; shift 2 ;;
	*) echo "desync_soak: unknown option $1"; exit 2 ;;
	esac
done
# The options that the mac mode gives to the local mode on the Mac
PASS=(--hours "$HOURS" "${LANDS_OPTION[@]}" --seeds "$SEEDS" --ai-seeds "$AI_SEEDS" --modes "$MODES" --observers "$OBSERVERS" --turns "$TURNS"
	--server-turns "$SERVER_TURNS" --shuffle-seed "$SHUFFLE" --date "$DATE")
[ -z "$PART_OBSERVER" ] || PASS+=(--part-observer)
PART_ARGS=()
[ -z "$PART_OBSERVER" ] || PART_ARGS=(--part-observer)
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
		# The sim tools use the Python of the lanes (pyenv 3.14.8) in their own environment; the older 3.9 one is the
		# fall-back
		PY=${OB_SIM_PYTHON:-$HOME/projects/openblack/cache/simvenv314/bin/python}
		[ -n "${OB_SIM_PYTHON:-}" ] || [ -x "$PY" ] || PY=$HOME/projects/openblack/cache/simvenv/bin/python
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
	# The games of a soak play no sound: OpenAL gets its null driver from a configuration file of this soak (no system
	# setting changes). ALSOFT_CONF from the caller is kept.
	if [ -z "${ALSOFT_CONF:-}" ]; then
		if [ "$EXE" = ".exe" ] && [ -f E:/openblack/tools/audio/alsoft_silent.ini ]; then
			export ALSOFT_CONF=E:/openblack/tools/audio/alsoft_silent.ini
		else
			printf '[general]\ndrivers=null\n' >"$OUT/alsoft-silent.conf"
			export ALSOFT_CONF="$OUT/alsoft-silent.conf"
		fi
	fi
	echo "desync_soak: the games play no sound (ALSOFT_CONF=$ALSOFT_CONF)"
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
	"$PY" "$TREE/tools/sim/ai_soak.py" "${LANDS_OPTION[@]}" "${PART_ARGS[@]}" --seeds "$SEEDS" --ai-seeds "$AI_SEEDS" --modes "$MODES" \
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
		echo
		echo "## One line for each game"
		echo
		echo '```'
		"$PY" "$TREE/tools/sim/ai_soak.py" --lines "$day/ai-soak.jsonl"
		echo '```'
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
	check_pc_bin || exit 2
	# The server_check games of the PC (--pc-bin) play no sound either
	export ALSOFT_CONF=${ALSOFT_CONF:-E:/openblack/tools/audio/alsoft_silent.ini}
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
	ssh "${ssh_opts[@]}" "$host" "cd $remote_out && COPYFILE_DISABLE=1 tar --no-xattrs -czf - --exclude='*/bin' --exclude='*.jsonl.gz' $DATE" |
		tar xzf - -C "$local_out"
	local code
	code=$(cat "$local_out/$DATE/done" 2>/dev/null || echo 1)
	if [ -n "$PC_BIN" ]; then
		cross_check "$local_out/$DATE" || code=1
	fi
	cat "$local_out/$DATE/report.md" 2>/dev/null
	if [ "$MODE" = maps ]; then
		echo
		python "$TREE/tools/sim/ai_soak.py" --lines "$local_out/$DATE/ai-soak.jsonl"
	fi
	return "$code"
}

# Stops before the soak when the PC programs of --pc-bin are not there
check_pc_bin() {
	[ -z "$PC_BIN" ] && return 0
	[ -f "$PC_BIN/openblack.exe" ] && [ -f "$PC_BIN/openblack-server.exe" ] && return 0
	echo "desync_soak: no openblack.exe and openblack-server.exe in $PC_BIN (--pc-bin)"
	return 1
}

# Plays each server_check of the Mac again on the PC and compares the server checksums of the two computers. The two
# builds have other processors and compilers; the world must still turn the same.
cross_check() {
	local day=$1 folder land seed ai args rc
	{
		echo
		echo "## The Mac against the PC (server checksums of each server_check, PC programs $PC_BIN)"
		echo
		echo "| Land | Seed | AI seed | Result |"
		echo "|---|---|---|---|"
	} >>"$day/report.md"
	rc=0
	for folder in "$day"/server-check/*/; do
		[ -f "$folder/server/server.log" ] || continue
		local name
		name=$(basename "$folder")
		land=${name%-s*}
		seed=${name##*-s}; seed=${seed%%-*}
		ai=${name##*-ai}
		args=$(server_land "$land") || continue
		local pc="$day/pc-server-check/$name"
		mkdir -p "$pc"
		eval "python \"$TREE/tools/sim/server_check.py\" $args --seed $seed --all-computer-gods --ai-seed $ai \
			--turns $SERVER_TURNS --bin \"$PC_BIN\" --out \"$pc\"" >"$pc.log" 2>&1
		local result
		if [ -f "$pc/server/server.log" ]; then
			result=$(python "$TREE/tools/sim/checksum_compare.py" "$folder/server/server.log" "$pc/server/server.log") || rc=1
		else
			result="no PC result (see $pc.log)"
			rc=1
		fi
		echo "| $land | $seed | $ai | $result |" >>"$day/report.md"
	done
	return $rc
}

case "$MODE" in
local) run_local ;;
mac | maps) run_mac ;;
cross)
	[ -n "$PC_BIN" ] && [ -n "$OUT" ] && [ -d "$OUT/server-check" ] || { echo "desync_soak: cross needs --out DAY and --pc-bin"; exit 2; }
	check_pc_bin || exit 2
	export ALSOFT_CONF=${ALSOFT_CONF:-E:/openblack/tools/audio/alsoft_silent.ini}
	cross_check "$OUT"
	;;
*) sed -n 2,30p "$0"; exit 2 ;;
esac
