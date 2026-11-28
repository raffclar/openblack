// Test of the game files of the site on the web page (web/page/launcher-assets.js). Run with node.
'use strict';

const assert = require('node:assert/strict');
const { createHash } = require('node:crypto');
const {
  isSafeAssetPath, parseAssetIndex, planAssetCache, assetAddress, hashedAssetAddress, assetRequest, hexText,
  refusalReason, fetchAssetIndex, fetchAsset, keepAssets, askToKeepStorage, streamAssets: streamWithTiers, isStartAsset,
  parseAssetTiers, fetchAssetTiers, planAssetTiers, planAssetPrefetch, assetBlockBounds, assetBlockRange,
  parseContentRange, assetWaitHoldsScreen, assetWaitShowsPanel, assetWaitPanelChange, AssetStore,
  k_AssetIndexAddress, k_AssetCacheFolder, k_AssetTiersAddress, k_AssetPagePhases, k_AssetSmallFile,
  k_AssetSmallDownloads, k_AssetPhaseDownloads, k_AssetDownloads, planAssetBackground, assetItemBytes,
  assetBackgroundStop, assetFreeSpace, assetPackWorth, assetPackWatch, k_AssetPackShare,
} = require('../web/page/launcher-assets.js');

let count = 0;
const tests = [];
const check = (name, body) => tests.push({ name, body });

const sha = (text) => createHash('sha256').update(text).digest('hex');
const digest = async (algorithm, bytes) => {
  assert.equal(algorithm, 'SHA-256');
  return createHash('sha256').update(bytes).digest();
};
const indexText = (files, extra = {}) => JSON.stringify({
  format: 1,
  archive_sha256: 'a'.repeat(64),
  files: Object.fromEntries(Object.entries(files).map(([path, text]) =>
    [path, { size: Buffer.byteLength(text), sha256: sha(text) }])),
  ...extra,
});

// A fake response of fetch. The body comes in parts of three bytes.
const fakeResponse = (status, text, headers = {}) => ({
  status,
  ok: status >= 200 && status < 300,
  headers: { get: (name) => headers[name.toLowerCase()] ?? null },
  text: async () => text,
  arrayBuffer: async () => new Uint8Array(Buffer.from(text)).buffer,
  body: {
    getReader() {
      const bytes = Buffer.from(text);
      let at = 0;
      return {
        read: async () => {
          if (at >= bytes.length) {
            return { done: true };
          }
          const value = new Uint8Array(bytes.subarray(at, at + 3));
          at += 3;
          return { done: false, value };
        },
        cancel: async () => {},
      };
    },
  },
});

// A fake fetch that gives the site's files. It counts the requests for each address.
// It also keeps the cache mode of each request.
const fakeSite = (files, { status = 200, index = indexText(files), change = {} } = {}) => {
  const requests = new Map();
  const caches = new Map();
  const fetchFile = async (address, options = {}) => {
    const range = options.headers && options.headers.Range;
    const key = range ? `${address} ${range}` : address;
    requests.set(key, (requests.get(key) ?? 0) + 1);
    caches.set(key, options.cache);
    if (address === k_AssetIndexAddress) {
      return fakeResponse(status, status === 200 ? index : 'Your account cannot download the game files yet.');
    }
    const path = Object.keys(files).find((name) =>
      assetAddress(name) === address || hashedAssetAddress(sha(files[name])) === address);
    if (path === undefined) {
      return fakeResponse(404, '');
    }
    const text = change[path] ?? files[path];
    const match = range && /^bytes=(\d+)-(\d+)$/.exec(range);
    if (match) {
      const [start, last] = [Number(match[1]), Number(match[2])];
      return fakeResponse(206, Buffer.from(text).subarray(start, last + 1).toString('latin1'),
        { 'content-range': `bytes ${start}-${last}/${Buffer.byteLength(text)}` });
    }
    return fakeResponse(200, text);
  };
  return { fetchFile, requests, caches };
};

// A fake folder of the private storage of the site
class FakeFolder {
  constructor() {
    this.kind = 'directory';
    this.children = new Map();
  }
  async getDirectoryHandle(name, { create = false } = {}) {
    if (!this.children.has(name) && create) {
      this.children.set(name, new FakeFolder());
    }
    const child = this.children.get(name);
    if (!child || child.kind !== 'directory') {
      throw new Error('NotFoundError');
    }
    return child;
  }
  async getFileHandle(name, { create = false } = {}) {
    if (!this.children.has(name) && create) {
      this.children.set(name, new FakeFile());
    }
    const child = this.children.get(name);
    if (!child || child.kind !== 'file') {
      throw new Error('NotFoundError');
    }
    return child;
  }
  async removeEntry(name) {
    this.children.delete(name);
  }
  async* entries() {
    yield* this.children.entries();
  }
}

class FakeFile {
  constructor(bytes = new Uint8Array()) {
    this.kind = 'file';
    this.bytes = bytes;
  }
  async getFile() {
    return Object.assign(new Blob([this.bytes]), { bytes: this.bytes });
  }
  async createWritable() {
    const parts = [];
    return {
      write: async (bytes) => parts.push(Buffer.from(bytes)),
      close: async () => {
        this.bytes = new Uint8Array(Buffer.concat(parts));
      },
    };
  }
}

const k_Files = {
  'Data/AllMeshes.g3d': 'meshes',
  'Scripts/Land1.txt': 'land one',
  'Audio/Music/a.sad': 'music',
  'Audio/Music/copy.sad': 'music',
};

check('a safe path is a relative path with no empty part, no dots and no back slash', () => {
  assert.equal(isSafeAssetPath('Data/AllMeshes.g3d'), true);
  assert.equal(isSafeAssetPath('Online Maps/Two Gods.lnd'), true);
  for (const path of ['', '/Data/a', 'Data//a', 'Data/./a', 'Data/../a', '..', 'Data\\a', 'C:/a', 'Data/a\n',
    'Data/a/', 'x'.repeat(600), 42, undefined]) {
    assert.equal(isSafeAssetPath(path), false, JSON.stringify(path));
  }
});

check('the list gives each file with its path, size and SHA-256', () => {
  const index = parseAssetIndex(indexText(k_Files, { total_bytes: 24 }));
  assert.equal(index.files.length, 4);
  assert.equal(index.totalBytes, 24);
  assert.equal(index.archive, 'a'.repeat(64));
  assert.deepEqual(index.files[0], {
    path: 'Data/AllMeshes.g3d', size: 6, sha256: sha('meshes'), address: '/assets/files/Data/AllMeshes.g3d',
    hashed: false,
  });
});

check('the page refuses a list that is not correct', () => {
  const good = { size: 1, sha256: 'b'.repeat(64) };
  const bad = [
    'not json',
    '[]',
    JSON.stringify({ format: 4, files: { a: good } }),
    JSON.stringify({ format: 1 }),
    JSON.stringify({ format: 1, files: {} }),
    JSON.stringify({ format: 1, files: { 'Data/../../etc/passwd': good } }),
    JSON.stringify({ format: 1, files: { 'Data\\a': good } }),
    JSON.stringify({ format: 1, files: { 'Data/a': good, 'data/A': good } }),
    JSON.stringify({ format: 1, files: { 'Data/a': { size: -1, sha256: 'b'.repeat(64) } } }),
    JSON.stringify({ format: 1, files: { 'Data/a': { size: 1.5, sha256: 'b'.repeat(64) } } }),
    JSON.stringify({ format: 1, files: { 'Data/a': { size: 1, sha256: 'B'.repeat(64) } } }),
    JSON.stringify({ format: 1, files: { 'Data/a': { size: 1, sha256: 'b'.repeat(63) } } }),
    JSON.stringify({ format: 1, files: { 'Data/a': good }, total_bytes: 2 }),
  ];
  for (const text of bad) {
    assert.throws(() => parseAssetIndex(text), Error, text);
  }
});

check('the plan downloads each content one time and removes the files that the list does not use', () => {
  const files = parseAssetIndex(indexText(k_Files)).files;
  const kept = new Map([
    [sha('meshes'), 6], // correct: ready
    [sha('land one'), 3], // incorrect size: remove and download again
    ['b'.repeat(64), 10], // the list does not use it
    ['.tmp', 1],
  ]);
  const plan = planAssetCache(files, kept);
  assert.deepEqual([...plan.ready], [sha('meshes')]);
  assert.deepEqual(plan.remove.sort(), [sha('land one'), 'b'.repeat(64), '.tmp'].sort());
  assert.deepEqual(plan.download.map((file) => file.path), ['Scripts/Land1.txt', 'Audio/Music/a.sad']);
});

check('the address of a file encodes each part of its path', () => {
  assert.equal(assetAddress('Online Maps/a#b?.lnd'), '/assets/files/Online%20Maps/a%23b%3F.lnd');
});

check('an answer of 204 gives no game files, with no attempt to read a list', async () => {
  let read = false;
  const answer = await fetchAssetIndex(async () => ({
    status: 204,
    ok: true,
    text: async () => {
      read = true;
      return '';
    },
  }));
  assert.deepEqual(answer, {
    granted: false, status: 204, reason: 'the site gives no game files to this account',
  });
  assert.equal(read, false);
});

check('the text of bytes has two hexadecimal digits for each byte', () => {
  assert.equal(hexText(new Uint8Array([0, 15, 255]).buffer), '000fff');
});

check('the reason of a refusal is the detail of the problem, or else the text', () => {
  const problem = JSON.stringify({ status: 403, detail: 'Your account cannot download the game files yet.' });
  assert.equal(refusalReason(problem), 'Your account cannot download the game files yet.');
  assert.equal(refusalReason('  Not found\n'), 'Not found');
  assert.equal(refusalReason('x'.repeat(300)).length, 200);
});

check('an account without the mark gets the refusal of the site', async () => {
  const refused = await fetchAssetIndex(fakeSite(k_Files, { status: 403 }).fetchFile);
  assert.equal(refused.granted, false);
  assert.equal(refused.status, 403);
  assert.match(refused.reason, /cannot download/);
  const signedOut = await fetchAssetIndex(fakeSite(k_Files, { status: 401 }).fetchFile);
  assert.equal(signedOut.status, 401);
  const offline = await fetchAssetIndex(async () => {
    throw new Error('network');
  });
  assert.deepEqual([offline.granted, offline.status], [false, 0]);
  const broken = await fetchAssetIndex(fakeSite(k_Files, { index: '{"format":1}' }).fetchFile);
  assert.equal(broken.granted, false);
});

check('the first visit downloads all the files and the second visit downloads none', async () => {
  const root = new FakeFolder();
  const site = fakeSite(k_Files);
  const { index } = await fetchAssetIndex(site.fetchFile);
  const states = [];
  const entries = await keepAssets(index, { fetchFile: site.fetchFile, digest, root, progress: (state) => states.push(state) });
  assert.deepEqual(entries.map((entry) => entry.path), Object.keys(k_Files));
  assert.equal(Buffer.from(entries[1].file.bytes).toString(), 'land one');
  assert.equal(entries[2].file, entries[3].file);
  assert.deepEqual(states.at(-1), { bytes: 19, totalBytes: 19, files: 3, totalFiles: 3 });
  const folder = await root.getDirectoryHandle(k_AssetCacheFolder);
  assert.deepEqual([...folder.children.keys()].sort(), [sha('meshes'), sha('land one'), sha('music')].sort());

  const again = fakeSite(k_Files);
  const second = await keepAssets(index, { fetchFile: again.fetchFile, digest, root });
  assert.equal(second.length, 4);
  assert.equal(again.requests.size, 0);
});

check('a changed list downloads only the changed file and removes the old one', async () => {
  const root = new FakeFolder();
  const site = fakeSite(k_Files);
  await keepAssets((await fetchAssetIndex(site.fetchFile)).index, { fetchFile: site.fetchFile, digest, root });
  const changed = { ...k_Files, 'Scripts/Land1.txt': 'land one, new' };
  const next = fakeSite(changed);
  await keepAssets((await fetchAssetIndex(next.fetchFile)).index, { fetchFile: next.fetchFile, digest, root });
  assert.deepEqual([...next.requests.keys()], [k_AssetIndexAddress, assetAddress('Scripts/Land1.txt')]);
  const folder = await root.getDirectoryHandle(k_AssetCacheFolder);
  assert.equal(folder.children.has(sha('land one')), false);
  assert.equal(folder.children.has(sha('land one, new')), true);
});

check('a file with an incorrect SHA-256 or size stops the download and the page keeps nothing of it', async () => {
  for (const wrong of ['MESHES', 'meshes and more', 'mesh']) {
    const root = new FakeFolder();
    const site = fakeSite(k_Files, { change: { 'Data/AllMeshes.g3d': wrong } });
    const { index } = await fetchAssetIndex(site.fetchFile);
    await assert.rejects(keepAssets(index, { fetchFile: site.fetchFile, digest, root }));
    const folder = await root.getDirectoryHandle(k_AssetCacheFolder);
    assert.equal(folder.children.has(sha('meshes')), false);
    // The page tries each file two times
    assert.equal(site.requests.get(assetAddress('Data/AllMeshes.g3d')), 2);
  }
});

check('the page gets only the files that the game reads', async () => {
  const root = new FakeFolder();
  const site = fakeSite(k_Files);
  const { index } = await fetchAssetIndex(site.fetchFile);
  const entries = await keepAssets(index, {
    fetchFile: site.fetchFile, digest, root, wanted: (path) => path.startsWith('Data/'),
  });
  assert.deepEqual(entries.map((entry) => entry.path), ['Data/AllMeshes.g3d']);
  assert.equal(site.requests.size, 2);
});

check('a compressed reply passes: the size check uses the body and not the Content-Length header', async () => {
  const text = 'meshes meshes meshes meshes';
  const file = { path: 'Data/AllMeshes.g3d', size: Buffer.byteLength(text), sha256: sha(text) };
  // The header says 9 bytes (the compressed size); the body that the browser gives has all the bytes
  const fetchFile = async () => ({ ...fakeResponse(200, text), headers: { get: () => '9' } });
  let received = 0;
  const bytes = await fetchAsset(fetchFile, digest, file, (count) => { received += count; });
  assert.equal(Buffer.from(bytes).toString(), text);
  assert.equal(received, file.size);
  // A body with a wrong size still fails, also when the header has the right size
  const shortFetch = async () => ({ ...fakeResponse(200, 'meshes'), headers: { get: () => String(file.size) } });
  await assert.rejects(fetchAsset(shortFetch, digest, file, () => {}), /bytes for Data\/AllMeshes.g3d/);
});

// A list of the second version: the music bank and the video have blocks of four bytes
const k_BlockSize = 4;
const k_Streamed = {
  'Data/AllMeshes.g3d': 'meshes',
  'Audio/Music/align/Good.sad': 'HEADmusic of the good god',
  'Data/INTRO.bik': 'the intro film',
  'Data/Landscape/Land1.lnd': 'land one',
  'Data/Landscape/Land2.lnd': 'land two',
  'Data/Landscape/land2.fot': 'paths two',
  'Data/Spells/Anims/heal.anm': 'heal',
  'Online Maps/mpm_2p_1.map': 'two gods',
};
const blocksOf = (text) => {
  const bytes = Buffer.from(text);
  const blocks = [];
  for (let at = 0; at < bytes.length; at += k_BlockSize) {
    blocks.push(sha(bytes.subarray(at, at + k_BlockSize)));
  }
  return blocks;
};
const streamedText = (files = k_Streamed, blocked = ['Audio/Music/align/Good.sad', 'Data/INTRO.bik'], extra = {}) =>
  JSON.stringify({
    format: 2,
    block_size: k_BlockSize,
    ...extra,
    files: Object.fromEntries(Object.entries(files).map(([path, text]) => [path, {
      size: Buffer.byteLength(text), sha256: sha(text), ...(blocked.includes(path) ? { blocks: blocksOf(text) } : {}),
    }])),
  });
const numberOf = (files, path) => files.findIndex((file) => file.path === path);

// A tiers file for the streamed files: the meshes and the first block of the music at the startup, land 1 and the intro
// film at the land load, the heal miracle in the play of land 1. Land 2 is a later land.
const tiersText = (extra = {}) => JSON.stringify({
  format: 1,
  made_from: { start: ['traces/land1.json'], later: ['traces/land2.json'] },
  phases: ['boot', 'pre-intro', 'startup', 'land-load', 'land-soon', 'later', 'background'],
  start_phases: 4,
  block_size: k_BlockSize,
  files: {
    'scripts/infoscript2.txt': { phase: 0, order: 0 },
    'data/logo.bik': { phase: 0, order: 1 },
    'data/pre_intro.bik': { phase: 1, order: 0 },
    'data/allmeshes.g3d': { phase: 2, order: 0 },
    'audio/music/align/good.sad': { phase: 2, order: 1, blocks: [[2, 0, 1]] },
    'data/landscape/land1.lnd': { phase: 3, order: 0 },
    'data/intro.bik': { phase: 3, order: 1 },
    'data/spells/anims/heal.anm': { phase: 4, order: 0 },
    'data/landscape/land2.fot': { phase: 5, order: 0 },
    'data/citadel/engine/mainlo_l3d.zzz': { phase: 6, order: 0 },
    'data/not/in/the/list.txt': { phase: 0, order: 0 },
  },
  lands: { land2: ['data/landscape/land2.fot', 'data/landscape/land2.lnd'] },
  ...extra,
});
const k_TestTiers = parseAssetTiers(tiersText());
// The page streams the files with the tiers of the test, but a test can give other tiers or no tiers
const streamAssets = (index, options) => streamWithTiers(index, { tiers: k_TestTiers, ...options });

check('the second version of the list gives the blocks of a file, and the first version still reads', () => {
  const index = parseAssetIndex(streamedText());
  assert.equal(index.blockSize, k_BlockSize);
  const bank = index.files.find((file) => file.path === 'Audio/Music/align/Good.sad');
  assert.deepEqual(bank.blocks, blocksOf(k_Streamed['Audio/Music/align/Good.sad']));
  assert.equal(index.files.find((file) => file.path === 'Data/AllMeshes.g3d').blocks, undefined);
  assert.equal(parseAssetIndex(indexText(k_Files)).blockSize, 0);
  const good = { size: 9, sha256: 'b'.repeat(64) };
  for (const wrong of [
    { format: 2, block_size: 4, files: { a: { ...good, blocks: ['c'.repeat(64)] } } },
    { format: 2, block_size: 4, files: { a: { ...good, blocks: ['c'.repeat(64), 'c'.repeat(64), 'C'.repeat(64)] } } },
    { format: 2, files: { a: { ...good, blocks: ['c'.repeat(64)] } } },
    { format: 2, block_size: -4, files: { a: good } },
    { format: 2, block_size: 4, files: { a: { size: 0, sha256: 'b'.repeat(64), blocks: [] } } },
  ]) {
    assert.throws(() => parseAssetIndex(JSON.stringify(wrong)), Error, JSON.stringify(wrong));
  }
});

check('the blocks of a read are the blocks that have its first and its last byte', () => {
  const file = { size: 10 };
  assert.deepEqual(assetBlockBounds(file, 4, 2), { start: 8, end: 10 });
  assert.deepEqual(assetBlockRange(file, 4, 0, 1), { first: 0, last: 0 });
  assert.deepEqual(assetBlockRange(file, 4, 3, 2), { first: 0, last: 1 });
  assert.deepEqual(assetBlockRange(file, 4, 5, 100), { first: 1, last: 2 });
  assert.equal(assetBlockRange(file, 4, 10, 4), null);
  assert.equal(assetBlockRange(file, 4, 2, 0), null);
});

check('the range of an answer must be a correct range of the file', () => {
  assert.deepEqual(parseContentRange('bytes 4-7/25'), { start: 4, last: 7, size: 25 });
  for (const wrong of ['bytes 7-4/25', 'bytes 4-25/25', 'bytes */25', 'bytes 4-7', '', null]) {
    assert.equal(parseContentRange(wrong), null, wrong);
  }
});

check('the plan keeps the blocks of a file that the page does not keep whole', () => {
  const { files } = parseAssetIndex(streamedText());
  const bank = files.find((file) => file.path === 'Audio/Music/align/Good.sad');
  const film = files.find((file) => file.path === 'Data/INTRO.bik');
  const kept = new Map([
    [bank.blocks[0], 4], // a block of a file that the page does not keep whole: keep
    [bank.blocks[1], 3], // an incorrect size: remove
    [film.sha256, film.size], // the whole film
    [film.blocks[0], 4], // a block of a file that the page keeps whole: remove
  ]);
  const plan = planAssetCache(files, kept, k_BlockSize);
  assert.deepEqual([...plan.blocks], [bank.blocks[0]]);
  assert.deepEqual(plan.remove.sort(), [bank.blocks[1], film.blocks[0]].sort());
  assert.deepEqual([...plan.ready], [film.sha256]);
});

check('with no tiers file the game starts with all the files of the start and the first block of each music bank', () => {
  for (const path of ['Data/AllMeshes.g3d', 'Audio/Music/align/Good.sad', 'Data/Landscape/Land1.lnd',
    'Data/Citadel/engine/main.cam', 'Scripts/Land2.txt']) {
    assert.equal(isStartAsset(path), true, path);
  }
  for (const path of ['Data/Landscape/Land2.lnd', 'DATA/LANDSCAPE/LandT.lnd', 'Data/Landscape/mpm_3p_2.lnd',
    'Data/Landscape/Multi_Player/a.lnd', 'Online Maps/mpm_2p_1.map', 'Data/Spells/Anims/heal.anm',
    'Data/Citadel/engine_old/a.zzz']) {
    assert.equal(isStartAsset(path), false, path);
  }
  const { files } = parseAssetIndex(streamedText());
  const phases = planAssetTiers(files, null, k_BlockSize);
  const paths = (items) => items.map(({ number, block }) => (block === null ? files[number].path : `${files[number].path} ${block}`));
  assert.deepEqual(phases.map(paths), [
    ['Data/AllMeshes.g3d', 'Audio/Music/align/Good.sad 0', 'Data/INTRO.bik', 'Data/Landscape/Land1.lnd'], [], [], [],
    ['Data/Spells/Anims/heal.anm'],
    ['Data/Landscape/Land2.lnd', 'Data/Landscape/land2.fot', 'Online Maps/mpm_2p_1.map']]);
});

check('the tiers file gives the phase of each file, the files of the first land and the files of each land', () => {
  const tiers = parseAssetTiers(tiersText());
  assert.equal(tiers.startPhases, 4);
  assert.equal(tiers.files.get('data/landscape/land1.lnd').phase, 3);
  assert.equal(tiers.files.get('data/allmeshes.g3d').blocks, null);
  assert.deepEqual(tiers.soon, ['data/spells/anims/heal.anm']);
  assert.deepEqual(tiers.lands.get('land2'), ['data/landscape/land2.fot', 'data/landscape/land2.lnd']);
  const good = JSON.parse(tiersText());
  for (const wrong of [
    { ...good, format: 2 },
    { ...good, start_phases: 12 },
    { ...good, block_size: 0 },
    { ...good, files: { 'Data/Upper.txt': { phase: 0, order: 0 } } },
    { ...good, files: { '../a.txt': { phase: 0, order: 0 } } },
    { ...good, files: { 'a.txt': { phase: 7, order: 0 } } },
    { ...good, files: { 'a.txt': { phase: 0, order: 0, blocks: [[0, 0, 0]] } } },
    { ...good, lands: { land2: ['Data/Upper.txt'] } },
  ]) {
    assert.throws(() => parseAssetTiers(JSON.stringify(wrong)), Error, JSON.stringify(wrong));
  }
  assert.throws(() => parseAssetTiers('not json'), /not JSON/);
});

check('the page uses its rules when it has no correct tiers file', async () => {
  const logged = [];
  const answer = (status, text) => async (address) => {
    assert.equal(address, k_AssetTiersAddress);
    return fakeResponse(status, text);
  };
  assert.equal(await fetchAssetTiers(answer(404, ''), (line) => logged.push(line)), null);
  assert.equal(await fetchAssetTiers(answer(200, '{"format": 9}'), (line) => logged.push(line)), null);
  assert.equal(await fetchAssetTiers(async () => {
    throw new Error('no network');
  }, (line) => logged.push(line)), null);
  assert.equal(logged.length, 3);
  assert.match(logged[0], /status 404/);
  assert.equal((await fetchAssetTiers(answer(200, tiersText()))).startPhases, 4);
});

check('the tiers give each file and each run of blocks its phase, and a file that they do not give is later', () => {
  const { files } = parseAssetIndex(streamedText());
  const tiers = parseAssetTiers(tiersText({ files: {
    'data/allmeshes.g3d': { phase: 2, order: 1 },
    'data/landscape/land1.lnd': { phase: 2, order: 0 },
    'audio/music/align/good.sad': { phase: 2, order: 2, blocks: [[2, 0, 1], [3, 2, 1], [4, 3, 2]] },
    'data/intro.bik': { phase: 6, order: 0 },
  } }));
  for (const path of ['Data/AllMeshes.g3d', 'DATA/LANDSCAPE/Land1.lnd', 'Audio/Music/align/Good.sad']) {
    assert.equal(isStartAsset(path, tiers), true, path);
  }
  for (const path of ['Data/INTRO.bik', 'Data/Landscape/land2.fot', 'Data/Spells/Anims/heal.anm']) {
    assert.equal(isStartAsset(path, tiers), false, path);
  }
  const phases = planAssetTiers(files, tiers, k_BlockSize);
  const paths = (items) => items.map(({ number, block }) => (block === null ? files[number].path : `${files[number].path} ${block}`));
  // The background phase of the tiers file is a later phase of the page
  assert.deepEqual(phases.map(paths), [[], [],
    ['Data/Landscape/Land1.lnd', 'Data/AllMeshes.g3d', 'Audio/Music/align/Good.sad 0'],
    ['Audio/Music/align/Good.sad 2'],
    ['Audio/Music/align/Good.sad 3', 'Audio/Music/align/Good.sad 4', 'Data/Spells/Anims/heal.anm'],
    ['Data/INTRO.bik 0', 'Data/INTRO.bik 1', 'Data/INTRO.bik 2', 'Data/INTRO.bik 3', 'Data/Landscape/Land2.lnd',
      'Data/Landscape/land2.fot', 'Online Maps/mpm_2p_1.map']]);
  // The list has blocks of another size: the page gets the whole file
  assert.deepEqual(paths(planAssetTiers(files, tiers, 8)[2]),
    ['Data/Landscape/Land1.lnd', 'Data/AllMeshes.g3d', 'Audio/Music/align/Good.sad']);
  assert.deepEqual(paths(planAssetTiers(files, tiers, 8)[5])[0], 'Data/INTRO.bik');
});

check('the page gets a film in its blocks, in the order of the film, in the phase of the film', () => {
  const films = { 'Data/pre_intro.bik': 'the pre-intro film, in four blocks' };
  const filmFiles = parseAssetIndex(streamedText(films, Object.keys(films))).files;
  const filmTiers = parseAssetTiers(tiersText({ files: { 'data/pre_intro.bik': { phase: 1, order: 0 } } }));
  const phases = planAssetTiers(filmFiles, filmTiers, k_BlockSize);
  assert.deepEqual(phases[1].map(({ block }) => block), filmFiles[0].blocks.map((_, block) => block));
  assert.ok(filmFiles[0].blocks.length > 1);
  assert.deepEqual(phases.filter((items, phase) => phase !== 1).flat(), []);
});

check('the tiers file of the page starts the game with the boot files and never gets the low-detail meshes first', () => {
  const tiers = parseAssetTiers(require('node:fs').readFileSync(`${__dirname}/../web/page/asset-tiers.json`, 'utf8'));
  const phaseOf = (path) => {
    const tier = tiers.files.get(path);
    return tier === undefined ? 'later' : tiers.phases[tier.phase];
  };
  for (const path of ['country.txt', 'scripts/infoscript2.txt', 'data/logo.bik', 'data/j0.fnt']) {
    assert.equal(phaseOf(path), 'boot', path);
  }
  assert.equal(phaseOf('data/pre_intro.bik'), 'pre-intro');
  assert.equal(phaseOf('data/landscape/land1.lnd'), 'land-load');
  for (const path of ['data/citadel/engine/creaturelo_l3d.zzz', 'data/landscape/land2.lnd', 'scripts/html/banner.gif']) {
    assert.equal(isStartAsset(path, tiers), false, path);
  }
  // A bank of sound effects comes whole, also when the tiers give it in blocks. Only a music bank comes in blocks.
  const banks = { 'Audio/SFX/Creature/btiger.sad': 'the tiger, in some blocks', 'Audio/Music/align/good.sad': 'the good music' };
  const bankFiles = parseAssetIndex(streamedText(banks, Object.keys(banks))).files;
  const bankTiers = parseAssetTiers(tiersText({ files: {
    'audio/sfx/creature/btiger.sad': { phase: 2, order: 0, blocks: [[2, 0, 1]] },
    'audio/music/align/good.sad': { phase: 2, order: 1, blocks: [[2, 0, 1]] },
  } }));
  assert.deepEqual(planAssetTiers(bankFiles, bankTiers, k_BlockSize)[2].map(({ block }) => block), [null, 0]);
  // A voice bank of a species that no trace read comes in the land-soon phase
  const files = parseAssetIndex(indexText({ 'Audio/SFX/Creature/btiger.sad': 'tiger', 'Data/Landscape/Land2.lnd': 'two' })).files;
  assert.deepEqual(planAssetTiers(files, k_TestTiers).map((items) => items.length), [0, 0, 0, 0, 1, 1]);
  const boot = [...tiers.files.values()].filter((tier) => tier.phase === 0);
  assert.ok(boot.length > 0 && boot.length < 20, `${boot.length} boot files`);
});

check('a read that waits for the site shows the map only when the screen has no new frame for a second', () => {
  assert.equal(assetWaitHoldsScreen('Data/AllMeshes.g3d'), true);
  assert.equal(assetWaitHoldsScreen('AUDIO/MUSIC/align/good.sad'), false);
  assert.equal(assetWaitShowsPanel({ waitedMs: 1500, stillMs: 1200 }), true);
  assert.equal(assetWaitShowsPanel({ waitedMs: 1500, stillMs: 300 }), false);
  assert.equal(assetWaitShowsPanel({ waitedMs: 400, stillMs: 5000 }), false);
  assert.equal(assetWaitShowsPanel({ waitedMs: -Infinity, stillMs: 5000 }), false);
  // The map opens after one second of wait with no new frame, and closes only when the read ends
  assert.equal(assetWaitPanelChange({ open: false, forWait: false, waitedMs: 1500, stillMs: 1200 }), 'open');
  assert.equal(assetWaitPanelChange({ open: false, forWait: false, waitedMs: 900, stillMs: 1200 }), undefined);
  assert.equal(assetWaitPanelChange({ open: true, forWait: true, waitedMs: 3000, stillMs: 0 }), undefined);
  assert.equal(assetWaitPanelChange({ open: true, forWait: true, waitedMs: -Infinity, stillMs: 0 }), 'close');
  // A map that the player opened stays open, and a map that the player closed during a wait stays closed
  assert.equal(assetWaitPanelChange({ open: true, forWait: false, waitedMs: -Infinity, stillMs: 0 }), undefined);
  assert.equal(assetWaitPanelChange({ open: false, forWait: false, dismissed: true, waitedMs: 5000, stillMs: 5000 }),
    undefined);
});

check('the plan for later gets the next land, and the files of a network map', () => {
  const { files } = parseAssetIndex(streamedText());
  const paths = (numbers) => numbers.map((number) => files[number].path);
  assert.deepEqual(paths(planAssetPrefetch(files, { path: 'DATA/LANDSCAPE/Land1.lnd' })),
    ['Data/Landscape/Land2.lnd', 'Data/Landscape/land2.fot']);
  const have = (sha256) => sha256 === sha('land two');
  assert.deepEqual(paths(planAssetPrefetch(files, { path: 'Data/Landscape/Land1.lnd', have })),
    ['Data/Landscape/land2.fot']);
  assert.deepEqual(paths(planAssetPrefetch(files, { path: 'Data/Landscape/Land5.lnd' })), []);
  assert.deepEqual(paths(planAssetPrefetch(files, { path: 'Data/AllMeshes.g3d' })), []);
});

check('with the tiers the plan for later gets the files of the next land of the tiers', () => {
  const { files } = parseAssetIndex(streamedText());
  const paths = (numbers) => numbers.map((number) => files[number].path);
  const tiers = parseAssetTiers(tiersText({ lands: { land2: ['data/landscape/land2.fot', 'online maps/mpm_2p_1.map'] } }));
  assert.deepEqual(paths(planAssetPrefetch(files, { path: 'Data/Landscape/Land1.lnd', tiers })),
    ['Data/Landscape/Land2.lnd', 'Data/Landscape/land2.fot', 'Online Maps/mpm_2p_1.map']);
  const have = (sha256) => sha256 === sha('land two');
  assert.deepEqual(paths(planAssetPrefetch(files, { path: 'Data/Landscape/Land1.lnd', have, tiers })),
    ['Data/Landscape/land2.fot', 'Online Maps/mpm_2p_1.map']);
});

// A site for the second version of the list, and the store of the page with what it gave to the game threads
const streamedSite = async (options = {}) => {
  const root = new FakeFolder();
  const site = fakeSite(k_Streamed, { index: streamedText(), ...options });
  const { index } = await fetchAssetIndex(site.fetchFile);
  const shared = [];
  const logged = [];
  const result = await streamAssets(index, {
    fetchFile: site.fetchFile, digest, root, share: (number, block, file) => shared.push({ number, block, file }),
    log: (line) => logged.push(line),
  });
  // The phases of the start come in the background. Most tests begin when they are complete.
  await result.store.drained();
  shared.length = 0;
  return { root, site, index, shared, logged, ...result };
};

check('before the start the page gets only the boot files, then the phases and the first block of the music', async () => {
  const { site, entries, index, store } = await streamedSite();
  const entry = (path) => entries[numberOf(index.files, path)];
  // The list has no file of the boot phase: the game starts with no file, and the phases come after
  assert.equal(entry('Data/Landscape/Land1.lnd').file, null);
  await store.drained();
  const music = assetAddress('Audio/Music/align/Good.sad');
  // The intro film comes in its blocks
  const intro = assetAddress('Data/INTRO.bik');
  assert.deepEqual([...site.requests.keys()].sort(), [k_AssetIndexAddress, assetAddress('Data/AllMeshes.g3d'),
    `${intro} bytes=0-3`, `${intro} bytes=4-7`, `${intro} bytes=8-11`, `${intro} bytes=12-13`,
    assetAddress('Data/Landscape/Land1.lnd'), `${music} bytes=0-3`, assetAddress('Data/Spells/Anims/heal.anm')].sort());
  assert.equal(Buffer.from(store.fileOf(numberOf(index.files, 'Data/Landscape/Land1.lnd')).bytes).toString(), 'land one');
  assert.equal(entry('Data/Landscape/Land2.lnd').file, null);
  assert.equal(entry('Data/Landscape/Land2.lnd').size, 8);
  const bank = store.blocksOf(numberOf(index.files, 'Audio/Music/align/Good.sad'));
  assert.equal(store.fileOf(numberOf(index.files, 'Audio/Music/align/Good.sad')), null);
  assert.equal(Buffer.from(bank[0].bytes).toString(), 'HEAD');
  assert.deepEqual(bank.slice(1), [null, null, null, null, null, null]);
});

check('the map of the downloads sees the start files fill and the second visit keeps them', async () => {
  const { AssetMapModel, buildAssetMap } = require('../web/page/launcher-map.js');
  const root = new FakeFolder();
  const site = fakeSite(k_Streamed, { index: streamedText() });
  const { index } = await fetchAssetIndex(site.fetchFile);
  const visit = async () => {
    const model = new AssetMapModel(buildAssetMap(index.files, { blockSize: index.blockSize }));
    const events = [];
    const result = await streamAssets(index, {
      fetchFile: site.fetchFile, digest, root,
      watch: (event, key, detail) => {
        events.push(event);
        model.handle(event, key, detail);
      },
    });
    return { model, events, ...result };
  };
  const first = await visit();
  await first.store.drained();
  const state = (model, path) => model.stateOf(model.map.groups.flatMap((group) => group.items)
    .find((item) => item.path === path));
  assert.equal(state(first.model, 'Data/Landscape/Land1.lnd'), 'present');
  assert.equal(state(first.model, 'Data/Landscape/Land2.lnd'), 'lazy');
  assert.equal(first.model.totals().received, 6 + 14 + 8 + 4 + 4);
  assert.ok(first.events.includes('queued') && first.events.includes('bytes') && first.events.includes('done'));
  // The game reads the next land: the store gives its download to the same map
  await first.store.obtain(numberOf(index.files, 'Data/Landscape/Land2.lnd'));
  assert.equal(state(first.model, 'Data/Landscape/Land2.lnd'), 'present');
  const second = await visit();
  assert.deepEqual([...new Set(second.events)], ['present']);
  assert.equal(state(second.model, 'Data/Landscape/Land1.lnd'), 'present');
  assert.deepEqual(second.model.partsOf(second.model.map.groups.find((group) => group.id === 'music').items[0])
    .map((part) => part.state), ['present', 'lazy', 'lazy', 'lazy', 'lazy', 'lazy', 'lazy']);
});

check('a list with hashed addresses gives each file at its address by SHA-256', () => {
  const files = { 'Data/AllMeshes.g3d': 'meshes', 'Online Maps/mpm_2p_1.map': 'two gods' };
  const hashed = parseAssetIndex(indexText(files, { format: 2, hashed_addresses: true })).files;
  assert.deepEqual(hashed.map((file) => [file.address, file.hashed]), [
    [`/assets/sha256/${sha('meshes')}`, true], [`/assets/sha256/${sha('two gods')}`, true]]);
  const byPath = parseAssetIndex(indexText(files)).files;
  assert.deepEqual(byPath.map((file) => [file.address, file.hashed]), [
    ['/assets/files/Data/AllMeshes.g3d', false], ['/assets/files/Online%20Maps/mpm_2p_1.map', false]]);
  assert.equal(parseAssetIndex(indexText(files, { hashed_addresses: false })).files[0].hashed, false);
  assert.throws(() => parseAssetIndex(indexText(files, { hashed_addresses: 'yes' })), /hashed_addresses/);
});

check('no file goes in the HTTP cache of the browser, because OPFS keeps the only copy', () => {
  assert.deepEqual(assetRequest({ path: 'Data/a b.txt', address: '/assets/sha256/x', hashed: true }),
    { address: '/assets/sha256/x', cache: 'no-store' });
  assert.deepEqual(assetRequest({ path: 'Data/a b.txt', address: '/assets/files/Data/a%20b.txt', hashed: false }),
    { address: '/assets/files/Data/a%20b.txt', cache: 'no-store' });
  // A file that a test makes with no address has its address by path
  assert.deepEqual(assetRequest({ path: 'Data/a b.txt' }),
    { address: '/assets/files/Data/a%20b.txt', cache: 'no-store' });
});

check('with hashed addresses the page gets the files and the blocks at their addresses by SHA-256', async () => {
  const root = new FakeFolder();
  const text = streamedText(k_Streamed, ['Audio/Music/align/Good.sad', 'Data/INTRO.bik'], { hashed_addresses: true });
  const site = fakeSite(k_Streamed, { index: text });
  const { index } = await fetchAssetIndex(site.fetchFile);
  const { store } = await streamAssets(index, { fetchFile: site.fetchFile, digest, root });
  await store.drained();
  const music = hashedAssetAddress(sha(k_Streamed['Audio/Music/align/Good.sad']));
  const intro = hashedAssetAddress(sha('the intro film'));
  assert.deepEqual([...site.requests.keys()].sort(), [k_AssetIndexAddress,
    hashedAssetAddress(sha('meshes')), `${intro} bytes=0-3`, `${intro} bytes=4-7`, `${intro} bytes=8-11`,
    `${intro} bytes=12-13`, hashedAssetAddress(sha('land one')), `${music} bytes=0-3`, hashedAssetAddress(sha('heal'))]
    .sort());
  await store.obtain(numberOf(index.files, 'Data/Landscape/Land2.lnd'));
  assert.equal(site.requests.get(hashedAssetAddress(sha('land two'))), 1);
  for (const [key, cache] of site.caches) {
    assert.equal(cache, key === k_AssetIndexAddress ? 'no-cache' : 'no-store', key);
  }
});

check('the page asks the browser to keep the storage, but not when the browser keeps it already', async () => {
  const fakeStorage = (persisted, answer) => {
    const storage = { asked: 0, persisted: async () => persisted };
    storage.persist = async () => {
      storage.asked += 1;
      if (answer instanceof Error) {
        throw answer;
      }
      return answer;
    };
    return storage;
  };
  const kept = fakeStorage(true, false);
  assert.equal(await askToKeepStorage(kept), true);
  assert.equal(kept.asked, 0);
  const given = fakeStorage(false, true);
  assert.equal(await askToKeepStorage(given), true);
  assert.equal(given.asked, 1);
  assert.equal(await askToKeepStorage(fakeStorage(false, false)), false);
  assert.equal(await askToKeepStorage(fakeStorage(false, new Error('SecurityError'))), null);
  assert.equal(await askToKeepStorage(undefined), null);
  assert.equal(await askToKeepStorage({}), null);
});

check('with no tiers file the voice banks of the species come after the start, after the miracles', () => {
  const files = parseAssetIndex(indexText({
    'Audio/SFX/Creature/btiger.sad': 'tiger',
    'Audio/SFX/Creature/creature.sad': 'every creature',
    'Data/Spells/Anims/heal.anm': 'heal',
    'Audio/SFX/Creature/BCOW.sad': 'cow',
    'Audio/SFX/Game/InGame.sad': 'hand',
  })).files;
  assert.equal(isStartAsset('Audio/SFX/Creature/btiger.sad'), false);
  assert.equal(isStartAsset('AUDIO/SFX/CREATURE/BGREEK.SAD'), false);
  assert.equal(isStartAsset('Audio/SFX/Creature/creature.sad'), true);
  assert.equal(isStartAsset('Audio/SFX/Game/InGame.sad'), true);
  assert.equal(isStartAsset('Audio/Dialogue/HelpSprites.sad'), true);
  const phases = planAssetTiers(files);
  const paths = (items) => items.map(({ number }) => files[number].path);
  assert.deepEqual(paths(phases[0]), ['Audio/SFX/Creature/creature.sad', 'Audio/SFX/Game/InGame.sad']);
  assert.deepEqual(paths(phases[4]),
    ['Data/Spells/Anims/heal.anm', 'Audio/SFX/Creature/btiger.sad', 'Audio/SFX/Creature/BCOW.sad']);
});

check('a second visit gets nothing again, and all the files come with the option all', async () => {
  const first = await streamedSite();
  const again = fakeSite(k_Streamed, { index: streamedText() });
  const { index } = await fetchAssetIndex(again.fetchFile);
  await streamAssets(index, { fetchFile: again.fetchFile, digest, root: first.root });
  assert.deepEqual([...again.requests.keys()], [k_AssetIndexAddress]);
  const all = await keepAssets(index, { fetchFile: again.fetchFile, digest, root: first.root });
  assert.equal(all.every((entry) => entry.file !== null), true);
  assert.equal(again.requests.has(assetAddress('Online Maps/mpm_2p_1.map')), true);
});

check('a file that the game reads comes from the site once and goes to every game thread', async () => {
  const { store, site, shared, index } = await streamedSite();
  const number = numberOf(index.files, 'Data/Landscape/Land2.lnd');
  await Promise.all([store.obtain(number), store.obtain(number)]);
  assert.equal(site.requests.get(assetAddress('Data/Landscape/Land2.lnd')), 1);
  assert.deepEqual(shared.map((given) => [given.number, given.block]), [[number, null]]);
  assert.equal(Buffer.from(store.fileOf(number).bytes).toString(), 'land two');
  await store.obtain(number);
  assert.equal(site.requests.get(assetAddress('Data/Landscape/Land2.lnd')), 1);
});

check('a file that came for later before the game could take it goes to the game thread when the game reads it', async () => {
  const { store, shared, index } = await streamedSite();
  const number = numberOf(index.files, 'Data/Spells/Anims/heal.anm');
  // The page gets the file for later while the game cannot take files yet: the share then gives it to no thread
  store.prefetch([number]);
  await Promise.all([...store.jobs.values()].map((job) => job.promise));
  shared.length = 0;
  // The game reads it: the page has it, and must give it again, or the game thread waits for ever
  await store.obtain(number);
  assert.deepEqual(shared.map((given) => [given.number, given.block]), [[number, null]]);
});

check('a block that the page has goes to the game thread again when the game reads it', async () => {
  const { store, shared, index } = await streamedSite();
  const number = numberOf(index.files, 'Audio/Music/align/Good.sad');
  await store.obtain(number, 4, 4);
  await Promise.all([...store.jobs.values()].map((job) => job.promise));
  shared.length = 0;
  await store.obtain(number, 4, 4);
  assert.deepEqual(shared.map((given) => [given.number, given.block]), [[number, 1]]);
});

check('a read of a music bank gets its blocks and the next blocks, and the whole bank is kept when complete', async () => {
  const { store, site, shared, index } = await streamedSite();
  const number = numberOf(index.files, 'Audio/Music/align/Good.sad');
  const address = assetAddress('Audio/Music/align/Good.sad');
  // Bytes 5 to 9 are in the blocks 1 and 2. The page then gets the blocks 3 and 4 for later.
  await store.obtain(number, 5, 5);
  assert.equal(site.requests.has(`${address} bytes=4-7`), true);
  assert.equal(site.requests.has(`${address} bytes=8-11`), true);
  await Promise.all([...store.jobs.values()].map((job) => job.promise));
  assert.equal(site.requests.has(`${address} bytes=12-15`), true);
  assert.equal(site.requests.has(`${address} bytes=16-19`), true);
  assert.equal(site.requests.has(`${address} bytes=20-23`), false);
  assert.equal(store.fileOf(number), null);
  await store.obtain(number, 20, 100);
  await Promise.all([...store.jobs.values()].map((job) => job.promise));
  assert.equal(Buffer.from(store.fileOf(number).bytes).toString(), k_Streamed['Audio/Music/align/Good.sad']);
  assert.deepEqual(shared.filter((given) => given.block === null).map((given) => given.number), [number]);
  assert.equal(shared.filter((given) => given.block !== null).length, 6);
});

check('a block with an incorrect SHA-256 or range fails, and the page keeps nothing of it', async () => {
  const root = new FakeFolder();
  const site = fakeSite(k_Streamed, { index: streamedText(),
    change: { 'Audio/Music/align/Good.sad': 'HEADMUSIC of the good god' } });
  const { index } = await fetchAssetIndex(site.fetchFile);
  const { store } = await streamAssets(index, { fetchFile: site.fetchFile, digest, root });
  const number = numberOf(index.files, 'Audio/Music/align/Good.sad');
  await assert.rejects(store.obtain(number, 4, 4), /SHA-256/);
  assert.equal(store.blocksOf(number)[1], null);
  // The page tries each block two times
  assert.equal(site.requests.get(`${assetAddress('Audio/Music/align/Good.sad')} bytes=4-7`), 2);
  // A range of a file of another size fails at the first block. The first block comes in a phase after the start, so
  // its failure goes to the log, and the game gets the error when it reads the block.
  const longer = fakeSite(k_Streamed, { index: streamedText(),
    change: { 'Audio/Music/align/Good.sad': 'HEADmusic of the good god and more' } });
  const logged = [];
  const streamed = await streamAssets((await fetchAssetIndex(longer.fetchFile)).index,
    { fetchFile: longer.fetchFile, digest, root: new FakeFolder(), log: (line) => logged.push(line) });
  await streamed.store.drained();
  assert.ok(logged.some((line) => /Good\.sad for its phase: .*range/.test(line)), logged.join('\n'));
  await assert.rejects(streamed.store.obtain(number, 0, 4), /range/);
});

check('a file that the game reads goes before the files for later, and later files come one at a time', async () => {
  const { store, index } = await streamedSite();
  const order = [];
  const gate = {};
  const opened = new Promise((resolve) => {
    gate.open = resolve;
  });
  const run = (name) => async () => {
    order.push(`start ${name}`);
    await opened;
    order.push(`end ${name}`);
  };
  const later = [store.schedule('a', run('a'), 'later'), store.schedule('b', run('b'), 'later')];
  const now = store.schedule('c', run('c'), 'now');
  // The later file a started before the game asked for c; b waits until nothing of the game waits
  assert.deepEqual(order, ['start a', 'start c']);
  gate.open();
  await Promise.all([...later, now]);
  assert.deepEqual(order.slice(2), ['end a', 'end c', 'start b', 'end b']);
  // A later file that the game then reads moves to the front
  const number = numberOf(index.files, 'Data/Spells/Anims/heal.anm');
  store.prefetch([number]);
  await store.obtain(number);
  assert.equal(Buffer.from(store.fileOf(number).bytes).toString(), 'heal');
});

// A store with no files, and jobs that end only when the test opens their gate
const gatedStore = () => {
  const store = new AssetStore([], { fetchFile: null, digest, folder: new FakeFolder() });
  const order = [];
  const gates = new Map();
  const run = (name) => () => new Promise((resolve) => {
    order.push(name);
    gates.set(name, resolve);
  });
  const open = async (name) => {
    gates.get(name)();
    gates.delete(name);
    await new Promise((resolve) => setImmediate(resolve));
  };
  return { store, order, run, open, gates };
};

check('the phases get 16 small files and 4 other files at the same time, in their order', async () => {
  const { store, order, run, open, gates } = gatedStore();
  const big = Array.from({ length: k_AssetPhaseDownloads + 2 }, (_, at) => `big ${at}`);
  const small = Array.from({ length: k_AssetSmallDownloads + 2 }, (_, at) => `small ${at}`);
  const jobs = [
    ...big.map((name) => store.schedule(name, run(name), 'phase', { size: k_AssetSmallFile })),
    ...small.map((name) => store.schedule(name, run(name), 'phase', { size: k_AssetSmallFile - 1 })),
  ];
  assert.deepEqual(order, [...big.slice(0, k_AssetPhaseDownloads), ...small.slice(0, k_AssetSmallDownloads)]);
  await open('big 0');
  await open('small 0');
  assert.deepEqual(order.slice(-2), [`big ${k_AssetPhaseDownloads}`, `small ${k_AssetSmallDownloads}`]);
  while (gates.size > 0) {
    await open(gates.keys().next().value);
  }
  await Promise.all(jobs);
  assert.deepEqual(order.sort(), [...big, ...small].sort());
});

check('while the game waits for a file, only small files of the phases start, and later files wait for both', async () => {
  const { store, order, run, open, gates } = gatedStore();
  const now = store.schedule('game', run('game'), 'now');
  store.schedule('big', run('big'), 'phase', { size: k_AssetSmallFile });
  store.schedule('small', run('small'), 'phase', { size: 10 });
  store.schedule('later', run('later'), 'later');
  assert.deepEqual(order, ['game', 'small']);
  await open('game');
  await now;
  assert.deepEqual(order, ['game', 'small', 'big']);
  await open('small');
  await open('big');
  assert.deepEqual(order, ['game', 'small', 'big', 'later']);
  await open('later');
  assert.equal(gates.size, 0);
  assert.equal(store.waitsNow(), false);
});

check('a file of a phase that the game reads goes to the queue now, and a block to read ahead goes first', async () => {
  const { store, order, run, open } = gatedStore();
  for (let at = 0; at < k_AssetPhaseDownloads; at += 1) {
    store.schedule(`busy ${at}`, run(`busy ${at}`), 'phase', { size: k_AssetSmallFile });
  }
  store.schedule('next', run('next'), 'phase', { size: k_AssetSmallFile });
  const wanted = store.schedule('wanted', run('wanted'), 'phase', { size: k_AssetSmallFile });
  store.schedule('ahead', run('ahead'), 'phase', { size: k_AssetSmallFile, front: true });
  store.schedule('soon', run('soon'), 'later', { size: 10 });
  // The game reads a file that waits in the phases, and a file that waits for later moves up to the phases
  store.schedule('wanted', run('wanted'), 'now');
  store.schedule('soon', run('soon'), 'phase', { size: 10 });
  assert.equal(order.at(-1), 'soon');
  assert.ok(order.includes('wanted'));
  await open('wanted');
  await wanted;
  await open('busy 0');
  assert.equal(order.at(-1), 'ahead');
  await open('busy 1');
  assert.equal(order.at(-1), 'next');
});

// A list of the site with a file of each phase of a new game to Land 1
const k_Phased = {
  'Scripts/InfoScript2.txt': 'the scripts of the front end',
  'Data/logo.bik': 'the logo film',
  'Data/pre_intro.bik': 'the pre-intro film',
  'Data/AllMeshes.g3d': 'meshes',
  'Data/Citadel/engine/mainlo_l3d.zzz': 'a low-detail mesh',
  'Data/Landscape/Land1.lnd': 'land one',
  'Data/Spells/Anims/heal.anm': 'heal',
  'Data/Landscape/Land2.lnd': 'land two',
};

check('the game starts with the boot files, and the page then gets the phases in order but no later file', async () => {
  const { AssetMapModel, buildAssetMap } = require('../web/page/launcher-map.js');
  const root = new FakeFolder();
  const site = fakeSite(k_Phased, { index: streamedText(k_Phased, []) });
  const { index } = await fetchAssetIndex(site.fetchFile);
  const model = new AssetMapModel(buildAssetMap(index.files, { blockSize: index.blockSize }));
  const asked = [];
  const fetchFile = (address, options) => {
    asked.push(address);
    return site.fetchFile(address, options);
  };
  const progress = [];
  const queued = new Map();
  const { entries, store } = await streamAssets(index, {
    fetchFile, digest, root, progress: (state) => progress.push(state),
    watch: (event, key, detail) => {
      if (event === 'queued') {
        queued.set(key, detail);
      }
      model.handle(event, key, detail);
    },
  });
  const entry = (path) => entries[numberOf(index.files, path)];
  assert.notEqual(entry('Data/logo.bik').file, null);
  assert.notEqual(entry('Scripts/InfoScript2.txt').file, null);
  assert.equal(entry('Data/pre_intro.bik').file, null);
  assert.equal(progress.at(-1).totalFiles, 2);
  const item = (path) => model.map.groups.flatMap((group) => group.items).find((one) => one.path === path);
  const key = (path) => `file ${sha(k_Phased[path])}`;
  assert.equal(queued.get(key('Data/Landscape/Land1.lnd')), 'phase');
  assert.equal(queued.get(key('Data/Spells/Anims/heal.anm')), 'later');
  assert.equal(queued.has(key('Data/Landscape/Land2.lnd')), false);
  assert.equal(model.stateOf(item('Data/Spells/Anims/heal.anm')), 'prefetch');
  assert.equal(model.stateOf(item('Data/Landscape/Land2.lnd')), 'lazy');
  await store.drained();
  assert.deepEqual(asked.slice(2), ['Data/pre_intro.bik', 'Data/AllMeshes.g3d', 'Data/Landscape/Land1.lnd',
    'Data/Spells/Anims/heal.anm'].map(assetAddress));
  assert.equal(store.fileOf(numberOf(index.files, 'Data/Citadel/engine/mainlo_l3d.zzz')), null);
  assert.equal(model.stateOf(item('Data/Landscape/Land1.lnd')), 'present');
  assert.equal(model.totals().remaining, 0);
});

check('a film comes in its blocks in its phase, the game reads its first block first, and the page joins it', async () => {
  const text = { 'Data/pre_intro.bik': 'the pre-intro film, in seven blocks' };
  const root = new FakeFolder();
  const site = fakeSite(text, { index: streamedText(text, Object.keys(text)) });
  const { index } = await fetchAssetIndex(site.fetchFile);
  const { store } = await streamAssets(index, { fetchFile: site.fetchFile, digest, root });
  // The game reads the header of the film: it waits only for the first block, and the film plays while the others come
  const read = store.obtain(0, 0, 4);
  assert.equal(store.waitsNow(), true);
  await read;
  // The other blocks can arrive first: the page then has joined the film, and keeps it whole in place of the block
  assert.ok(store.blocksOf(0)[0] !== null || store.fileOf(0) !== null);
  await store.drained();
  assert.equal(store.waitsNow(), false);
  const film = assetAddress('Data/pre_intro.bik');
  const ranges = index.files[0].blocks.map((_, block) => {
    const start = block * k_BlockSize;
    return `${film} bytes=${start}-${Math.min(index.files[0].size, start + k_BlockSize) - 1}`;
  });
  assert.deepEqual([...site.requests.keys()].sort(), [k_AssetIndexAddress, ...ranges].sort());
  assert.ok([...site.requests.values()].every((count) => count === 1));
  // With all its blocks, the page keeps the film whole
  assert.equal(Buffer.from(store.fileOf(0).bytes).toString(), text['Data/pre_intro.bik']);
});

check('a caller can start the phases itself, for example after the program of the game arrived', async () => {
  const root = new FakeFolder();
  const site = fakeSite(k_Phased, { index: streamedText(k_Phased, []) });
  const { index } = await fetchAssetIndex(site.fetchFile);
  const { store, phases } = await streamAssets(index, { fetchFile: site.fetchFile, digest, root, startPhases: false });
  assert.equal(store.jobs.size, 0);
  assert.equal(site.requests.has(assetAddress('Data/pre_intro.bik')), false);
  store.queuePhases(phases);
  await store.drained();
  assert.equal(site.requests.get(assetAddress('Data/pre_intro.bik')), 1);
  const all = await streamAssets(index, { fetchFile: site.fetchFile, digest, root: new FakeFolder(), all: true });
  assert.equal(all.phases, null);
});

check('the page removes the blocks of a bank when it keeps the whole bank, and gets no block again', async () => {
  const { store, site, root, index, shared } = await streamedSite();
  const number = numberOf(index.files, 'Audio/Music/align/Good.sad');
  const file = index.files[number];
  await store.obtain(number, 0, 100);
  await store.drained();
  const folder = await root.getDirectoryHandle(k_AssetCacheFolder);
  assert.equal(folder.children.has(file.sha256), true);
  assert.deepEqual(file.blocks.filter((block) => folder.children.has(block)), []);
  assert.deepEqual(store.blocksOf(number), file.blocks.map(() => null));
  // A read of the bank after the join gets the whole bank, with no request for a block
  const before = site.requests.size;
  shared.length = 0;
  await store.obtain(number, 8, 4);
  store.background([{ number, block: 3 }]);
  await store.drained();
  assert.equal(site.requests.size, before);
  assert.deepEqual(shared.map((given) => [given.number, given.block]), [[number, null]]);
});

check('a bank that comes whole after its first block removes the block', async () => {
  const { store, root, index } = await streamedSite();
  const number = numberOf(index.files, 'Audio/Music/align/Good.sad');
  const file = index.files[number];
  const folder = await root.getDirectoryHandle(k_AssetCacheFolder);
  // The phases got the first block of the bank
  assert.equal(folder.children.has(file.blocks[0]), true);
  await store.getWholeJob(number, 'now');
  assert.equal(folder.children.has(file.sha256), true);
  assert.deepEqual(file.blocks.filter((block) => folder.children.has(block)), []);
});

check('the page never removes a bank of one block, because its block is the whole bank', async () => {
  const files = { 'Audio/Music/small.sad': 'HEAD', 'Audio/Music/align/Good.sad': 'HEADmusic of the good god' };
  const site = fakeSite(files, { index: streamedText(files, Object.keys(files)) });
  const root = new FakeFolder();
  const { index } = await fetchAssetIndex(site.fetchFile);
  const { store } = await streamAssets(index, { fetchFile: site.fetchFile, digest, root });
  await store.drained();
  const small = numberOf(index.files, 'Audio/Music/small.sad');
  const good = numberOf(index.files, 'Audio/Music/align/Good.sad');
  await store.obtain(small, 0, 4);
  await store.getWholeJob(good, 'now');
  await store.obtain(good, 0, 100);
  await store.drained();
  const folder = await root.getDirectoryHandle(k_AssetCacheFolder);
  assert.equal(folder.children.has(sha('HEAD')), true);
  assert.notEqual(store.fileOf(small), null);
});

check('a block that a bank that is not whole also has stays in the folder', async () => {
  const files = { 'Audio/Music/one.sad': 'HEADfirst bank', 'Audio/Music/two.sad': 'HEADother bank' };
  const site = fakeSite(files, { index: streamedText(files, Object.keys(files)) });
  const root = new FakeFolder();
  const { index } = await fetchAssetIndex(site.fetchFile);
  const { store } = await streamAssets(index, { fetchFile: site.fetchFile, digest, root });
  await store.drained();
  const one = numberOf(index.files, 'Audio/Music/one.sad');
  await store.obtain(one, 0, 100);
  await store.drained();
  const folder = await root.getDirectoryHandle(k_AssetCacheFolder);
  const head = sha('HEAD');
  assert.equal(store.fileOf(one) !== null, true);
  assert.equal(folder.children.has(head), true);
  assert.equal(folder.children.has(index.files[one].blocks[1]), false);
});

check('the background plan gets the music in blocks, then the lands, the maps, the creatures and the rest', () => {
  const block = (name) => sha(name);
  const files = [
    { path: 'Data/Misc.txt', sha256: sha('misc'), size: 4 },
    { path: 'Data/CTR/cow.ctr', sha256: sha('cow'), size: 3 },
    { path: 'Online Maps/mpm_2p_1.map', sha256: sha('map'), size: 3 },
    { path: 'Data/Landscape/Land3.lnd', sha256: sha('land3'), size: 5 },
    { path: 'Audio/Music/b.sad', sha256: sha('bank'), size: 8, blocks: [block('b0'), block('b1')] },
    { path: 'Audio/Music/same.sad', sha256: sha('bank'), size: 8, blocks: [block('b0'), block('b1')] },
    { path: 'Data/Landscape/Land2.lnd', sha256: sha('land2'), size: 5 },
  ];
  const kept = new Set([sha('land2'), block('b0')]);
  const plan = planAssetBackground(files, { have: (sha256) => kept.has(sha256) });
  assert.deepEqual(plan, [
    { number: 4, block: 1 }, { number: 3, block: null }, { number: 2, block: null }, { number: 1, block: null },
    { number: 0, block: null },
  ]);
  assert.equal(assetItemBytes(files, plan, 4), 4 + 5 + 3 + 3 + 4);
});

check('the page gets files in the background only when the player and the browser allow it', () => {
  const ok = { enabled: true, kept: true, free: 1000, rest: 500 };
  assert.equal(assetBackgroundStop(ok), null);
  assert.match(assetBackgroundStop({ ...ok, enabled: false }), /turned it off/);
  assert.match(assetBackgroundStop({ ...ok, kept: false }), /does not keep/);
  assert.match(assetBackgroundStop({ ...ok, kept: null }), /does not keep/);
  assert.match(assetBackgroundStop({ ...ok, saveData: true }), /less data/);
  assert.match(assetBackgroundStop({ ...ok, full: true }), /no more space/);
  assert.match(assetBackgroundStop({ ...ok, free: 999 }), /free space/);
  assert.match(assetBackgroundStop({ ...ok, hidden: true }), /hidden/);
  assert.equal(assetFreeSpace({ quota: 100, usage: 30 }), 70);
  assert.equal(assetFreeSpace({ quota: 10, usage: 30 }), 0);
  assert.equal(assetFreeSpace(null), Infinity);
  assert.equal(assetFreeSpace({ usage: 30 }), Infinity);
});

check('a background job starts one at a time, only when nothing else waits and the background is allowed', async () => {
  const { store, order, run, open } = gatedStore();
  let allowed = false;
  store.backgroundAllowed = () => allowed;
  const jobs = [store.schedule('x', run('x'), 'background'), store.schedule('y', run('y'), 'background')];
  assert.deepEqual(order, []);
  allowed = true;
  store.pump();
  assert.deepEqual(order, ['x']);
  // A file for later waits for the background job that runs, and then goes before the next background job
  const later = store.schedule('l', run('l'), 'later');
  await open('x');
  assert.deepEqual(order, ['x', 'l']);
  await open('l');
  assert.deepEqual(order, ['x', 'l', 'y']);
  await open('y');
  await Promise.all([...jobs, later]);
  // The game can ask for a background job that waits: it goes to the queue now at once
  allowed = false;
  const waiting = store.schedule('z', run('z'), 'background');
  assert.deepEqual(order, ['x', 'l', 'y']);
  store.schedule('z', run('z'), 'now');
  assert.deepEqual(order, ['x', 'l', 'y', 'z']);
  await open('z');
  await waiting;
});

// A folder of the private storage that has space for a number of bytes only
class SmallFolder extends FakeFolder {
  constructor(space) {
    super();
    this.space = space;
  }
  async getDirectoryHandle(name, options = {}) {
    if (!this.children.has(name) && options.create) {
      this.children.set(name, new SmallFolder(this.space));
    }
    return super.getDirectoryHandle(name, options);
  }
  async getFileHandle(name, { create = false } = {}) {
    if (!this.children.has(name) && create) {
      const folder = this;
      const file = new FakeFile();
      const write = file.createWritable.bind(file);
      file.createWritable = async () => {
        const writable = await write();
        return {
          ...writable,
          write: async (bytes) => {
            const used = [...folder.children.values()].reduce((sum, child) => sum + (child.bytes?.length ?? 0), 0);
            if (used + bytes.byteLength > folder.space) {
              throw Object.assign(new Error('no space'), { name: 'QuotaExceededError' });
            }
            return writable.write(bytes);
          },
          abort: async () => {},
        };
      };
      this.children.set(name, file);
    }
    return super.getFileHandle(name, { create });
  }
}

check('with no more space the store keeps the next files in memory, tells it one time and joins no blocks', async () => {
  const files = { 'Data/AllMeshes.g3d': 'meshes', 'Data/Landscape/Land2.lnd': 'land two',
    'Data/Landscape/Land3.lnd': 'land three', 'Audio/Music/align/Good.sad': 'HEADmusic of the good god' };
  const site = fakeSite(files, { index: streamedText(files, ['Audio/Music/align/Good.sad']) });
  const root = new SmallFolder(12);
  const { index } = await fetchAssetIndex(site.fetchFile);
  const logged = [];
  let full = 0;
  const shared = [];
  const { store } = await streamAssets(index, {
    fetchFile: site.fetchFile, digest, root, log: (line) => logged.push(line), onFull: () => { full += 1; },
    share: (number, block, file) => shared.push({ number, block, file }),
  });
  await store.drained();
  const land2 = numberOf(index.files, 'Data/Landscape/Land2.lnd');
  const land3 = numberOf(index.files, 'Data/Landscape/Land3.lnd');
  await store.obtain(land2);
  await store.obtain(land3);
  assert.equal(store.full, true);
  assert.equal(full, 1);
  assert.equal(logged.filter((line) => /no more space/.test(line)).length, 1);
  assert.equal(Buffer.from(await store.fileOf(land3).arrayBuffer()).toString(), 'land three');
  const folder = await root.getDirectoryHandle(k_AssetCacheFolder);
  assert.equal(folder.children.has(sha('land three')), false);
  // The blocks of the music come from memory, and the page does not join them
  const music = numberOf(index.files, 'Audio/Music/align/Good.sad');
  await store.obtain(music, 0, 100);
  await store.drained();
  assert.equal(store.fileOf(music), null);
  assert.equal(store.blocksOf(music).every((block) => block !== null), true);
});

check('the page checks the free space before it downloads, and says why when the start has no space', async () => {
  // The boot phase of the test tiers has the script, and Land 2 is a later file
  const files = { 'Scripts/InfoScript2.txt': 'boot script', 'Data/Landscape/Land2.lnd': 'land two' };
  const site = fakeSite(files);
  const { index } = await fetchAssetIndex(site.fetchFile);
  // All the files need 19 bytes. With 15 bytes free the page gets only the start (11 bytes).
  const logged = [];
  const some = await streamAssets(index, { fetchFile: site.fetchFile, digest, root: new FakeFolder(), all: true,
    estimate: async () => ({ quota: 115, usage: 100 }), log: (line) => logged.push(line) });
  assert.equal(some.all, false);
  assert.deepEqual(some.space, { free: 15, rest: 19 });
  assert.equal(some.entries[numberOf(index.files, 'Data/Landscape/Land2.lnd')].file, null);
  assert.ok(logged.some((line) => /free space/.test(line)), logged.join('\n'));
  // With enough space the option all gets all the files
  const every = await streamAssets(index, { fetchFile: site.fetchFile, digest, root: new FakeFolder(), all: true,
    estimate: async () => ({ quota: 1000, usage: 0 }) });
  assert.equal(every.all, true);
  // With less space than the start, the page stops with the reason
  await assert.rejects(streamAssets(index, { fetchFile: site.fetchFile, digest, root: new FakeFolder(),
    estimate: async () => ({ quota: 5, usage: 0 }) }), /free space for the game files/);
  // A write before the start that finds no space stops with the reason
  await assert.rejects(streamAssets(index, { fetchFile: site.fetchFile, digest, root: new SmallFolder(3) }),
    /no more space for the game files/);
});

// The list of the third version with packs of the files at the paths of each group, and the body of each pack by its
// address
const packedIndex = (files, groups) => {
  const packs = groups.map((paths) => {
    let offset = 0;
    const members = paths.map((path) => {
      const size = Buffer.byteLength(files[path]);
      const member = [sha(files[path]), offset, size];
      offset += size;
      return member;
    });
    const body = paths.map((path) => files[path]).join('');
    return { sha256: sha(body), size: offset, members, body };
  });
  const text = indexText(files, { format: 3, hashed_addresses: true, packs: packs.map(({ body, ...pack }) => pack) });
  return { text, bodies: new Map(packs.map((pack) => [hashedAssetAddress(pack.sha256), pack.body])) };
};

// A site with the packs of packedIndex. With broken, the last byte of each pack is not correct.
const packedSite = (files, groups, { broken = false } = {}) => {
  const { text, bodies } = packedIndex(files, groups);
  const site = fakeSite(files, { index: text });
  const fetchFile = async (address, options = {}) => {
    if (!bodies.has(address)) {
      return site.fetchFile(address, options);
    }
    site.requests.set(address, (site.requests.get(address) ?? 0) + 1);
    const body = bodies.get(address);
    return fakeResponse(200, broken ? `${body.slice(0, -1)}?` : body);
  };
  return { ...site, fetchFile, index: parseAssetIndex(text) };
};

const k_Packed = {
  'Scripts/a.txt': 'alpha',
  'Scripts/b.txt': 'bravo',
  'Data/c.raw': 'charlie',
  'Data/big.raw': 'x'.repeat(40),
};
const k_PackGroups = [['Scripts/a.txt', 'Scripts/b.txt', 'Data/c.raw']];

check('the page reads the packs of the third version of the list', () => {
  const { index } = packedSite(k_Packed, k_PackGroups);
  assert.equal(index.packs.length, 1);
  const [pack] = index.packs;
  assert.equal(pack.size, 17);
  assert.equal(pack.address, hashedAssetAddress(pack.sha256));
  assert.deepEqual(pack.members, [
    { sha256: sha('alpha'), offset: 0, size: 5, path: 'Scripts/a.txt' },
    { sha256: sha('bravo'), offset: 5, size: 5, path: 'Scripts/b.txt' },
    { sha256: sha('charlie'), offset: 10, size: 7, path: 'Data/c.raw' },
  ]);
  // A list of the second version has no packs
  assert.deepEqual(parseAssetIndex(indexText(k_Packed, { format: 2 })).packs, []);
});

check('the page refuses packs that are not correct', () => {
  const { text } = packedIndex(k_Packed, k_PackGroups);
  const good = JSON.parse(text);
  const changed = (change) => {
    const index = structuredClone(good);
    change(index, index.packs[0]);
    return JSON.stringify(index);
  };
  const bad = [
    changed((index) => delete index.hashed_addresses),
    changed((index) => {
      index.packs = {};
    }),
    changed((index, pack) => {
      pack.sha256 = 'X'.repeat(64);
    }),
    changed((index, pack) => {
      pack.members = pack.members.slice(0, 1);
      pack.size = 5;
    }),
    changed((index, pack) => {
      pack.members[1][1] = 6;
    }),
    changed((index, pack) => {
      pack.members[2][2] = 8;
    }),
    changed((index, pack) => {
      pack.size = 18;
    }),
    changed((index, pack) => {
      pack.members[0][0] = 'c'.repeat(64);
    }),
    changed((index, pack) => {
      pack.members.push([sha('alpha'), 17, 5]);
      pack.size = 22;
    }),
    changed((index, pack) => index.packs.push(structuredClone(pack))),
    changed((index, pack) => {
      pack.members[0] = { sha256: sha('alpha') };
    }),
  ];
  for (const text of bad) {
    assert.throws(() => parseAssetIndex(text), Error, text);
  }
});

check('the page gets a pack when the files that it wants have half of the bytes of the pack or more', () => {
  const pack = { size: 20, members: [{ sha256: 'a', size: 10 }, { sha256: 'b', size: 6 }, { sha256: 'c', size: 4 }] };
  assert.equal(k_AssetPackShare, 0.5);
  assert.equal(assetPackWorth(pack, (sha256) => sha256 === 'a'), true);
  assert.equal(assetPackWorth(pack, (sha256) => sha256 !== 'a'), true);
  assert.equal(assetPackWorth(pack, (sha256) => sha256 === 'b'), false);
  assert.equal(assetPackWorth(pack, () => false), false);
  assert.equal(assetPackWorth({ size: 0, members: [] }, () => true), false);
});

check('the map sees the bytes of a pack as the bytes of its files, in their order', () => {
  const pack = { members: [{ sha256: 'a', offset: 0, size: 5 }, { sha256: 'b', offset: 5, size: 2 }] };
  const events = [];
  const watch = assetPackWatch(pack, (...event) => events.push(event), (sha256) => sha256 === 'b');
  watch.event('started');
  watch.bytes(3);
  watch.bytes(3);
  watch.bytes(1);
  assert.deepEqual(events, [
    ['started', 'file b', undefined], ['bytes', 'file a', 3], ['bytes', 'file a', 2], ['bytes', 'file b', 1],
    ['bytes', 'file b', 1],
  ]);
});

check('before the start the page gets the small files of a pack in one request and keeps each file', async () => {
  const site = packedSite(k_Packed, k_PackGroups);
  const root = new FakeFolder();
  const progress = [];
  await streamWithTiers(site.index, {
    fetchFile: site.fetchFile, digest, root, all: true, progress: (state) => progress.push(state),
  });
  const [pack] = site.index.packs;
  assert.equal(site.requests.get(pack.address), 1);
  for (const path of k_PackGroups[0]) {
    assert.equal(site.requests.get(hashedAssetAddress(sha(k_Packed[path]))), undefined);
  }
  assert.equal(site.requests.get(hashedAssetAddress(sha(k_Packed['Data/big.raw']))), 1);
  const folder = await root.getDirectoryHandle(k_AssetCacheFolder);
  for (const [path, text] of Object.entries(k_Packed)) {
    assert.equal(Buffer.from((await folder.getFileHandle(sha(text))).bytes).toString(), text, path);
  }
  assert.deepEqual(progress.at(-1), { bytes: 57, totalBytes: 57, files: 4, totalFiles: 4 });
  // A second visit that lost one small file gets it alone: it has less than half of the bytes of the pack
  await folder.removeEntry(sha('charlie'));
  site.requests.clear();
  await streamWithTiers(site.index, { fetchFile: site.fetchFile, digest, root, all: true });
  assert.deepEqual([...site.requests.keys()].sort(), [hashedAssetAddress(sha('charlie'))]);
});

check('before the start a pack that fails brings its files alone', async () => {
  const site = packedSite(k_Packed, k_PackGroups, { broken: true });
  const root = new FakeFolder();
  const logged = [];
  const result = await streamWithTiers(site.index, {
    fetchFile: site.fetchFile, digest, root, all: true, log: (line) => logged.push(line),
  });
  assert.equal(site.requests.get(site.index.packs[0].address), 2);
  for (const path of Object.keys(k_Packed)) {
    assert.equal(site.requests.get(hashedAssetAddress(sha(k_Packed[path]))), 1, path);
  }
  assert.ok(logged.some((line) => /cannot get a pack/.test(line)));
  assert.equal(result.all, true);
});

// The store of the files of a packed site, with the numbers of the files that it gave to the game threads
const packedStore = (options = {}) => {
  const site = packedSite(k_Packed, k_PackGroups, options);
  const shared = [];
  const logged = [];
  const events = [];
  const store = new AssetStore(site.index.files, {
    fetchFile: site.fetchFile, digest, folder: new FakeFolder(), packs: site.index.packs,
    share: (number, block) => shared.push({ number, block }), log: (line) => logged.push(line),
    watch: (...event) => events.push(event),
  });
  const number = (path) => numberOf(site.index.files, path);
  return { site, store, shared, logged, events, number };
};

check('after the start the store gets the small files for later in their pack', async () => {
  const { site, store, shared, events, number } = packedStore();
  store.prefetch(site.index.files.map((_, at) => at));
  await store.drained();
  const [pack] = site.index.packs;
  assert.equal(site.requests.get(pack.address), 1);
  assert.equal(site.requests.size, 2);
  for (const path of Object.keys(k_Packed)) {
    assert.equal(Buffer.from(store.fileOf(number(path)).bytes).toString(), k_Packed[path]);
  }
  assert.deepEqual(shared.map((share) => share.number).sort(), [0, 1, 2, 3]);
  // The map sees each file of the pack wait, start, fill and end
  for (const member of pack.members) {
    const key = `file ${member.sha256}`;
    const own = events.filter((event) => event[1] === key);
    assert.deepEqual(own.filter((event) => event[0] !== 'bytes').map((event) => event[0]),
      ['queued', 'started', 'done'], key);
    assert.equal(own.filter((event) => event[0] === 'bytes').reduce((sum, event) => sum + event[2], 0), member.size);
  }
  assert.equal(events.some((event) => event[1].startsWith('pack ')), false);
  assert.equal(store.received.files, 4);
});

check('the store gets a small file alone for the game, and the pack only when it is still worth it', async () => {
  const { site, store, number } = packedStore();
  // The game waits for one small file and no pack job brings it: the file comes alone
  await store.obtain(number('Scripts/a.txt'));
  assert.deepEqual([...site.requests.keys()], [hashedAssetAddress(sha('alpha'))]);
  // The other two files have 12 of the 17 bytes of the pack: the pack brings them
  store.prefetch([number('Scripts/b.txt'), number('Data/c.raw')]);
  await store.drained();
  assert.equal(site.requests.get(site.index.packs[0].address), 1);
  assert.equal(site.requests.size, 2);
  assert.equal(Buffer.from(store.fileOf(number('Data/c.raw')).bytes).toString(), 'charlie');
});

check('a file that the game reads while its pack waits in a queue comes with the pack', async () => {
  const { site, store, number } = packedStore();
  store.queuePhases([[], [{ number: number('Scripts/a.txt'), block: null }], [], [], []]);
  await store.obtain(number('Data/c.raw'));
  await store.drained();
  assert.equal(site.requests.get(site.index.packs[0].address), 1);
  assert.equal(site.requests.size, 1);
  assert.equal(Buffer.from(store.fileOf(number('Scripts/b.txt')).bytes).toString(), 'bravo');
});

check('the store gets the files of a pack that fails alone', async () => {
  const { site, store, logged, number } = packedStore({ broken: true });
  store.prefetch(k_PackGroups[0].map(number));
  await store.drained();
  assert.equal(site.requests.get(site.index.packs[0].address), 2);
  for (const path of k_PackGroups[0]) {
    assert.equal(site.requests.get(hashedAssetAddress(sha(k_Packed[path]))), 1, path);
    assert.equal(Buffer.from(store.fileOf(number(path)).bytes).toString(), k_Packed[path]);
  }
  assert.equal(logged.filter((line) => /cannot get a pack/.test(line)).length, 1);
});

check('the scripts of the page about the game files load in one scope with no name twice', () => {
  const vm = require('node:vm');
  const fs = require('node:fs');
  const scope = vm.createContext({ console });
  for (const name of ['launcher-assets.js', 'launcher-map.js', 'launcher-asset-trace.js']) {
    vm.runInContext(fs.readFileSync(`${__dirname}/../web/page/${name}`, 'utf8'), scope, { filename: name });
  }
});

// A test that never ends leaves node with no work, and node stops with no error. The run then fails here.
process.on('exit', (code) => {
  if (code === 0 && count < tests.length) {
    console.error(`not ok: the test "${tests[count].name}" did not end`);
    process.exitCode = 1;
  }
});

(async () => {
  for (const { name, body } of tests) {
    await body();
    count += 1;
    console.log(`ok ${count} ${name}`);
  }
  console.log(`1..${count}`);
})().catch((error) => {
  console.error(error);
  process.exit(1);
});
