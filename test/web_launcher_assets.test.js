// Test of the game files of the site on the web page (web/page/launcher-assets.js). Run with node.
'use strict';

const assert = require('node:assert/strict');
const { createHash } = require('node:crypto');
const {
  isSafeAssetPath, parseAssetIndex, planAssetCache, assetAddress, hashedAssetAddress, assetRequest, hexText,
  refusalReason, fetchAssetIndex, fetchAsset, keepAssets, askToKeepStorage, streamAssets, isStartAsset, planAssetStart,
  planAssetPrefetch, assetBlockBounds, assetBlockRange,
  parseContentRange, parseAssetTiers, fetchAssetTiers, k_AssetTiersAddress,
  k_AssetIndexAddress, k_AssetCacheFolder,
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
    JSON.stringify({ format: 3, files: { a: good } }),
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

check('the start gets the files that the start of the game reads, and only the first block of each music bank', () => {
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
  const start = planAssetStart(files);
  assert.deepEqual(start.whole.map((number) => files[number].path),
    ['Data/AllMeshes.g3d', 'Data/INTRO.bik', 'Data/Landscape/Land1.lnd']);
  assert.deepEqual(start.blocks, [{ number: numberOf(files, 'Audio/Music/align/Good.sad'), block: 0 }]);
});

check('the plan for later gets the miracles after the start, the next land, and the files of a network map', () => {
  const { files } = parseAssetIndex(streamedText());
  const paths = (numbers) => numbers.map((number) => files[number].path);
  assert.deepEqual(paths(planAssetPrefetch(files, {})), ['Data/Spells/Anims/heal.anm']);
  assert.deepEqual(paths(planAssetPrefetch(files, { path: 'DATA/LANDSCAPE/Land1.lnd' })),
    ['Data/Landscape/Land2.lnd', 'Data/Landscape/land2.fot']);
  const have = (sha256) => sha256 === sha('land two');
  assert.deepEqual(paths(planAssetPrefetch(files, { path: 'Data/Landscape/Land1.lnd', have })),
    ['Data/Landscape/land2.fot']);
  assert.deepEqual(paths(planAssetPrefetch(files, { path: 'Data/Landscape/Land5.lnd' })), []);
  assert.deepEqual(paths(planAssetPrefetch(files, { path: 'Data/AllMeshes.g3d' })), []);
});

// A tiers file for the streamed files: the meshes at the startup, land 1 at the land load, the first and the third
// block of the music at the startup and the heal miracle in the play of land 1. Land 2 is a later land.
const tiersText = (extra = {}) => JSON.stringify({
  format: 1,
  made_from: { start: ['traces/land1.json'], later: ['traces/land2.json'] },
  phases: ['boot', 'pre-intro', 'startup', 'land-load', 'land-soon', 'later', 'background'],
  start_phases: 4,
  block_size: k_BlockSize,
  files: {
    'data/allmeshes.g3d': { phase: 2, order: 0 },
    'audio/music/align/good.sad': { phase: 2, order: 1, blocks: [[2, 0, 1], [2, 2, 1], [4, 3, 2]] },
    'data/landscape/land1.lnd': { phase: 3, order: 0 },
    'data/spells/anims/heal.anm': { phase: 4, order: 1 },
    'data/intro.bik': { phase: 4, order: 0 },
    'data/landscape/land2.fot': { phase: 5, order: 0 },
    'data/not/in/the/list.txt': { phase: 0, order: 0 },
  },
  lands: { land2: ['data/landscape/land2.fot', 'data/landscape/land2.lnd'] },
  ...extra,
});

check('the tiers file gives the phase of each file, the files of the first land and the files of each land', () => {
  const tiers = parseAssetTiers(tiersText());
  assert.equal(tiers.startPhases, 4);
  assert.equal(tiers.files.get('data/landscape/land1.lnd').phase, 3);
  assert.equal(tiers.files.get('data/allmeshes.g3d').blocks, null);
  assert.deepEqual(tiers.soon, ['data/intro.bik', 'data/spells/anims/heal.anm']);
  assert.deepEqual(tiers.lands.get('land2'), ['data/landscape/land2.fot', 'data/landscape/land2.lnd']);
  const good = JSON.parse(tiersText());
  for (const wrong of [
    { ...good, format: 2 },
    { ...good, start_phases: 9 },
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

check('with the tiers the start gets the files and the blocks of the phases before the start, in the list order', () => {
  const { files } = parseAssetIndex(streamedText());
  const tiers = parseAssetTiers(tiersText());
  for (const path of ['Data/AllMeshes.g3d', 'DATA/LANDSCAPE/Land1.lnd', 'Audio/Music/align/Good.sad']) {
    assert.equal(isStartAsset(path, tiers), true, path);
  }
  // A file of the play of the first land, a later file and a file that no trace reads
  for (const path of ['Data/INTRO.bik', 'Data/Landscape/land2.fot', 'Data/Landscape/Land2.lnd']) {
    assert.equal(isStartAsset(path, tiers), false, path);
  }
  const start = planAssetStart(files, tiers, k_BlockSize);
  assert.deepEqual(start.whole.map((number) => files[number].path), ['Data/AllMeshes.g3d', 'Data/Landscape/Land1.lnd']);
  const bank = numberOf(files, 'Audio/Music/align/Good.sad');
  assert.deepEqual(start.blocks, [{ number: bank, block: 0 }, { number: bank, block: 2 }]);
  // The list has blocks of another size: the page gets the whole file
  assert.deepEqual(planAssetStart(files, tiers, 8).whole.map((number) => files[number].path),
    ['Data/AllMeshes.g3d', 'Audio/Music/align/Good.sad', 'Data/Landscape/Land1.lnd']);
});

check('with the tiers the plan for later gets the files of the first land first, then those of the next land', () => {
  const { files } = parseAssetIndex(streamedText());
  const tiers = parseAssetTiers(tiersText());
  const paths = (numbers) => numbers.map((number) => files[number].path);
  // The play of the first land in the order of the trace, then the rules for the miracles
  assert.deepEqual(paths(planAssetPrefetch(files, { tiers })), ['Data/INTRO.bik', 'Data/Spells/Anims/heal.anm']);
  assert.deepEqual(paths(planAssetPrefetch(files, { path: 'Data/Landscape/Land1.lnd', tiers })),
    ['Data/Landscape/Land2.lnd', 'Data/Landscape/land2.fot']);
  const have = (sha256) => sha256 === sha('land two');
  assert.deepEqual(paths(planAssetPrefetch(files, { path: 'Data/Landscape/Land1.lnd', have, tiers })),
    ['Data/Landscape/land2.fot']);
  // A network map gets its other files as with the rules (the file that the game read is there already in the page)
  assert.deepEqual(paths(planAssetPrefetch(files, { path: 'Online Maps/mpm_2p_1.map', tiers })),
    ['Online Maps/mpm_2p_1.map']);
});

check('with the tiers the start downloads the files and blocks of the tiers', async () => {
  const root = new FakeFolder();
  const site = fakeSite(k_Streamed, { index: streamedText() });
  const { index } = await fetchAssetIndex(site.fetchFile);
  await streamAssets(index, { fetchFile: site.fetchFile, digest, root, tiers: parseAssetTiers(tiersText()) });
  const music = assetAddress('Audio/Music/align/Good.sad');
  assert.deepEqual([...site.requests.keys()].sort(), [k_AssetIndexAddress, assetAddress('Data/AllMeshes.g3d'),
    assetAddress('Data/Landscape/Land1.lnd'), `${music} bytes=0-3`, `${music} bytes=8-11`].sort());
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
  return { root, site, index, shared, logged, ...result };
};

check('before the start the page gets only the start files and the first block of the music', async () => {
  const { site, entries, index } = await streamedSite();
  const music = assetAddress('Audio/Music/align/Good.sad');
  assert.deepEqual([...site.requests.keys()].sort(), [k_AssetIndexAddress, assetAddress('Data/AllMeshes.g3d'),
    assetAddress('Data/INTRO.bik'), assetAddress('Data/Landscape/Land1.lnd'), `${music} bytes=0-3`].sort());
  const entry = (path) => entries[numberOf(index.files, path)];
  assert.equal(Buffer.from(entry('Data/Landscape/Land1.lnd').file.bytes).toString(), 'land one');
  assert.equal(entry('Data/Landscape/Land2.lnd').file, null);
  assert.equal(entry('Data/Landscape/Land2.lnd').size, 8);
  const bank = entry('Audio/Music/align/Good.sad');
  assert.equal(bank.file, null);
  assert.equal(Buffer.from(bank.blocks[0].bytes).toString(), 'HEAD');
  assert.deepEqual(bank.blocks.slice(1), [null, null, null, null, null, null]);
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
  const state = (model, path) => model.stateOf(model.map.groups.flatMap((group) => group.items)
    .find((item) => item.path === path));
  assert.equal(state(first.model, 'Data/Landscape/Land1.lnd'), 'present');
  assert.equal(state(first.model, 'Data/Landscape/Land2.lnd'), 'lazy');
  assert.equal(first.model.totals().received, 6 + 14 + 8 + 4);
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

check('only a file at its address by SHA-256 can come from the cache of the browser', () => {
  assert.deepEqual(assetRequest({ path: 'Data/a b.txt', address: '/assets/sha256/x', hashed: true }),
    { address: '/assets/sha256/x', cache: 'default' });
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
  const music = hashedAssetAddress(sha(k_Streamed['Audio/Music/align/Good.sad']));
  assert.deepEqual([...site.requests.keys()].sort(), [k_AssetIndexAddress,
    hashedAssetAddress(sha('meshes')), hashedAssetAddress(sha('the intro film')), hashedAssetAddress(sha('land one')),
    `${music} bytes=0-3`].sort());
  await store.obtain(numberOf(index.files, 'Data/Landscape/Land2.lnd'));
  assert.equal(site.requests.get(hashedAssetAddress(sha('land two'))), 1);
  for (const [key, cache] of site.caches) {
    assert.equal(cache, key === k_AssetIndexAddress ? 'no-cache' : 'default', key);
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

check('the voice banks of the species come after the start, after the files of the miracles', () => {
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
  assert.deepEqual(planAssetStart(files).whole.map((number) => files[number].path),
    ['Audio/SFX/Creature/creature.sad', 'Audio/SFX/Game/InGame.sad']);
  assert.deepEqual(planAssetPrefetch(files, {}).map((number) => files[number].path),
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
  // A range of a file of another size fails at the first block
  const longer = fakeSite(k_Streamed, { index: streamedText(),
    change: { 'Audio/Music/align/Good.sad': 'HEADmusic of the good god and more' } });
  await assert.rejects(streamAssets((await fetchAssetIndex(longer.fetchFile)).index,
    { fetchFile: longer.fetchFile, digest, root: new FakeFolder() }), /range/);
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
