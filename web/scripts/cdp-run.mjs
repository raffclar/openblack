// Runs the web page in headless Chrome on this computer, for tests. It writes the page's console to a file, gives the
// game folder to the page's file input, clicks a button, saves screenshots, then closes the Chrome that it started.
//
// Usage: node cdp-run.mjs <chrome.exe> <url> <seconds> <output folder> [options]
//   --game-folder <folder>   give this folder to the file input of the page (#folder-input)
//   --pre-click <selector>   click this element before the page gets the game folder
//   --click <selector>       click this element with the mouse once it is enabled (a real click, as of a user)
//   --keep-profile           keep the Chrome profile (and the private storage of the site) of the last run
//   --shots <count>          the number of screenshots (default 1, at the end)
//   --shots-from <seconds>   the time of the first screenshot, after the click (default: the end)
//   --shot-every <ms>        the time between two screenshots (default 500)
//   --virtual-pointer-lock   (the default, the flag is accepted and does nothing) the page gets a pointer lock that
//                            only the page knows. Chrome on Windows confines the real cursor of the user to the page
//                            while a page has the real lock, and a Chrome that is stopped can leave the confinement
//                            set. The page and the game free the virtual lock as they free the real one (a key sent
//                            by this script does not make Chrome free a real lock by itself either).
//   --real-pointer-lock      the page gets Chrome's real pointer lock. Only for a run by hand on a computer where
//                            nobody works. No merge script and no check may pass it.
//   --gpu                    use the graphics card through Direct3D 11 (default: the SwiftShader software renderer)
//   --size <width>x<height>  the size of the window (default 1280x1024)
//   --eval <expression>      at the end, print the value of this expression on the page
//   --checksums <file>       write the checksum line of each turn of the game (--sim-checksums - in the game's
//                            arguments) to this file, as the game makes them
//   --checksum-turns <count> with --checksums, stop once the game made the lines of this many turns (before the time)
//   --actions <file.json>    after the click, send these steps of input to the page, as of a user. The file is a list of
//                            steps, each with "at" (the seconds after the click) and "do":
//                            {"do": "mouse", "type": "mouseMoved" | "mousePressed" | "mouseReleased", "x", "y", "button"}
//                            {"do": "drag", "from": [x, y], "to": [x, y], "button", "steps", "ms"}
//                            {"do": "wheel", "x", "y", "deltaY"}
//                            {"do": "key", "key": "Escape", "code": "Escape", "keyCode": 27, "modifiers": 0}
//                            {"do": "shot", "name": "grip"}    {"do": "eval", "expression": "..."}
//                            {"do": "synth-move", "dx", "dy", "count", "ms", "buttons"}
//                            {"do": "synth-button", "type": "mousedown" | "mouseup", "button": 0 | 1 | 2}
//                            {"do": "wait-log", "text": "...", "seconds": 60}
//                            {"do": "resize", "width": 1920, "height": 1080}   (the new size of the window)
//                            The wait-log step waits until the game log of the page has the text (at most the
//                            seconds), and then moves the times of all the next steps and the end later by the wait.
//                            {"do": "wait-pointer", "x", "y", "within": 40, "seconds": 60}
//                            The wait-log step has an optional "count": the number of times the text must be in the log.
//                            {"do": "wait-frames", "count": 3, "seconds": 60}
//                            {"do": "look-pointer", "label": "after the click"}  (writes where the game pointer is)
//                            The wait-pointer step takes screenshots until the pointer of the game (its teal picture) has
//                            its middle within the distance of the place, for two screenshots in a row. The wait-frames
//                            step takes screenshots until the game has drawn the number of new pictures (the screen of
//                            the start-up screens moves all the time). Both give the real state of the game, which
//                            can be behind the page by seconds on a busy computer. Like wait-log, each moves the next
//                            steps later by the wait.
//   --run-all-steps          do not end the run before the last step is done
//                            The synth steps send page events to the game canvas. Use them while the pointer is
//                            locked: headless Chrome gives no movement for its own mouse events then.
import { spawn } from 'node:child_process';
import { createHash } from 'node:crypto';
import { appendFileSync, mkdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { decodePng, findPointer } from './png.mjs';

const [chromePath, url, secondsText, out, ...rest] = process.argv.slice(2);
const option = (name, fallback) => {
  const at = rest.indexOf(name);
  return at >= 0 ? rest[at + 1] : fallback;
};
const flag = (name) => rest.includes(name);
const seconds = Number(secondsText);
const [width, height] = option('--size', '1280x1024').split('x').map(Number);
const shots = Number(option('--shots', '1'));
const shotEvery = Number(option('--shot-every', '500'));
const shotsFrom = Number(option('--shots-from', String(seconds)));
mkdirSync(out, { recursive: true });
const profile = path.join(out, 'chrome-profile');
if (!flag('--keep-profile')) {
  rmSync(profile, { recursive: true, force: true });
}
const consoleFile = path.join(out, 'console.log');
writeFileSync(consoleFile, '');

const port = 9300 + Math.floor(Math.random() * 600);
const graphics = flag('--gpu')
  ? ['--use-angle=d3d11', '--enable-gpu', '--ignore-gpu-blocklist']
  : ['--use-angle=swiftshader', '--enable-unsafe-swiftshader'];
const chrome = spawn(chromePath, [
  '--headless=new', `--remote-debugging-port=${port}`, `--user-data-dir=${profile}`, '--no-first-run',
  '--no-default-browser-check', '--disable-extensions', '--disable-background-networking', '--disable-sync',
  '--metrics-recording-only', '--disable-component-update', `--window-size=${width},${height}`,
  '--autoplay-policy=user-gesture-required', ...graphics, 'about:blank',
], { stdio: 'ignore' });
console.log(`chrome pid ${chrome.pid} port ${port}`);

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
const stamp = () => new Date().toISOString().slice(11, 23);
const record = (text) => appendFileSync(consoleFile, `${stamp()} ${text}\n`);

let target;
for (let i = 0; i < 100 && !target; ++i) {
  await sleep(200);
  try {
    const list = await (await fetch(`http://127.0.0.1:${port}/json/list`)).json();
    target = list.find((t) => t.type === 'page');
  } catch { /* Chrome is not ready yet */ }
}
if (!target) {
  console.log('no page target');
  chrome.kill();
  process.exit(1);
}

const ws = new WebSocket(target.webSocketDebuggerUrl);
let id = 0;
const pending = new Map();
const send = (method, params = {}, sessionId = undefined) => new Promise((resolve) => {
  const n = ++id;
  pending.set(n, resolve);
  ws.send(JSON.stringify({ id: n, method, params, sessionId }));
});
const frames = (d) => (d.stackTrace?.callFrames ?? []).slice(0, 25)
  .map((f) => `
    at ${f.functionName || '?'} (${f.url.split('/').pop()}:${f.lineNumber}:${f.columnNumber})`).join('');
ws.onmessage = (event) => {
  const m = JSON.parse(event.data);
  if (m.id && pending.has(m.id)) {
    pending.get(m.id)(m.result ?? { error: m.error });
    pending.delete(m.id);
  } else if (m.method === 'Runtime.consoleAPICalled') {
    record(`[${m.params.type}] ` + m.params.args.map((a) => a.value ?? a.description ?? '').join(' '));
  } else if (m.method === 'Runtime.exceptionThrown') {
    const d = m.params.exceptionDetails;
    record(`[exception${m.sessionId ? ' worker' : ''}] ${d.text} ${d.exception?.description ?? ''}${frames(d)}`);
  } else if (m.method === 'Target.attachedToTarget') {
    const session = m.params.sessionId;
    send('Runtime.enable', {}, session);
    send('Target.setAutoAttach', { autoAttach: true, waitForDebuggerOnStart: false, flatten: true }, session);
  } else if (m.method === 'Log.entryAdded') {
    record(`[log ${m.params.entry.level}] ${m.params.entry.text}`);
  }
};
await new Promise((resolve) => { ws.onopen = resolve; });
await send('Runtime.enable');
await send('Target.setAutoAttach', { autoAttach: true, waitForDebuggerOnStart: false, flatten: true });
await send('Log.enable');
await send('Page.enable');
await send('DOM.enable');
await send('Emulation.setDeviceMetricsOverride', { width, height, deviceScaleFactor: 1, mobile: false });
// A headless page has no focus of its own. The pointer lock and the keyboard lock need a page with the focus.
await send('Emulation.setFocusEmulationEnabled', { enabled: true });
await send('Page.bringToFront');
if (!flag('--real-pointer-lock')) {
  await send('Page.addScriptToEvaluateOnNewDocument', { source: `(() => {
    let locked = null;
    Object.defineProperty(Document.prototype, 'pointerLockElement', { configurable: true, get: () => locked });
    Element.prototype.requestPointerLock = function requestPointerLock() {
      const element = this;
      return new Promise((resolve) => setTimeout(() => {
        locked = element;
        document.dispatchEvent(new Event('pointerlockchange'));
        resolve();
      }, 0));
    };
    Document.prototype.exitPointerLock = function exitPointerLock() {
      if (locked) {
        locked = null;
        document.dispatchEvent(new Event('pointerlockchange'));
      }
    };
  })();` });
}
await send('Page.navigate', { url });
await sleep(1500);

const evaluate = async (expression) => (await send('Runtime.evaluate', { expression, returnByValue: true, awaitPromise: true }))?.result?.value;

const preClick = option('--pre-click');
if (preClick) {
  await clickOn(preClick);
}
const gameFolder = option('--game-folder');
if (gameFolder) {
  // On a busy computer the page can take longer than the first wait to load. The node of the input is then not there
  // yet, so the step asks again until the page has it.
  let result = { error: 'the page has no folder input' };
  for (let i = 0; i < 300 && result.error; ++i) {
    const { root } = await send('DOM.getDocument', { depth: 0 });
    const { nodeId } = await send('DOM.querySelector', { nodeId: root.nodeId, selector: '#folder-input' });
    result = nodeId ? await send('DOM.setFileInputFiles', { nodeId, files: [path.resolve(gameFolder)] })
      : { error: 'the page has no folder input' };
    if (result.error) {
      await sleep(200);
    }
  }
  console.log(`game folder given: ${JSON.stringify(result)}`);
}

async function clickOn(selector) {
  let box;
  for (let i = 0; i < 6000 && !box; ++i) {
    box = await evaluate(`(() => { const e = document.querySelector(${JSON.stringify(selector)});
      if (!e || e.disabled || e.hidden || e.offsetParent === null) return null;
      const r = e.getBoundingClientRect(); return { x: r.x + r.width / 2, y: r.y + r.height / 2 }; })()`);
    if (!box) {
      await sleep(100);
    }
  }
  if (!box) {
    console.log(`cannot click ${selector}`);
    return false;
  }
  for (const type of ['mouseMoved', 'mousePressed', 'mouseReleased']) {
    await send('Input.dispatchMouseEvent', { type, x: box.x, y: box.y, button: 'left', clickCount: 1 });
  }
  console.log(`clicked ${selector}`);
  return true;
}

const click = option('--click');
let clicked = Date.now();
if (click && await clickOn(click)) {
  clicked = Date.now();
}

// The mouse of the steps starts at the place of the click
let mouse = { x: width / 2, y: height / 2 };
const mouseEvent = (type, x, y, button = 'none', buttons = 0) => {
  mouse = { x, y };
  return send('Input.dispatchMouseEvent', { type, x, y, button, buttons, clickCount: type === 'mouseMoved' ? 0 : 1 });
};
const k_Buttons = { none: 0, left: 1, right: 2, middle: 4 };
// A screenshot of a page that Chrome has not drawn for a long time can take very long. The waiting steps ask again.
const screenshotWithin = (ms) => Promise.race([send('Page.captureScreenshot', { format: 'png' }), sleep(ms).then(() => undefined)]);
const shot = async (name) => {
  const picture = await send('Page.captureScreenshot', { format: 'png' });
  if (picture?.data) {
    writeFileSync(path.join(out, `${name}.png`), Buffer.from(picture.data, 'base64'));
    record(`[shot] ${name}.png`);
  }
};
// A step that waits moves the times of the next steps, and the end of the run, later by the wait
const shiftTimeline = (step) => {
  const late = Math.max(0, Date.now() - (clicked + step.at * 1000));
  clicked += late;
  end += late;
  next += late;
};
const runStep = async (step) => {
  record(`[step] ${JSON.stringify(step)}`);
  switch (step.do) {
  case 'mouse':
    await mouseEvent(step.type, step.x, step.y, step.button ?? 'none', k_Buttons[step.button ?? 'none']);
    break;
  case 'drag': {
    const button = step.button ?? 'left';
    const count = step.steps ?? 20;
    await mouseEvent('mouseMoved', step.from[0], step.from[1]);
    await mouseEvent('mousePressed', step.from[0], step.from[1], button, k_Buttons[button]);
    for (let i = 1; i <= count; ++i) {
      const x = step.from[0] + (step.to[0] - step.from[0]) * i / count;
      const y = step.from[1] + (step.to[1] - step.from[1]) * i / count;
      await mouseEvent('mouseMoved', x, y, button, k_Buttons[button]);
      await sleep((step.ms ?? 500) / count);
    }
    await mouseEvent('mouseReleased', step.to[0], step.to[1], button, 0);
    break;
  }
  case 'wheel':
    await send('Input.dispatchMouseEvent', { type: 'mouseWheel', x: step.x ?? mouse.x, y: step.y ?? mouse.y, deltaX: 0,
      deltaY: step.deltaY ?? 100 });
    break;
  case 'key': {
    const key = { key: step.key, code: step.code ?? step.key, windowsVirtualKeyCode: step.keyCode ?? 0,
      modifiers: step.modifiers ?? 0 };
    await send('Input.dispatchKeyEvent', { type: step.key.length === 1 ? 'keyDown' : 'rawKeyDown', text: step.key.length === 1 ? step.key : undefined, ...key });
    await sleep(step.hold ?? 60);
    await send('Input.dispatchKeyEvent', { type: 'keyUp', ...key });
    break;
  }
  case 'shot':
    await shot(step.name ?? `step-${Date.now()}`);
    break;
  case 'synth-move':
    for (let i = 0; i < (step.count ?? 1); ++i) {
      await evaluate(`document.getElementById('canvas').dispatchEvent(new MouseEvent('mousemove', { bubbles: true,
        movementX: ${step.dx ?? 0}, movementY: ${step.dy ?? 0}, buttons: ${step.buttons ?? 0} }))`);
      await sleep((step.ms ?? 0) / (step.count ?? 1));
    }
    break;
  case 'synth-button': {
    const target = step.type === 'mouseup' ? 'document' : "document.getElementById('canvas')";
    await evaluate(`${target}.dispatchEvent(new MouseEvent('${step.type}', { bubbles: true, button: ${step.button ?? 0},
      buttons: ${step.type === 'mouseup' ? 0 : [1, 4, 2][step.button ?? 0]} }))`);
    break;
  }
  case 'resize':
    await send('Emulation.setDeviceMetricsOverride', { width: step.width, height: step.height, deviceScaleFactor: 1,
      mobile: false });
    break;
  case 'eval':
    record(`[eval] ${step.expression} = ${JSON.stringify(await evaluate(step.expression))}`);
    break;
  case 'wait-log': {
    const until = Date.now() + (step.seconds ?? 60) * 1000;
    const expression = `(globalThis.openblackLogText?.() ?? document.getElementById('log')?.textContent ?? '')
      .split(${JSON.stringify(step.text)}).length - 1 >= ${step.count ?? 1}`;
    let found = false;
    while (!found && Date.now() < until) {
      found = await evaluate(expression);
      if (!found) {
        await sleep(100);
      }
    }
    record(`[wait-log] ${found ? 'found' : 'NOT FOUND'}: ${step.text}`);
    shiftTimeline(step);
    break;
  }
  case 'wait-pointer': {
    const until = Date.now() + (step.seconds ?? 60) * 1000;
    const within = step.within ?? 40;
    let last = { x: -1, y: -1, count: 0 };
    let near = 0;
    let looks = 0;
    while (near < 2 && Date.now() < until) {
      const picture = await screenshotWithin(20000);
      ++looks;
      last = picture?.data ? findPointer(decodePng(Buffer.from(picture.data, 'base64'))) : last;
      near = last.count > 0 && Math.hypot(last.x - step.x, last.y - step.y) <= within ? near + 1 : 0;
    }
    record(`[wait-pointer] ${near >= 2 ? 'found' : 'NOT FOUND'}: pointer at ${last.x}, ${last.y} (wanted ${step.x}, ${step.y}) after ${looks} screenshots`);
    shiftTimeline(step);
    break;
  }
  case 'look-pointer': {
    const picture = await screenshotWithin(20000);
    const seen = picture?.data ? findPointer(decodePng(Buffer.from(picture.data, 'base64'))) : { x: -1, y: -1 };
    record(`[look-pointer] ${step.label ?? ''} pointer at ${seen.x}, ${seen.y}`);
    break;
  }
  case 'wait-frames': {
    const until = Date.now() + (step.seconds ?? 60) * 1000;
    let previous = '';
    let drawn = 0;
    let looks = 0;
    while (drawn < (step.count ?? 1) && Date.now() < until) {
      const picture = await screenshotWithin(20000);
      ++looks;
      const hash = picture?.data ? createHash('sha1').update(picture.data).digest('hex') : '';
      drawn += previous !== '' && hash !== '' && hash !== previous ? 1 : 0;
      previous = hash || previous;
    }
    record(`[wait-frames] ${drawn >= (step.count ?? 1) ? 'found' : 'NOT FOUND'}: ${drawn} new pictures after ${looks} screenshots`);
    shiftTimeline(step);
    break;
  }
  default:
    record(`[step] unknown step ${step.do}`);
  }
};
const steps = option('--actions') ? JSON.parse(readFileSync(option('--actions'), 'utf8')) : [];
steps.sort((a, b) => a.at - b.at);

// The checksum lines of the game, which the page keeps (see openblackChecksums in launcher.js)
const checksumsFile = option('--checksums');
const checksumTurns = Number(option('--checksum-turns', '0'));
let checksumCount = 0;
let checksumTurnLines = 0;
let checksumPoll = 0;
if (checksumsFile) {
  writeFileSync(checksumsFile, '');
}
const takeChecksums = async () => {
  const got = await evaluate(`globalThis.openblackChecksums?.(${checksumCount})`);
  if (got && got.lines.length > 0) {
    appendFileSync(checksumsFile, got.lines.map((line) => `${line}\n`).join(''));
    checksumCount += got.lines.length;
    checksumTurnLines += got.lines.filter((line) => line.startsWith('{"turn":')).length;
  }
  return checksumTurns > 0 && checksumTurnLines >= checksumTurns;
};

let taken = 0;
let end = clicked + seconds * 1000;
let next = clicked + shotsFrom * 1000;
while (Date.now() < end || taken < Math.min(shots, 1) || (flag('--run-all-steps') && steps.length > 0)) {
  if (taken < shots && Date.now() >= next) {
    const picture = await send('Page.captureScreenshot', { format: 'png' });
    if (picture?.data) {
      writeFileSync(path.join(out, `shot-${String(taken).padStart(3, '0')}.png`), Buffer.from(picture.data, 'base64'));
      record(`[shot] shot-${String(taken).padStart(3, '0')}.png`);
    }
    ++taken;
    next += shotEvery;
    continue;
  }
  if (steps.length > 0 && Date.now() >= clicked + steps[0].at * 1000) {
    await runStep(steps.shift());
    continue;
  }
  if (checksumsFile && Date.now() >= checksumPoll) {
    checksumPoll = Date.now() + 1000;
    if (await takeChecksums()) {
      end = Date.now();
      record(`[checksums] ${checksumCount} lines`);
    }
  }
  if (Date.now() >= end) {
    next = 0;
    continue;
  }
  await sleep(10);
}
if (checksumsFile) {
  await takeChecksums();
  console.log(`checksums ${checksumCount} in ${checksumsFile}`);
}
if (option('--eval')) {
  console.log(`eval: ${JSON.stringify(await evaluate(option('--eval')))}`);
}
writeFileSync(path.join(out, 'page-log.txt'), (await evaluate("globalThis.openblackLogText?.() ?? document.getElementById('log')?.textContent ?? ''")) ?? '');
await send('Browser.close');
await sleep(1000);
if (chrome.exitCode === null) {
  chrome.kill();
}
console.log(`shots ${taken}; console in ${consoleFile}`);
process.exit(0);
