// Checks the mouse and keyboard input of the web page in headless Chrome, as a user gives it. It runs cdp-run.mjs two
// times on the page and then checks what the game did:
// 1. A first start: the new player's name box. Moves of the mouse, two keys, a click that locks the pointer, moves of
//    the locked pointer onto the OK button and a click on it. The game must make the player profile with the name.
// 2. The testbed: a click locks the pointer, a drag with the left button moves the land, the first Escape frees the
//    pointer, the second Escape opens the game menu, and a click locks the pointer again.
//
// Usage: node mouse-check.mjs <chrome.exe> <page url> <game folder> <output folder> [--renderer webgpu|webgl2]
//   The page url is the address of index.html, for example http://127.0.0.1:8641/index.html. The game data stays on
//   this computer: the page reads it from the game folder.
// The exit code is 0 when all the checks pass.
//
// The game can be seconds behind the page on a busy computer: it does the events of the page only between two of its
// frames, and a frame can be slow. Thus no step waits for a time. Each step that needs the game to be at some state
// waits for a sign of that state: a line of the game log, the picture of the game pointer where it must be, or new
// pictures of the game. The times of the steps only keep the steps in order, and the run does not end before the last
// step is done.
import { spawnSync } from 'node:child_process';
import { existsSync, readFileSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { readPng } from './png.mjs';

const [chromePath, pageUrl, gameFolder, out, ...rest] = process.argv.slice(2);
if (!chromePath || !pageUrl || !gameFolder || !out) {
  console.log('usage: node mouse-check.mjs <chrome.exe> <page url> <game folder> <output folder> [--renderer name]');
  process.exit(2);
}
const at = rest.indexOf('--renderer');
const renderer = at >= 0 ? rest[at + 1] : 'webgpu';
const here = path.dirname(fileURLToPath(import.meta.url));

// The window is 1280x1024, the default size of the game. The pointer speed of the game at this size is 0.8 of the
// movement of the mouse (see src/Input/PointerSpeed.h).
const k_PointerScale = 0.8;
const k_LockAt = [640, 330];
// The OK button of the name box at 1280x1024
const k_NameBoxOk = [290, 700];
// Where the picture of the game pointer is when its point is on the OK button: the middle of the picture is a little
// right of and below the point
const k_PointerPictureOffset = [35, 12];
const k_MoveSteps = 25;
const k_Name = 'ab';

const nameBoxSteps = [
  { at: 1, do: 'wait-log', text: 'asking the new player for their name', seconds: 240 },
  { at: 5, do: 'mouse', type: 'mouseMoved', x: 600, y: 300 },
  { at: 5.3, do: 'mouse', type: 'mouseMoved', x: k_LockAt[0], y: k_LockAt[1] },
  ...[...k_Name].map((key, i) => ({ at: 6 + i * 0.5, do: 'key', key, code: `Key${key.toUpperCase()}`,
    keyCode: key.toUpperCase().charCodeAt(0), hold: 150 })),
  { at: 7.5, do: 'mouse', type: 'mousePressed', x: k_LockAt[0], y: k_LockAt[1], button: 'left' },
  { at: 7.7, do: 'mouse', type: 'mouseReleased', x: k_LockAt[0], y: k_LockAt[1], button: 'left' },
  // The game logs the lock when it reads the first event after the lock. It reads the events in the order of the page,
  // so the keys and the first moves are done by then.
  { at: 8, do: 'wait-log', text: 'The page locked the pointer', seconds: 240 },
  { at: 8.5, do: 'eval', expression: 'document.pointerLockElement?.id ?? "none"' },
  { at: 9, do: 'synth-move', dx: (k_NameBoxOk[0] - k_LockAt[0]) / k_PointerScale / k_MoveSteps,
    dy: (k_NameBoxOk[1] - k_LockAt[1]) / k_PointerScale / k_MoveSteps, count: k_MoveSteps, ms: 1500 },
  // The game draws its pointer from the place that the moves gave it
  { at: 10.5, do: 'wait-pointer', x: k_NameBoxOk[0] + k_PointerPictureOffset[0],
    y: k_NameBoxOk[1] + k_PointerPictureOffset[1], within: 30, seconds: 240 },
  { at: 11, do: 'shot', name: 'name-over-ok' },
  { at: 11.5, do: 'synth-button', type: 'mousedown', button: 0 },
  // The button stays down for some frames of the game, as a click of a user does
  { at: 11.6, do: 'wait-frames', count: 3, seconds: 120 },
  { at: 12, do: 'synth-button', type: 'mouseup', button: 0 },
  { at: 12.5, do: 'wait-log', text: `Made the player profile "${k_Name}"`, seconds: 120 },
];

const testbedSteps = [
  // The loading screen is over when the game has drawn its first frame
  { at: 2, do: 'wait-log', text: 'Load startup: first frame', seconds: 240 },
  { at: 3, do: 'wait-frames', count: 3, seconds: 120 },
  { at: 3.5, do: 'mouse', type: 'mouseMoved', x: 640, y: 512 },
  { at: 3.8, do: 'mouse', type: 'mousePressed', x: 640, y: 512, button: 'left' },
  { at: 3.9, do: 'mouse', type: 'mouseReleased', x: 640, y: 512, button: 'left' },
  { at: 4.5, do: 'wait-log', text: 'The page locked the pointer', seconds: 240 },
  { at: 5, do: 'eval', expression: 'document.pointerLockElement?.id ?? "none"' },
  { at: 5.5, do: 'wait-frames', count: 3, seconds: 120 },
  { at: 6, do: 'shot', name: 'still-1' },
  { at: 6.6, do: 'shot', name: 'still-2' },
  { at: 7, do: 'synth-button', type: 'mousedown', button: 0 },
  { at: 7.1, do: 'synth-move', dx: -20, dy: 0, count: 30, ms: 900, buttons: 1 },
  { at: 8.1, do: 'synth-button', type: 'mouseup', button: 0 },
  // The game has done the drag when it has drawn some frames after it
  { at: 8.5, do: 'wait-frames', count: 4, seconds: 120 },
  { at: 9, do: 'shot', name: 'dragged' },
  { at: 9.5, do: 'key', key: 'Escape', code: 'Escape', keyCode: 27 },
  { at: 10, do: 'eval', expression: 'document.pointerLockElement?.id ?? "none"' },
  { at: 10.5, do: 'wait-log', text: 'Escape freed the pointer', seconds: 240 },
  { at: 11, do: 'wait-frames', count: 3, seconds: 120 },
  { at: 11.5, do: 'shot', name: 'escape-1' },
  { at: 12, do: 'key', key: 'Escape', code: 'Escape', keyCode: 27 },
  { at: 12.5, do: 'wait-frames', count: 5, seconds: 30 },
  { at: 13, do: 'shot', name: 'escape-2' },
  { at: 13.5, do: 'key', key: 'Escape', code: 'Escape', keyCode: 27 },
  { at: 14, do: 'wait-frames', count: 3, seconds: 30 },
  { at: 14.5, do: 'mouse', type: 'mousePressed', x: 640, y: 512, button: 'left' },
  { at: 14.6, do: 'mouse', type: 'mouseReleased', x: 640, y: 512, button: 'left' },
  { at: 15, do: 'eval', expression: 'document.pointerLockElement?.id ?? "none"' },
  { at: 15.5, do: 'wait-log', text: 'The page locked the pointer', count: 2, seconds: 240 },
];

const run = (name, args, steps, seconds) => {
  const folder = path.join(out, `${renderer}-${name}`);
  const actions = path.join(out, `${renderer}-${name}-actions.json`);
  writeFileSync(actions, JSON.stringify(steps, null, 1));
  const query = `renderer=${renderer}&args=${encodeURIComponent(args)}`;
  const url = `${pageUrl}${pageUrl.includes('?') ? '&' : '?'}${query}`;
  const result = spawnSync(process.execPath, [path.join(here, 'cdp-run.mjs'), chromePath, url, String(seconds), folder,
    '--game-folder', gameFolder, '--click', '#play', '--gpu', '--run-all-steps', '--actions', actions], { encoding: 'utf8', timeout: 600000 });
  if (result.status !== 0) {
    console.log(`${name}: cdp-run failed (${result.status}): ${result.stdout}${result.stderr}`);
  }
  const read = (file) => (existsSync(path.join(folder, file)) ? readFileSync(path.join(folder, file), 'utf8') : '');
  return { folder, console: read('console.log'), log: read('page-log.txt') };
};

// The values of the eval steps in their order
const evals = (text) => [...text.matchAll(/\[eval\] .* = (.*)$/gm)].map((m) => JSON.parse(m[1]));
const count = (text, part) => text.split(part).length - 1;

// The share of the pixels whose colour changed by more than 24 in one channel
function changedShare(folder, a, b) {
  const first = path.join(folder, `${a}.png`);
  const second = path.join(folder, `${b}.png`);
  if (!existsSync(first) || !existsSync(second)) {
    return -1;
  }
  const x = readPng(first);
  const y = readPng(second);
  if (x.width !== y.width || x.height !== y.height) {
    return 1;
  }
  let changed = 0;
  for (let i = 0; i < x.width * x.height; ++i) {
    for (let c = 0; c < 3; ++c) {
      if (Math.abs(x.pixels[i * x.channels + c] - y.pixels[i * y.channels + c]) > 24) {
        ++changed;
        break;
      }
    }
  }
  return changed / (x.width * x.height);
}

const results = [];
const check = (name, pass, detail) => {
  results.push({ name, pass, detail });
  console.log(`${pass ? 'PASS' : 'FAIL'} ${renderer} ${name}${detail ? `: ${detail}` : ''}`);
};

const box = run('name-box', '--skip-logos --profile-folder /work/mouse-check', nameBoxSteps, 14);
const boxEvals = evals(box.console);
check('name box shown', box.console.includes('[wait-log] found: asking the new player'));
check('click locks the pointer', boxEvals[0] === 'canvas', `lock element ${boxEvals[0]}`);
check('game follows the lock',
  box.log.includes(`The page locked the pointer: the game moves its own pointer from ${k_LockAt[0]}, ${k_LockAt[1]}`));
// What the steps saw of the game: when it read the lock, where its pointer was, and how many frames it drew
const waits = box.console.split(/\r?\n/).filter((line) => /\[wait-(pointer|frames)\]/.test(line))
  .map((line) => line.replace(/^\S+ \[wait-/, '').replace('] ', ' ')).join('; ');
check('keys, locked moves and a locked click answer the name box',
  box.log.includes(`Made the player profile "${k_Name}"`),
  `typed "${k_Name}", ${box.log.includes('Made the player profile') ? 'profile made' : 'no profile made'}; ${waits}`);

const bed = run('testbed', '--skip-logos --testbed', testbedSteps, 23);
const bedEvals = evals(bed.console);
check('testbed: click locks the pointer', bedEvals[0] === 'canvas' && bed.log.includes('The page locked the pointer'),
  `lock element ${bedEvals[0]}`);
const still = changedShare(bed.folder, 'still-1', 'still-2');
const drag = changedShare(bed.folder, 'still-2', 'dragged');
check('left drag moves the land', drag > 0.1 && drag > 5 * still,
  `changed ${(drag * 100).toFixed(1)}% after the drag, ${(still * 100).toFixed(1)}% with no input`);
check('first Escape frees the pointer', bedEvals[1] === 'none' && count(bed.log, 'Input: the pointer is free') >= 1,
  `lock element ${bedEvals[1]}`);
const menu = changedShare(bed.folder, 'escape-1', 'escape-2');
check('second Escape opens the menu', menu > 0.05 && menu > 3 * still,
  `changed ${(menu * 100).toFixed(1)}% after the second Escape`);
check('a click locks the pointer again', bedEvals[2] === 'canvas' && count(bed.log, 'Input: the game has the pointer') >= 2,
  `lock element ${bedEvals[2]}`);

const failed = results.filter((r) => !r.pass).length;
writeFileSync(path.join(out, `${renderer}-results.json`), JSON.stringify(results, null, 1));
console.log(`${renderer}: ${results.length - failed} of ${results.length} checks pass`);
process.exit(failed === 0 ? 0 : 1);
