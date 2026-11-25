// Copyright (c) 2018-2026 openblack developers
//
// Test of the map of the game files on the web page (web/page/launcher-map.js), and of the events of the downloads
// that the map reads (web/page/launcher-assets.js). Run with node.
'use strict';

const assert = require('node:assert/strict');
const { createHash } = require('node:crypto');
const {
  assetGroupOf, buildAssetMap, squarify, layoutAssetMap, assetCellAt, formatAssetBytes, formatAssetTime, AssetMapModel,
  describeAssetCell,
} = require('../web/page/launcher-map.js');
const { AssetStore } = require('../web/page/launcher-assets.js');

let count = 0;
const tests = [];
const check = (name, body) => tests.push({ name, body });

const sha = (text) => createHash('sha256').update(text).digest('hex');
const file = (path, size, extra = {}) => ({ path, size, sha256: sha(path), ...extra });
const near = (actual, expected, why) => assert.ok(Math.abs(actual - expected) < 1e-6, `${why}: ${actual} != ${expected}`);

check('each path goes to its part of the game', () => {
  assert.equal(assetGroupOf('Data/INTRO.bik'), 'films');
  assert.equal(assetGroupOf('Audio/Music/align/Good.sad'), 'music');
  assert.equal(assetGroupOf('Audio/Dialogue/HelpSprites.sad'), 'dialogue');
  assert.equal(assetGroupOf('Audio/SFX/Creature/bape.sad'), 'sounds');
  assert.equal(assetGroupOf('Online Maps/mpm_2p_1.map'), 'online');
  assert.equal(assetGroupOf('Data/Landscape/Land1.lnd'), 'lands');
  assert.equal(assetGroupOf('Scripts/Land1.txt'), 'scripts');
  assert.equal(assetGroupOf('Data/Textures/sky.raw'), 'textures');
  assert.equal(assetGroupOf('Data/AllMeshes.g3d'), 'meshes');
  assert.equal(assetGroupOf('Plug Ins/plugin.dll'), 'plugins');
  assert.equal(assetGroupOf('Data/readme.txt'), 'other');
});

check('the map has each content one time, the largest part and file first, and the blocks of each file', () => {
  const files = [
    file('Scripts/a.txt', 10),
    file('Audio/Music/align/Good.sad', 9, { blocks: ['b0', 'b1', 'b2'] }),
    { ...file('Scripts/b.txt', 30), sha256: sha('Scripts/a.txt') },
    file('Scripts/c.txt', 20),
  ];
  const map = buildAssetMap(files, { blockSize: 4, extra: [{ group: 'program', path: 'openblack.wasm', size: 50, key: 'program' }] });
  assert.deepEqual(map.groups.map((group) => group.id), ['program', 'scripts', 'music']);
  assert.equal(map.size, 50 + 30 + 9);
  const scripts = map.groups[1];
  assert.deepEqual(scripts.items.map((item) => item.path), ['Scripts/c.txt', 'Scripts/a.txt']);
  assert.deepEqual(scripts.items[1].also, ['Scripts/b.txt']);
  // The last block is the part of the file after the full blocks
  assert.deepEqual(map.groups[2].items[0].blocks.map((block) => block.size), [4, 4, 1]);
  assert.equal(map.groups[2].items[0].blocks[0].key, 'block b0');
});

check('the squarified layout fills the rectangle with areas that agree with the values', () => {
  const values = [60, 30, 20, 10, 5, 3, 1];
  const rect = { x: 10, y: 20, w: 300, h: 200 };
  const rects = squarify(values, rect);
  const total = values.reduce((sum, value) => sum + value, 0);
  rects.forEach((part, index) => {
    near(part.w * part.h, (values[index] * rect.w * rect.h) / total, `area ${index}`);
    assert.ok(part.x >= rect.x - 1e-9 && part.y >= rect.y - 1e-9, `inside ${index}`);
    assert.ok(part.x + part.w <= rect.x + rect.w + 1e-6 && part.y + part.h <= rect.y + rect.h + 1e-6, `inside ${index}`);
  });
  // No two rectangles overlap
  for (let a = 0; a < rects.length; a += 1) {
    for (let b = a + 1; b < rects.length; b += 1) {
      const p = rects[a];
      const q = rects[b];
      const overlap = Math.max(0, Math.min(p.x + p.w, q.x + q.w) - Math.max(p.x, q.x)) *
        Math.max(0, Math.min(p.y + p.h, q.y + q.h) - Math.max(p.y, q.y));
      assert.ok(overlap < 1e-6, `${a} and ${b} overlap`);
    }
  }
  // A square value set gives squares
  const squares = squarify([1, 1, 1, 1], { x: 0, y: 0, w: 2, h: 2 });
  squares.forEach((part) => {
    near(part.w, 1, 'width');
    near(part.h, 1, 'height');
  });
  assert.deepEqual(squarify([], rect), []);
  assert.deepEqual(squarify([1], { x: 0, y: 0, w: 0, h: 5 }), [{ x: 0, y: 0, w: 0, h: 0 }]);
});

check('the layout puts small files into one square of their part, and a point finds its cell', () => {
  const files = [file('Scripts/big.txt', 1000), ...Array.from({ length: 50 }, (_, n) => file(`Scripts/s${n}.txt`, 1))];
  const map = buildAssetMap(files);
  const layout = layoutAssetMap(map, 200, 100);
  assert.equal(layout.frames.length, 1);
  assert.equal(layout.frames[0].label, true);
  assert.deepEqual(layout.cells.map((cell) => cell.kind), ['item', 'small']);
  assert.equal(layout.cells[1].items.length, 50);
  const big = layout.cells[0];
  assert.equal(assetCellAt(layout, big.x + 1, big.y + 1), big);
  assert.equal(assetCellAt(layout, -5, -5), null);
  // The cells of a part stay below the name of the part
  assert.ok(layout.cells.every((cell) => cell.y >= layout.frames[0].y + 15));
  // A small map has no names
  assert.equal(layoutAssetMap(map, 60, 14, { labels: false }).frames[0].label, false);
});

check('the events of the downloads give the state of each square and the totals', () => {
  const files = [
    file('Scripts/a.txt', 100),
    file('Audio/Music/align/Good.sad', 9, { blocks: ['b0', 'b1', 'b2'] }),
    file('Data/Landscape/Land2.lnd', 50),
    file('Data/Landscape/Land3.lnd', 40),
  ];
  const map = buildAssetMap(files, { blockSize: 4 });
  const model = new AssetMapModel(map);
  const item = (path) => map.groups.flatMap((group) => group.items).find((entry) => entry.path === path);
  const key = (path) => item(path).key;
  assert.equal(model.stateOf(item('Scripts/a.txt')), 'lazy');
  const version = model.version;
  model.handle('queued', key('Scripts/a.txt'), 'now');
  model.handle('queued', key('Data/Landscape/Land2.lnd'), 'later');
  model.handle('present', 'block b0');
  model.handle('failed', key('Data/Landscape/Land3.lnd'));
  model.handle('bytes', 'not on the map', 5);
  assert.equal(model.version, version + 4);
  assert.equal(model.stateOf(item('Scripts/a.txt')), 'urgent');
  assert.equal(model.stateOf(item('Data/Landscape/Land2.lnd')), 'prefetch');
  assert.equal(model.stateOf(item('Data/Landscape/Land3.lnd')), 'failed');
  // A file in blocks shows each block until the browser keeps the whole file
  assert.deepEqual(model.partsOf(item('Audio/Music/align/Good.sad')).map((part) => part.state), ['present', 'lazy', 'lazy']);
  model.handle('started', key('Scripts/a.txt'));
  model.handle('bytes', key('Scripts/a.txt'), 40);
  let totals = model.totals();
  assert.equal(totals.bytes.present, 4 + 40);
  assert.equal(totals.bytes.downloading, 60);
  assert.equal(totals.bytes.prefetch, 50);
  assert.equal(totals.bytes.failed, 40);
  assert.equal(totals.bytes.lazy, 5);
  assert.equal(totals.remaining, 60 + 50);
  assert.equal(totals.active, 1);
  assert.equal(totals.received, 40);
  assert.equal(model.animating(), true);
  // A second queue event does not take a file out of its download
  model.handle('queued', key('Scripts/a.txt'), 'later');
  assert.equal(model.stateOf(item('Scripts/a.txt')), 'downloading');
  model.handle('bytes', key('Scripts/a.txt'), 60);
  model.handle('done', key('Scripts/a.txt'));
  // A block has no event for its bytes: its size counts when it is done
  model.handle('started', 'block b1');
  model.handle('done', 'block b1');
  totals = model.totals();
  assert.equal(totals.received, 100 + 4);
  assert.equal(model.stateOf(item('Scripts/a.txt')), 'present');
  // The whole bank that the page joined from its blocks
  model.handle('present', key('Audio/Music/align/Good.sad'));
  assert.deepEqual(model.partsOf(item('Audio/Music/align/Good.sad')).map((part) => part.state), ['present']);
  assert.equal(model.animating(), false);
  model.addWire(70);
  assert.equal(model.totals().wire, 70);
});

check('the speed and the time left come from the bytes of the last seconds', () => {
  const map = buildAssetMap([file('Scripts/a.txt', 10000)]);
  const model = new AssetMapModel(map);
  const key = map.groups[0].items[0].key;
  model.handle('queued', key, 'now');
  model.handle('started', key);
  assert.equal(model.speed(), 0);
  model.sample(0);
  model.handle('bytes', key, 1000);
  model.sample(1000);
  model.handle('bytes', key, 1000);
  model.sample(2000);
  assert.equal(model.speed(), 1000);
  assert.equal(model.totals().secondsLeft, 8);
  // The samples older than the window go
  for (let time = 3000; time <= 20000; time += 1000) {
    model.sample(time);
  }
  assert.equal(model.speed(), 0);
  assert.equal(model.totals().secondsLeft, null);
  assert.ok(model.samples.length <= 7);
});

check('the text of a cell names its part, the file, its size and its state', () => {
  const files = [
    file('Audio/Music/align/Good.sad', 9 * 1024 * 1024, { blocks: ['b0', 'b1', 'b2', 'b3', 'b4', 'b5', 'b6', 'b7', 'b8'] }),
    file('Scripts/a.txt', 2048),
    file('Scripts/b.txt', 1024),
  ];
  const map = buildAssetMap(files, { blockSize: 1024 * 1024 });
  const model = new AssetMapModel(map);
  model.handle('present', 'block b0');
  const music = map.groups.find((group) => group.id === 'music');
  const text = describeAssetCell({ kind: 'item', group: music, item: music.items[0] }, model);
  assert.equal(text.group, 'Music');
  assert.equal(text.title, 'Audio/Music/align/Good.sad');
  assert.deepEqual(text.lines, ['9.0 MB, Not needed yet', '1 of 9 blocks in this browser']);
  const scripts = map.groups.find((group) => group.id === 'scripts');
  model.handle('present', scripts.items[0].key);
  const small = describeAssetCell({ kind: 'small', group: scripts, items: scripts.items, size: 3072 }, model);
  assert.equal(small.title, '2 small files, 3 KB');
  assert.deepEqual(small.lines, ['In this browser: 2 KB', 'Not needed yet: 1 KB']);
});

check('the bytes and the times for the player', () => {
  assert.equal(formatAssetBytes(512), '1 KB');
  assert.equal(formatAssetBytes(1536 * 1024), '1.5 MB');
  assert.equal(formatAssetBytes(700 * 1024 * 1024), '700 MB');
  assert.equal(formatAssetBytes(3 * 1024 * 1024 * 1024), '3.00 GB');
  assert.equal(formatAssetTime(0.2), '1 s');
  assert.equal(formatAssetTime(200), '3 min 20 s');
  assert.equal(formatAssetTime(3900), '1 h 5 min');
  assert.equal(formatAssetTime(null), '-');
  assert.equal(formatAssetTime(Infinity), '-');
});

check('the store gives each change of a download to watch, and an error of watch stops no download', async () => {
  const text = 'land two';
  const files = [{ path: 'Data/Landscape/Land2.lnd', size: text.length, sha256: sha(text) }];
  const events = [];
  const folder = {
    async getFileHandle(name) {
      return {
        async createWritable() {
          return { async write() {}, async close() {} };
        },
        async getFile() {
          return { name, size: text.length };
        },
      };
    },
  };
  const fetchFile = async () => ({
    status: 200,
    body: {
      getReader() {
        let sent = false;
        return {
          async read() {
            if (sent) {
              return { done: true };
            }
            sent = true;
            return { done: false, value: Buffer.from(text) };
          },
          async cancel() {},
        };
      },
    },
  });
  const digest = async (algorithm, bytes) => createHash('sha256').update(bytes).digest();
  const store = new AssetStore(files, {
    fetchFile, digest, folder,
    watch: (event, key, detail) => {
      events.push([event, key, detail]);
      throw new Error('the map fails');
    },
  });
  store.prefetch([0]);
  await store.obtain(0);
  const key = `file ${sha(text)}`;
  assert.deepEqual(events, [
    ['queued', key, 'later'], ['started', key, undefined], ['bytes', key, text.length],
    ['done', key, undefined],
  ]);
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
