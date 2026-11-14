// openblack web page: the game files that the site sends to an account with the mark for them.
//
// The site gives the list of the game files at /assets/index.json, with the size and the SHA-256 of each file, and
// each file at /assets/files/<path>. An account without the mark gets 401 or 403, and the page shows the folder picker.
// The page keeps each file in the private storage of the site (OPFS), with its SHA-256 as its name. Thus a second
// visit downloads only the files that changed. The page checks the size and the SHA-256 of each file that it gets.
'use strict';

// The address of the list of the game files, and the start of the address of each file
const k_AssetIndexAddress = '/assets/index.json';
const k_AssetFilesAddress = '/assets/files/';
// The version of the list that the page reads
const k_AssetIndexFormat = 1;
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
 * The game files in the text of the list. Gives { archive, totalBytes, files }, where each file has a path, a size and
 * a sha256. Throws an error when the list is not correct. The game finds its files with no letter case, so two paths
 * that have only a different letter case are an error.
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
  if (index.format !== k_AssetIndexFormat) {
    throw new Error(`the list of the game files has the format ${index.format}, not ${k_AssetIndexFormat}`);
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
    return { path, size: entry.size, sha256: entry.sha256 };
  });
  const totalBytes = files.reduce((sum, file) => sum + file.size, 0);
  if (index.total_bytes !== undefined && index.total_bytes !== totalBytes) {
    throw new Error(`the list of the game files gives ${index.total_bytes} bytes, but its files have ${totalBytes}`);
  }
  return { archive: typeof index.archive_sha256 === 'string' ? index.archive_sha256 : '', totalBytes, files };
}

/**
 * What the page does with the files that it keeps, for the files of the list. The argument kept is a map from the name
 * of each kept file to its size. Gives:
 * - download: one entry { sha256, size, path } for each content that the page does not keep, with the first path,
 * - ready: the SHA-256 of each content that the page keeps with the correct size,
 * - remove: the names of the kept files that the list does not use or that have an incorrect size.
 */
function planAssetCache(files, kept) {
  const needed = new Map();
  for (const file of files) {
    if (!needed.has(file.sha256)) {
      needed.set(file.sha256, file);
    }
  }
  const ready = new Set();
  const remove = [];
  for (const [name, size] of kept) {
    const file = needed.get(name);
    if (file !== undefined && size === file.size) {
      ready.add(name);
    } else {
      remove.push(name);
    }
  }
  const download = [...needed.values()]
    .filter((file) => !ready.has(file.sha256))
    .map((file) => ({ sha256: file.sha256, size: file.size, path: file.path }));
  return { download, ready, remove };
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
  if (at !== file.size) {
    throw new Error(`the site gave ${at} bytes for ${file.path}, not ${file.size}`);
  }
  const sha256 = hexText(await digest('SHA-256', bytes));
  if (sha256 !== file.sha256) {
    throw new Error(`the SHA-256 of ${file.path} is ${sha256}, not ${file.sha256}`);
  }
  return bytes;
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
async function keepAssets(index, { fetchFile, digest, root, wanted = () => true, progress = () => {} }) {
  const files = index.files.filter((file) => wanted(file.path));
  const folder = await root.getDirectoryHandle(k_AssetCacheFolder, { create: true });
  const plan = planAssetCache(files, await keptAssets(folder));
  // Remove the old files first, so that the new files have their space
  for (const name of plan.remove) {
    await folder.removeEntry(name, { recursive: true });
  }
  const state = {
    bytes: 0,
    totalBytes: plan.download.reduce((sum, file) => sum + file.size, 0),
    files: 0,
    totalFiles: plan.download.length,
  };
  progress({ ...state });
  const queue = [...plan.download];
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
  await Promise.all(Array.from({ length: k_AssetDownloads }, getNext));
  const contents = new Map();
  for (const file of files) {
    if (!contents.has(file.sha256)) {
      const content = await (await folder.getFileHandle(file.sha256)).getFile();
      if (content.size !== file.size) {
        throw new Error(`the kept copy of ${file.path} has ${content.size} bytes, not ${file.size}`);
      }
      contents.set(file.sha256, content);
    }
  }
  return files.map((file) => ({ path: file.path, file: contents.get(file.sha256) }));
}

if (typeof module !== 'undefined') {
  module.exports = {
    isSafeAssetPath, parseAssetIndex, planAssetCache, assetAddress, hexText, refusalReason, fetchAssetIndex, fetchAsset,
    keepAssets,
    k_AssetIndexAddress, k_AssetCacheFolder,
  };
}
