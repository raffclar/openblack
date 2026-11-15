// openblack web page: the saved games and the creature files of the player on the site.
//
// The game keeps its Profiles folder in the private storage of the site in this browser (OPFS). When the page is the
// game frame of the site's /play/ page and a player is signed in on the site, the page also keeps a copy of each saved
// game file, creature file and the list of profiles on the site, for the account of the player:
// - Before the game starts, the page gets the list of the files on the site, and brings back each file that this
//   browser does not have, as in a new browser or after the browser cleared the storage. It never replaces a file that
//   the browser has.
// - When the game writes a file, the page sends the new file to the site in the background. A send that fails tries
//   again at the next check, and no send stops a frame of the game.
// - The page shows the list of the files on the site, each with a Download button. The Upload button shows but does
//   not work yet.
// The page asks the site's /play/ page around it, which asks the site with the site session. The page gives no address
// to that page, only the kind of question.
'use strict';

// The folder of the Profiles folder of the game in the private storage of the site
const k_CloudProfiles = ['openblack', 'Profiles'];
// The time between two checks for new files while the game plays, and the wait after the game wrote files
const k_CloudCheckMs = 60000;
const k_CloudNoticeMs = 2000;
// The longest wait for an answer of the site's page
const k_CloudAskMs = 120000;
// The longest wait of the start of the game for the files that come back
const k_CloudRestoreWaitMs = 120000;
// The key of the list of the files that wait for a send, in the storage of this browser
const k_CloudWaitingKey = 'openblack-cloud-waiting';
// The most entries of a folder that the page reads, and the deepest folder
const k_CloudMostEntries = 2000;
const k_CloudDeepest = 5;

// The places where the game writes the files that the site keeps: the names of each kind and the folders they are in.
// The site has the same rules (bwgame-service, bwgame/player_files.py) and checks each file again.
const k_CloudPlayer = '[A-Za-z0-9_]{1,64}';
const k_CloudSlot = new RegExp(`^${k_CloudPlayer}/Saved Games/[0-9]{1,2}(/[0-9]{1,2})?$`);
const k_CloudPlaces = [
  { name: /^[0-9]{1,2}\.obsav$/, folders: [k_CloudSlot] },
  { name: /^[0-9]{1,2}\.sav$/, folders: [k_CloudSlot] },
  { name: /^Script_[0-9]{1,2}\.sav$/, folders: [k_CloudSlot] },
  { name: /^SaveGame\.inf$/, folders: [k_CloudSlot] },
  { name: /^SavedCreature[0-9]{1,3}$/, folders: [k_CloudSlot] },
  { name: /^C[0-9a-f]{1,8}\.erc$/, folders: [/^Scripts\/CreatureMind$/] },
  { name: /^PhysiqueC[0-9a-f]{1,8}\.erc$/, folders: [/^Scripts\/CreatureMind$/] },
  { name: /^save_game_picture_(hires|lores)_[0-9]{1,2}\.raw$/,
    folders: [new RegExp(`^${k_CloudPlayer}/Saved Games/Pictures$`)] },
  { name: /^screenshots_(hires|lores)_[0-9]{1,2}_map_0\.raw$/,
    folders: [k_CloudSlot, new RegExp(`^${k_CloudPlayer}/Current$`)] },
  { name: /^creature\.lhp$/, folders: [new RegExp(`^${k_CloudPlayer}$`)] },
  { name: /^Profiles\.txt$/, folders: [/^$/] },
];

/** True when the game writes a file that the site keeps at this folder of the Profiles folder and with this name. */
function isCloudPlace(folder, name) {
  if (typeof folder !== 'string' || typeof name !== 'string') {
    return false;
  }
  if (folder !== '' && folder.split('/').some((part) => part === '' || part === '.' || part === '..')) {
    return false;
  }
  return k_CloudPlaces.some((place) => place.name.test(name) && place.folders.some((pattern) => pattern.test(folder)));
}

/** The key of a file by its place */
function cloudKey(folder, name) {
  return folder === '' ? name : `${folder}/${name}`;
}

/**
 * The entries of the site's list that the page keeps: each with a place where the game writes such a file, an id, a size
 * and a SHA-256. Nothing for a list that is not the site's.
 */
function parseCloudList(text) {
  let list;
  try {
    list = JSON.parse(text);
  } catch {
    return null;
  }
  if (!list || !Array.isArray(list.files)) {
    return null;
  }
  const files = list.files.filter((file) => file && isCloudPlace(file.folder, file.name) &&
    typeof file.id === 'string' && /^[0-9a-f-]{36}$/.test(file.id) && Number.isSafeInteger(file.size) &&
    file.size > 0 && typeof file.sha256 === 'string' && /^[0-9a-f]{64}$/.test(file.sha256));
  const number = (value) => (Number.isSafeInteger(value) && value >= 0 ? value : 0);
  return {
    files: files.map(({ id, folder, name, size, sha256, updated }) => ({ id, folder, name, size, sha256, updated })),
    usage: { files: number(list.usage && list.usage.files), bytes: number(list.usage && list.usage.bytes) },
    limits: { files: number(list.limits && list.limits.files), bytes: number(list.limits && list.limits.bytes) },
  };
}

/** The files of the site that this browser does not have: the page brings these back. */
function planRestore(remote, hasLocal) {
  return remote.filter((file) => !hasLocal(cloudKey(file.folder, file.name)));
}

/**
 * The local files that the page sends: a file that the site does not have, and a file that the game wrote since the
 * page started (its size or its time changed) or that waits from an earlier visit. The page then sends the file only
 * when its SHA-256 is not the site's. Thus a browser with an old copy of a file does not replace a newer file on the
 * site that another browser sent.
 */
function planSends(local, remoteByKey, firstSeen, waiting) {
  return local.filter((file) => {
    const key = cloudKey(file.folder, file.name);
    if (!remoteByKey.has(key)) {
      return true;
    }
    const first = firstSeen.get(key);
    return waiting.has(key) || first === undefined || first !== fileStamp(file);
  });
}

/** The size and the time of a local file, which change when the game writes the file */
function fileStamp(file) {
  return `${file.size}:${file.lastModified}`;
}

/** The hexadecimal text of bytes */
function cloudHex(bytes) {
  return Array.from(new Uint8Array(bytes), (byte) => byte.toString(16).padStart(2, '0')).join('');
}

/** Whether to try a send again later: no answer, too many sends, or a fault of the site. A refusal does not repeat. */
function sendAgainLater(status) {
  return status === 0 || status === 429 || status >= 500 && status !== 507;
}

/** Every file of a folder of the private storage, with its folder relative to the start. */
async function readCloudFolder(folder, prefix = '', files = [], depth = 0) {
  if (depth > k_CloudDeepest) {
    return files;
  }
  let count = 0;
  for await (const [name, handle] of folder.entries()) {
    if (++count > k_CloudMostEntries) {
      break;
    }
    if (handle.kind === 'directory') {
      await readCloudFolder(handle, prefix ? `${prefix}/${name}` : name, files, depth + 1);
    } else if (isCloudPlace(prefix, name)) {
      files.push({ folder: prefix, name, handle });
    }
  }
  return files;
}

/**
 * The saved games and creature files of the player on the site. The page gives its parts: ask (a question to the site's
 * page), profiles (the Profiles folder of the private storage, or undefined), write (writes the files that come back),
 * digest (SHA-256) and log.
 */
class CloudSaves {
  constructor({ ask, profiles, write, digest, log, store = undefined }) {
    this.ask = ask;
    this.profiles = profiles;
    this.write = write;
    this.digest = digest;
    this.log = log;
    this.store = store;
    this.remote = new Map();
    this.firstSeen = new Map();
    this.hashes = new Map();
    this.refused = new Set();
    this.waiting = new Set(this.readWaiting());
    this.state = 'unknown';
    this.listing = null;
    this.checking = null;
    this.checkAgain = false;
    this.restoreStopped = false;
    this.restoring = true;
    this.onChange = () => {};
  }

  readWaiting() {
    try {
      const text = this.store && this.store.getItem(k_CloudWaitingKey);
      const keys = text ? JSON.parse(text) : [];
      return Array.isArray(keys) ? keys.filter((key) => typeof key === 'string') : [];
    } catch {
      return [];
    }
  }

  keepWaiting() {
    try {
      if (this.store) {
        this.store.setItem(k_CloudWaitingKey, JSON.stringify([...this.waiting]));
      }
    } catch {
      // The browser does not keep the list: the files wait only while the page is open
    }
  }

  /** Gets the list of the site. Returns false when the site keeps no files for this page. */
  async list() {
    const reply = await this.ask({ op: 'list' });
    if (reply.status === 401 || reply.status === 403) {
      this.state = 'signed-out';
      return false;
    }
    const listing = reply.status === 200 ? parseCloudList(reply.body) : null;
    if (!listing) {
      this.state = reply.status === 0 ? 'no-site' : 'failed';
      return false;
    }
    this.state = 'ready';
    this.listing = listing;
    this.remote = new Map(listing.files.map((file) => [cloudKey(file.folder, file.name), file]));
    this.onChange();
    return true;
  }

  /** The local files that the site keeps, with their File objects. A file that the game has open now is left out. */
  async localFiles() {
    if (!this.profiles) {
      return [];
    }
    const found = await readCloudFolder(this.profiles);
    const files = [];
    for (const entry of found) {
      try {
        const file = await entry.handle.getFile();
        files.push({ folder: entry.folder, name: entry.name, size: file.size, lastModified: file.lastModified, file });
      } catch {
        // The game writes the file now: the next check sends it
      }
    }
    return files;
  }

  /** Brings back the files of the site that this browser does not have. Returns the number of files written. */
  async restore() {
    try {
      return await this.restoreMissing();
    } finally {
      this.restoring = false;
    }
  }

  async restoreMissing() {
    if (!(await this.list()) || !this.profiles) {
      return 0;
    }
    const local = await this.localFiles();
    for (const file of local) {
      this.firstSeen.set(cloudKey(file.folder, file.name), fileStamp(file));
    }
    const have = new Set(local.map((file) => cloudKey(file.folder, file.name)));
    const missing = planRestore([...this.remote.values()], (key) => have.has(key));
    let written = 0;
    for (const file of missing) {
      if (this.restoreStopped) {
        break;
      }
      const reply = await this.ask({ op: 'get', file: file.id });
      if (reply.status !== 200 || !(reply.bytes instanceof ArrayBuffer) || reply.bytes.byteLength !== file.size ||
          cloudHex(await this.digest(reply.bytes)) !== file.sha256) {
        this.log(`The site did not give the file ${cloudKey(file.folder, file.name)} (${reply.status}).`);
        continue;
      }
      if (await this.write(file.folder, file.name, reply.bytes)) {
        written += 1;
      }
    }
    // The files that came back are as the site has them: the next check does not send them
    for (const file of await this.localFiles()) {
      this.firstSeen.set(cloudKey(file.folder, file.name), fileStamp(file));
    }
    if (written) {
      this.log(`The page brought back ${written} saved game and creature files from the site.`);
    }
    return written;
  }

  /** Sends the files that the game wrote. Only one check runs at a time; a call during a check asks for one more. */
  check() {
    if (this.restoring || (this.state !== 'ready' && this.state !== 'failed')) {
      return Promise.resolve();
    }
    if (this.checking) {
      this.checkAgain = true;
      return this.checking;
    }
    this.checking = (async () => {
      try {
        do {
          this.checkAgain = false;
          await this.sendNew();
        } while (this.checkAgain);
      } finally {
        this.checking = null;
      }
    })();
    return this.checking;
  }

  async sendNew() {
    // A list that failed before: the page asks again first
    if (this.state !== 'ready' && !(await this.list())) {
      return;
    }
    const local = await this.localFiles();
    let changed = false;
    for (const file of planSends(local, this.remote, this.firstSeen, this.waiting)) {
      const key = cloudKey(file.folder, file.name);
      const stamp = fileStamp(file);
      if (this.refused.has(`${key}:${stamp}`)) {
        continue;
      }
      let bytes;
      try {
        bytes = await file.file.arrayBuffer();
      } catch {
        // The game changed the file after the page read it: the next check sends it
        this.waiting.add(key);
        continue;
      }
      const hash = cloudHex(await this.digest(bytes));
      const remote = this.remote.get(key);
      if (remote && remote.sha256 === hash) {
        this.firstSeen.set(key, stamp);
        this.waiting.delete(key);
        continue;
      }
      this.waiting.add(key);
      this.keepWaiting();
      const reply = await this.ask({ op: 'put', folder: file.folder, name: file.name, bytes }, [bytes]);
      if (reply.status === 200 || reply.status === 201) {
        this.log(`The page sent ${key} to the site.`);
        this.firstSeen.set(key, stamp);
        this.waiting.delete(key);
        changed = true;
      } else if (sendAgainLater(reply.status)) {
        this.log(`The page could not send ${key} to the site (${reply.status}); it tries again later.`);
      } else {
        this.refused.add(`${key}:${stamp}`);
        this.waiting.delete(key);
        this.log(`The site did not take ${key} (${reply.status}): ${reply.body || ''}`.slice(0, 300));
      }
    }
    this.keepWaiting();
    if (changed) {
      await this.list();
    }
  }

  /** Stops the files that come back, when the game starts: the game then owns the Profiles folder. */
  stopRestore() {
    this.restoreStopped = true;
  }
}

/** Asks the site's /play/ page around this page. The answer has the status (0 with no answer), the body or the bytes. */
function askSitePage(question, transfer = []) {
  if (window.parent === window) {
    return Promise.resolve({ status: 0, body: '' });
  }
  askSitePage.next = (askSitePage.next || 0) + 1;
  const id = askSitePage.next;
  return new Promise((resolve) => {
    const timer = setTimeout(() => {
      window.removeEventListener('message', listen);
      resolve({ status: 0, body: '' });
    }, k_CloudAskMs);
    const listen = (event) => {
      if (event.source !== window.parent || event.origin !== window.location.origin) return;
      const reply = event.data;
      if (!reply || reply.type !== 'bw-player-files-reply' || reply.id !== id) return;
      clearTimeout(timer);
      window.removeEventListener('message', listen);
      resolve({ status: reply.status | 0, body: typeof reply.body === 'string' ? reply.body : '', bytes: reply.bytes });
    };
    window.addEventListener('message', listen);
    window.parent.postMessage({ type: 'bw-player-files', id, ...question }, window.location.origin, transfer);
  });
}

/** Writes the files that come back into the private storage, in a worker. A file that is there stays as it is. */
function writeInWorker(folder, name, bytes) {
  return new Promise((resolve) => {
    const worker = new Worker('cloud-saves-worker.js');
    worker.onmessage = (event) => {
      worker.terminate();
      resolve(Boolean(event.data && event.data.written));
    };
    worker.onerror = () => {
      worker.terminate();
      resolve(false);
    };
    worker.postMessage({ root: k_CloudProfiles, folder, name, bytes }, [bytes]);
  });
}

/** The Profiles folder of the private storage, made when it is not there. Undefined with no private storage. */
async function cloudProfilesFolder() {
  try {
    let folder = await navigator.storage.getDirectory();
    for (const part of k_CloudProfiles) {
      folder = await folder.getDirectoryHandle(part, { create: true });
    }
    return folder;
  } catch {
    return undefined;
  }
}

function megabytesText(bytes) {
  return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
}

/** Shows the files of the site in the section of the page, each with a Download button */
function showCloudFiles(cloud, element) {
  const state = element('cloud-saves-state');
  const list = element('cloud-saves-list');
  list.replaceChildren();
  if (cloud.state === 'signed-out') {
    state.textContent = 'Sign in on the site to keep your saved games and creatures on the site.';
    return;
  }
  if (cloud.state === 'no-site') {
    state.textContent = 'The saved games and creatures stay in this browser only: no page of the site is around ' +
      'this page.';
    return;
  }
  if (cloud.state !== 'ready' || !cloud.listing) {
    state.textContent = 'The site did not give the list of your saved games and creatures.';
    return;
  }
  const { usage, limits } = cloud.listing;
  state.textContent = `The site keeps ${usage.files} file${usage.files === 1 ? '' : 's'} for you ` +
    `(${megabytesText(usage.bytes)} of ${megabytesText(limits.bytes)}).`;
  for (const file of cloud.listing.files) {
    const item = document.createElement('li');
    const label = document.createElement('span');
    label.textContent = `${file.name} in ${file.folder || 'Profiles'} (${megabytesText(file.size)})`;
    const button = document.createElement('button');
    button.type = 'button';
    button.textContent = 'Download';
    button.addEventListener('click', async () => {
      button.disabled = true;
      const reply = await cloud.ask({ op: 'get', file: file.id });
      button.disabled = false;
      if (reply.status !== 200 || !(reply.bytes instanceof ArrayBuffer)) {
        cloud.log(`The site did not give the file ${file.name} (${reply.status}).`);
        return;
      }
      const link = document.createElement('a');
      link.href = URL.createObjectURL(new Blob([reply.bytes], { type: 'application/octet-stream' }));
      link.download = file.name;
      link.click();
      setTimeout(() => URL.revokeObjectURL(link.href), 60000);
    });
    item.append(label, button);
    list.append(item);
  }
}

/**
 * Starts the saved games and creatures on the site for this page: the list, the files that come back, and the sends
 * when the game writes. Returns the CloudSaves and the promise of the files that come back.
 */
function startCloudSaves({ element, log }) {
  const section = element('cloud-saves');
  const cloud = new CloudSaves({
    ask: askSitePage,
    profiles: undefined,
    write: writeInWorker,
    digest: (bytes) => crypto.subtle.digest('SHA-256', bytes),
    log,
    store: (() => {
      try {
        return window.localStorage;
      } catch {
        return undefined;
      }
    })(),
  });
  cloud.onChange = () => showCloudFiles(cloud, element);
  const restored = (async () => {
    cloud.profiles = await cloudProfilesFolder();
    try {
      await cloud.restore();
    } catch (error) {
      log(`The page could not bring back the files from the site: ${error}`);
    }
    section.hidden = false;
    showCloudFiles(cloud, element);
  })();
  // The game tells the page after it wrote files of the profiles. The page also checks at times, for files that a
  // check did not send.
  let noticeTimer = 0;
  globalThis.openblackProfileWrite = () => {
    clearTimeout(noticeTimer);
    noticeTimer = setTimeout(() => cloud.check(), k_CloudNoticeMs);
  };
  setInterval(() => cloud.check(), k_CloudCheckMs);
  document.addEventListener('visibilitychange', () => {
    if (document.visibilityState === 'hidden') {
      cloud.check();
    }
  });
  return { cloud, restored };
}

/** Waits for the files that come back, at most k_CloudRestoreWaitMs, then stops them: the game owns the folder. */
async function waitForCloudRestore(started) {
  if (!started) {
    return;
  }
  await Promise.race([started.restored, new Promise((resolve) => setTimeout(resolve, k_CloudRestoreWaitMs))]);
  started.cloud.stopRestore();
}

if (typeof module !== 'undefined') {
  module.exports = {
    isCloudPlace, cloudKey, parseCloudList, planRestore, planSends, fileStamp, cloudHex, sendAgainLater,
    readCloudFolder, CloudSaves,
    k_CloudProfiles, k_CloudWaitingKey,
  };
}
