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
//   --size <width>x<height>  the size of the window in CSS pixels (default 1280x1024)
//   --dpr <ratio>            the device pixel ratio of the screen (default 1). At 2 the screen has 2 pixels for each
//                            CSS pixel, and the screenshots have 2 pixels for each CSS pixel too.
//   --trust-spki <hash>      tests on this computer only: trust the certificate of a test server with this key (the
//                            base64 SHA-256 of its public key), as for a test lobby on 127.0.0.1
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
//                            {"do": "synth-click", "button": 0 | 1 | 2, "hold": 80}   (a press and release under the
//                            lock, timed by the page)
//                            {"do": "pointer-to", "x", "y", "count": 10, "ms": 200, "buttons": 0}
//                            {"do": "pointer-to-colour", "low": [r, g, b], "high": [r, g, b], "box": [l, t, r, b],
//                            "minCount": 50}   (as pointer-to, to the middle of the pixels of a colour range in a
//                            screenshot; nothing moves when fewer pixels than minCount match)
//                            The pointer-to step moves the game's own pointer onto a place of the page (CSS pixels)
//                            with page movements, while the page has the lock. The script follows the pointer from the
//                            mouse press that took the lock, with the pointer speed of the game; a pointer that the
//                            game moves itself (a hand demonstration) is not followed.
//                            {"do": "wait-log", "text": "...", "seconds": 60}
//                            {"do": "resize", "width": 1920, "height": 1080, "dpr": 2}   (the new size of the window;
//                            the device pixel ratio stays the same when the step has no "dpr")
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
//   --frame-url <text>       the game page is in a frame of the page, as on the site's /play/ page: every step, the
//                            folder input, the click and the expressions use the frame whose address has this text,
//                            and the mouse positions include the place of the frame
//   --cookie-file <file.json> tests on this computer only: before the page opens, give Chrome the cookies of this file
//                            (a list of {"name", "value", "url"}; each is Secure and HttpOnly). The script never prints
//                            them. Delete the file after the run.
//   --run-all-steps          do not end the run before the last step is done
//                            player-run.json beside this script is the run of a new player on a 1280x1024 window,
//                            with no developer parameters and a new Chrome profile: the name box, the first question
//                            (skip all of the creature tutorial), the ape chosen with two Action clicks, a leash tap
//                            and a leash order. The last step prints the loudness of the sound each second.
//                            The synth steps send page events to the game canvas. Use them while the pointer is
//                            locked: headless Chrome gives no movement for its own mouse events then.
//   --cpu-profile <seconds>  this many seconds after the click, record a CPU profile of the page and of each of its
//                            workers (the game thread is one of them) for --cpu-profile-seconds (default 10). Each
//                            profile goes to cpu-<n>.cpuprofile in the output folder, which the Performance panel of
//                            DevTools opens. A page linked with --profiling-funcs gives the names of the functions.
import { spawn } from 'node:child_process';
import { createHash } from 'node:crypto';
import { appendFileSync, mkdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { decodePng, findColour, findPointer } from './png.mjs';

const [chromePath, url, secondsText, out, ...rest] = process.argv.slice(2);
const option = (name, fallback) => {
  const at = rest.indexOf(name);
  return at >= 0 ? rest[at + 1] : fallback;
};
const flag = (name) => rest.includes(name);
const seconds = Number(secondsText);
const [width, height] = option('--size', '1280x1024').split('x').map(Number);
let pixelRatio = Number(option('--dpr', '1'));
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
  `--force-device-scale-factor=${pixelRatio}`,
  '--autoplay-policy=user-gesture-required', ...graphics,
  ...(option('--trust-spki', '') ? [`--ignore-certificate-errors-spki-list=${option('--trust-spki', '')}`] : []),
  'about:blank',
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
// The sessions of the workers of the page, for a CPU profile
const sessions = [];
// The scripts of the page and of its frames: the id of each, with the frame that it belongs to
const contexts = new Map();
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
    sessions.push({ session, type: m.params.targetInfo?.type, url: m.params.targetInfo?.url });
    send('Runtime.enable', {}, session);
    send('Target.setAutoAttach', { autoAttach: true, waitForDebuggerOnStart: false, flatten: true }, session);
  } else if (m.method === 'Runtime.executionContextCreated' && !m.sessionId) {
    contexts.set(m.params.context.id, m.params.context.auxData ?? {});
  } else if (m.method === 'Runtime.executionContextDestroyed' && !m.sessionId) {
    contexts.delete(m.params.executionContextId);
  } else if (m.method === 'Runtime.executionContextsCleared' && !m.sessionId) {
    contexts.clear();
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
await send('Emulation.setDeviceMetricsOverride', { width, height, deviceScaleFactor: pixelRatio, mobile: false });
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
const cookieFile = option('--cookie-file');
if (cookieFile) {
  await send('Network.enable');
  for (const cookie of JSON.parse(readFileSync(cookieFile, 'utf8'))) {
    const result = await send('Network.setCookie', { name: cookie.name, value: cookie.value, url: cookie.url, secure: true,
      httpOnly: true, sameSite: 'Lax' });
    console.log(`cookie ${cookie.name} given: ${result.success !== false}`);
  }
}
await send('Page.navigate', { url });
await sleep(1500);

// With --frame-url, the scripts of the steps run in the frame of the game page
const frameUrl = option('--frame-url');
let frameContext = null;
const findFrame = (tree) => {
  if (tree.frame.url.includes(frameUrl) && tree.frame.parentId) {
    return tree.frame.id;
  }
  for (const child of tree.childFrames ?? []) {
    const found = findFrame(child);
    if (found) {
      return found;
    }
  }
  return null;
};
const gameContext = async () => {
  if (!frameUrl) {
    return undefined;
  }
  if (frameContext !== null && contexts.has(frameContext)) {
    return frameContext;
  }
  frameContext = null;
  const { frameTree } = await send('Page.getFrameTree');
  const frameId = frameTree ? findFrame(frameTree) : null;
  for (const [contextId, aux] of contexts) {
    if (frameId && aux.frameId === frameId && aux.isDefault) {
      frameContext = contextId;
    }
  }
  return frameContext ?? -1;
};
const evaluate = async (expression) => {
  const contextId = await gameContext();
  if (contextId === -1) {
    return undefined;
  }
  return (await send('Runtime.evaluate', { expression, returnByValue: true, awaitPromise: true, contextId }))?.result?.value;
};
// The place of the frame of the game page in the page, in CSS pixels
const frameOffset = async () => {
  if (!frameUrl) {
    return { x: 0, y: 0 };
  }
  const offset = (await send('Runtime.evaluate', { returnByValue: true, expression: `(() => {
    const frame = [...document.querySelectorAll('iframe')].find((f) => f.src.includes(${JSON.stringify(frameUrl)}));
    if (!frame) return { x: 0, y: 0 };
    const r = frame.getBoundingClientRect();
    return { x: r.left + frame.clientLeft, y: r.top + frame.clientTop }; })()` }))?.result?.value;
  return offset ?? { x: 0, y: 0 };
};

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
    if (frameUrl) {
      const contextId = await gameContext();
      const found = contextId === -1 ? null
        : (await send('Runtime.evaluate', { expression: "document.querySelector('#folder-input')", contextId }))?.result;
      result = found?.objectId ? await send('DOM.setFileInputFiles', { objectId: found.objectId, files: [path.resolve(gameFolder)] })
        : { error: 'the page has no folder input' };
    } else {
      const { root } = await send('DOM.getDocument', { depth: 0 });
      const { nodeId } = await send('DOM.querySelector', { nodeId: root.nodeId, selector: '#folder-input' });
      result = nodeId ? await send('DOM.setFileInputFiles', { nodeId, files: [path.resolve(gameFolder)] })
        : { error: 'the page has no folder input' };
    }
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
  const offset = await frameOffset();
  box = { x: box.x + offset.x, y: box.y + offset.y };
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
// The game's own pointer while the page has the lock, in the pixels of the back buffer. The game puts it at the click
// that took the lock, and moves it by each page movement times the device pixel ratio times its pointer speed (see
// src/Input/PointerSpeed.h: half of the height of the 4:3 middle of the screen over 600). It stays on the canvas.
let gamePointer = null;
const canvasBox = async () => {
  const box = await evaluate(`(() => { const r = document.getElementById('canvas').getBoundingClientRect();
    return { left: r.left, top: r.top, width: r.width, height: r.height }; })()`);
  const offset = await frameOffset();
  return box ? { ...box, left: box.left + offset.x, top: box.top + offset.y } : box;
};
const pointerScale = (box) => 0.5 * Math.min(box.height * pixelRatio, box.width * pixelRatio * 0.75) / 600;
const moveGamePointer = (dx, dy, scale, box) => {
  const clamp = (v, high) => Math.min(Math.max(v, 0.5), Math.max(high - 0.5, 0.5));
  gamePointer = { x: clamp(gamePointer.x + dx * pixelRatio * scale, box.width * pixelRatio),
    y: clamp(gamePointer.y + dy * pixelRatio * scale, box.height * pixelRatio) };
};
// A press while the page has no lock takes the lock, and the game pointer starts at its place
const followLockClick = async (x, y) => {
  if (await evaluate("document.pointerLockElement === document.getElementById('canvas')")) {
    return;
  }
  const box = await canvasBox();
  if (!box || x < box.left || y < box.top || x >= box.left + box.width || y >= box.top + box.height) {
    return;
  }
  gamePointer = { x: Math.floor(Math.max(0, Math.round(x - box.left)) * pixelRatio) + 0.5,
    y: Math.floor(Math.max(0, Math.round(y - box.top)) * pixelRatio) + 0.5 };
};
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
    if (step.type === 'mousePressed') {
      await followLockClick(step.x, step.y);
    }
    await mouseEvent(step.type, step.x, step.y, step.button ?? 'none', k_Buttons[step.button ?? 'none']);
    break;
  case 'pointer-to-colour': {
    // Finds the pixels of a colour range in a screenshot (in a box of CSS pixels), then moves the pointer to their middle
    const picture = await screenshotWithin(20000);
    const box = (step.box ?? [0, 0, width, height]).map((v) => Math.round(v * pixelRatio));
    const seen = picture?.data ? findColour(decodePng(Buffer.from(picture.data, 'base64')), step.low, step.high, box)
      : { x: -1, y: -1, count: 0 };
    record(`[pointer-to-colour] ${seen.count} pixels, middle at ${seen.x}, ${seen.y}`);
    if (seen.count < (step.minCount ?? 50)) {
      break;
    }
    step = { ...step, x: seen.x / pixelRatio, y: seen.y / pixelRatio };
  }
  // falls through
  case 'pointer-to': {
    // Moves the game's own pointer onto a place of the page with page movements, as a mouse does under the lock
    if (!gamePointer) {
      record('[pointer-to] the game has no pointer of its own yet: click the game first');
      break;
    }
    const box = await canvasBox();
    const scale = pointerScale(box);
    const target = { x: (step.x - box.left) * pixelRatio, y: (step.y - box.top) * pixelRatio };
    const count = step.count ?? 10;
    let sent = { x: 0, y: 0 };
    const total = { x: Math.round((target.x - gamePointer.x) / (scale * pixelRatio)),
      y: Math.round((target.y - gamePointer.y) / (scale * pixelRatio)) };
    for (let i = 1; i <= count; ++i) {
      const dx = Math.round(total.x * i / count) - sent.x;
      const dy = Math.round(total.y * i / count) - sent.y;
      sent = { x: sent.x + dx, y: sent.y + dy };
      await evaluate(`document.getElementById('canvas').dispatchEvent(new MouseEvent('mousemove', { bubbles: true,
        movementX: ${dx}, movementY: ${dy}, buttons: ${step.buttons ?? 0} }))`);
      moveGamePointer(dx, dy, scale, box);
      await sleep((step.ms ?? 200) / count);
    }
    record(`[pointer-to] the game pointer is at ${Math.floor(gamePointer.x)}, ${Math.floor(gamePointer.y)} pixels of the canvas (wanted ${Math.floor(target.x)}, ${Math.floor(target.y)})`);
    break;
  }
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
      if (gamePointer) {
        const box = await canvasBox();
        moveGamePointer(step.dx ?? 0, step.dy ?? 0, pointerScale(box), box);
      }
      await sleep((step.ms ?? 0) / (step.count ?? 1));
    }
    break;
  case 'synth-button': {
    const target = step.type === 'mouseup' ? 'document' : "document.getElementById('canvas')";
    await evaluate(`${target}.dispatchEvent(new MouseEvent('${step.type}', { bubbles: true, button: ${step.button ?? 0},
      buttons: ${step.type === 'mouseup' ? 0 : [1, 4, 2][step.button ?? 0]} }))`);
    break;
  }
  case 'synth-click': {
    // The press and the release in the page itself, so the time between them is the hold and not the time of two calls
    // of this script (the game takes a press of at most 225 ms as a tap)
    const button = step.button ?? 0;
    await evaluate(`new Promise((resolve) => {
      document.getElementById('canvas').dispatchEvent(new MouseEvent('mousedown', { bubbles: true, button: ${button},
        buttons: ${[1, 4, 2][button]} }));
      setTimeout(() => { document.dispatchEvent(new MouseEvent('mouseup', { bubbles: true, button: ${button}, buttons: 0 }));
        resolve(); }, ${step.hold ?? 80});
    })`);
    break;
  }
  case 'resize':
    pixelRatio = step.dpr ?? pixelRatio;
    await send('Emulation.setDeviceMetricsOverride', { width: step.width, height: step.height,
      deviceScaleFactor: pixelRatio, mobile: false });
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
  if (checksumTurns > 0 && checksumTurnLines >= checksumTurns) {
    return true;
  }
  // A game that loaded the world state of the server after a part has no checksum for the turns between the two: its
  // count stays below the turns. The run also ends once the game stopped.
  return checksumTurns > 0 &&
    await evaluate("(globalThis.openblackLogText?.() ?? '').includes('The game stopped with the code')") === true;
};

// The CPU profile of the page and its workers
const cpuProfileFrom = option('--cpu-profile');
const cpuProfileSeconds = Number(option('--cpu-profile-seconds', '10'));
let cpuProfileState = cpuProfileFrom === undefined ? 'none' : 'waiting';
const cpuProfileTargets = () => [{ session: undefined, type: 'page', url }, ...sessions];
const stepCpuProfile = async () => {
  if (cpuProfileState === 'waiting' && Date.now() >= clicked + Number(cpuProfileFrom) * 1000) {
    for (const target of cpuProfileTargets()) {
      await send('Profiler.enable', {}, target.session);
      await send('Profiler.setSamplingInterval', { interval: 200 }, target.session);
      await send('Profiler.start', {}, target.session);
    }
    record(`[cpu-profile] started on ${cpuProfileTargets().length} targets`);
    cpuProfileState = 'running';
  } else if (cpuProfileState === 'running' &&
             Date.now() >= clicked + (Number(cpuProfileFrom) + cpuProfileSeconds) * 1000) {
    let n = 0;
    for (const target of cpuProfileTargets()) {
      const result = await send('Profiler.stop', {}, target.session);
      if (result?.profile) {
        writeFileSync(path.join(out, `cpu-${n}.cpuprofile`), JSON.stringify(result.profile));
        record(`[cpu-profile] cpu-${n}.cpuprofile: ${target.type} ${target.url ?? ''}`);
        ++n;
      }
    }
    cpuProfileState = 'done';
  }
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
    // On a busy computer a step can take longer than its plan. Then the next steps and the end move later by the
    // same time, so that the time between two steps stays as the plan gives it.
    const late = steps.length > 0 ? Date.now() - (clicked + steps[0].at * 1000) : 0;
    if (late > 0) {
      clicked += late;
      end += late;
      next += late;
    }
    continue;
  }
  if (cpuProfileState === 'waiting' || cpuProfileState === 'running') {
    await stepCpuProfile();
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
