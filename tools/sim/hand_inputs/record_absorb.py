"""Records absorb.json: the game plays the testbed scenario miracles.recorded_absorb at its own pace, and this script puts
the pointer on each fireball and clicks the Action button on it, three times. The game records the input from its first
land frame, and server_check.py --input makes it again frame for frame.

    python tools/sim/hand_inputs/record_absorb.py tools/sim/hand_inputs/absorb.json 300 <a file with one pointer event,
        [640, 900] at frame 0> [--config Debug]
"""
import os
import subprocess
import sys
import tempfile
import time

W = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, W + '/tools/inspector')
import openblack_inspector_mcp as inspector  # noqa: E402

out = sys.argv[1]
turns = int(sys.argv[2]) if len(sys.argv) > 2 else 300
config = sys.argv[sys.argv.index('--config') + 1] if '--config' in sys.argv else 'Debug'
exe = os.path.join('E:/openblack/builds', os.path.basename(W), 'bin', config, 'openblack.exe')
# The game's working folder, with its log and profile, is a new folder for each run
cwd = tempfile.mkdtemp(prefix='record_absorb_')
args = [exe, '-g', 'C:/projects/black', '--skip-logos', '--seed', '1', '--sim-draw-every', '1', '--sim-frame-ms', '20',
        '--sim-until-turn', str(turns + 1), '--inspect-lock-input', '-n', '4000000000', '--background', '-l',
        os.path.join(cwd, 'game.log'), '--sim-network-game', '--scenario', 'miracles.recorded_absorb',
        '--scenario-hide-window', '--sim-input', os.path.abspath(sys.argv[3]), '--inspect-port', '0',
        '--sim-record-input', os.path.abspath(out), '--sim-record', os.path.join(cwd, 'record.jsonl'), '--sim-record-every', '1']
process = subprocess.Popen(args, cwd=cwd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                           creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
print('pid', process.pid, flush=True)
connection = None
deadline = time.monotonic() + 120
while connection is None and time.monotonic() < deadline:
    games = [g for g in inspector.read_games(inspector.discovery_folder()) if g.get('pid') == process.pid]
    if games:
        connection = inspector.InspectorConnection(games[0], timeout=30.0)
    time.sleep(0.5)


def call(query, params=None):
    answer = connection.request({'query': query, 'params': params or {}})
    return answer.get('result') if answer.get('ok') else None


clicked = set()
presses = 0
try:
    while process.poll() is None and presses < 3:
        result = call('ecs.entities', {'component': 'MagicFireBall'})
        if isinstance(result, dict):
            result = result.get('items') or result.get('entities') or []
        if result and not clicked:
            print('answer', str(result)[:300], flush=True)
        for entity in result or []:
            ident = entity.get('id')
            place = entity.get('position') or entity.get('place')
            if ident in clicked or place is None:
                continue
            # Only a ball a little way up
            if place[1] < 9.0:
                continue
            call('input.pointer', {'world': place})
            call('input.button', {'button': 'right', 'action': 'click'})
            clicked.add(ident)
            presses += 1
            print('click', ident, place, flush=True)
        time.sleep(0.05)
    process.wait(timeout=900)
finally:
    if process.poll() is None:
        process.kill()
print('exit', process.returncode)
