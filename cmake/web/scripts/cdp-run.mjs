// Starts headless Chrome on a local page, prints the page's console and errors, saves a screenshot, then closes Chrome.
// Usage: node cdp-run.mjs <url> <seconds> <screenshot.png> <profile-dir>
import { spawn } from 'node:child_process';
import { writeFileSync } from 'node:fs';

const [url, seconds, shot, profile] = process.argv.slice(2);
const port = 9333;
const chrome = spawn('C:/Program Files/Google/Chrome/Application/chrome.exe', [
  '--headless=new', `--remote-debugging-port=${port}`, `--user-data-dir=${profile}`, '--no-first-run',
  '--disable-extensions', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--window-size=1024,768',
  'about:blank',
], { stdio: 'ignore' });
console.log(`chrome pid ${chrome.pid}`);

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
let target;
for (let i = 0; i < 50 && !target; ++i) {
  await sleep(200);
  try {
    const list = await (await fetch(`http://127.0.0.1:${port}/json/list`)).json();
    target = list.find((t) => t.type === 'page');
  } catch { /* not up yet */ }
}
if (!target) { console.log('no page target'); chrome.kill(); process.exit(1); }

const ws = new WebSocket(target.webSocketDebuggerUrl);
let id = 0;
const pending = new Map();
const send = (method, params = {}) => new Promise((resolve) => {
  const n = ++id; pending.set(n, resolve); ws.send(JSON.stringify({ id: n, method, params }));
});
ws.onmessage = (event) => {
  const m = JSON.parse(event.data);
  if (m.id && pending.has(m.id)) { pending.get(m.id)(m.result ?? m.error); pending.delete(m.id); return; }
  if (m.method === 'Runtime.consoleAPICalled') {
    console.log(`[${m.params.type}] ` + m.params.args.map((a) => a.value ?? a.description ?? '').join(' '));
  } else if (m.method === 'Runtime.exceptionThrown') {
    const d = m.params.exceptionDetails;
    console.log(`[exception] ${d.text} ${d.exception?.description ?? ''}`);
  } else if (m.method === 'Log.entryAdded') {
    console.log(`[log ${m.params.entry.level}] ${m.params.entry.text}`);
  }
};
await new Promise((r) => { ws.onopen = r; });
await send('Runtime.enable');
await send('Log.enable');
await send('Page.enable');
await send('Page.navigate', { url });
await sleep(Number(seconds) * 1000);
const picture = await send('Page.captureScreenshot', { format: 'png' });
if (picture?.data) writeFileSync(shot, Buffer.from(picture.data, 'base64'));
const text = await send('Runtime.evaluate', { expression: "document.getElementById('log')?.textContent ?? ''" });
console.log('--- page log ---');
console.log(text?.result?.value ?? '');
await send('Browser.close');
await sleep(500);
try { chrome.kill(); } catch { /* closed */ }
process.exit(0);
