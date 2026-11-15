// openblack web page: the game files that the site sends to an account with the mark for them.
//
// The site gives the list of the game files at /assets/index.json, with the size and the SHA-256 of each file, and
// each file at /assets/files/<path>. An account without the mark gets 401 or 403, and the page shows the folder picker.
// The page keeps each file in the private storage of the site (OPFS), with its SHA-256 as its name. Thus a second
// visit downloads only the files that changed. The page checks the size and the SHA-256 of each file that it gets.
//
// Before the game starts, the page gets only the files that the start of the game reads. The game sees every file of
// the list. When the game first reads a file that the page does not have, the page gets it at once. After the start,
// the page gets the files that the game will probably read next, one at a time. A player can also get all the files
// before the start.
'use strict';

// The address of the list of the game files, and the start of the address of each file
const k_AssetIndexAddress = '/assets/index.json';
const k_AssetFilesAddress = '/assets/files/';
// The versions of the list that the page reads. The second version also gives the SHA-256 of each block of the large
// files that the site sends with no compression, so that the page can get only the blocks that the game reads.
const k_AssetIndexFormats = [1, 2];
// The largest block of a file that the page accepts in the list
const k_AssetMostBlockSize = 64 * 1024 * 1024;
// The folder of the private storage of the site that keeps the files that the page got
const k_AssetCacheFolder = 'game-files';
// The number of files that the page gets at the same time
const k_AssetDownloads = 4;
// The number of attempts for each file
const k_AssetAttempts = 2;
// The longest path and the most files that the page accepts in the list
const k_AssetPathLength = 512;
const k_AssetMostFiles = 20000;
// The longest reason of a refusal that the page writes in the log
const k_AssetReasonLength = 200;
// The number of files that the page gets at the same time after the start, for the files that the game will probably
// read next. A file that the game reads goes first.
const k_AssetPrefetchDownloads = 1;

// The files that the start of the game does not read: the landscapes of the other lands and of the network maps, the
// network maps, the files of the old citadel, and the animations and light maps of the miracles. The game reads the
// other files on the way to the first land: the films, the sound banks, the meshes, the textures and the scripts.
const k_AssetLaterRules = [
  /^data\/landscape\/(land[2-5t]|construct|mpm_|twogods|threegods|fourgods)[^/]*$/,
  /^data\/landscape\/multi_player\//,
  /^online maps\//,
  /^data\/citadel\/engine_old\//,
  /^data\/spells\/(anims|lightmaps)\//,
];
// The files that the page gets first after the start, because the first land can read them in its play: the
// animations and the light maps of the miracles. The landscapes of the other lands come when the game starts a land.
const k_AssetSoonRules = [
  /^data\/spells\//,
];
// The music banks. The start of the game reads only the list of the parts at the start of each bank, and the music
// reads the parts of one bank while it plays. Thus the page gets only the first block of each bank before the start.
const k_AssetHeaderRules = [
  /^audio\/music\/.*\.sad$/,
];
// The number of blocks after a block that the game reads, which the page gets next, so that the music plays on
const k_AssetReadAheadBlocks = 2;

const k_Sha256Pattern = /^[0-9a-f]{64}$/;

/**
 * True when the path of a game file is safe in the game folder: a relative path with "/" between its parts, with no
 * empty part, no "." or "..", no "\", no ":" and no control character.
 */
function isSafeAssetPath(path) {
  if (typeof path !== 'string' || path.length === 0 || path.length > k_AssetPathLength) {
    return false;
  }
  if (/[\\:\u0000-\u001f\u007f]/.test(path)) {
    return false;
  }
  return path.split('/').every((part) => part !== '' && part !== '.' && part !== '..');
}

/** True when the value is a size of a file: an integer from zero that JavaScript keeps with no error */
function isFileSize(value) {
  return Number.isSafeInteger(value) && value >= 0;
}

/**
 * The game files in the text of the list. Gives { archive, totalBytes, blockSize, files }, where each file has a path, a
 * size and a sha256. A file of the second version can also have blocks: the SHA-256 of each part of blockSize bytes, in
 * order. Throws an error when the list is not correct. The game finds its files with no letter case, so two paths that
 * have only a different letter case are an error.
 */
function parseAssetIndex(text) {
  let index;
  try {
    index = JSON.parse(text);
  } catch (error) {
    throw new Error(`the list of the game files is not JSON: ${error.message}`);
  }
  if (index === null || typeof index !== 'object' || Array.isArray(index)) {
    throw new Error('the list of the game files is not an object');
  }
  if (!k_AssetIndexFormats.includes(index.format)) {
    throw new Error(`the list of the game files has the format ${index.format}, not ${k_AssetIndexFormats.join(' or ')}`);
  }
  const blockSize = index.format >= 2 && index.block_size !== undefined ? index.block_size : 0;
  if (!Number.isSafeInteger(blockSize) || blockSize < 0 || blockSize > k_AssetMostBlockSize) {
    throw new Error(`the list of the game files has the block size ${index.block_size}`);
  }
  const listed = index.files;
  if (listed === null || typeof listed !== 'object' || Array.isArray(listed)) {
    throw new Error('the list of the game files has no files');
  }
  const paths = Object.keys(listed);
  if (paths.length === 0 || paths.length > k_AssetMostFiles) {
    throw new Error(`the list of the game files has ${paths.length} files`);
  }
  const seen = new Set();
  const files = paths.map((path) => {
    const entry = listed[path];
    if (!isSafeAssetPath(path)) {
      throw new Error(`the list of the game files has a path that is not safe: ${JSON.stringify(path)}`);
    }
    const lower = path.toLowerCase();
    if (seen.has(lower)) {
      throw new Error(`the list of the game files has the path ${path} two times`);
    }
    seen.add(lower);
    if (entry === null || typeof entry !== 'object' || !isFileSize(entry.size) ||
      typeof entry.sha256 !== 'string' || !k_Sha256Pattern.test(entry.sha256)) {
      throw new Error(`the list of the game files has no correct size and SHA-256 for ${path}`);
    }
    if (entry.blocks === undefined || index.format < 2) {
      return { path, size: entry.size, sha256: entry.sha256 };
    }
    if (blockSize === 0 || !Array.isArray(entry.blocks) || entry.blocks.length !== Math.ceil(entry.size / blockSize) ||
      entry.size === 0 || !entry.blocks.every((block) => typeof block === 'string' && k_Sha256Pattern.test(block))) {
      throw new Error(`the list of the game files has no correct blocks for ${path}`);
    }
    return { path, size: entry.size, sha256: entry.sha256, blocks: [...entry.blocks] };
  });
  const totalBytes = files.reduce((sum, file) => sum + file.size, 0);
  if (index.total_bytes !== undefined && index.total_bytes !== totalBytes) {
    throw new Error(`the list of the game files gives ${index.total_bytes} bytes, but its files have ${totalBytes}`);
  }
  return { archive: typeof index.archive_sha256 === 'string' ? index.archive_sha256 : '', totalBytes, blockSize, files };
}

/** The first byte and the byte after the last of a block of a file */
function assetBlockBounds(file, blockSize, block) {
  const start = block * blockSize;
  return { start, end: Math.min(file.size, start + blockSize) };
}

/**
 * The first and the last block of a file that have the bytes from offset, for length bytes. Gives no value for a read
 * that has no byte of the file.
 */
function assetBlockRange(file, blockSize, offset, length) {
  const end = Math.min(file.size, offset + length);
  if (!(offset >= 0) || !(length > 0) || offset >= end) {
    return null;
  }
  return { first: Math.floor(offset / blockSize), last: Math.floor((end - 1) / blockSize) };
}

/**
 * What the page does with the files and the blocks that it keeps, for the files of the list. The argument kept is a
 * map from the name of each kept file to its size. The page keeps a whole file by its SHA-256, and a block by the
 * SHA-256 of the block. Gives:
 * - download: one entry { sha256, size, path } for each content that the page does not keep whole, with the first path,
 * - ready: the SHA-256 of each content that the page keeps with the correct size,
 * - blocks: the SHA-256 of each block that the page keeps, of the files that it does not keep whole,
 * - remove: the names of the kept files that the list does not use, that have an incorrect size, or that are the blocks
 *   of a file that the page keeps whole.
 */
function planAssetCache(files, kept, blockSize = 0) {
  const needed = new Map();
  for (const file of files) {
    if (!needed.has(file.sha256)) {
      needed.set(file.sha256, file);
    }
  }
  const ready = new Set();
  for (const [name, size] of kept) {
    const file = needed.get(name);
    if (file !== undefined && size === file.size) {
      ready.add(name);
    }
  }
  const neededBlocks = new Map();
  for (const file of needed.values()) {
    if (file.blocks && !ready.has(file.sha256)) {
      file.blocks.forEach((block, number) => {
        const bounds = assetBlockBounds(file, blockSize, number);
        neededBlocks.set(block, bounds.end - bounds.start);
      });
    }
  }
  const blocks = new Set();
  const remove = [];
  for (const [name, size] of kept) {
    if (ready.has(name)) {
      continue;
    }
    if (neededBlocks.get(name) === size) {
      blocks.add(name);
    } else {
      remove.push(name);
    }
  }
  const download = [...needed.values()]
    .filter((file) => !ready.has(file.sha256))
    .map((file) => ({ sha256: file.sha256, size: file.size, path: file.path }));
  return { download, ready, blocks, remove };
}

/** The address of a game file on the site, with each part of its path encoded */
function assetAddress(path) {
  return k_AssetFilesAddress + path.split('/').map(encodeURIComponent).join('/');
}

/** The text of the bytes, two hexadecimal digits for each byte */
function hexText(buffer) {
  return [...new Uint8Array(buffer)].map((byte) => byte.toString(16).padStart(2, '0')).join('');
}

/** The reason in the text of a refusal: the detail of a problem of the site, or else the text */
function refusalReason(text) {
  try {
    const problem = JSON.parse(text);
    if (problem && typeof problem.detail === 'string') {
      return problem.detail.slice(0, k_AssetReasonLength);
    }
  } catch {
    // The text is not JSON. The reason is the text.
  }
  return text.trim().slice(0, k_AssetReasonLength);
}

/**
 * Asks the site for the list of the game files. Gives { granted: true, index } when the account gets the files, or
 * { granted: false, status, reason } when the site refuses them or the list is not correct.
 */
async function fetchAssetIndex(fetchFile) {
  let response;
  try {
    response = await fetchFile(k_AssetIndexAddress, { cache: 'no-cache', credentials: 'same-origin' });
  } catch (error) {
    return { granted: false, status: 0, reason: String(error) };
  }
  const text = await response.text();
  if (!response.ok) {
    return { granted: false, status: response.status, reason: refusalReason(text) };
  }
  try {
    return { granted: true, index: parseAssetIndex(text) };
  } catch (error) {
    return { granted: false, status: response.status, reason: error.message };
  }
}

/**
 * Gets one game file. The body must have the size of the list, and its SHA-256 must agree with the list. Calls
 * received with the number of bytes of each part that arrives. Gives the bytes.
 */
async function fetchAsset(fetchFile, digest, file, received) {
  const response = await fetchFile(assetAddress(file.path), { cache: 'no-store', credentials: 'same-origin' });
  if (response.status !== 200) {
    throw new Error(`the site gave the status ${response.status} for ${file.path}`);
  }
  const bytes = new Uint8Array(file.size);
  let at = 0;
  const reader = response.body.getReader();
  for (;;) {
    const { done, value } = await reader.read();
    if (done) {
      break;
    }
    if (at + value.byteLength > file.size) {
      await reader.cancel();
      throw new Error(`the site gave more than ${file.size} bytes for ${file.path}`);
    }
    bytes.set(value, at);
    at += value.byteLength;
    received(value.byteLength);
  }
  const problem = await assetProblem(digest, file, bytes.subarray(0, at));
  if (problem !== null) {
    throw new Error(problem);
  }
  return bytes;
}

/** True when the start of the game reads the file at this path, so that the page gets it before the start */
function isStartAsset(path) {
  const lower = path.toLowerCase();
  return !k_AssetLaterRules.some((rule) => rule.test(lower));
}

/**
 * The numbers of the files that the page gets after the start, in the order to get them, when the game read the file at
 * the path just now. With no path, the files to get just after the start. The argument has: the files of the list in
 * the order of their numbers, with a path and a sha256 each, and a function that is true for the SHA-256 of each file
 * that the page has or gets now.
 * - After the start: the small files that the first land can read in its play.
 * - After the game read the landscape of a land: the landscape and the footpaths of the next land.
 * - After the game read a file of a network map: the other files of the same map.
 */
function planAssetPrefetch(files, { path = null, have = () => false }) {
  const numbers = [];
  const seen = new Set();
  const add = (test) => {
    files.forEach((file, number) => {
      if (!seen.has(file.sha256) && !have(file.sha256) && test(file.path.toLowerCase())) {
        seen.add(file.sha256);
        numbers.push(number);
      }
    });
  };
  if (path === null) {
    add((lower) => !isStartAsset(lower) && k_AssetSoonRules.some((rule) => rule.test(lower)));
    return numbers;
  }
  const lower = path.toLowerCase();
  const land = /^data\/landscape\/land([1-4])\.lnd$/.exec(lower);
  if (land) {
    const next = Number(land[1]) + 1;
    add((other) => other === `data/landscape/land${next}.lnd` || other === `data/landscape/land${next}.fot`);
    return numbers;
  }
  const map = /^(online maps\/|data\/landscape\/(multi_player\/)?)([^/]+?)\.[^./]+$/.exec(lower);
  if (map && !isStartAsset(lower)) {
    const name = map[3];
    add((other) => {
      const stem = /([^/]+?)\.[^./]+$/.exec(other);
      return stem !== null && stem[1] === name && !isStartAsset(other);
    });
  }
  return numbers;
}

/**
 * The check of a file that the page got against the list: the bytes must have the size of the list, and their SHA-256
 * must agree with the list. Gives no value when the file is correct, or else the reason.
 */
async function assetProblem(digest, file, bytes) {
  if (bytes.byteLength !== file.size) {
    return `the site gave ${bytes.byteLength} bytes for ${file.path}, not ${file.size}`;
  }
  const sha256 = hexText(await digest('SHA-256', bytes));
  return sha256 === file.sha256 ? null : `the SHA-256 of ${file.path} is ${sha256}, not ${file.sha256}`;
}

/**
 * What the page gets before the start: whole: the numbers of the files to get whole; blocks: { number, block } for
 * the first block of each music bank that the list gives in blocks. The files are the files of the list in the order
 * of their numbers.
 */
function planAssetStart(files) {
  const whole = [];
  const blocks = [];
  files.forEach((file, number) => {
    if (!isStartAsset(file.path)) {
      return;
    }
    if (file.blocks && k_AssetHeaderRules.some((rule) => rule.test(file.path.toLowerCase()))) {
      blocks.push({ number, block: 0 });
    } else {
      whole.push(number);
    }
  });
  return { whole, blocks };
}

/** The first byte, the last byte and the size of the file in a Content-Range header, or no value */
function parseContentRange(text) {
  const match = /^bytes (\d+)-(\d+)\/(\d+)$/.exec(typeof text === 'string' ? text.trim() : '');
  if (!match) {
    return null;
  }
  const [start, last, size] = match.slice(1).map(Number);
  return start <= last && last < size ? { start, last, size } : null;
}

/**
 * Gets one block of a game file with a range request. The site must give the bytes of the block in the file, and
 * their SHA-256 must agree with the list. Gives the bytes.
 */
async function fetchAssetBlock(fetchFile, digest, file, blockSize, block) {
  const { start, end } = assetBlockBounds(file, blockSize, block);
  const response = await fetchFile(assetAddress(file.path), {
    cache: 'no-store', credentials: 'same-origin', headers: { Range: `bytes=${start}-${end - 1}` },
  });
  if (response.status !== 206) {
    throw new Error(`the site gave the status ${response.status} for block ${block} of ${file.path}`);
  }
  const given = response.headers.get('Content-Range');
  const range = parseContentRange(given);
  if (!range || range.start !== start || range.last !== end - 1 || range.size !== file.size) {
    throw new Error(`the site gave the range ${given} for block ${block} of ${file.path}`);
  }
  const bytes = new Uint8Array(await response.arrayBuffer());
  const part = { path: `block ${block} of ${file.path}`, size: end - start, sha256: file.blocks[block] };
  const problem = await assetProblem(digest, part, bytes);
  if (problem !== null) {
    throw new Error(problem);
  }
  return bytes;
}

/**
 * The page's store of the game files for one game. It keeps each whole file by its SHA-256 and each block by the
 * SHA-256 of the block, in the folder of kept files. It gets the files and the blocks that the game asks for first,
 * at most k_AssetDownloads at a time. When nothing waits, it gets the files that the game will probably read next,
 * k_AssetPrefetchDownloads at a time. The options are:
 * - fetchFile, digest and folder: the fetch, the SHA-256 and the folder of the kept files (tests give fakes),
 * - blockSize: the size of the blocks of the list,
 * - kept: the kept File objects, by their names,
 * - share: a function that gets (number, block, file) for each file (block null) and each block that the page gets,
 * - log: a function that gets a line for the game log.
 */
class AssetStore {
  constructor(files, { fetchFile, digest, folder, blockSize = 0, kept = new Map(), share = () => {}, log = () => {} }) {
    Object.assign(this, { files, fetchFile, digest, folder, blockSize, kept, share, log });
    this.jobs = new Map();
    this.waiting = { now: [], later: [] };
    this.running = { now: 0, later: 0 };
    this.received = { files: 0, blocks: 0, bytes: 0 };
    // The SHA-256 of each file that the store joins from its blocks now
    this.joining = new Set();
    // The numbers of the files that have each content, and { number, block } of each block that has each content
    this.users = new Map();
    this.blockUsers = new Map();
    files.forEach((file, number) => {
      this.users.set(file.sha256, [...(this.users.get(file.sha256) ?? []), number]);
      (file.blocks ?? []).forEach((block, index) => {
        this.blockUsers.set(block, [...(this.blockUsers.get(block) ?? []), { number, block: index }]);
      });
    });
  }

  /** The kept File of the whole file, or no value */
  fileOf(number) {
    return this.kept.get(this.files[number].sha256) ?? null;
  }

  /** The kept File of each block of the file, with null for a block that the page does not have */
  blocksOf(number) {
    return (this.files[number].blocks ?? []).map((block) => this.kept.get(block) ?? null);
  }

  /**
   * Makes sure that the page has the bytes of the file from offset, for length bytes, before all the files that the
   * game will probably read next. Gives a promise that is done when the page has them and gave them to share. For a
   * file in blocks, the page then gets the next blocks for later.
   */
  async obtain(number, offset = 0, length = Infinity) {
    const file = this.files[number];
    if (this.fileOf(number)) {
      return;
    }
    const range = file.blocks ? assetBlockRange(file, this.blockSize, offset, length) : null;
    if (range === null) {
      await this.schedule(`file ${file.sha256}`, () => this.getWhole(number), 'now');
      return;
    }
    const wanted = [];
    for (let block = range.first; block <= range.last; block += 1) {
      wanted.push(this.getBlockJob(number, block, 'now'));
    }
    const ahead = Math.min(file.blocks.length - 1, range.last + k_AssetReadAheadBlocks);
    for (let block = range.last + 1; block <= ahead; block += 1) {
      this.getBlockJob(number, block, 'later').catch((error) => this.log(`The page cannot get ${error.message}`));
    }
    await Promise.all(wanted);
  }

  /** Gets the whole files after the files that the game asks for. A failure goes only to the log. */
  prefetch(numbers) {
    for (const number of numbers) {
      if (!this.fileOf(number)) {
        this.schedule(`file ${this.files[number].sha256}`, () => this.getWhole(number), 'later')
          .catch((error) => this.log(`The page cannot get ${this.files[number].path} for later: ${error.message}`));
      }
    }
  }

  getBlockJob(number, block, priority) {
    const sha256 = this.files[number].blocks[block];
    if (this.kept.has(sha256)) {
      return Promise.resolve();
    }
    return this.schedule(`block ${sha256}`, () => this.getBlock(number, block), priority);
  }

  /** Runs a job once for its key. A job that the game asks for goes before the jobs for later. */
  schedule(key, run, priority) {
    let job = this.jobs.get(key);
    if (job) {
      if (priority === 'now' && job.priority === 'later' && !job.started) {
        this.waiting.later = this.waiting.later.filter((other) => other !== job);
        job.priority = 'now';
        this.waiting.now.push(job);
        this.pump();
      }
      return job.promise;
    }
    job = { key, run, priority, started: false };
    job.promise = new Promise((resolve, reject) => {
      job.resolve = resolve;
      job.reject = reject;
    });
    this.jobs.set(key, job);
    this.waiting[priority].push(job);
    this.pump();
    return job.promise;
  }

  /** Starts the jobs that can run now: first the jobs of the game, then one job for later when nothing waits */
  pump() {
    const start = (job) => {
      const lane = job.priority;
      job.started = true;
      this.running[lane] += 1;
      (async () => {
        for (let attempt = 1; ; attempt += 1) {
          try {
            return await job.run();
          } catch (error) {
            if (attempt >= k_AssetAttempts) {
              throw error;
            }
          }
        }
      })().then(job.resolve, job.reject).finally(() => {
        this.jobs.delete(job.key);
        this.running[lane] -= 1;
        this.pump();
      });
    };
    while (this.running.now < k_AssetDownloads && this.waiting.now.length > 0) {
      start(this.waiting.now.shift());
    }
    while (this.waiting.now.length === 0 && this.running.now === 0 &&
      this.running.later < k_AssetPrefetchDownloads && this.waiting.later.length > 0) {
      start(this.waiting.later.shift());
    }
  }

  async write(name, bytes) {
    const handle = await this.folder.getFileHandle(name, { create: true });
    const writable = await handle.createWritable();
    await writable.write(bytes);
    await writable.close();
    const kept = await handle.getFile();
    this.kept.set(name, kept);
    return kept;
  }

  async getWhole(number) {
    const file = this.files[number];
    const bytes = await fetchAsset(this.fetchFile, this.digest, file, () => {});
    const kept = await this.write(file.sha256, bytes);
    this.received.files += 1;
    this.received.bytes += bytes.byteLength;
    for (const user of this.users.get(file.sha256)) {
      this.share(user, null, kept);
    }
  }

  async getBlock(number, block) {
    const file = this.files[number];
    const bytes = await fetchAssetBlock(this.fetchFile, this.digest, file, this.blockSize, block);
    const sha256 = file.blocks[block];
    const kept = await this.write(sha256, bytes);
    this.received.blocks += 1;
    this.received.bytes += bytes.byteLength;
    const done = new Set();
    for (const user of this.blockUsers.get(sha256)) {
      this.share(user.number, user.block, kept);
      done.add(user.number);
    }
    for (const user of done) {
      await this.joinBlocks(user);
    }
  }

  /**
   * When the page has all the blocks of a file, it checks the whole file against the list and keeps it whole. The
   * blocks stay in the folder until the next visit, because a game thread can still read them.
   */
  async joinBlocks(number) {
    const file = this.files[number];
    const blocks = this.blocksOf(number);
    if (this.fileOf(number) || this.joining.has(file.sha256) || blocks.some((block) => block === null)) {
      return;
    }
    // Two blocks can arrive at the same time. Only one of them joins the file.
    this.joining.add(file.sha256);
    try {
      const bytes = new Uint8Array(await new Blob(blocks).arrayBuffer());
      const problem = await assetProblem(this.digest, file, bytes);
      if (problem !== null) {
        this.log(`The page keeps the blocks of ${file.path}, but the whole file is not correct: ${problem}`);
        return;
      }
      const kept = await this.write(file.sha256, bytes);
      for (const user of this.users.get(file.sha256)) {
        this.share(user, null, kept);
      }
    } finally {
      this.joining.delete(file.sha256);
    }
  }
}

/** The size of each file in the folder of kept files, by its name */
async function keptAssets(folder) {
  const kept = new Map();
  for await (const [name, handle] of folder.entries()) {
    if (handle.kind === 'file') {
      kept.set(name, (await handle.getFile()).size);
    } else {
      kept.set(name, -1);
    }
  }
  return kept;
}

/**
 * Makes sure that the private storage of the site keeps each game file of the list, and gives the entries
 * { path, file } of the game files for the game. The options are:
 * - fetchFile and digest: the fetch and the SHA-256 of the browser (tests give fakes),
 * - root: the root folder of the private storage of the site,
 * - wanted: a function that is true for each path that the game reads,
 * - progress: a function that gets { bytes, totalBytes, files, totalFiles } after each part that arrives.
 */
async function keepAssets(index, options) {
  const { entries } = await streamAssets(index, { ...options, all: true });
  return entries.map((entry) => ({ path: entry.path, file: entry.file }));
}

/**
 * Makes sure that the private storage of the site keeps the game files of the list that the start of the game reads,
 * or all the game files with the option all. Gives the entries { path, size, file, blocks } of the game files for the
 * game, with file null for each file that the page does not have whole yet, and the store that gets the other files
 * and blocks. The options are those of keepAssets, and also all, share and log (see AssetStore).
 */
async function streamAssets(index, {
  fetchFile, digest, root, wanted = () => true, progress = () => {}, all = false, share = () => {}, log = () => {},
}) {
  const files = index.files.filter((file) => wanted(file.path));
  const blockSize = index.blockSize ?? 0;
  const folder = await root.getDirectoryHandle(k_AssetCacheFolder, { create: true });
  const plan = planAssetCache(files, await keptAssets(folder), blockSize);
  // Remove the old files first, so that the new files have their space
  for (const name of plan.remove) {
    await folder.removeEntry(name, { recursive: true });
  }
  const start = planAssetStart(files);
  const startShas = new Set(start.whole.map((number) => files[number].sha256));
  const download = all ? plan.download : plan.download.filter((file) => startShas.has(file.sha256));
  const blockNames = new Set();
  const startBlocks = all ? [] : start.blocks
    .map(({ number, block }) => ({ file: files[number], block }))
    .filter(({ file, block }) => {
      const name = file.blocks[block];
      const wantedBlock = !plan.ready.has(file.sha256) && !plan.blocks.has(name) && !blockNames.has(name);
      blockNames.add(name);
      return wantedBlock;
    });
  const blockBytes = ({ file, block }) => {
    const bounds = assetBlockBounds(file, blockSize, block);
    return bounds.end - bounds.start;
  };
  const state = {
    bytes: 0,
    totalBytes: download.reduce((sum, file) => sum + file.size, 0) +
      startBlocks.reduce((sum, next) => sum + blockBytes(next), 0),
    files: 0,
    totalFiles: download.length + startBlocks.length,
  };
  progress({ ...state });
  const queue = [...download];
  const getNext = async () => {
    for (let file = queue.shift(); file !== undefined; file = queue.shift()) {
      let bytes;
      for (let attempt = 1; ; attempt += 1) {
        let got = 0;
        try {
          bytes = await fetchAsset(fetchFile, digest, file, (count) => {
            got += count;
            state.bytes += count;
            progress({ ...state });
          });
          break;
        } catch (error) {
          state.bytes -= got;
          if (attempt >= k_AssetAttempts) {
            throw error;
          }
        }
      }
      const writable = await (await folder.getFileHandle(file.sha256, { create: true })).createWritable();
      await writable.write(bytes);
      await writable.close();
      state.files += 1;
      progress({ ...state });
    }
  };
  const blockQueue = [...startBlocks];
  const getNextBlock = async () => {
    for (let next = blockQueue.shift(); next !== undefined; next = blockQueue.shift()) {
      let bytes;
      for (let attempt = 1; ; attempt += 1) {
        try {
          bytes = await fetchAssetBlock(fetchFile, digest, next.file, blockSize, next.block);
          break;
        } catch (error) {
          if (attempt >= k_AssetAttempts) {
            throw error;
          }
        }
      }
      const handle = await folder.getFileHandle(next.file.blocks[next.block], { create: true });
      const writable = await handle.createWritable();
      await writable.write(bytes);
      await writable.close();
      state.bytes += bytes.byteLength;
      state.files += 1;
      progress({ ...state });
    }
  };
  await Promise.all(Array.from({ length: k_AssetDownloads }, getNext));
  await Promise.all(Array.from({ length: k_AssetDownloads }, getNextBlock));
  // The kept File of each whole file and each block that the folder has now with the correct size
  const sizes = new Map();
  for (const file of files) {
    sizes.set(file.sha256, file.size);
    (file.blocks ?? []).forEach((block, number) => sizes.set(block, blockBytes({ file, block: number })));
  }
  const kept = new Map();
  for (const [name, size] of await keptAssets(folder)) {
    if (sizes.get(name) === size) {
      kept.set(name, await (await folder.getFileHandle(name)).getFile());
    }
  }
  if (all) {
    for (const file of files) {
      if (!kept.has(file.sha256)) {
        throw new Error(`the page does not keep a correct copy of ${file.path}`);
      }
    }
  }
  const store = new AssetStore(files, { fetchFile, digest, folder, blockSize, kept, share, log });
  const entries = files.map((file, number) => ({
    path: file.path, size: file.size, file: store.fileOf(number), blocks: file.blocks ? store.blocksOf(number) : null,
  }));
  return { entries, store, blockSize };
}

if (typeof module !== 'undefined') {
  module.exports = {
    isSafeAssetPath, parseAssetIndex, planAssetCache, assetAddress, hexText, refusalReason, fetchAssetIndex, fetchAsset,
    keepAssets, streamAssets, isStartAsset, planAssetStart, planAssetPrefetch, assetProblem, assetBlockBounds,
    assetBlockRange, parseContentRange, fetchAssetBlock, AssetStore,
    k_AssetIndexAddress, k_AssetCacheFolder,
  };
}
