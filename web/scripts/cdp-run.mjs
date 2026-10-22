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
//   --gpu                    use the graphics card through Direct3D 11 (default: the SwiftShader software renderer)
//   --size <width>x<height>  the size of the window (default 1280x1024)
//   --eval <expression>      at the end, print the value of this expression on the page
//   --actions <file.json>    after the click, send these steps of input to the page, as of a user. The file is a list of
//                            steps, each with "at" (the seconds after the click) and "do":
//                            {"do": "mouse", "type": "mouseMoved" | "mousePressed" | "mouseReleased", "x", "y", "button"}
//                            {"do": "drag", "from": [x, y], "to": [x, y], "button", "steps", "ms"}
//                            {"do": "wheel", "x", "y", "deltaY"}
//                            {"do": "key", "key": "Escape", "code": "Escape", "keyCode": 27, "modifiers": 0}
//                            {"do": "shot", "name": "grip"}    {"do": "eval", "expression": "..."}
//                            {"do": "synth-move", "dx", "dy", "count", "ms", "buttons"}
//                            {"do": "synth-button", "type": "mousedown" | "mouseup", "button": 0 | 1 | 2}
//                            The synth steps send page events to the game canvas. Use them while the pointer is
//                            locked: headless Chrome gives no movement for its own mouse events then.
import { spawn } from 'node:child_process';
import { appendFileSync, mkdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import path from 'node:path';

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
await send('Page.navigate', { url });
await sleep(1500);

const evaluate = async (expression) => (await send('Runtime.evaluate', { expression, returnByValue: true, awaitPromise: true }))?.result?.value;

const preClick = option('--pre-click');
if (preClick) {
  await clickOn(preClick);
}
const gameFolder = option('--game-folder');
if (gameFolder) {
  const { root } = await send('DOM.getDocument', { depth: 0 });
  const { nodeId } = await send('DOM.querySelector', { nodeId: root.nodeId, selector: '#folder-input' });
  const result = await send('DOM.setFileInputFiles', { nodeId, files: [path.resolve(gameFolder)] });
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
const shot = async (name) => {
  const picture = await send('Page.captureScreenshot', { format: 'png' });
  if (picture?.data) {
    writeFileSync(path.join(out, `${name}.png`), Buffer.from(picture.data, 'base64'));
    record(`[shot] ${name}.png`);
  }
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
  case 'eval':
    record(`[eval] ${step.expression} = ${JSON.stringify(await evaluate(step.expression))}`);
    break;
  default:
    record(`[step] unknown step ${step.do}`);
  }
};
const steps = option('--actions') ? JSON.parse(readFileSync(option('--actions'), 'utf8')) : [];
steps.sort((a, b) => a.at - b.at);

let taken = 0;
const end = clicked + seconds * 1000;
let next = clicked + shotsFrom * 1000;
while (Date.now() < end || taken < Math.min(shots, 1)) {
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
  if (Date.now() >= end) {
    next = 0;
    continue;
  }
  await sleep(10);
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
