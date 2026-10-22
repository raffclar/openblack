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
import { spawn } from 'node:child_process';
import { appendFileSync, mkdirSync, rmSync, writeFileSync } from 'node:fs';
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
  if (Date.now() >= end) {
    next = 0;
    continue;
  }
  await sleep(50);
}
if (option('--eval')) {
  console.log(`eval: ${JSON.stringify(await evaluate(option('--eval')))}`);
}
writeFileSync(path.join(out, 'page-log.txt'), (await evaluate("document.getElementById('log')?.textContent ?? ''")) ?? '');
await send('Browser.close');
await sleep(1000);
if (chrome.exitCode === null) {
  chrome.kill();
}
console.log(`shots ${taken}; console in ${consoleFile}`);
process.exit(0);
