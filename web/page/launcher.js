// openblack web page: it gets the user's Black & White folder, checks the install and starts the game.
//
// The page sends nothing to a server. It reads the game data only with the local file interfaces of the browser:
// - the folder picker (Chrome, Edge), which reads the install in place and keeps the choice of the folder,
// - a file input or a dropped folder (all browsers), which give local File objects,
// - an optional copy in the private storage of the site (OPFS), for browsers that cannot keep the choice of a folder.
'use strict';

const k_Database = 'openblack';
const k_HandleStore = 'handles';
const k_FolderKey = 'game-folder';
const k_CopyFolder = 'game-copy';
const k_CopyComplete = '.openblack-copy-complete';

// The top folders of the install that the game reads. The page gives no other folder to the game. A network game plays
// a land of the folder "Online Maps".
const k_GameFolders = new Set(['audio', 'data', 'scripts', 'plug ins', 'online maps']);

// The parts of an install that the game needs, without letter case. A name with a slash at the end is a folder.
const k_Needed = [
  'data/allmeshes.g3d', 'data/allanims.anm', 'data/landscape/', 'scripts/land1.txt',
  'audio/dialogue/', 'audio/music/', 'audio/sfx/',
];

const element = (id) => document.getElementById(id);

// The build writes the openblack commit that it comes from into version.txt next to the page: the full git commit hash,
// with "-dirty" when the source tree had changes, or "unknown". The page shows the short hash with a link to the
// commit, and gives the text of the file to the game, which shows it on its loading screen.
const k_VersionFile = 'version.txt';
const k_CommitAddress = 'https://github.com/raffclar/openblack/commit/';
const k_ShortHash = 7;

/** The build version in the text of the version file */
function parseVersion(text) {
  const match = /^([0-9a-f]{40})(-dirty)?$/i.exec((text ?? '').trim());
  if (!match) {
    return { file: text, text: 'unknown', short: 'unknown', address: null };
  }
  const hash = match[1].toLowerCase();
  const dirty = match[2] ? '-dirty' : '';
  return { file: text, text: hash + dirty, short: hash.slice(0, k_ShortHash) + dirty, address: k_CommitAddress + hash };
}

// A build with the developer tools puts developer-tools.txt next to the page. Only then does the page take its
// developer parameters: "args" (more arguments for the game) and "tool-file". The builds that players get do not have
// the file, thus their page ignores these parameters.
const k_DeveloperToolsFile = 'developer-tools.txt';
const developerTools = fetch(k_DeveloperToolsFile, { cache: 'no-cache' })
  .then((response) => response.ok)
  .catch(() => false);

const buildVersion = fetch(k_VersionFile, { cache: 'no-cache' })
  .then((response) => (response.ok ? response.text() : undefined))
  .catch(() => undefined)
  .then(parseVersion);

/** Show the build on the launcher and on the badge that the page shows while the game plays */
function showVersion(version) {
  for (const link of document.querySelectorAll('.build-link')) {
    link.textContent = version.short;
    link.title = `openblack build ${version.text}`;
    if (version.address) {
      link.href = version.address;
    } else {
      link.removeAttribute('href');
    }
  }
}
const logElement = element('log');

// The page keeps the last lines of the game log. It writes them into the log box only while the box is open, and at most
// once in each frame of the page. A change of the box text makes the browser lay out the page again. The page must not
// do that for each line, because the main thread of the page also shows the game's frames.
const k_LogLines = 20000;
const logLines = [];
let logShown = 0;
let logUpdateDue = false;

function showLog() {
  logUpdateDue = false;
  if (!logElement.parentElement.open) {
    return;
  }
  if (logShown > logLines.length || logElement.textContent.length === 0) {
    logElement.textContent = logLines.join('\n');
  } else if (logShown < logLines.length) {
    logElement.append(logLines.slice(logShown).map((line) => '\n' + line).join(''));
  }
  logShown = logLines.length;
  logElement.scrollTop = logElement.scrollHeight;
}

// The checksum lines of a tool's run (--sim-checksums -), which the page keeps all of, for the cross-play check
const k_ChecksumTag = 'openblack-checksum ';
const checksumLines = [];

// The crash handler of the game writes this line before it writes its report to the private storage
const k_CrashReportTag = 'Crash report written to ';

function log(text) {
  if (text.startsWith(k_ChecksumTag)) {
    checksumLines.push(text.slice(k_ChecksumTag.length));
    return;
  }
  if (text.startsWith(k_CrashReportTag)) {
    gameCrashed(text.slice(k_CrashReportTag.length).split('/').pop());
  }
  logLines.push(text);
  if (logLines.length > 2 * k_LogLines) {
    logLines.splice(0, logLines.length - k_LogLines);
    // The box gets all its text again at the next update
    logShown = logLines.length + 1;
  }
  if (!logUpdateDue && logElement.parentElement.open) {
    logUpdateDue = true;
    requestAnimationFrame(showLog);
  }
}

/** All the lines of the game log that the page keeps, for the test scripts */
globalThis.openblackLogText = () => logLines.join('\n');
/** The checksum lines of the turns from a first line on, and the count of all of them, for the test scripts */
globalThis.openblackChecksums = (from = 0) => ({ count: checksumLines.length, lines: checksumLines.slice(from) });
logElement.parentElement.addEventListener('toggle', showLog);

// ------------------------------------------------------------------------------------------------ diagnostics

// The page writes its own lines for the diagnostics with this tag. The game writes the lines of its connection log with
// the tag "net: " (src/Online/ConnectionLog.h).
const k_PageTag = 'page: ';
const k_NetTag = 'net: ';
/** The most lines of the connection log in the report of "Copy diagnostics" */
const k_DiagnosticLines = 300;
/** The most other lines of the game log at the end of the report, for the context of a crash */
const k_DiagnosticTail = 40;

// What the page found out for the report: the renderer and its adapter, and the size of the canvas
const diagnostics = { renderer: 'not chosen', adapter: 'not known', canvas: 'not known' };

/** Write a line of the page to the game log, with the tag of the page */
function pageLog(text) {
  log(k_PageTag + text);
}
globalThis.openblackPageLog = pageLog;

/** The text with each secret in it replaced by a mark, as the game does (MaskSecrets) */
function maskSecrets(text) {
  return text
    .replace(/v4\.(public|local)\.[A-Za-z0-9_\-.=+/]+/g, '<hidden>')
    .replace(/Bearer [A-Za-z0-9_\-.=+/]+/g, '<hidden>')
    .replace(/(\w*(?:ticket|password|token|session|secret)\w*"?\s*[=:]\s*"?)([^"\s,;&]+)/gi, '$1<hidden>');
}

/** The text of the report of "Copy diagnostics": the build, the browser, the renderer and the last lines of the logs */
async function diagnosticsReport() {
  const version = await buildVersion;
  const memory = globalThis.Module && Module.HEAP8 ? `${Math.round(Module.HEAP8.length / (1024 * 1024))} MB` : 'not known';
  const net = logLines.filter((line) => line.includes(k_NetTag) || line.startsWith(k_PageTag)).slice(-k_DiagnosticLines);
  const header = [
    'openblack diagnostics (web)',
    `Time: ${new Date().toISOString()}`,
    `Build: ${version.text}`,
    `Browser: ${navigator.userAgent}`,
    `Renderer: ${diagnostics.renderer}`,
    `Adapter: ${diagnostics.adapter}`,
    `Canvas: ${diagnostics.canvas}`,
    `Page: ${document.visibilityState}, ${document.hasFocus() ? 'with' : 'without'} the focus`,
    `WebAssembly memory: ${memory}`,
    `Cross-origin isolated: ${globalThis.crossOriginIsolated ? 'yes' : 'no'}`,
    '',
    `The last ${net.length} lines of the connection log:`,
    ...net,
    '',
    `The last ${k_DiagnosticTail} lines of the game log:`,
    ...logLines.slice(-k_DiagnosticTail),
  ];
  return maskSecrets(header.join('\n'));
}

/** Put the report of "Copy diagnostics" on the clipboard. When the browser refuses the clipboard, the page gives a file. */
async function copyDiagnostics(button) {
  const report = await diagnosticsReport();
  let copied = false;
  try {
    await navigator.clipboard.writeText(report);
    copied = true;
  } catch (error) {
    // The clipboard needs the focus of the page: an older way works from the press of a button
    const area = document.createElement('textarea');
    area.value = report;
    area.setAttribute('readonly', '');
    area.style.position = 'fixed';
    area.style.opacity = '0';
    document.body.append(area);
    area.select();
    try {
      copied = document.execCommand('copy');
    } catch {
      copied = false;
    }
    area.remove();
  }
  if (!copied) {
    download(`openblack_diagnostics_${fileTime()}.txt`, report);
  }
  pageLog(copied ? `The diagnostics went to the clipboard (${report.length} characters)`
    : 'The browser refused the clipboard: the page gives the diagnostics as a file');
  if (button) {
    const label = button.textContent;
    button.textContent = copied ? 'Copied' : 'Saved as a file';
    setTimeout(() => { button.textContent = label; }, 2000);
  }
}
globalThis.openblackCopyDiagnostics = copyDiagnostics;

/** A button of a message box */
function messageButton(label, action) {
  const button = document.createElement('button');
  button.type = 'button';
  button.textContent = label;
  button.addEventListener('click', (event) => action(event.currentTarget));
  return button;
}

/** Close a message box. The keys go to the game again. */
function closeMessage(box) {
  box.remove();
  const canvas = element('canvas');
  if (document.body.classList.contains('playing') && canvas) {
    canvas.focus();
  }
}

// Escape closes the newest message box that tells of a stop of the game. While the game plays, Escape is a key of the
// game, thus Escape closes only a message of a game that stopped.
window.addEventListener('keydown', (event) => {
  if (event.key !== 'Escape') {
    return;
  }
  const stops = element('messages').querySelectorAll('.message.stop');
  if (stops.length > 0) {
    event.preventDefault();
    event.stopPropagation();
    closeMessage(stops[stops.length - 1]);
  }
}, true);

/**
 * Show a message on the page. The game calls this too, for an error that stops it. Each file gets a download link. The
 * player can close each message. A message of a stop of the game also has the buttons that copy the diagnostics and that
 * load the page again, and Escape closes it.
 */
function showMessage(title, text, files = [], { stop = false } = {}) {
  const box = document.createElement('div');
  box.className = stop ? 'message stop' : 'message';
  box.setAttribute('role', stop ? 'alertdialog' : 'status');
  const heading = document.createElement('strong');
  heading.textContent = title;
  box.append(heading, document.createTextNode(text));
  if (files.length > 0) {
    const list = document.createElement('ul');
    for (const file of files) {
      const link = document.createElement('a');
      link.href = URL.createObjectURL(file);
      link.download = file.name;
      link.textContent = `${file.name} (${new Date(file.lastModified).toLocaleString()})`;
      const item = document.createElement('li');
      item.append(link);
      list.append(item);
    }
    box.append(list);
  }
  const buttons = document.createElement('div');
  buttons.className = 'message-buttons';
  if (stop) {
    buttons.append(messageButton('Copy diagnostics', copyDiagnostics),
      messageButton('Reload the page', () => location.reload()));
  }
  buttons.append(messageButton(stop ? 'Close (Esc)' : 'Close', () => closeMessage(box)));
  box.append(buttons);
  element('messages').append(box);
  console.error(`openblack: ${title}: ${text}`);
}
globalThis.openblackShowMessage = showMessage;

/** A debug file that the game wrote outside the Profiles folder (a screenshot, a saved mind) gets a download link */
globalThis.openblackOfferDownload = (name, bytes) => {
  const type = name.toLowerCase().endsWith('.png') ? 'image/png' : 'application/octet-stream';
  showMessage('The game wrote a file', 'Use the link to download it.', [new File([bytes], name, { type })]);
};

/** Give the text to the user as a file to download */
function download(name, text) {
  const link = document.createElement('a');
  link.href = URL.createObjectURL(new Blob([text], { type: 'text/plain' }));
  link.download = name;
  link.click();
  setTimeout(() => URL.revokeObjectURL(link.href), 60000);
}

/** The time as the start of a file name, such as 2026-11-03_14-05-09 */
function fileTime(date = new Date()) {
  const two = (value) => String(value).padStart(2, '0');
  return `${date.getFullYear()}-${two(date.getMonth() + 1)}-${two(date.getDate())}_` +
    `${two(date.getHours())}-${two(date.getMinutes())}-${two(date.getSeconds())}`;
}

/** True when the game reads the file at this path, relative to the install folder. */
function isGameFile(path) {
  const parts = path.split('/');
  if (parts.at(-1).startsWith('.openblack')) {
    return false;
  }
  if (parts.length === 1) {
    return !/\.(exe|dll|sys|zip|ico)$/i.test(path);
  }
  return k_GameFolders.has(parts[0].toLowerCase());
}

/** The result of the install check: the missing parts and the version that the marker files show. */
function checkInstall(entries) {
  const paths = new Set(entries.map((entry) => entry.path.toLowerCase()));
  const has = (needed) => needed.endsWith('/')
    ? entries.some((entry) => entry.path.toLowerCase().startsWith(needed))
    : paths.has(needed);
  const missing = k_Needed.filter((needed) => !has(needed));
  const patches = [...paths]
    .map((path) => /^readme-patch-v(\d)(\d+)\.txt$/.exec(path))
    .filter(Boolean)
    .map((match) => `${match[1]}.${match[2]}`)
    .sort();
  let version = 'The version is not known: the install has no patch files.';
  if (paths.has('scripts/infoscriptmultiplayer2.txt')) {
    version = 'The install has patch 1.20 or later.';
  } else if (paths.has('scripts/infoscriptpatch2.txt')) {
    version = 'The install has patch 1.10 or later.';
  }
  if (patches.length > 0) {
    version += ` Its readme files name the patches ${patches.join(', ')}.`;
  }
  return { missing, version };
}

// ------------------------------------------------------------------------------------------------ the folder

/** The files of a folder handle (the folder picker or the copy in OPFS), with their paths relative to the folder. */
async function readHandle(folder, prefix = '', entries = []) {
  for await (const [name, handle] of folder.entries()) {
    const path = prefix + name;
    if (handle.kind === 'directory') {
      if (prefix !== '' || k_GameFolders.has(name.toLowerCase())) {
        await readHandle(handle, path + '/', entries);
      }
    } else if (isGameFile(path)) {
      entries.push({ path, file: await handle.getFile() });
    }
  }
  return entries;
}

/** The files of a file input, without the name of the folder that the user chose. */
function readFileList(files) {
  const entries = [];
  for (const file of files) {
    const path = (file.webkitRelativePath || file.name).split('/').slice(1).join('/');
    if (path !== '' && isGameFile(path)) {
      entries.push({ path, file });
    }
  }
  return entries;
}

/** The files of a dropped folder, with their paths relative to the folder. */
async function readDroppedFolder(folder, prefix = '', entries = []) {
  const reader = folder.createReader();
  for (;;) {
    const batch = await new Promise((resolve, reject) => reader.readEntries(resolve, reject));
    if (batch.length === 0) {
      return entries;
    }
    for (const child of batch) {
      const path = prefix + child.name;
      if (child.isDirectory) {
        if (prefix !== '' || k_GameFolders.has(child.name.toLowerCase())) {
          await readDroppedFolder(child, path + '/', entries);
        }
      } else if (isGameFile(path)) {
        entries.push({ path, file: await new Promise((resolve, reject) => child.file(resolve, reject)) });
      }
    }
  }
}

// ------------------------------------------------------------------------------------------------ kept choices

function openDatabase() {
  return new Promise((resolve, reject) => {
    const request = indexedDB.open(k_Database, 1);
    request.onupgradeneeded = () => request.result.createObjectStore(k_HandleStore);
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error);
  });
}

async function keepHandle(handle) {
  try {
    const database = await openDatabase();
    database.transaction(k_HandleStore, 'readwrite').objectStore(k_HandleStore).put(handle, k_FolderKey);
  } catch (error) {
    log(`The page cannot keep the choice of the folder: ${error}`);
  }
}

async function keptHandle() {
  try {
    const database = await openDatabase();
    return await new Promise((resolve) => {
      const request = database.transaction(k_HandleStore).objectStore(k_HandleStore).get(k_FolderKey);
      request.onsuccess = () => resolve(request.result);
      request.onerror = () => resolve(undefined);
    });
  } catch {
    return undefined;
  }
}

async function storageRoot() {
  try {
    return await navigator.storage.getDirectory();
  } catch {
    return undefined;
  }
}

// The game keeps its crash reports in this folder of the private storage of the site (src/Web/WebPlatform.cpp). The
// page also keeps a report there when the game stops on an error of the browser, such as a WebAssembly trap.
const k_CrashFolder = ['openblack', 'crashes'];
const k_CrashReportsShown = 5;
const k_CrashLogLines = 2000;

async function crashFolder(create = false) {
  let folder = await storageRoot();
  try {
    for (const name of k_CrashFolder) {
      folder = folder && await folder.getDirectoryHandle(name, { create });
    }
    return folder;
  } catch {
    return undefined;
  }
}

/** The crash reports that the private storage keeps, the newest first */
async function crashReports() {
  const folder = await crashFolder();
  const reports = [];
  if (!folder) {
    return reports;
  }
  try {
    for await (const [name, handle] of folder.entries()) {
      if (handle.kind === 'file' && name.endsWith('.txt')) {
        reports.push(await handle.getFile());
      }
    }
  } catch (error) {
    log(`The page cannot read the crash reports: ${error}`);
  }
  return reports.sort((a, b) => b.lastModified - a.lastModified).slice(0, k_CrashReportsShown);
}

/**
 * Show the crash reports that the private storage keeps, with links to download them. Gives the count shown. A stop of
 * the game shows them as a message of a stop.
 */
async function showCrashReports(title, text, stop = false) {
  const reports = await crashReports();
  if (reports.length > 0) {
    showMessage(title, text, reports, { stop });
  }
  return reports.length;
}

/** Keep a crash report of the page, with the reason and the last lines of the game log, in the private storage */
async function keepPageCrashReport(reason) {
  const version = await buildVersion;
  const text = [`openblack build ${version.text}`, `The game stopped: ${reason}`, `Browser: ${navigator.userAgent}`, '',
    'The last lines of the game log:', ...logLines.slice(-k_CrashLogLines)].join('\n');
  try {
    const folder = await crashFolder(true);
    const file = await folder.getFileHandle(`${fileTime()}_page.txt`, { create: true });
    const writable = await file.createWritable();
    await writable.write(text);
    await writable.close();
    return true;
  } catch (error) {
    log(`The page cannot keep the crash report in the private storage: ${error}`);
    return false;
  }
}

const k_CrashReportWaits = 40;
const k_CrashReportWaitMs = 250;

// The game crashed and its crash handler writes the report with this name: the page waits until the private storage
// has the report, then shows the reports. The crash handler stops the game without the exit of the page.
async function gameCrashed(name) {
  for (let wait = 0; wait < k_CrashReportWaits; ++wait) {
    const reports = await crashReports();
    if (reports.some((report) => report.name === name)) {
      showMessage('The game stopped after a crash',
        'The browser keeps the crash reports below. The "Save log" button gives the whole game log.', reports,
        { stop: true });
      return;
    }
    await new Promise((resolve) => setTimeout(resolve, k_CrashReportWaitMs));
  }
  showMessage('The game stopped after a crash',
    `The private storage of the browser does not have the report ${name}. The "Save log" button gives the game log.`, [],
    { stop: true });
}

// The game stopped on an error of the browser: the page keeps one report, then shows the reports
let pageCrashKept = false;
async function gameFailed(reason) {
  if (pageCrashKept) {
    return;
  }
  pageCrashKept = true;
  log(`The game stopped on an error: ${reason}`);
  const kept = await keepPageCrashReport(reason);
  if (!kept || (await showCrashReports('The game stopped on an error',
    'The browser keeps the crash reports below. The "Save log" button gives the whole game log.', true)) === 0) {
    showMessage('The game stopped on an error', `${reason} The "Save log" button gives the game log.`, [], { stop: true });
  }
}

async function keptCopy() {
  const root = await storageRoot();
  if (!root) {
    return undefined;
  }
  try {
    const copy = await root.getDirectoryHandle(k_CopyFolder);
    await copy.getFileHandle(k_CopyComplete);
    return copy;
  } catch {
    return undefined;
  }
}

/** Copy the files into the private storage of the site. A worker does the copy, as only workers write in place. */
function copyToStorage(entries) {
  const progress = element('copy-progress');
  progress.hidden = false;
  const total = entries.reduce((sum, entry) => sum + entry.file.size, 0);
  return new Promise((resolve) => {
    const worker = new Worker('copy-worker.js');
    worker.onmessage = (event) => {
      if (event.data.bytes !== undefined) {
        progress.textContent = `The page copies the game data into this browser: ${Math.floor(100 * event.data.bytes / total)}%`;
      } else {
        progress.textContent = event.data.error
          ? `The copy failed: ${event.data.error}. The game reads the folder that you chose.`
          : 'The copy of the game data is complete. The next visit can use it.';
        worker.terminate();
        resolve(!event.data.error);
      }
    };
    worker.postMessage({ entries, folder: k_CopyFolder, marker: k_CopyComplete });
  });
}

// ------------------------------------------------------------------------------------------------ start

let chosen = null;

// The game thread draws into an OffscreenCanvas. In the frames of its game loop, the thread gives control back to the
// browser after each frame, and the browser shows the game canvas. While it draws other frames, such as a loading
// screen, it does not give control back, so it sends each frame as an image, and the screen canvas shows it. The game
// canvas stays on top of the screen canvas, and takes the mouse and the keys. Without a picture it is not visible.
let screenContext = null;
function setPresent(present) {
  element('game').classList.toggle('canvas-present', present === 'canvas');
}

function showFrame(frame) {
  const screen = element('screen');
  if (screen.width !== frame.width || screen.height !== frame.height) {
    screen.width = frame.width;
    screen.height = frame.height;
  }
  screenContext ??= screen.getContext('bitmaprenderer');
  screenContext.transferFromImageBitmap(frame);
}

async function choose(entries, source) {
  chosen = entries;
  const check = checkInstall(entries);
  const list = element('check-list');
  list.replaceChildren();
  const line = (text, good) => {
    const item = document.createElement('li');
    item.textContent = text;
    item.className = good ? 'good' : 'bad';
    list.append(item);
  };
  line(`${entries.length} files of the game, from ${source}.`, entries.length > 0);
  for (const missing of check.missing) {
    line(`The folder does not have ${missing}`, false);
  }
  if (check.missing.length === 0) {
    line('The folder has all the parts of the game that the check looks for.', true);
  }
  element('check-version').textContent = check.version;
  element('check').hidden = false;
  setPlayEnabled(false);
  log(`The page has ${entries.length} files from ${source}. Missing: ${check.missing.join(', ') || 'none'}.`);
  if (check.missing.length === 0 && element('keep-copy').checked && source !== 'the copy in this browser') {
    await copyToStorage(entries);
  }
  setPlayEnabled(check.missing.length === 0);
  if (check.missing.length === 0 && new URLSearchParams(location.search).has('autoplay')) {
    start();
  }
}

// The Play button starts the game as the desktop game starts. The Join Online Game button starts the game at the
// multiplayer front end. The page parameter "multiplayer" shows only the second button.
function setPlayEnabled(enabled) {
  element('play').disabled = !enabled;
  element('play-online').disabled = !enabled;
}

// The game draws with WebGPU, and with WebGL 2 when the browser has no WebGPU. The page asks the browser for a WebGPU
// adapter before the game starts. When it gets none, the game starts with WebGL 2 at once. The page parameter
// "renderer" (webgpu or webgl2) selects the renderer, and a renderer on the game's command line ("args") wins.
/** The adapter of WebGPU in a few words, from the information that the browser gives */
function describeGpuAdapter(adapter) {
  const info = adapter.info || {};
  const part = (value) => value || 'not given';
  return `WebGPU: vendor ${part(info.vendor)}, architecture ${part(info.architecture)}, device ${part(info.device)}, ` +
    `description ${part(info.description)}`;
}

/** The renderer of WebGL 2 in a few words. The browser gives the names of the GPU only with its debug extension. */
function describeWebGl() {
  try {
    const context = document.createElement('canvas').getContext('webgl2');
    if (!context) {
      return 'WebGL 2: no context';
    }
    const debug = context.getExtension('WEBGL_debug_renderer_info');
    const vendor = context.getParameter(debug ? debug.UNMASKED_VENDOR_WEBGL : context.VENDOR);
    const renderer = context.getParameter(debug ? debug.UNMASKED_RENDERER_WEBGL : context.RENDERER);
    const lose = context.getExtension('WEBGL_lose_context');
    if (lose) {
      lose.loseContext();
    }
    return `WebGL 2: vendor ${vendor}, renderer ${renderer}`;
  } catch (error) {
    return `WebGL 2: no information (${error})`;
  }
}

/** Find the adapter of the renderer for the diagnostics, and write it to the log */
async function describeAdapter(backend) {
  if (backend === 'WebGPU' && navigator.gpu) {
    try {
      const adapter = await navigator.gpu.requestAdapter({ powerPreference: 'high-performance' });
      diagnostics.adapter = adapter ? describeGpuAdapter(adapter) : 'WebGPU: no adapter';
    } catch (error) {
      diagnostics.adapter = `WebGPU: the adapter request failed (${error})`;
    }
  } else {
    diagnostics.adapter = describeWebGl();
  }
  pageLog(`Adapter: ${diagnostics.adapter}`);
}

/** Write the size of the game canvas to the log */
function logCanvasSize() {
  const canvas = element('canvas');
  const box = canvas.getBoundingClientRect();
  diagnostics.canvas = `${Math.round(box.width)} x ${Math.round(box.height)} CSS pixels, device pixel ratio ` +
    `${window.devicePixelRatio}, window ${window.innerWidth} x ${window.innerHeight}`;
  pageLog(`Canvas: ${diagnostics.canvas}`);
}

/** The page writes the changes of its size, of its visibility and of its life to the log, for the diagnostics */
function watchPage() {
  logCanvasSize();
  let resizeTimer;
  window.addEventListener('resize', () => {
    clearTimeout(resizeTimer);
    resizeTimer = setTimeout(logCanvasSize, 1000);
  });
  // A hidden tab gets few timers and display frames, thus the game sends no pings and the server can close the socket
  document.addEventListener('visibilitychange', () => pageLog(`The page is ${document.visibilityState}`));
  document.addEventListener('freeze', () => pageLog('The browser froze the page'));
  document.addEventListener('resume', () => pageLog('The browser resumed the page'));
  window.addEventListener('pagehide', () => pageLog('The page goes away'));
}

async function chooseRenderer(parameters, gameArguments) {
  if (gameArguments.some((argument) => argument === '-b' || argument.startsWith('--backend-type'))) {
    return { backend: undefined, reason: 'the game arguments select the renderer' };
  }
  const forced = (parameters.get('renderer') || '').toLowerCase();
  if (forced === 'webgl2' || forced === 'webgl') {
    return { backend: 'OpenGLES', reason: 'the page parameter selects WebGL 2' };
  }
  if (forced === 'webgpu') {
    return { backend: 'WebGPU', reason: 'the page parameter selects WebGPU' };
  }
  if (forced !== '') {
    log(`The page does not know the renderer "${forced}". Use webgpu or webgl2.`);
  }
  if (!navigator.gpu) {
    return { backend: 'OpenGLES', reason: 'the browser has no WebGPU' };
  }
  try {
    if (await navigator.gpu.requestAdapter({ powerPreference: 'high-performance' })) {
      return { backend: 'WebGPU', reason: 'the browser gives a WebGPU adapter' };
    }
  } catch (error) {
    log(`The WebGPU adapter request failed: ${error}`);
  }
  return { backend: 'OpenGLES', reason: 'the browser gives no WebGPU adapter' };
}

// A test script can give the game more files: each page parameter "tool-file" names a file on the page's own server.
// The game reads it at /game/tool-files/<name>, beside the game folder. For example the commands of a tool's run.
async function addToolFiles(parameters) {
  for (const name of parameters.getAll('tool-file')) {
    if (!/^[\w.-]+$/.test(name)) {
      log(`The page does not take the tool file "${name}": use letters, digits, ".", "-" and "_" only.`);
      continue;
    }
    const response = await fetch(name, { cache: 'no-store' });
    if (!response.ok) {
      log(`The page cannot get the tool file ${name}: ${response.status}`);
      continue;
    }
    const file = new File([await response.blob()], name);
    chosen = chosen.filter((entry) => entry.path !== `tool-files/${name}`);
    chosen.push({ file, path: `tool-files/${name}` });
    log(`The game gets the tool file ${name} at /game/tool-files/${name} (${file.size} bytes).`);
  }
}

async function start(online = false) {
  if (!chosen) {
    return;
  }
  // The page finds JSPI by its feature, not by the name of the browser
  if (typeof WebAssembly.Suspending !== 'function') {
    showMessage('The page cannot start the game',
      'This browser does not have JSPI (WebAssembly JavaScript Promise Integration), which the game needs. ' +
      'Use a browser with JSPI support. For Safari, use version 27 or later.');
    return;
  }
  setPlayEnabled(false);
  const storage = (await storageRoot()) !== undefined;
  if (storage && navigator.storage.persist) {
    // Ask the browser to keep the saves when it needs space
    navigator.storage.persist().then((kept) => log(`The browser keeps the saves when it needs space: ${kept}`));
  }
  const parameters = new URLSearchParams(location.search);
  const developer = await developerTools;
  if (developer) {
    await addToolFiles(parameters);
  } else if (parameters.has('args') || parameters.has('tool-file')) {
    log('This build has no developer tools: the page ignores the parameters "args" and "tool-file".');
  }
  const list = chosen.map((entry, index) => `${index}\t${entry.file.size}\t${entry.path}`).join('\n');
  // The page takes the arguments of a player from a list (launcher-arguments.js), and "args" in a developer build.
  // With no profile, the multiplayer front end asks for the new player's name first.
  const gameArguments = playerArguments(parameters, developer, online);
  const renderer = await chooseRenderer(parameters, gameArguments);
  if (renderer.backend) {
    gameArguments.push('--backend-type', renderer.backend);
  }
  log(`The game gets the arguments: ${gameArguments.join(' ') || '(none)'}`);
  diagnostics.renderer = `${renderer.backend === 'OpenGLES' ? 'WebGL 2' : renderer.backend ?? 'from the game arguments'}, ` +
    `because ${renderer.reason}`;
  log(`Renderer: ${diagnostics.renderer}.`);
  describeAdapter(renderer.backend);
  const version = await buildVersion;
  element('chooser').hidden = true;
  element('game').hidden = false;
  element('build-badge').hidden = false;
  document.body.classList.add('playing');
  watchPage();
  const canvas = element('canvas');
  canvas.addEventListener('contextmenu', (event) => event.preventDefault());
  // SDL takes the size of a window that the player can change from the size of the canvas on the page. The canvas fills
  // the page (page.css). A size on the command line gives the canvas that fixed size, for the tests.
  const size = (short, long) => {
    const at = gameArguments.findIndex((argument) => argument === short || argument === long);
    return at >= 0 && at + 1 < gameArguments.length ? Number(gameArguments[at + 1]) : undefined;
  };
  const width = size('-W', '--width');
  const height = size('-H', '--height');
  if (width && height) {
    canvas.style.width = `${width}px`;
    canvas.style.height = `${height}px`;
  }
  // The Join Online Game button over the game asks the game for the multiplayer front end. The game takes the request
  // at its next frame, also while a cut scene has the cinema bars in.
  const requests = new Int32Array(new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT));
  element('join-online').addEventListener('click', (event) => {
    Atomics.store(requests, 0, 1);
    log('Join Online Game: the page asks the game for the multiplayer front end.');
    // The keys go to the game again, not to the button
    event.currentTarget.blur();
  });
  // The game thread starts each frame at a display frame of the page. The page counts its display frames in a shared
  // array and wakes the threads that wait for the count to change. The display frames of a worker thread do not always
  // follow the display (see the frame wait of the web platform code).
  const displayFrames = new Int32Array(new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT));
  const countDisplayFrame = () => {
    Atomics.add(displayFrames, 0, 1);
    Atomics.notify(displayFrames, 0);
    requestAnimationFrame(countDisplayFrame);
  };
  requestAnimationFrame(countDisplayFrame);
  globalThis.Module = {
    canvas,
    arguments: gameArguments,
    print: log,
    printErr: log,
    openblackWeb: {
      folder: { list, files: chosen.map((entry) => entry.file) }, storage, version: version.file, showFrame, setPresent,
      requests, displayFrames,
    },
    onExit: (code) => log(`The game stopped with the code ${code}.`),
    onAbort: (reason) => gameFailed(String(reason)),
  };
  // An error on a game thread that the game does not catch, such as a WebAssembly trap, comes to the page as an error.
  // The page ignores the error of the size watch of the browser, which is harmless.
  window.addEventListener('error', (event) => {
    // A game thread error comes as an error event, which has the message of the error
    const reason = (event.error && event.error.message) || event.message || String(event.error);
    if (!reason.includes('ResizeObserver')) {
      gameFailed(reason);
    }
  });
  const script = document.createElement('script');
  script.src = 'openblack.js';
  document.body.append(script);
}

async function main() {
  buildVersion.then((version) => {
    showVersion(version);
    log(`openblack build ${version.text}`);
  });
  // While the game plays, the log is a panel over the game, which the badge opens and closes
  // The whole game log as a file, for a bug report
  element('log-save').addEventListener('click', (event) => {
    download(`openblack_log_${fileTime()}.txt`, logLines.join('\n'));
    // The keys go to the game again, not to the button
    event.currentTarget.blur();
  });
  // The report of the connection log for a bug report, also while the game plays
  element('copy-diagnostics').addEventListener('click', (event) => {
    const button = event.currentTarget;
    copyDiagnostics(button);
    // The keys go to the game again, not to the button
    button.blur();
  });
  element('log-toggle').addEventListener('click', (event) => {
    const open = document.body.classList.toggle('log-open');
    logElement.parentElement.open = open;
    // The keys go to the game again, not to the button
    event.currentTarget.blur();
  });

  if (!globalThis.crossOriginIsolated) {
    showMessage('The page cannot start the game',
      'The site does not send the headers that the game threads need (COOP and COEP).');
  }

  const pick = element('pick-folder');
  if ('showDirectoryPicker' in globalThis) {
    pick.addEventListener('click', async () => {
      try {
        const folder = await showDirectoryPicker({ id: 'black-and-white', mode: 'read' });
        await keepHandle(folder);
        await choose(await readHandle(folder), `the folder ${folder.name}`);
      } catch (error) {
        if (error.name !== 'AbortError') {
          showMessage('The page cannot read the folder', String(error));
        }
      }
    });
    const kept = await keptHandle();
    if (kept) {
      const again = element('use-last-folder');
      again.textContent = `Use the folder ${kept.name} again`;
      again.hidden = false;
      again.addEventListener('click', async () => {
        if ((await kept.requestPermission({ mode: 'read' })) === 'granted') {
          await choose(await readHandle(kept), `the folder ${kept.name}`);
        } else {
          showMessage('No permission', 'The browser did not give the page permission to read the folder.');
        }
      });
    }
  } else {
    pick.addEventListener('click', () => element('folder-input').click());
  }

  element('folder-input').addEventListener('change', (event) =>
    choose(readFileList(event.target.files), 'the file dialog'));

  const drop = element('drop-zone');
  drop.addEventListener('dragover', (event) => {
    event.preventDefault();
    drop.classList.add('active');
  });
  drop.addEventListener('dragleave', () => drop.classList.remove('active'));
  drop.addEventListener('drop', async (event) => {
    event.preventDefault();
    drop.classList.remove('active');
    // Get the folders before the first wait: the list of dropped items is empty after the event
    const folders = [...event.dataTransfer.items]
      .map((item) => item.webkitGetAsEntry && item.webkitGetAsEntry())
      .filter((entry) => entry && entry.isDirectory);
    const entries = [];
    for (const folder of folders) {
      await readDroppedFolder(folder, '', entries);
    }
    await choose(entries, 'the dropped folder');
  });

  showCrashReports('Crash reports', 'The browser keeps these crash reports of earlier games.');

  const copy = await keptCopy();
  if (copy) {
    const useCopy = element('use-copy');
    useCopy.hidden = false;
    useCopy.addEventListener('click', async () => choose(await readHandle(copy), 'the copy in this browser'));
  }
  if (!(await storageRoot())) {
    element('keep-copy-label').hidden = true;
  }

  element('play').addEventListener('click', () => start(false));
  element('play-online').addEventListener('click', () => start(true));
  if (parameterIsOn(new URLSearchParams(location.search), 'multiplayer')) {
    element('play').hidden = true;
  }
}

main();
