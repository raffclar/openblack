// Checks that the game fills the web page and follows a change of the window size, in headless Chrome. It runs
// cdp-run.mjs once on the testbed in a 1280x720 window, changes the window to 1920x1080 and checks:
// - at each size the game canvas fills the window, the page has no scroll bars and the log box takes no space,
// - the game draws at the new size (its log), and its picture fills the new window,
// - after the change, a click takes the pointer at the place of the click, and a drag moves the land,
// - the page shows the build version with a link to its commit.
//
// Usage: node resize-check.mjs <chrome.exe> <page url> <game folder> <output folder> [--renderer webgpu|webgl2]
// The exit code is 0 when all the checks pass.
import { spawnSync } from 'node:child_process';
import { existsSync, readFileSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { changedShare, litShare, pictureSize } from './png.mjs';

const [chromePath, pageUrl, gameFolder, out, ...rest] = process.argv.slice(2);
if (!chromePath || !pageUrl || !gameFolder || !out) {
  console.log('usage: node resize-check.mjs <chrome.exe> <page url> <game folder> <output folder> [--renderer name]');
  process.exit(2);
}
const at = rest.indexOf('--renderer');
const renderer = at >= 0 ? rest[at + 1] : 'webgpu';
const here = path.dirname(fileURLToPath(import.meta.url));

const k_First = [1280, 720];
const k_Second = [1920, 1080];
// The click that takes the pointer after the change, far from the first window's size
const k_Click = [1500, 900];

// What the page looks like: the canvas box, the scroll size, the log box and the build link
const k_Layout = `JSON.stringify({ canvas: (({ x, y, width, height }) => [x, y, width, height])(
  document.getElementById('canvas').getBoundingClientRect()), window: [innerWidth, innerHeight],
  scroll: [document.documentElement.scrollWidth, document.documentElement.scrollHeight],
  log: getComputedStyle(document.getElementById('log-box')).display,
  badge: document.querySelector('#build-badge .build-link')?.textContent ?? '',
  link: document.querySelector('#build-badge .build-link')?.href ?? '' })`;

const steps = [
  { at: 2, do: 'wait-log', text: 'pipelines made', seconds: 240 },
  { at: 10, do: 'eval', expression: k_Layout },
  { at: 10.2, do: 'shot', name: 'first' },
  { at: 11, do: 'resize', width: k_Second[0], height: k_Second[1] },
  { at: 12, do: 'wait-log', text: `The game draws at the new window size ${k_Second[0]}x${k_Second[1]}`, seconds: 30 },
  { at: 15, do: 'eval', expression: k_Layout },
  { at: 15.2, do: 'shot', name: 'second' },
  { at: 16, do: 'mouse', type: 'mouseMoved', x: k_Click[0], y: k_Click[1] },
  { at: 16.3, do: 'mouse', type: 'mousePressed', x: k_Click[0], y: k_Click[1], button: 'left' },
  { at: 16.4, do: 'mouse', type: 'mouseReleased', x: k_Click[0], y: k_Click[1], button: 'left' },
  { at: 17, do: 'eval', expression: 'document.pointerLockElement?.id ?? "none"' },
  { at: 18, do: 'shot', name: 'still-1' },
  { at: 18.6, do: 'shot', name: 'still-2' },
  { at: 19, do: 'synth-button', type: 'mousedown', button: 0 },
  { at: 19.1, do: 'synth-move', dx: -20, dy: 0, count: 30, ms: 900, buttons: 1 },
  { at: 20.1, do: 'synth-button', type: 'mouseup', button: 0 },
  { at: 21, do: 'shot', name: 'dragged' },
  { at: 21.5, do: 'key', key: 'Escape', code: 'Escape', keyCode: 27 },
  { at: 22, do: 'resize', width: k_First[0], height: k_First[1] },
  { at: 23, do: 'wait-log', text: `The game draws at the new window size ${k_First[0]}x${k_First[1]}`, seconds: 30 },
  { at: 25, do: 'shot', name: 'back' },
];

const folder = path.join(out, `${renderer}-resize`);
const actions = path.join(out, `${renderer}-resize-actions.json`);
writeFileSync(actions, JSON.stringify(steps, null, 1));
const query = `renderer=${renderer}&args=${encodeURIComponent('--skip-logos --testbed')}`;
const url = `${pageUrl}${pageUrl.includes('?') ? '&' : '?'}${query}`;
const result = spawnSync(process.execPath, [path.join(here, 'cdp-run.mjs'), chromePath, url, '26', folder,
  '--game-folder', gameFolder, '--click', '#play', '--gpu', '--size', `${k_First[0]}x${k_First[1]}`, '--actions', actions],
{ encoding: 'utf8', timeout: 600000 });
if (result.status !== 0) {
  console.log(`cdp-run failed (${result.status}): ${result.stdout}${result.stderr}`);
}
const read = (file) => (existsSync(path.join(folder, file)) ? readFileSync(path.join(folder, file), 'utf8') : '');
const consoleText = read('console.log');
const log = read('page-log.txt');
const evals = [...consoleText.matchAll(/\[eval\] .* = (.*)$/gm)].map((m) => JSON.parse(m[1]));
const layouts = evals.filter((value) => typeof value === 'string').map((value) => JSON.parse(value));
const locks = evals.filter((value) => value === 'canvas' || value === 'none');

const results = [];
const check = (name, pass, detail) => {
  results.push({ name, pass, detail });
  console.log(`${pass ? 'PASS' : 'FAIL'} ${renderer} ${name}${detail ? `: ${detail}` : ''}`);
};
const fills = (layout, size) => layout !== undefined && JSON.stringify(layout.canvas) === JSON.stringify([0, 0, ...size]) &&
  layout.scroll[0] <= size[0] && layout.scroll[1] <= size[1] && layout.log === 'none';
// The picture fills the window: each corner box of the screenshot has the game's picture in it
const corners = (name, size) => {
  const file = path.join(folder, `${name}.png`);
  const [width, height] = pictureSize(file);
  const box = [Math.floor(size[0] / 4), Math.floor(size[1] / 4)];
  const shares = [[0, 0], [size[0] - box[0], 0], [0, size[1] - box[1]], [size[0] - box[0], size[1] - box[1]]]
    .map(([x, y]) => (width === size[0] && height === size[1] ? litShare(file, [x, y, box[0], box[1]]) : -1));
  return { pass: shares.every((share) => share > 0.2), detail: `${width}x${height}, lit ${shares.map((s) => s.toFixed(2)).join(' ')}` };
};

check('canvas fills the 1280x720 page', fills(layouts[0], k_First), JSON.stringify(layouts[0]));
const first = corners('first', k_First);
check('picture fills the 1280x720 page', first.pass, first.detail);
check('game draws at 1920x1080 after the change', log.includes(`The game draws at the new window size ${k_Second[0]}x${k_Second[1]}`));
check('canvas fills the 1920x1080 page', fills(layouts[1], k_Second), JSON.stringify(layouts[1]));
const second = corners('second', k_Second);
check('picture fills the 1920x1080 page', second.pass, second.detail);
check('click after the change takes the pointer at its place', locks[0] === 'canvas' &&
  log.includes(`The page locked the pointer: the game moves its own pointer from ${k_Click[0]}, ${k_Click[1]}`),
`lock element ${locks[0]}`);
const still = changedShare(folder, 'still-1', 'still-2');
const drag = changedShare(folder, 'still-2', 'dragged');
check('left drag moves the land after the change', drag > 0.1 && drag > 5 * still,
  `changed ${(drag * 100).toFixed(1)}% after the drag, ${(still * 100).toFixed(1)}% with no input`);
check('game draws at 1280x720 again', log.includes(`The game draws at the new window size ${k_First[0]}x${k_First[1]}`));
const back = corners('back', k_First);
check('picture fills the page again', back.pass, back.detail);
const badge = layouts[0]?.badge ?? '';
check('page shows the build', /^([0-9a-f]{7}(-dirty)?|unknown)$/.test(badge) &&
  (badge === 'unknown' || (layouts[0]?.link ?? '').startsWith('https://github.com/raffclar/openblack/commit/')),
`${badge} ${layouts[0]?.link ?? ''}`);
// The page logs the build, and the game logs the build that it read from the file
const builds = [...log.matchAll(/openblack build ([0-9a-f]{40}(-dirty)?|unknown)/g)].map((m) => m[1]);
check('page and game log the same build', builds.length >= 2 && builds.every((b) => b === builds[0]), builds.join(' '));

const failed = results.filter((r) => !r.pass).length;
writeFileSync(path.join(out, `${renderer}-resize-results.json`), JSON.stringify(results, null, 1));
console.log(`${renderer}: ${results.length - failed} of ${results.length} resize checks pass`);
process.exit(failed === 0 ? 0 : 1);
