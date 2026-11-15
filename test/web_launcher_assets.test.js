// Test of the game files of the site on the web page (web/page/launcher-assets.js). Run with node.
'use strict';

const assert = require('node:assert/strict');
const { createHash } = require('node:crypto');
const {
  isSafeAssetPath, parseAssetIndex, planAssetCache, assetAddress, hexText, refusalReason, fetchAssetIndex, fetchAsset,
  keepAssets,
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
const fakeResponse = (status, text) => ({
  status,
  ok: status >= 200 && status < 300,
  text: async () => text,
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
const fakeSite = (files, { status = 200, index = indexText(files), change = {} } = {}) => {
  const requests = new Map();
  const fetchFile = async (address) => {
    requests.set(address, (requests.get(address) ?? 0) + 1);
    if (address === k_AssetIndexAddress) {
      return fakeResponse(status, status === 200 ? index : 'Your account cannot download the game files yet.');
    }
    const path = Object.keys(files).find((name) => assetAddress(name) === address);
    if (path === undefined) {
      return fakeResponse(404, '');
    }
    return fakeResponse(200, change[path] ?? files[path]);
  };
  return { fetchFile, requests };
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
    return { size: this.bytes.length, bytes: this.bytes };
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
  assert.deepEqual(index.files[0], { path: 'Data/AllMeshes.g3d', size: 6, sha256: sha('meshes') });
});

check('the page refuses a list that is not correct', () => {
  const good = { size: 1, sha256: 'b'.repeat(64) };
  const bad = [
    'not json',
    '[]',
    JSON.stringify({ format: 2, files: { a: good } }),
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
