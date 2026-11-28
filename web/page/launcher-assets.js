// openblack web page: the game files that the site sends to an account with the mark for them.
//
// The site gives the list of the game files at /assets/index.json, with the size and the SHA-256 of each file, and
// each file at /assets/files/<path>. A list with "hashed_addresses" also gives each file at /assets/sha256/<SHA-256>,
// which the browser can keep for a long time with no check, because other bytes never have that address. An account
// without the mark gets 401 or 403, and the page shows the folder picker.
// The page keeps each file in the private storage of the site (OPFS), with its SHA-256 as its name. Thus a second
// visit downloads only the files that changed. Of a changed file in blocks, it downloads only the changed blocks, and
// takes the other blocks from the old file. The page checks the size and the SHA-256 of each file that it gets.
//
// Before the game starts, the page gets only the files of the boot phase: the files that the game reads before its
// first frame. The game sees every file of the list. The page then gets the files of the next phases of the start in
// the order that the game reads them, while the logos and the pre-intro film play. When the game reads a file that the
// page does not have, the game waits, and the page gets that file first. After the phases, the page gets the files that
// the game will probably read next, one at a time. A player can also get all the files before the start.
//
// When the land is ready, the page gets the other files in the background, one at a time, when nothing else waits: the
// music banks, the other lands, the network maps and the files of the creature species. The alignment music of the
// alignment that the game plays now comes before the other music banks. The player can turn this off.
// The page does this only when the browser keeps the storage, and when the browser has the space for the files.
//
// The list of the third version also gives packs: the bytes of two or more small files one after the other, at the
// address of the pack by its SHA-256. The page gets a pack in one request, checks each file in it, and keeps each file
// by its own SHA-256. Thus the start makes a small number of requests, and not one request for each small file.
//
// The page keeps only one copy of each file. When it joins the blocks of a file, it removes the blocks. It tells the
// browser not to keep a second copy of each file in its HTTP cache. When the browser has no more space, the game takes
// the next files from memory for this visit.
//
// The file asset-tiers.json of the page gives the phase of the game in which the game first reads each file. The tool
// tools/web-asset-tiers makes it from the traces of the game (launcher-asset-trace.js). With no correct file, the page
// uses the rules below, and gets all the files of the start before the game starts.
'use strict';

// The address of the list of the game files, and the start of the address of each file
const k_AssetIndexAddress = '/assets/index.json';
const k_AssetFilesAddress = '/assets/files/';
// The start of the address of each file by its SHA-256, when the list gives these addresses
const k_AssetHashedAddress = '/assets/sha256/';
// The versions of the list that the page reads. The second version also gives the SHA-256 of each block of the large
// files that the site sends with no compression, so that the page can get only the blocks that the game reads. The
// third version also gives the packs of the small files.
const k_AssetIndexFormats = [1, 2, 3];
// The page gets a pack, and not its files alone, when the files that it wants have this part of the bytes of the pack
// or more
const k_AssetPackShare = 0.5;
// The largest block of a file that the page accepts in the list
const k_AssetMostBlockSize = 64 * 1024 * 1024;
// The folder of the private storage of the site that keeps the files that the page got
const k_AssetCacheFolder = 'game-files';
// The file of the private storage of the site that keeps the SHA-256 of each file in blocks of the last visit, and its
// format. When a new version changes a file in blocks, the page takes the blocks that did not change from the old file.
const k_AssetPathRecord = 'game-files-paths.json';
const k_AssetPathRecordFormat = 1;
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

// The voice banks of the creature species. The web game reads the samples of a bank only when a creature of its species
// is in the world (src/Simulation/WorldStart.cpp, k_SpeciesVoiceBanks in src/Creature/CreatureAudio.h).
const k_AssetVoiceBankRules = [
  /^audio\/sfx\/creature\/b(ape|bear|cow|greek|horse|rhino|sheep|tiger|tort|wolf)\.sad$/,
];
// With no tiers file: the files that the start of the game does not read: the landscapes of the other lands and of the
// network maps, the network maps, the files of the old citadel, the animations and light maps of the miracles, and the
// voice banks of the species. The game reads the other files on the way to the first land: the films, the other sound
// banks, the meshes, the textures and the scripts.
const k_AssetLaterRules = [
  /^data\/landscape\/(land[2-5t]|construct|mpm_|twogods|threegods|fourgods)[^/]*$/,
  /^data\/landscape\/multi_player\//,
  /^online maps\//,
  /^data\/citadel\/engine_old\//,
  /^data\/spells\/(anims|lightmaps)\//,
  ...k_AssetVoiceBankRules,
];
// With no tiers file: the files that the page gets first after the start, because the first land can read them in its
// play, in this order: the animations and the light maps of the miracles, then the voice banks of the species. A
// creature can come into the world at any time, and its first sound must not wait for the site.
const k_AssetSoonRules = [
  /^data\/spells\//,
  ...k_AssetVoiceBankRules,
];
// With no tiers file: the music banks. The start of the game reads only the list of the parts at the start of each
// bank, and the music reads the parts of one bank while it plays. Thus the page gets only the first block of each bank
// before the start.
const k_AssetHeaderRules = [
  /^audio\/music\/.*\.sad$/,
];
// With a tiers file: the films. The game plays a film while its blocks arrive (src/Resources/Loaders.cpp, VideoLoader),
// thus the page gets each film in blocks, in the order of the film, in the phase of the film.
const k_AssetFilmRules = [
  /\.bik$/,
];
// The number of blocks after a block that the game reads, which the page gets next, so that the music plays on
const k_AssetReadAheadBlocks = 2;

// The phases of the start of the game, in the order that the game reads their files (the phases of the tiers file).
// The game starts when the page has the files of the boot phase. The page then gets the files of the pre-intro,
// startup and land-load phases in this order while the logos, the pre-intro film and the name box play. It gets the
// land-soon files after them, and the later files only when the game reads them. A phase of the tiers file with another
// name is a later phase.
const k_AssetPagePhases = ['boot', 'pre-intro', 'startup', 'land-load', 'land-soon', 'later'];
const k_AssetPhaseBoot = 0;
const k_AssetPhaseLandLoad = 3;
const k_AssetPhaseSoon = 4;
const k_AssetPhaseLater = 5;
// A file below this size goes in a lane of the small files, because its time is mostly the time of its request
const k_AssetSmallFile = 256 * 1024;
// The number of small files that the page gets at the same time before the start and in the phases after it. The
// site sends many requests on one connection (HTTP/2).
const k_AssetSmallDownloads = 16;
// The number of the other files and blocks of the phases that the page gets at the same time
const k_AssetPhaseDownloads = 4;
// The order of the queues of the store (AssetStore): a smaller rank goes first
const k_AssetQueueRanks = { now: 0, phase: 1, later: 2, background: 3 };
// The number of files that the page gets at the same time in the background
const k_AssetBackgroundDownloads = 1;
// The groups of the files that the page gets in the background when the land is ready, in their order: the music banks,
// the landscapes of the other lands and of the network maps, the network maps, and the files of the creature species.
// All the other files come after these groups.
const k_AssetBackgroundRules = [
  /^audio\/music\//,
  /^data\/landscape\//,
  /^online maps\//,
  /^(audio\/sfx\/creature\/|data\/ctr\/|data\/creaturemesh\/)/,
];
// The alignment music banks: the music of the land and of the towns in the evil, neutral or good version. The land
// plays the version of the alignment at the camera, and the temple and the citadel play the version of the alignment of
// the player (src/Sound/GameMusic.cpp).
const k_AssetAlignmentMusicRule = /^audio\/music\/align\/(?:[a-z]+_)?(evil|neutral|good)\.sad$/;
// The versions of the alignment music, in the order of the alignment index of the game: evil, neutral and good
const k_AssetAlignmentNames = ['evil', 'neutral', 'good'];
// The game divides the alignment from evil to good into seven steps, and each step has a version of the music. The step
// is (alignment + 1) / 2 * k_AssetAlignmentSteps, truncated, and at most 6.
const k_AssetAlignmentSteps = 6.9999995;
const k_AssetAlignmentIndex = [0, 0, 1, 1, 1, 2, 2];
// The line of the game log when the music changes. It gives the alignment at the camera and the alignment of the
// player.
const k_AssetMusicLogRule = /Music Playing \S+ \(alignment at the camera (-?[\d.]+), the player's (-?[\d.]+)\)/;
// The line of the game log for a step of the load of a land or a map, with the name of the land or the map
const k_AssetLandLogRule = /Load land ([^:\s]+): /;
// The page gets the files in the background only when the free space of the browser is at least this number of times
// the bytes of these files, so that the browser does not remove other data of the site
const k_AssetBackgroundSpaceFactor = 2;
// The time in milliseconds that a read of the game waits for the site, with no new frame on the screen, before the page
// shows the map of the game files over the game. The game cannot draw its loading screen while its read waits.
const k_AssetWaitPanelMs = 1000;
// The files that only the thread of the music reads while the game draws its frames, so that their wait does not stop
// the screen
const k_AssetMusicRule = /^audio\/music\//;

const k_Sha256Pattern = /^[0-9a-f]{64}$/;

// The address of the tiers file of the page, and the format of the tiers file that the page reads
const k_AssetTiersAddress = 'asset-tiers.json';
const k_AssetTiersFormat = 1;
// The name of the phase of the files that the first land reads in its play
const k_AssetSoonPhase = 'land-soon';

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
 * size, a sha256 and an address. A file of the second version can also have blocks: the SHA-256 of each part of
 * blockSize bytes, in order. When the list has "hashed_addresses", the address of each file is its address by its
 * SHA-256, and the file has hashed set. Throws an error when the list is not correct. The game finds its files with no
 * letter case, so two paths that have only a different letter case are an error.
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
  if (index.hashed_addresses !== undefined && typeof index.hashed_addresses !== 'boolean') {
    throw new Error(`the list of the game files has the value ${JSON.stringify(index.hashed_addresses)} for ` +
      'hashed_addresses');
  }
  const hashed = index.hashed_addresses === true;
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
    const address = hashed ? hashedAssetAddress(entry.sha256) : assetAddress(path);
    if (entry.blocks === undefined || index.format < 2) {
      return { path, size: entry.size, sha256: entry.sha256, address, hashed };
    }
    if (blockSize === 0 || !Array.isArray(entry.blocks) || entry.blocks.length !== Math.ceil(entry.size / blockSize) ||
      entry.size === 0 || !entry.blocks.every((block) => typeof block === 'string' && k_Sha256Pattern.test(block))) {
      throw new Error(`the list of the game files has no correct blocks for ${path}`);
    }
    return { path, size: entry.size, sha256: entry.sha256, address, hashed, blocks: [...entry.blocks] };
  });
  const totalBytes = files.reduce((sum, file) => sum + file.size, 0);
  if (index.total_bytes !== undefined && index.total_bytes !== totalBytes) {
    throw new Error(`the list of the game files gives ${index.total_bytes} bytes, but its files have ${totalBytes}`);
  }
  const packs = index.format >= 3 && index.packs !== undefined ? parseAssetPacks(index.packs, files, hashed) : [];
  return {
    archive: typeof index.archive_sha256 === 'string' ? index.archive_sha256 : '', totalBytes, blockSize, files, packs,
  };
}

/**
 * The packs of the list of the third version. A pack is the bytes of two or more files one after the other, at its
 * address by its SHA-256. Gives a list of { sha256, size, address, members }, where each member has the sha256, the
 * offset in the pack, the size and the first path of a file. Throws an error when a pack is not correct:
 * - each member must be a content of the list, with its size,
 * - the members must follow each other from the first byte of the pack to the last,
 * - a content can be in one pack only.
 */
function parseAssetPacks(listed, files, hashed) {
  if (!Array.isArray(listed) || listed.length > k_AssetMostFiles) {
    throw new Error('the list of the game files has no correct packs');
  }
  if (listed.length > 0 && !hashed) {
    throw new Error('the list of the game files has packs, but no addresses by SHA-256');
  }
  const contents = new Map();
  for (const file of files) {
    if (!contents.has(file.sha256)) {
      contents.set(file.sha256, file);
    }
  }
  const packed = new Set();
  return listed.map((entry, number) => {
    const problem = (reason) => new Error(`the list of the game files has an incorrect pack ${number}: ${reason}`);
    if (entry === null || typeof entry !== 'object' || typeof entry.sha256 !== 'string' ||
      !k_Sha256Pattern.test(entry.sha256) || !isFileSize(entry.size) || !Array.isArray(entry.members) ||
      entry.members.length < 2) {
      throw problem('it has no SHA-256, size or members');
    }
    let offset = 0;
    const members = entry.members.map((member) => {
      if (!Array.isArray(member) || member.length !== 3) {
        throw problem('a member is not [SHA-256, offset, size]');
      }
      const [sha256, at, size] = member;
      const file = contents.get(sha256);
      if (file === undefined || size !== file.size || at !== offset) {
        throw problem(`the member ${JSON.stringify(member)} is not a file of the list at the next offset`);
      }
      if (packed.has(sha256)) {
        throw problem(`the content of ${file.path} is in two packs`);
      }
      packed.add(sha256);
      offset += size;
      return { sha256, offset: at, size, path: file.path };
    });
    if (offset !== entry.size) {
      throw problem(`its members have ${offset} bytes, not ${entry.size}`);
    }
    return { sha256: entry.sha256, size: entry.size, address: hashedAssetAddress(entry.sha256), members };
  });
}

/**
 * True when the page gets the pack, and not its files alone: the files that the page wants (wanted gives true for
 * their SHA-256) have k_AssetPackShare or more of the bytes of the pack.
 */
function assetPackWorth(pack, wanted) {
  const bytes = pack.members.reduce((sum, member) => sum + (wanted(member.sha256) ? member.size : 0), 0);
  return bytes > 0 && bytes >= pack.size * k_AssetPackShare;
}

/**
 * Gives the changes of the download of a pack to watch as changes of its files, because the map of the downloads shows
 * files. The function shown gives true for the SHA-256 of a file that the map shows as not kept. Gives:
 * - event(name, detail): gives the change to each file that shown accepts,
 * - bytes(count): gives the next bytes of the body to the files that have them, in their order.
 */
function assetPackWatch(pack, watch, shown) {
  let at = 0;
  return {
    event(name, detail) {
      for (const member of pack.members) {
        if (shown(member.sha256)) {
          watch(name, `file ${member.sha256}`, detail);
        }
      }
    },
    bytes(count) {
      const end = at + count;
      for (const member of pack.members) {
        const part = Math.min(end, member.offset + member.size) - Math.max(at, member.offset);
        if (part > 0) {
          watch('bytes', `file ${member.sha256}`, part);
        }
      }
      at = end;
    },
  };
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
 * - download: one entry { sha256, size, path, address, hashed } for each content that the page does not keep whole,
 *   with the first path,
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
    .map((file) => ({
      sha256: file.sha256, size: file.size, path: file.path, address: file.address, hashed: file.hashed,
    }));
  return { download, ready, blocks, remove };
}

/**
 * The record of the last version of the files in blocks: an object from the lower-case path of each file of the list
 * that has blocks to its SHA-256. The next visit finds the old version of a changed file with it.
 */
function assetPathRecord(files) {
  const record = {};
  for (const file of files) {
    if (file.blocks) {
      record[file.path.toLowerCase()] = file.sha256;
    }
  }
  return record;
}

/** The record of the last visit (assetPathRecord) in the text of its file, or an empty record */
function parseAssetPathRecord(text) {
  try {
    const value = JSON.parse(text);
    if (value === null || typeof value !== 'object' || value.format !== k_AssetPathRecordFormat ||
      value.files === null || typeof value.files !== 'object') {
      return {};
    }
    return Object.fromEntries(Object.entries(value.files)
      .filter(([path, sha256]) => typeof sha256 === 'string' && k_Sha256Pattern.test(sha256) && path !== ''));
  } catch {
    return {};
  }
}

/**
 * The changed files in blocks of which the page keeps the old version whole: a list of { number, old }, with the
 * number of the file in the list and the SHA-256 of the old version. The page can take the blocks of the new version
 * that did not change from the old version, and download only the changed blocks. The arguments are the files of the
 * list, the map of the kept files (see planAssetCache) and the record of the last visit (assetPathRecord).
 */
function planAssetReuse(files, kept, record) {
  const reuse = [];
  const seen = new Set();
  files.forEach((file, number) => {
    const old = record[file.path.toLowerCase()];
    if (!file.blocks || seen.has(file.sha256) || kept.get(file.sha256) === file.size || old === undefined ||
      old === file.sha256 || !(kept.get(old) > 0)) {
      return;
    }
    seen.add(file.sha256);
    reuse.push({ number, old });
  });
  return reuse;
}

/** The address of a game file on the site, with each part of its path encoded */
function assetAddress(path) {
  return k_AssetFilesAddress + path.split('/').map(encodeURIComponent).join('/');
}

/** The address of a game file on the site by its SHA-256 */
function hashedAssetAddress(sha256) {
  return k_AssetHashedAddress + sha256;
}

/**
 * The address of a file of the list and the cache mode of the browser for it. The page keeps each file in OPFS, which
 * is the only copy. Thus no file goes in the HTTP cache of the browser, also a file at an address by SHA-256 that the
 * site sends with a long immutable cache. A second copy of 700 MB would use the space of the player for no gain.
 */
function assetRequest(file) {
  return {
    address: file.address ?? assetAddress(file.path),
    cache: 'no-store',
  };
}

/** True when the error is the error of the browser for storage with no more space */
function isQuotaError(error) {
  return error?.name === 'QuotaExceededError';
}

/**
 * The free space in bytes that the browser gives to the site, from the result of navigator.storage.estimate(). Gives
 * Infinity when the browser does not tell it.
 */
function assetFreeSpace(estimate) {
  const { quota, usage } = estimate ?? {};
  if (!Number.isFinite(quota) || !Number.isFinite(usage)) {
    return Infinity;
  }
  return Math.max(0, quota - usage);
}

/**
 * What the page gets in the background when the land is ready, in the order to get it: a list of { number, block },
 * with block null for a whole file. The files are the files of the list in the order of their numbers, and have is a
 * function that is true for the SHA-256 of each file and block that the page keeps. The groups of
 * k_AssetBackgroundRules come first, in their order, then all the other files. A music bank in blocks comes in blocks,
 * so that the music can read each block when it arrives. The page joins the blocks when it has all of them.
 * alignments is a list of the versions of the alignment music (k_AssetAlignmentNames) that the game plays now. The
 * alignment music banks of these versions come before all the other files.
 */
function planAssetBackground(files, { have = () => false, alignments = [] } = {}) {
  const seen = new Set();
  const items = [];
  files.forEach((file, number) => {
    const lower = file.path.toLowerCase();
    if (have(file.sha256) || seen.has(file.sha256)) {
      return;
    }
    seen.add(file.sha256);
    const group = k_AssetBackgroundRules.findIndex((rule) => rule.test(lower));
    const version = k_AssetAlignmentMusicRule.exec(lower)?.[1];
    let order = group < 0 ? k_AssetBackgroundRules.length : group;
    if (version !== undefined && alignments.includes(version)) {
      order = -1;
    }
    if (file.blocks && k_AssetHeaderRules.some((rule) => rule.test(lower))) {
      file.blocks.forEach((block, index) => {
        if (!have(block) && !seen.has(block)) {
          seen.add(block);
          items.push({ number, block: index, order });
        }
      });
    } else {
      items.push({ number, block: null, order });
    }
  });
  // The sort keeps the order of the list in each group
  return items.sort((a, b) => a.order - b.order).map(({ number, block }) => ({ number, block }));
}

/**
 * The lower-case name of the land or the map that the game starts to load, from a line of the game log, or null. The
 * game writes a line with the name for each step of the load (src/Common/LoadTimer.cpp).
 */
function assetLandOfLog(line) {
  const found = k_AssetLandLogRule.exec(line);
  return found ? found[1].toLowerCase() : null;
}

/** The version of the alignment music (k_AssetAlignmentNames) of an alignment from -1 (evil) to 1 (good) */
function alignmentMusicName(alignment) {
  const step = Math.trunc(Math.min((alignment + 1) / 2 * k_AssetAlignmentSteps, k_AssetAlignmentIndex.length - 1));
  return k_AssetAlignmentNames[k_AssetAlignmentIndex[Math.max(0, step)]];
}

/**
 * The versions of the alignment music that the game can play now, from a line of the game log: the version of the
 * alignment of the player first, then the version of the alignment at the camera when it is another version. Gives
 * null for a line that is not a line of the music.
 */
function musicAlignmentsOfLog(line) {
  const found = k_AssetMusicLogRule.exec(line);
  if (!found) {
    return null;
  }
  const camera = alignmentMusicName(Number(found[1]));
  const player = alignmentMusicName(Number(found[2]));
  return player === camera ? [player] : [player, camera];
}

/** The bytes of the items of planAssetBackground */
function assetItemBytes(files, items, blockSize) {
  return items.reduce((sum, { number, block }) => {
    if (block === null) {
      return sum + files[number].size;
    }
    const bounds = assetBlockBounds(files[number], blockSize, block);
    return sum + bounds.end - bounds.start;
  }, 0);
}

/**
 * The reason why the page does not get files in the background now, or null when it can. The argument has:
 * - enabled: false when the player turned the background download off,
 * - kept: true when the browser keeps the storage of the site (askToKeepStorage),
 * - saveData: true when the browser asks the site to send less data,
 * - hidden: true when the tab was hidden for a long time,
 * - full: true when the browser has no more space for the game files,
 * - free and rest: the free space of the browser and the bytes of the files that the page gets in the background.
 */
function assetBackgroundStop({ enabled, kept, saveData = false, hidden = false, full = false, free = Infinity, rest = 0 }) {
  if (!enabled) {
    return 'the player turned it off';
  }
  if (kept !== true) {
    return 'the browser does not keep the storage of the site';
  }
  if (saveData) {
    return 'the browser asks for less data';
  }
  if (full) {
    return 'the browser has no more space';
  }
  if (free < k_AssetBackgroundSpaceFactor * rest) {
    return `the browser has only ${megabytesOf(free)} of free space`;
  }
  if (hidden) {
    return 'the tab is hidden';
  }
  return null;
}

/** The text of a number of bytes in megabytes, for the log and the messages of the page */
function megabytesOf(bytes) {
  return `${Math.round(bytes / (1024 * 1024))} MB`;
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
  // The site answers 204, with no body, to an account that has no game files. The page does not read a list then.
  if (response.status === 204) {
    return { granted: false, status: 204, reason: 'the site gives no game files to this account' };
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
  const request = assetRequest(file);
  const response = await fetchFile(request.address, { cache: request.cache, credentials: 'same-origin' });
  if (response.status !== 200) {
    throw new Error(`the site gave the status ${response.status} for ${file.path}`);
  }
  // The site can send the file compressed. The Content-Length header then gives the size of the compressed copy,
  // but the body that the browser gives is the whole file. The size check uses the bytes of the body only.
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

/**
 * Gets one pack. The body must have the size and the SHA-256 of the list, and the SHA-256 of each file in it must agree
 * with the list. Calls received with the number of bytes of each part that arrives. Gives { sha256, bytes } for each
 * file of the pack, where bytes is a view of the body.
 */
async function fetchAssetPack(fetchFile, digest, pack, received) {
  const whole = { path: `the pack ${pack.sha256}`, size: pack.size, sha256: pack.sha256, address: pack.address };
  const body = await fetchAsset(fetchFile, digest, whole, received);
  const parts = [];
  for (const member of pack.members) {
    const bytes = body.subarray(member.offset, member.offset + member.size);
    const problem = await assetProblem(digest, member, bytes);
    if (problem !== null) {
      throw new Error(problem);
    }
    parts.push({ sha256: member.sha256, bytes });
  }
  return parts;
}

/**
 * The tiers of the game files in the text of the tiers file. Gives { phases, startPhases, blockSize, files, soon, lands }:
 * - phases: the names of the phases, in order; startPhases: the number of phases that come before the start,
 * - blockSize: the size of a block in the tiers file,
 * - files: a Map from the lower-case path to { phase, order, blocks }, where blocks is null for a file that the game
 *   reads whole, or a list of [phase, first block, number of blocks],
 * - soon: the lower-case paths of the files that the first land reads in its play, in the order of their first read,
 * - lands: a Map from the lower-case name of a land or a map to the lower-case paths that it reads, in order.
 * The page ignores a path that is not in the list of the game files. Throws an error when the file is not correct.
 */
function parseAssetTiers(text) {
  let tiers;
  try {
    tiers = JSON.parse(text);
  } catch (error) {
    throw new Error(`the tiers of the game files are not JSON: ${error.message}`);
  }
  if (tiers === null || typeof tiers !== 'object' || tiers.format !== k_AssetTiersFormat) {
    throw new Error(`the tiers of the game files do not have the format ${k_AssetTiersFormat}`);
  }
  const { phases } = tiers;
  if (!Array.isArray(phases) || phases.length === 0 || !phases.every((name) => typeof name === 'string')) {
    throw new Error('the tiers of the game files have no phases');
  }
  const startPhases = tiers.start_phases;
  if (!Number.isSafeInteger(startPhases) || startPhases < 0 || startPhases > phases.length) {
    throw new Error(`the tiers of the game files have ${tiers.start_phases} phases before the start`);
  }
  const isPhase = (value) => Number.isSafeInteger(value) && value >= 0 && value < phases.length;
  const isCount = (value) => Number.isSafeInteger(value) && value >= 0;
  const blockSize = tiers.block_size;
  if (!Number.isSafeInteger(blockSize) || blockSize <= 0 || blockSize > k_AssetMostBlockSize) {
    throw new Error(`the tiers of the game files have the block size ${tiers.block_size}`);
  }
  const listed = tiers.files;
  if (listed === null || typeof listed !== 'object' || Array.isArray(listed) ||
    Object.keys(listed).length > k_AssetMostFiles) {
    throw new Error('the tiers of the game files have no correct files');
  }
  const files = new Map();
  for (const [path, entry] of Object.entries(listed)) {
    if (!isSafeAssetPath(path) || path !== path.toLowerCase() || entry === null || typeof entry !== 'object' ||
      !isPhase(entry.phase) || !isCount(entry.order)) {
      throw new Error(`the tiers of the game files have no correct phase for ${JSON.stringify(path)}`);
    }
    const blocks = entry.blocks ?? null;
    if (blocks !== null && !(Array.isArray(blocks) && blocks.every((run) => Array.isArray(run) && run.length === 3 &&
      isPhase(run[0]) && isCount(run[1]) && isCount(run[2]) && run[2] > 0))) {
      throw new Error(`the tiers of the game files have no correct blocks for ${path}`);
    }
    files.set(path, { phase: entry.phase, order: entry.order, blocks });
  }
  const soonPhase = phases.indexOf(k_AssetSoonPhase);
  const soon = [...files].filter(([, entry]) => entry.phase === soonPhase)
    .sort(([, a], [, b]) => a.order - b.order).map(([path]) => path);
  const listedLands = tiers.lands ?? {};
  if (listedLands === null || typeof listedLands !== 'object' || Array.isArray(listedLands)) {
    throw new Error('the tiers of the game files have no correct lands');
  }
  const lands = new Map();
  for (const [name, paths] of Object.entries(listedLands)) {
    if (!Array.isArray(paths) || !paths.every((path) => isSafeAssetPath(path) && path === path.toLowerCase())) {
      throw new Error(`the tiers of the game files have no correct paths for ${JSON.stringify(name)}`);
    }
    lands.set(name.toLowerCase(), [...paths]);
  }
  return { phases, startPhases, blockSize, files, soon, lands };
}

/**
 * Gets the tiers file of the page. Gives the tiers, or no value when the page has no correct tiers file. The page then
 * uses its rules. The function log gets the reason.
 */
async function fetchAssetTiers(fetchFile, log = () => {}) {
  try {
    const response = await fetchFile(k_AssetTiersAddress, { cache: 'no-cache', credentials: 'same-origin' });
    if (!response.ok) {
      log(`The page has no tiers of the game files (status ${response.status}). It uses its rules.`);
      return null;
    }
    return parseAssetTiers(await response.text());
  } catch (error) {
    log(`The page cannot read the tiers of the game files: ${error.message || error}. It uses its rules.`);
    return null;
  }
}

/**
 * True when the start of the game reads the file at this path, so that the page gets it before the start. With the
 * tiers, the file must have a phase before the start.
 */
function isStartAsset(path, tiers = null) {
  const lower = path.toLowerCase();
  if (tiers) {
    const tier = tiers.files.get(lower);
    return tier !== undefined && tier.phase < tiers.startPhases;
  }
  return !k_AssetLaterRules.some((rule) => rule.test(lower));
}

/** The phase of the page (k_AssetPagePhases) of a phase of the tiers file, by its name */
function assetPhaseOf(tiers, phase) {
  const found = k_AssetPagePhases.indexOf(tiers.phases[phase]);
  return found < 0 ? k_AssetPhaseLater : found;
}

/**
 * What the page gets in each phase (k_AssetPagePhases), in the order to get it: for each phase a list of { number, block },
 * with block null for a whole file. The files are the files of the list in the order of their numbers, blockSize the
 * size of their blocks, and tiers the result of parseAssetTiers.
 * - With the tiers, each file goes in its phase, in the order of the tiers. For a file that the list gives in blocks,
 *   with blocks in the tiers, each run of blocks of a music bank goes in its own phase. The other blocks come when the
 *   game reads them. Each other file comes whole.
 *   A file that the tiers do not give is a later file, but a file of k_AssetSoonRules is a land-soon file after the
 *   others, so that the first sound of each species never waits for the site.
 * - With no tiers, the boot phase has all the files of the start and the first block of each music bank, and the
 *   land-soon phase has the files that the first land can read in its play (k_AssetSoonRules).
 */
function planAssetTiers(files, tiers = null, blockSize = 0) {
  const phases = k_AssetPagePhases.map(() => []);
  if (tiers === null) {
    files.forEach((file, number) => {
      const lower = file.path.toLowerCase();
      if (!isStartAsset(lower)) {
        const soon = k_AssetSoonRules.findIndex((rule) => rule.test(lower));
        phases[soon < 0 ? k_AssetPhaseLater : k_AssetPhaseSoon].push({ number, block: null, order: soon });
      } else if (file.blocks && k_AssetHeaderRules.some((rule) => rule.test(lower))) {
        phases[k_AssetPhaseBoot].push({ number, block: 0, order: 0 });
      } else {
        phases[k_AssetPhaseBoot].push({ number, block: null, order: 0 });
      }
    });
  } else {
    files.forEach((file, number) => {
      const tier = tiers.files.get(file.path.toLowerCase());
      if (tier === undefined) {
        // A file that the first land can read in its play, but that the traces did not read, comes after the files of
        // the land-soon phase of the tiers
        const soon = k_AssetSoonRules.findIndex((rule) => rule.test(file.path.toLowerCase()));
        phases[soon < 0 ? k_AssetPhaseLater : k_AssetPhaseSoon].push({ number, block: null, order: k_AssetMostFiles + soon });
        return;
      }
      // Only the music banks and the films come in blocks. The page gets each other bank whole in its phase, so that
      // no sample of the dialogue or of the sound effects waits for the site when it first plays.
      const lower = file.path.toLowerCase();
      if (file.blocks && blockSize === tiers.blockSize && k_AssetFilmRules.some((rule) => rule.test(lower))) {
        file.blocks.forEach((_, block) => {
          phases[assetPhaseOf(tiers, tier.phase)].push({ number, block, order: tier.order });
        });
        return;
      }
      const inBlocks = file.blocks && tier.blocks !== null && blockSize === tiers.blockSize &&
        k_AssetHeaderRules.some((rule) => rule.test(lower));
      if (!inBlocks) {
        phases[assetPhaseOf(tiers, tier.phase)].push({ number, block: null, order: tier.order });
        return;
      }
      for (const [phase, first, count] of tier.blocks) {
        for (let block = first; block < first + count && block < file.blocks.length; block += 1) {
          phases[assetPhaseOf(tiers, phase)].push({ number, block, order: tier.order });
        }
      }
    });
  }
  return phases.map((items) => items
    .sort((a, b) => a.order - b.order || a.number - b.number || (a.block ?? 0) - (b.block ?? 0))
    .map(({ number, block }) => ({ number, block })));
}

/**
 * The numbers of the files that the page gets for later, in the order to get them, when the game read the file at the
 * path just now. The argument has: the files of the list in the order of their numbers, with a path and a sha256 each,
 * a function that is true for the SHA-256 of each file that the page has or gets now, and the tiers (see
 * parseAssetTiers).
 * - After the game read the landscape of a land: the landscape and the footpaths of the next land, and the files that
 *   the tiers give for the next land.
 * - After the game read a file of a network map: the other files of the same map, and the files that the tiers give
 *   for the map.
 * With land in place of path, the files that the tiers give for the land or the map of that lower-case name. The game
 * makes some maps, such as the map for 32 gods, and reads no file of such a map (see assetLandOfLog).
 */
function planAssetPrefetch(files, { path = null, land = null, have = () => false, tiers = null }) {
  const numbers = [];
  const seen = new Set();
  const addNumber = (number) => {
    const file = files[number];
    if (!seen.has(file.sha256) && !have(file.sha256)) {
      seen.add(file.sha256);
      numbers.push(number);
    }
  };
  const add = (test) => {
    files.forEach((file, number) => {
      if (test(file.path.toLowerCase())) {
        addNumber(number);
      }
    });
  };
  // The files of a list of the tiers, in the order of the list
  const addTiers = (paths) => {
    const numberOf = new Map(files.map((file, number) => [file.path.toLowerCase(), number]));
    paths.map((other) => numberOf.get(other)).filter((number) => number !== undefined).forEach(addNumber);
  };
  if (land !== null) {
    addTiers(tiers?.lands.get(land) ?? []);
    return numbers;
  }
  const lower = path.toLowerCase();
  const story = /^data\/landscape\/land([1-4])\.lnd$/.exec(lower);
  if (story) {
    const next = Number(story[1]) + 1;
    add((other) => other === `data/landscape/land${next}.lnd` || other === `data/landscape/land${next}.fot`);
    addTiers(tiers?.lands.get(`land${next}`) ?? []);
    return numbers;
  }
  const map = /^(online maps\/|data\/landscape\/(multi_player\/)?)([^/]+?)\.[^./]+$/.exec(lower);
  if (map && !isStartAsset(lower, tiers)) {
    const name = map[3];
    add((other) => {
      const stem = /([^/]+?)\.[^./]+$/.exec(other);
      return stem !== null && stem[1] === name && !isStartAsset(other, tiers);
    });
    addTiers(tiers?.lands.get(name) ?? []);
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

/** True when a read of the game of the file at this path can stop the frames of the screen while it waits */
function assetWaitHoldsScreen(path) {
  return !k_AssetMusicRule.test(path.toLowerCase());
}

/**
 * True when the page shows the map of the game files over the game: a read of the game that can stop the screen waits
 * for the site for waitedMs, and the screen has no new frame for stillMs, both for k_AssetWaitPanelMs or more.
 */
function assetWaitShowsPanel({ waitedMs, stillMs }) {
  return waitedMs >= k_AssetWaitPanelMs && stillMs >= k_AssetWaitPanelMs;
}

/**
 * The change of the map over the game at a check of the reads that wait: 'open', 'close' or no value. The argument has:
 * open: true when the map is open; forWait: true when the page opened it for a wait; dismissed: true when the player
 * closed or opened the map during the waits; waitedMs and stillMs as for assetWaitShowsPanel, with waitedMs -Infinity
 * when no read that can stop the screen waits. The page closes only a map that it opened for a wait, and only when the
 * read ends. A map that the player opened or closed stays so until the waits end.
 */
function assetWaitPanelChange({ open, forWait, dismissed = false, waitedMs, stillMs }) {
  if (!open && !dismissed && assetWaitShowsPanel({ waitedMs, stillMs })) {
    return 'open';
  }
  if (open && forWait && waitedMs === -Infinity) {
    return 'close';
  }
  return undefined;
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
  const request = assetRequest(file);
  const response = await fetchFile(request.address, {
    cache: request.cache, credentials: 'same-origin', headers: { Range: `bytes=${start}-${end - 1}` },
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

/** Keeps the bytes in the folder as the file with the name, and gives the kept File */
async function writeAsset(folder, name, bytes) {
  const handle = await folder.getFileHandle(name, { create: true });
  const writable = await handle.createWritable();
  try {
    await writable.write(bytes);
    await writable.close();
  } catch (error) {
    // A write that fails does not change the file. The next visit removes an empty file of the wrong size.
    await Promise.resolve(writable.abort?.()).catch(() => {});
    throw error;
  }
  return handle.getFile();
}

/**
 * The page's store of the game files for one game. It keeps each whole file by its SHA-256 and each block by the
 * SHA-256 of the block, in the folder of kept files. It has three queues:
 * - now: the files and the blocks that the game waits for, at most k_AssetDownloads at a time,
 * - phase: the files and the blocks of the phases of the start (planAssetTiers), in their order. The small files go
 *   k_AssetSmallDownloads at a time, and the others k_AssetPhaseDownloads at a time. While the game waits for a file,
 *   no other file of the phases starts, apart from the small files, so that the file of the game gets the network,
 * - later: the files that the game will probably read later, k_AssetPrefetchDownloads at a time, only when the other
 *   queues are empty,
 * - background: the other files when the land is ready (background), k_AssetBackgroundDownloads at a time, only when
 *   the other queues are empty and backgroundAllowed() is true.
 * A job that the game asks for goes to the queue now from the other queues. When the browser has no more space, the
 * store keeps the next files in memory for this visit, and full becomes true. The options are:
 * - fetchFile, digest and folder: the fetch, the SHA-256 and the folder of the kept files (tests give fakes),
 * - blockSize: the size of the blocks of the list,
 * - packs: the packs of the list (parseAssetPacks). The store gets a small file in its pack when the pack is worth it
 *   (assetPackWorth), apart from a file that the game waits for and no pack job brings,
 * - kept: the kept File objects, by their names,
 * - share: a function that gets (number, block, file) for each file (block null) and each block that the page gets,
 * - log: a function that gets a line for the game log,
 * - watch: a function that gets (event, key, detail) for each change of a download (see safeWatch). It only
 *   observes: it does not change the order of the downloads,
 * - onFull: a function that the store calls one time when the browser has no more space for the game files.
 */
class AssetStore {
  constructor(files, {
    fetchFile, digest, folder, blockSize = 0, packs = [], kept = new Map(), share = () => {}, log = () => {},
    watch = () => {}, onFull = () => {},
  }) {
    Object.assign(this, { files, fetchFile, digest, folder, blockSize, kept, share, log, onFull });
    this.watch = safeWatch(watch);
    this.jobs = new Map();
    this.waiting = { now: [], phase: [], later: [], background: [] };
    // The jobs that run in each lane: the queue now, the small files and the other files of the phases, later and the
    // background
    this.running = { now: 0, small: 0, phase: 0, later: 0, background: 0 };
    // True when the browser has no more space for the game files
    this.full = false;
    // A function that is true when the store can start a job of the background queue. The caller can change it, and
    // then calls pump.
    this.backgroundAllowed = () => true;
    // The number of running jobs of the other queues that the game waits for
    this.wanted = 0;
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
    // Each pack by its SHA-256, and the pack of each content of the files of the store that is in a pack
    this.packs = new Map(packs.map((pack) => [pack.sha256, pack]));
    this.packOf = new Map();
    for (const pack of packs) {
      for (const member of pack.members) {
        if (this.users.has(member.sha256)) {
          this.packOf.set(member.sha256, pack);
        }
      }
    }
    // The SHA-256 of each pack that the store did not get. The store then gets its files alone.
    this.brokenPacks = new Set();
  }

  /**
   * Gives a change of a download to watch. A change of a pack goes to each file of the pack that the page does not
   * keep, because the map shows files. The pack tells the end of each file itself (getPack).
   */
  notify(event, key, detail) {
    const pack = key.startsWith('pack ') ? this.packs.get(key.slice('pack '.length)) : undefined;
    if (pack === undefined) {
      this.watch(event, key, detail);
    } else if (event !== 'done') {
      assetPackWatch(pack, this.watch, (sha256) => !this.kept.has(sha256)).event(event, detail);
    }
  }

  /**
   * The pack that brings the content, or no value when the content comes alone. When a job gets the pack already, the
   * pack brings the content. Else the content comes alone in these conditions:
   * - the game waits for it (one small file comes faster than its pack),
   * - a job gets it alone already,
   * - the files of the pack that the page does not keep have less than k_AssetPackShare of the bytes of the pack.
   */
  packFor(sha256, priority) {
    const pack = this.packOf.get(sha256);
    if (pack === undefined || this.brokenPacks.has(pack.sha256)) {
      return null;
    }
    if (this.jobs.has(`pack ${pack.sha256}`)) {
      return pack;
    }
    if (priority === 'now' || this.jobs.has(`file ${sha256}`)) {
      return null;
    }
    return assetPackWorth(pack, (member) => !this.kept.has(member)) ? pack : null;
  }

  /**
   * The job that gets the whole file: its pack (packFor) or the file alone. When the pack fails, the store gets its
   * files alone.
   */
  wholeJob(number, priority) {
    const file = this.files[number];
    const pack = this.packFor(file.sha256, priority);
    if (pack === null) {
      return this.schedule(`file ${file.sha256}`, () => this.getWhole(number), priority, { size: file.size });
    }
    return this.schedule(`pack ${pack.sha256}`, () => this.getPack(pack), priority, { size: pack.size })
      .catch((error) => {
        if (!this.brokenPacks.has(pack.sha256)) {
          this.brokenPacks.add(pack.sha256);
          this.log(`The page cannot get a pack of the game files, and gets its files alone: ${error.message}`);
        }
        return this.fileOf(number) ? undefined : this.wholeJob(number, priority);
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
   * A file or a block that the page has already goes to share again: the page can get a file before the game can
   * take it (a file for later that arrives before the game starts), and the game thread waits until share gives it.
   */
  async obtain(number, offset = 0, length = Infinity) {
    const file = this.files[number];
    const whole = this.fileOf(number);
    if (whole) {
      this.share(number, null, whole);
      return;
    }
    const range = file.blocks ? assetBlockRange(file, this.blockSize, offset, length) : null;
    // A file that a phase gets whole now or next comes whole to the game, and not a second time in blocks
    if (range === null || this.jobs.has(`file ${file.sha256}`)) {
      await this.wholeJob(number, 'now');
      return;
    }
    const wanted = [];
    for (let block = range.first; block <= range.last; block += 1) {
      const kept = this.kept.get(file.blocks[block]);
      if (kept) {
        this.share(number, block, kept);
      } else {
        wanted.push(this.getBlockJob(number, block, 'now'));
      }
    }
    // The blocks after them go first in the queue of the phases, so that the music or the film plays on
    const ahead = Math.min(file.blocks.length - 1, range.last + k_AssetReadAheadBlocks);
    for (let block = ahead; block > range.last; block -= 1) {
      this.getBlockJob(number, block, 'phase', true).catch((error) => this.log(`The page cannot get ${error.message}`));
    }
    await Promise.all(wanted);
  }

  /** Gets the whole files after the files that the game asks for. A failure goes only to the log. */
  prefetch(numbers) {
    for (const number of numbers) {
      this.getWholeJob(number, 'later')
        .catch((error) => this.log(`The page cannot get ${this.files[number].path} for later: ${error.message}`));
    }
  }

  /**
   * Puts the files and the blocks of the background (planAssetBackground) in the background queue, in their order. A
   * failure goes only to the log: the game gets the file when it reads it.
   */
  background(items) {
    for (const { number, block } of items) {
      const job = block === null ? this.getWholeJob(number, 'background') : this.getBlockJob(number, block, 'background');
      job.catch((error) => this.log(`The page cannot get ${this.files[number].path} in the background: ${error.message}`));
    }
  }

  /**
   * Puts the waiting jobs of the background queue in the order of a new plan of the background (planAssetBackground).
   * A waiting job that the plan does not give keeps its place after the jobs of the plan.
   */
  reorderBackground(items) {
    const ranks = new Map();
    items.forEach(({ number, block }, rank) => {
      const file = this.files[number];
      const key = block === null ? `file ${file.sha256}` : `block ${file.blocks[block]}`;
      if (!ranks.has(key)) {
        ranks.set(key, rank);
      }
    });
    const rankOf = (job) => ranks.get(job.key) ?? items.length;
    // The sort keeps the order of the jobs that have the same rank
    this.waiting.background.sort((a, b) => rankOf(a) - rankOf(b));
  }

  /**
   * Puts the files and the blocks of the phases of the start in the queues, in their order: the pre-intro, startup and
   * land-load phases in the queue of the phases, and the land-soon phase in the queue for later. The argument is the
   * result of planAssetTiers. The page has the files of the boot phase already, and gets the later files only when the
   * game reads them. A failure goes only to the log: the game gets the file when it reads it.
   */
  queuePhases(phases) {
    const queue = (items, priority) => {
      for (const { number, block } of items) {
        const job = block === null ? this.getWholeJob(number, priority) : this.getBlockJob(number, block, priority);
        job.catch((error) => this.log(`The page cannot get ${this.files[number].path} for its phase: ` +
          `${error.message}`));
      }
    };
    phases.slice(k_AssetPhaseBoot + 1, k_AssetPhaseLandLoad + 1).forEach((items) => queue(items, 'phase'));
    queue(phases[k_AssetPhaseSoon] ?? [], 'later');
  }

  /**
   * Gives a promise that is done when no job waits or runs. A failed job does not make it fail. A job of the background
   * queue that waits while backgroundAllowed() is false keeps the promise open.
   */
  async drained() {
    while (this.jobs.size > 0) {
      await Promise.allSettled([...this.jobs.values()].map((job) => job.promise));
    }
  }

  /** True while a job of the queue now waits or runs: the game or the start waits for a file */
  waitsNow() {
    return this.waiting.now.length > 0 || this.running.now > 0 || this.wanted > 0;
  }

  getWholeJob(number, priority) {
    const file = this.files[number];
    if (this.fileOf(number)) {
      return Promise.resolve();
    }
    return this.wholeJob(number, priority);
  }

  getBlockJob(number, block, priority, front = false) {
    const file = this.files[number];
    const sha256 = file.blocks[block];
    // The page removes the blocks of a file that it keeps whole
    if (this.kept.has(sha256) || this.fileOf(number)) {
      return Promise.resolve();
    }
    const { start, end } = assetBlockBounds(file, this.blockSize, block);
    return this.schedule(`block ${sha256}`, () => this.getBlock(number, block), priority, { size: end - start, front });
  }

  /**
   * Runs a job once for its key, in the queue of its priority: now, phase or later. A job that waits in a later queue
   * goes to an earlier queue when a request of an earlier queue asks for it. A job with front goes first in its queue.
   */
  schedule(key, run, priority, { size = Infinity, front = false } = {}) {
    let job = this.jobs.get(key);
    if (job) {
      // The game waits for a job that runs in another queue: it counts as a job of the queue now
      if (job.started && priority === 'now' && job.priority !== 'now' && !job.wanted) {
        job.wanted = true;
        this.wanted += 1;
      }
      if (!job.started && k_AssetQueueRanks[priority] < k_AssetQueueRanks[job.priority]) {
        this.waiting[job.priority] = this.waiting[job.priority].filter((other) => other !== job);
        job.priority = priority;
        this.enqueue(job, front);
        this.notify('queued', key, priority);
        this.pump();
      }
      return job.promise;
    }
    job = { key, run, priority, size, started: false };
    job.promise = new Promise((resolve, reject) => {
      job.resolve = resolve;
      job.reject = reject;
    });
    this.jobs.set(key, job);
    this.enqueue(job, front);
    this.notify('queued', key, priority);
    this.pump();
    return job.promise;
  }

  enqueue(job, front) {
    if (front) {
      this.waiting[job.priority].unshift(job);
    } else {
      this.waiting[job.priority].push(job);
    }
  }

  /**
   * Starts the jobs that can run now: first the jobs of the game, then the jobs of the phases in their order, then one
   * job for later when nothing else waits or runs
   */
  pump() {
    const start = (job, lane) => {
      job.started = true;
      this.running[lane] += 1;
      (async () => {
        for (let attempt = 1; ; attempt += 1) {
          try {
            this.notify('started', job.key);
            return await job.run();
          } catch (error) {
            if (attempt >= k_AssetAttempts) {
              throw error;
            }
          }
        }
      })().then((value) => {
        this.notify('done', job.key);
        job.resolve(value);
      }, (error) => {
        this.notify('failed', job.key);
        job.reject(error);
      }).finally(() => {
        this.jobs.delete(job.key);
        this.running[lane] -= 1;
        if (job.wanted) {
          this.wanted -= 1;
        }
        this.pump();
      });
    };
    while (this.running.now < k_AssetDownloads && this.waiting.now.length > 0) {
      start(this.waiting.now.shift(), 'now');
    }
    const free = {
      small: k_AssetSmallDownloads - this.running.small,
      phase: this.waitsNow() ? 0 : k_AssetPhaseDownloads - this.running.phase,
    };
    const phase = this.waiting.phase;
    for (let at = 0; at < phase.length && (free.small > 0 || free.phase > 0);) {
      const lane = phase[at].size < k_AssetSmallFile ? 'small' : 'phase';
      if (free[lane] > 0) {
        free[lane] -= 1;
        start(phase.splice(at, 1)[0], lane);
      } else {
        at += 1;
      }
    }
    const idle = !this.waitsNow() && phase.length === 0 && this.running.small === 0 && this.running.phase === 0;
    while (idle && this.running.later < k_AssetPrefetchDownloads && this.waiting.later.length > 0) {
      start(this.waiting.later.shift(), 'later');
    }
    const quiet = idle && this.running.later === 0 && this.waiting.later.length === 0;
    while (quiet && this.running.background < k_AssetBackgroundDownloads && this.waiting.background.length > 0 &&
      this.backgroundAllowed()) {
      start(this.waiting.background.shift(), 'background');
    }
  }

  /**
   * Keeps the bytes in the folder with the name, and gives the kept File. When the browser has no more space, the store
   * keeps the bytes in memory for this visit, and the next visit downloads them again.
   */
  async write(name, bytes) {
    if (!this.full) {
      try {
        const kept = await writeAsset(this.folder, name, bytes);
        this.kept.set(name, kept);
        return kept;
      } catch (error) {
        if (!isQuotaError(error)) {
          throw error;
        }
        this.full = true;
        this.log('The browser has no more space for the game files. The page keeps the next files in memory for ' +
          'this visit only.');
        this.onFull();
      }
    }
    const kept = new Blob([bytes]);
    this.kept.set(name, kept);
    return kept;
  }

  async getWhole(number) {
    const file = this.files[number];
    // A file of which the page keeps some blocks, for example the blocks that did not change in a new version, comes
    // in its other blocks
    if (file.blocks && this.blocksOf(number).some((kept) => kept !== null)) {
      await this.getOtherBlocks(number);
      return;
    }
    const key = `file ${file.sha256}`;
    const bytes = await fetchAsset(this.fetchFile, this.digest, file, (count) => this.watch('bytes', key, count));
    const kept = await this.write(file.sha256, bytes);
    this.received.files += 1;
    this.received.bytes += bytes.byteLength;
    for (const user of this.users.get(file.sha256)) {
      this.share(user, null, kept);
    }
    // The page can keep blocks of the file from before, for example the first block of a music bank of the start
    if (file.blocks && !this.full) {
      await this.removeJoinedBlocks(number);
    }
  }

  /**
   * Gets a pack, and keeps each file of the pack that the page does not keep by its SHA-256. The files of a pack are
   * small, thus they have no blocks.
   */
  async getPack(pack) {
    const watch = assetPackWatch(pack, this.watch, () => false);
    const parts = await fetchAssetPack(this.fetchFile, this.digest, pack, (count) => watch.bytes(count));
    for (const { sha256, bytes } of parts) {
      const users = this.users.get(sha256);
      if (users === undefined || this.kept.has(sha256)) {
        continue;
      }
      const kept = await this.write(sha256, bytes);
      this.received.files += 1;
      this.received.bytes += bytes.byteLength;
      this.watch('done', `file ${sha256}`);
      for (const user of users) {
        this.share(user, null, kept);
      }
    }
  }

  /** Gets each block of the file that the page does not keep, one at a time, then joins the blocks */
  async getOtherBlocks(number) {
    const file = this.files[number];
    for (let block = 0; block < file.blocks.length && !this.fileOf(number); block += 1) {
      if (!this.kept.has(file.blocks[block])) {
        await this.getBlock(number, block);
      }
    }
    await this.joinBlocks(number);
    if (!this.fileOf(number)) {
      throw new Error(`the page cannot join the blocks of ${file.path}`);
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
      // The whole file can arrive while the block downloads. The page then does not keep the block.
      if (this.fileOf(user) && !this.full) {
        await this.removeJoinedBlocks(user);
      }
    }
  }

  /**
   * When the page has all the blocks of a file, it checks the whole file against the list and keeps it whole. It then
   * removes the blocks, so that the folder keeps only one copy. A game thread that reads a removed block asks for the
   * bytes again, and gets the whole file (see src/Web/UserFolderBackend.cpp). With no more space in the browser, the
   * page keeps the blocks and does not join them.
   */
  async joinBlocks(number) {
    const file = this.files[number];
    const blocks = this.blocksOf(number);
    if (this.full || this.fileOf(number) || this.joining.has(file.sha256) || blocks.some((block) => block === null)) {
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
      this.watch('present', `file ${file.sha256}`);
      for (const user of this.users.get(file.sha256)) {
        this.share(user, null, kept);
      }
      await this.removeJoinedBlocks(number);
    } finally {
      this.joining.delete(file.sha256);
    }
  }

  /**
   * Removes each block of the file from the folder, when each file that has the block is kept whole. A file of one
   * block has the SHA-256 of the block, and the page never removes the content of a whole file. A failure goes only to
   * the log: the next visit removes the block.
   */
  async removeJoinedBlocks(number) {
    for (const block of new Set(this.files[number].blocks)) {
      const users = this.blockUsers.get(block) ?? [];
      if (!this.kept.has(block) || this.users.has(block) || users.some((user) => !this.fileOf(user.number))) {
        continue;
      }
      this.kept.delete(block);
      try {
        await this.folder.removeEntry(block);
      } catch (error) {
        this.log(`The page cannot remove a block of ${this.files[number].path}: ${error.message}`);
      }
    }
  }
}

/**
 * A function that gives each change of a download to watch, and ignores an error of watch, so that the map of the
 * downloads can never stop a download. The events are, each with the key "file <sha256>" or "block <sha256>":
 * - present: the page keeps the file or the block,
 * - queued: the file or the block waits in a queue, with the detail 'now' (the game or the start needs it), 'phase'
 *   (a phase of the start needs it next) or 'later',
 * - started: an attempt to get it starts,
 * - bytes: a part of the body arrived, with the detail the number of bytes,
 * - done: the page got it and kept it,
 * - failed: the last attempt failed.
 */
function safeWatch(watch) {
  return (event, key, detail) => {
    try {
      watch(event, key, detail);
    } catch {
      // The map of the downloads only observes them
    }
  };
}

/** The size of each file in the folder of kept files, by its name */
/**
 * Asks the browser to keep the private storage of the site (the game files in OPFS and the saves) when it needs space.
 * Without this, the storage is "best effort", and the browser can remove 700 MB of game files. Chrome and Safari give
 * or refuse it with no question to the player. The argument is navigator.storage. Gives true when the browser keeps the
 * storage, false when it does not, and null when the browser cannot keep it.
 */
async function askToKeepStorage(storage) {
  if (!storage || typeof storage.persist !== 'function') {
    return null;
  }
  try {
    if (typeof storage.persisted === 'function' && await storage.persisted()) {
      return true;
    }
    return await storage.persist();
  } catch {
    return null;
  }
}

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

/** The record of the last visit (assetPathRecord) in the root folder, or an empty record */
async function readAssetPathRecord(root) {
  try {
    const handle = await root.getFileHandle(k_AssetPathRecord);
    return parseAssetPathRecord(await (await handle.getFile()).text());
  } catch {
    return {};
  }
}

/** Keeps the record of the files in the root folder for the next visit. A failure goes only to the log. */
async function writeAssetPathRecord(root, files, log) {
  try {
    const handle = await root.getFileHandle(k_AssetPathRecord, { create: true });
    const writable = await handle.createWritable();
    await writable.write(JSON.stringify({ format: k_AssetPathRecordFormat, files: assetPathRecord(files) }));
    await writable.close();
  } catch (error) {
    log(`The page cannot keep the record of the game files: ${error.message || error}`);
  }
}

/**
 * Takes the blocks of each changed file of reuse (planAssetReuse) that did not change from its old version: the page
 * makes the SHA-256 of each block of the old file, and keeps each block that the new file also has. It adds each kept
 * block to the map kept. Gives { blocks, bytes }: the count and the bytes of the blocks that the page does not get.
 * A failure goes only to the log: the page then downloads the blocks.
 */
async function reuseAssetBlocks(folder, digest, files, reuse, kept, blockSize, log) {
  const result = { blocks: 0, bytes: 0 };
  for (const { number, old } of reuse) {
    const file = files[number];
    try {
      const oldFile = await (await folder.getFileHandle(old)).getFile();
      const wanted = new Map();
      file.blocks.forEach((block, index) => {
        const bounds = assetBlockBounds(file, blockSize, index);
        if (kept.get(block) !== bounds.end - bounds.start) {
          wanted.set(block, bounds.end - bounds.start);
        }
      });
      for (let start = 0; start < oldFile.size && wanted.size > 0; start += blockSize) {
        const part = oldFile.slice(start, Math.min(oldFile.size, start + blockSize));
        const bytes = new Uint8Array(await part.arrayBuffer());
        const sha256 = hexText(await digest('SHA-256', bytes));
        if (wanted.get(sha256) !== bytes.byteLength) {
          continue;
        }
        const writable = await (await folder.getFileHandle(sha256, { create: true })).createWritable();
        await writable.write(bytes);
        await writable.close();
        wanted.delete(sha256);
        kept.set(sha256, bytes.byteLength);
        result.blocks += 1;
        result.bytes += bytes.byteLength;
      }
    } catch (error) {
      log(`The page cannot take the blocks of the old version of ${file.path}: ${error.message || error}`);
    }
  }
  return result;
}

/**
 * Joins the blocks of the file that the folder keeps, checks the whole file against the list and keeps it whole. Gives
 * true when the folder keeps the whole file.
 */
async function joinKeptAssetBlocks(folder, digest, file) {
  const parts = [];
  for (const block of file.blocks) {
    parts.push(await (await folder.getFileHandle(block)).getFile());
  }
  const bytes = new Uint8Array(await new Blob(parts).arrayBuffer());
  if (await assetProblem(digest, file, bytes) !== null) {
    return false;
  }
  const writable = await (await folder.getFileHandle(file.sha256, { create: true })).createWritable();
  await writable.write(bytes);
  await writable.close();
  return true;
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
 * Makes sure that the private storage of the site keeps the game files of the boot phase of the start (see
 * planAssetTiers), or all the game files with the option all. Gives the entries { path, size, file, blocks } of the
 * game files for the game, with file null for each file that the page does not have whole yet, the store that gets
 * the other files and blocks, and the phases (planAssetTiers). Without the option all, the store then gets the files of
 * the next phases in their order, while the game starts and plays. The options are those of keepAssets, and also all,
 * share, log and watch (see AssetStore), tiers (see parseAssetTiers), and startPhases: false when the caller starts the
 * phases itself with store.queuePhases(phases), for example after the program of the game arrived.
 * With the option estimate (a function that gives the result of navigator.storage.estimate()), the page checks the free
 * space of the browser before it downloads:
 * - when the free space is less than the files that the page does not keep, the page does not get all the files before
 *   the start, also with the option all,
 * - when the free space is less than the files of the start, the page stops with an error.
 * The result also has all (true when the page got all the files) and space: { free, rest }, the free space and the
 * bytes of the files that the page does not keep, before the download.
 */
async function streamAssets(index, {
  fetchFile, digest, root, wanted = () => true, progress = () => {}, all: allAsked = false, share = () => {},
  log = () => {}, watch = () => {}, tiers = null, startPhases = true, estimate = null, onFull = () => {},
}) {
  const observe = safeWatch(watch);
  const files = index.files.filter((file) => wanted(file.path));
  const blockSize = index.blockSize ?? 0;
  const folder = await root.getDirectoryHandle(k_AssetCacheFolder, { create: true });
  const keptSizes = await keptAssets(folder);
  // Before the page removes the old version of a changed file in blocks, it takes the blocks that did not change
  const reuse = planAssetReuse(files, keptSizes, await readAssetPathRecord(root));
  if (reuse.length > 0) {
    const reused = await reuseAssetBlocks(folder, digest, files, reuse, keptSizes, blockSize, log);
    log(`The page takes ${reused.blocks} blocks (${Math.round(reused.bytes / (1024 * 1024))} MB) of the old versions ` +
      `of ${reuse.length} changed files, and downloads only the changed blocks.`);
  }
  const plan = planAssetCache(files, keptSizes, blockSize);
  // Remove the old files first, so that the new files have their space
  for (const name of plan.remove) {
    await folder.removeEntry(name, { recursive: true });
  }
  await writeAssetPathRecord(root, files, log);
  const phases = planAssetTiers(files, tiers, blockSize);
  const boot = phases[k_AssetPhaseBoot];
  const startShas = new Set(boot.filter(({ block }) => block === null).map(({ number }) => files[number].sha256));
  const space = { free: Infinity, rest: plan.download.reduce((sum, file) => sum + file.size, 0) };
  let all = allAsked;
  if (estimate !== null) {
    try {
      space.free = assetFreeSpace(await estimate());
    } catch {
      // The browser does not tell its space. The page tries to keep the files.
    }
    if (all && space.free < space.rest) {
      all = false;
      log(`The browser has ${megabytesOf(space.free)} of free space, but the game files need ` +
        `${megabytesOf(space.rest)}. The page gets each file only when the game reads it.`);
    }
  }
  const startFiles = all ? plan.download : plan.download.filter((file) => startShas.has(file.sha256));
  // A file of which the page keeps some blocks comes in its other blocks, and the page then joins the blocks
  const fileOfSha = new Map(files.map((file) => [file.sha256, file]));
  const inParts = ({ sha256 }) => fileOfSha.get(sha256).blocks?.some((block) => plan.blocks.has(block)) ?? false;
  const joined = startFiles.filter(inParts).map(({ sha256 }) => fileOfSha.get(sha256));
  const download = startFiles.filter((file) => !inParts(file));
  // The packs that the start gets whole (assetPackWorth), and the files that it gets alone
  const wantedShas = new Set(download.map((file) => file.sha256));
  const packs = (index.packs ?? []).filter((pack) => assetPackWorth(pack, (sha256) => wantedShas.has(sha256)));
  const packedShas = new Set(packs.flatMap((pack) => pack.members.map((member) => member.sha256)));
  const alone = download.filter((file) => !packedShas.has(file.sha256));
  const blockNames = new Set();
  const startBlocks = [
    ...(all ? [] : boot.filter(({ block }) => block !== null).map(({ number, block }) => ({ file: files[number], block }))),
    ...joined.flatMap((file) => file.blocks.map((_, block) => ({ file, block }))),
  ].filter(({ file, block }) => {
    const name = file.blocks[block];
    const wantedBlock = !plan.ready.has(file.sha256) && !plan.blocks.has(name) && !blockNames.has(name);
    blockNames.add(name);
    return wantedBlock;
  });
  // The map of the downloads shows the kept files and blocks, then the files and blocks that the start needs
  plan.ready.forEach((sha256) => observe('present', `file ${sha256}`));
  plan.blocks.forEach((sha256) => observe('present', `block ${sha256}`));
  download.forEach((file) => observe('queued', `file ${file.sha256}`, 'now'));
  startBlocks.forEach(({ file, block }) => observe('queued', `block ${file.blocks[block]}`, 'now'));
  const blockBytes = ({ file, block }) => {
    const bounds = assetBlockBounds(file, blockSize, block);
    return bounds.end - bounds.start;
  };
  const state = {
    bytes: 0,
    totalBytes: alone.reduce((sum, file) => sum + file.size, 0) + packs.reduce((sum, pack) => sum + pack.size, 0) +
      startBlocks.reduce((sum, next) => sum + blockBytes(next), 0),
    files: 0,
    totalFiles: download.length + startBlocks.length,
  };
  if (space.free < state.totalBytes) {
    throw new Error(`the browser has only ${megabytesOf(space.free)} of free space for the game files, and the start ` +
      `needs ${megabytesOf(state.totalBytes)}`);
  }
  progress({ ...state });
  // A write before the start that finds no more space stops the start with a clear reason
  const keep = async (name, bytes) => {
    try {
      await writeAsset(folder, name, bytes);
    } catch (error) {
      if (isQuotaError(error)) {
        throw new Error('the browser has no more space for the game files. Free some space, or let the browser keep ' +
          'more data for this site');
      }
      throw error;
    }
  };
  // The small files go in their own lanes, because their time is mostly the time of their requests. The packs go in
  // the lanes of the large files.
  const queues = {
    small: alone.filter((file) => file.size < k_AssetSmallFile),
    large: [...packs, ...alone.filter((file) => file.size >= k_AssetSmallFile)],
  };
  const known = new Set(files.map((file) => file.sha256));
  const downloadOf = new Map(download.map((file) => [file.sha256, file]));
  // A file of a pack goes to the folder when the store uses its content and the folder does not keep it
  const unkept = (sha256) => known.has(sha256) && !plan.ready.has(sha256);
  // Gets one file or one pack, with k_AssetAttempts attempts. Gives the bytes of the file, or the parts of the pack.
  const getUnit = async (unit) => {
    const pack = unit.members ? assetPackWatch(unit, observe, unkept) : null;
    const key = `file ${unit.sha256}`;
    for (let attempt = 1; ; attempt += 1) {
      let got = 0;
      const received = (count) => {
        got += count;
        state.bytes += count;
        if (pack) {
          pack.bytes(count);
        } else {
          observe('bytes', key, count);
        }
        progress({ ...state });
      };
      if (pack) {
        pack.event('started');
      } else {
        observe('started', key);
      }
      try {
        return pack ? await fetchAssetPack(fetchFile, digest, unit, received) :
          await fetchAsset(fetchFile, digest, unit, received);
      } catch (error) {
        state.bytes -= got;
        if (attempt >= k_AssetAttempts) {
          if (pack) {
            pack.event('failed');
          } else {
            observe('failed', key);
          }
          throw error;
        }
      }
    }
  };
  const getNext = async (queue) => {
    for (let unit = queue.shift(); unit !== undefined; unit = queue.shift()) {
      if (!unit.members) {
        await keep(unit.sha256, await getUnit(unit));
        observe('done', `file ${unit.sha256}`);
        state.files += 1;
        progress({ ...state });
        continue;
      }
      let parts;
      try {
        parts = await getUnit(unit);
      } catch (error) {
        // The start then gets the files of the pack that it wants alone
        log(`The page cannot get a pack of the game files, and gets its files alone: ${error.message}`);
        const instead = unit.members.map((member) => downloadOf.get(member.sha256)).filter((file) => file);
        state.totalBytes += instead.reduce((sum, file) => sum + file.size, 0) - unit.size;
        queue.push(...instead);
        progress({ ...state });
        continue;
      }
      for (const { sha256, bytes } of parts) {
        if (unkept(sha256)) {
          await keep(sha256, bytes);
          observe('done', `file ${sha256}`);
          state.files += wantedShas.has(sha256) ? 1 : 0;
        }
      }
      progress({ ...state });
    }
  };
  const blockQueue = [...startBlocks];
  const getNextBlock = async () => {
    for (let next = blockQueue.shift(); next !== undefined; next = blockQueue.shift()) {
      const key = `block ${next.file.blocks[next.block]}`;
      let bytes;
      for (let attempt = 1; ; attempt += 1) {
        observe('started', key);
        try {
          bytes = await fetchAssetBlock(fetchFile, digest, next.file, blockSize, next.block);
          break;
        } catch (error) {
          if (attempt >= k_AssetAttempts) {
            observe('failed', key);
            throw error;
          }
        }
      }
      await keep(next.file.blocks[next.block], bytes);
      observe('done', key);
      state.bytes += bytes.byteLength;
      state.files += 1;
      progress({ ...state });
    }
  };
  await Promise.all([
    ...Array.from({ length: k_AssetSmallDownloads }, () => getNext(queues.small)),
    ...Array.from({ length: k_AssetDownloads }, () => getNext(queues.large)),
  ]);
  await Promise.all(Array.from({ length: k_AssetDownloads }, getNextBlock));
  for (const file of joined) {
    if (!await joinKeptAssetBlocks(folder, digest, file)) {
      log(`The page keeps the blocks of ${file.path}, but the whole file is not correct`);
    }
  }
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
  const store = new AssetStore(files, {
    fetchFile, digest, folder, blockSize, packs: index.packs ?? [], kept, share, log, watch, onFull,
  });
  const entries = files.map((file, number) => ({
    path: file.path, size: file.size, file: store.fileOf(number), blocks: file.blocks ? store.blocksOf(number) : null,
  }));
  if (!all && startPhases) {
    store.queuePhases(phases);
  }
  return { entries, store, blockSize, phases: all ? null : phases, all, space };
}

if (typeof module !== 'undefined') {
  module.exports = {
    isSafeAssetPath, parseAssetIndex, planAssetCache, assetAddress, hashedAssetAddress, assetRequest, hexText,
    refusalReason, fetchAssetIndex, fetchAsset, keepAssets, askToKeepStorage, streamAssets, isStartAsset,
    assetPhaseOf, parseAssetTiers, fetchAssetTiers, planAssetTiers, planAssetPrefetch, assetProblem,
    assetBlockBounds, assetWaitHoldsScreen, assetWaitShowsPanel, assetWaitPanelChange,
    assetBlockRange, parseContentRange, fetchAssetBlock, AssetStore, assetPathRecord, parseAssetPathRecord,
    planAssetReuse, isQuotaError, assetFreeSpace, planAssetBackground, assetItemBytes, assetBackgroundStop, writeAsset,
    alignmentMusicName, musicAlignmentsOfLog, assetLandOfLog,
    parseAssetPacks, assetPackWorth, assetPackWatch, fetchAssetPack,
    k_AssetIndexAddress, k_AssetCacheFolder, k_AssetTiersAddress, k_AssetPagePhases, k_AssetSmallFile, k_AssetSmallDownloads,
    k_AssetPhaseDownloads, k_AssetDownloads, k_AssetPackShare,
  };
}
