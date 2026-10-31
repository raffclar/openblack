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

function log(text) {
  if (text.startsWith(k_ChecksumTag)) {
    checksumLines.push(text.slice(k_ChecksumTag.length));
    return;
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

/** Show a message on the page. The game calls this too, for an error that stops it. */
function showMessage(title, text) {
  const box = document.createElement('div');
  box.className = 'message';
  const heading = document.createElement('strong');
  heading.textContent = title;
  box.append(heading, document.createTextNode(text));
  element('messages').append(box);
  console.error(`openblack: ${title}: ${text}`);
}
globalThis.openblackShowMessage = showMessage;

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
  if (typeof WebAssembly.Suspending !== 'function') {
    showMessage('The page cannot start the game',
      'This browser does not have JSPI (WebAssembly JavaScript Promise Integration), which the game needs.');
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
  log(`Renderer: ${renderer.backend === 'OpenGLES' ? 'WebGL 2' : renderer.backend ?? 'from the game arguments'}, ` +
    `because ${renderer.reason}.`);
  const version = await buildVersion;
  element('chooser').hidden = true;
  element('game').hidden = false;
  element('build-badge').hidden = false;
  document.body.classList.add('playing');
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
  globalThis.Module = {
    canvas,
    arguments: gameArguments,
    print: log,
    printErr: log,
    openblackWeb: {
      folder: { list, files: chosen.map((entry) => entry.file) }, storage, version: version.file, showFrame, setPresent,
      requests,
    },
    onExit: (code) => log(`The game stopped with the code ${code}.`),
  };
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
