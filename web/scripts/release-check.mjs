// Checks that the web game lets go of a mouse button when the browser does not give its release. It runs cdp-run.mjs
// one time on the testbed and then checks what the game did. In each case the left button drags the land, then the
// release does not come in the usual way, then the mouse moves with no button down. The land must not move then.
// 1. The page loses the focus during the drag (as at Alt+Tab).
// 2. The page goes hidden during the drag.
// 3. The release goes to another window: the next movement shows the button as up.
// 4. A control: the left and the right button go down and up as usual.
// After case 1 the run also presses the Up arrow key and moves the mouse again, to show what the key does to a drag
// that stays on.
//
// Usage: node release-check.mjs <chrome.exe> <page url> <game folder> <output folder> [--renderer webgpu|webgl2]
//        [--report-only]
//   With --report-only the run prints the results and its exit code is 0. Use it on a page without the fix.
// The exit code is 0 when all the checks pass.
import { spawnSync } from 'node:child_process';
import { existsSync, readFileSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { changedShare } from './png.mjs';

const [chromePath, pageUrl, gameFolder, out, ...rest] = process.argv.slice(2);
if (!chromePath || !pageUrl || !gameFolder || !out) {
  console.log('usage: node release-check.mjs <chrome.exe> <page url> <game folder> <output folder> [--renderer name]');
  process.exit(2);
}
const at = rest.indexOf('--renderer');
const renderer = at >= 0 ? rest[at + 1] : 'webgpu';
const reportOnly = rest.includes('--report-only');
const here = path.dirname(fileURLToPath(import.meta.url));

// The page movements of one drag: 20 steps of 15 CSS pixels to the left, and the movements after a lost release go
// back to the right. Thus the game pointer stays near the middle of the canvas.
const drag = (time) => [
  { at: time, do: 'synth-button', type: 'mousedown', button: 0 },
  { at: time + 0.1, do: 'synth-move', dx: -15, dy: 0, count: 20, ms: 600, buttons: 1 },
];
const look = (time, name, dx = 15) => [
  { at: time, do: 'wait-frames', count: 4, seconds: 120 },
  { at: time + 0.1, do: 'shot', name: `${name}-0` },
  { at: time + 0.2, do: 'synth-move', dx, dy: 0, count: 20, ms: 600, buttons: 0 },
  { at: time + 0.9, do: 'wait-frames', count: 4, seconds: 120 },
  { at: time + 1, do: 'shot', name: `${name}-1` },
];
// The end of a case: a release, as the player clicks again, so that the next case starts with no button down
const finish = (time) => [
  { at: time, do: 'synth-button', type: 'mouseup', button: 0 },
  { at: time + 0.1, do: 'wait-frames', count: 3, seconds: 120 },
];
const hide = `(() => {
  Object.defineProperty(document, 'visibilityState', { configurable: true, get: () => 'hidden' });
  document.dispatchEvent(new Event('visibilitychange'));
  delete document.visibilityState;
  document.dispatchEvent(new Event('visibilitychange'));
  return document.visibilityState;
})()`;

const steps = [
  { at: 2, do: 'wait-log', text: 'Load startup: first frame', seconds: 240 },
  { at: 3, do: 'wait-frames', count: 3, seconds: 120 },
  { at: 3.5, do: 'mouse', type: 'mouseMoved', x: 640, y: 512 },
  { at: 3.8, do: 'mouse', type: 'mousePressed', x: 640, y: 512, button: 'left' },
  { at: 3.9, do: 'mouse', type: 'mouseReleased', x: 640, y: 512, button: 'left' },
  { at: 4.5, do: 'wait-log', text: 'The page locked the pointer', seconds: 240 },
  { at: 5, do: 'wait-frames', count: 3, seconds: 120 },
  { at: 5.5, do: 'shot', name: 'still-1' },
  { at: 6, do: 'shot', name: 'still-2' },
  // A usual drag, to know how much of the picture a drag changes
  ...drag(6.5),
  { at: 7.2, do: 'synth-button', type: 'mouseup', button: 0 },
  { at: 7.3, do: 'wait-frames', count: 4, seconds: 120 },
  { at: 7.4, do: 'shot', name: 'dragged' },
  { at: 7.5, do: 'synth-move', dx: 15, dy: 0, count: 20, ms: 600, buttons: 0 },
  { at: 8.2, do: 'wait-frames', count: 4, seconds: 120 },
  { at: 8.3, do: 'shot', name: 'moved' },
  // 1. The focus goes, then comes back
  ...drag(9),
  { at: 9.8, do: 'eval', expression: "(window.dispatchEvent(new Event('blur')), window.dispatchEvent(new Event('focus')), 'blur')" },
  ...look(10, 'blur'),
  { at: 11.2, do: 'key', key: 'ArrowUp', code: 'ArrowUp', keyCode: 38 },
  ...look(11.5, 'arrow', -15),
  ...finish(12.7),
  // 2. The page goes hidden, then visible again
  ...drag(13),
  { at: 13.8, do: 'eval', expression: hide },
  ...look(14, 'hidden'),
  ...finish(15.2),
  // 3. The release goes elsewhere
  ...drag(15.5),
  ...look(16.3, 'lost'),
  ...finish(17.5),
  // 4. Both buttons go down and up as usual
  { at: 18, do: 'synth-button', type: 'mousedown', button: 0 },
  { at: 18.1, do: 'synth-button', type: 'mousedown', button: 2 },
  { at: 18.2, do: 'synth-move', dx: -15, dy: 0, count: 10, ms: 300, buttons: 3 },
  { at: 18.6, do: 'synth-button', type: 'mouseup', button: 2 },
  { at: 18.7, do: 'synth-button', type: 'mouseup', button: 0 },
  ...look(18.8, 'control'),
  { at: 20, do: 'eval', expression: "(globalThis.openblackLogText?.() ?? '').split('the page gives the release').length - 1" },
];

const folder = path.join(out, `${renderer}-release`);
const actions = path.join(out, `${renderer}-release-actions.json`);
writeFileSync(actions, JSON.stringify(steps, null, 1));
const query = `renderer=${renderer}&args=${encodeURIComponent('--skip-logos --testbed')}`;
const url = `${pageUrl}${pageUrl.includes('?') ? '&' : '?'}${query}`;
const result = spawnSync(process.execPath, [path.join(here, 'cdp-run.mjs'), chromePath, url, '22', folder, '--game-folder',
  gameFolder, '--click', '#play', '--gpu', '--virtual-pointer-lock', '--run-all-steps', '--actions', actions],
{ encoding: 'utf8', timeout: 900000 });
if (result.status !== 0) {
  console.log(`cdp-run failed (${result.status}): ${result.stdout}${result.stderr}`);
}

const share = (a, b) => changedShare(folder, a, b);
const still = share('still-1', 'still-2');
const dragged = share('still-2', 'dragged');
// A movement with no button down moves only the hand and the pointer
const moved = share('dragged', 'moved');
// A movement after a lost release that changes much more of the picture than a plain movement is a drag that stays on
const stays = (name) => share(`${name}-0`, `${name}-1`);
const limit = Math.max(3 * moved, (moved + dragged) / 2);
const results = [];
const check = (name, pass, detail) => {
  results.push({ name, pass, detail });
  console.log(`${pass ? 'PASS' : 'FAIL'} ${renderer} ${name}${detail ? `: ${detail}` : ''}`);
};
const percent = (value) => `${(value * 100).toFixed(1)}%`;
check('a usual drag moves the land', dragged > 0.1 && dragged > 5 * still,
  `drag ${percent(dragged)}, plain movement ${percent(moved)}, no input ${percent(still)}`);
for (const [name, text] of [['blur', 'a lost focus ends the drag'], ['hidden', 'a hidden page ends the drag'],
  ['lost', 'a movement with the button up ends the drag'], ['control', 'both buttons up end the drag']]) {
  const value = stays(name);
  // A missing screenshot gives a negative share
  check(text, value >= 0 && value < limit, `changed ${percent(value)} (a drag that stays on changes more than ${percent(limit)})`);
}
// Only a report: what the Up arrow key did to a drag that stayed on after case 1
console.log(`INFO ${renderer} after the Up arrow key a movement changed ${percent(stays('arrow'))}`);
writeFileSync(path.join(out, `${renderer}-release-results.json`), JSON.stringify(results, null, 1));
process.exit(reportOnly || results.every((r) => r.pass) ? 0 : 1);
