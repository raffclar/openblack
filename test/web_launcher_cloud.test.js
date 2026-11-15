// Test of the saved games and creatures on the site (web/page/launcher-cloud.js). Run with node.
'use strict';

const assert = require('node:assert/strict');
const { createHash } = require('node:crypto');
const { readFileSync } = require('node:fs');
const { join } = require('node:path');
const {
  isCloudPlace, cloudKey, parseCloudList, planRestore, planSends, fileStamp, cloudHex, sendAgainLater, CloudSaves,
  k_CloudWaitingKey,
} = require('../web/page/launcher-cloud.js');

const tests = [];
const check = (name, body) => tests.push({ name, body });

const SLOT = '_A_n_n/Saved Games/3';
const sha = (bytes) => createHash('sha256').update(Buffer.from(bytes)).digest('hex');
const digest = async (bytes) => createHash('sha256').update(Buffer.from(bytes)).digest();
const bytesOf = (text) => new TextEncoder().encode(text).buffer;
const uuid = (n) => `00000000-0000-4000-8000-${String(n).padStart(12, '0')}`;

// A fake folder of the private storage: a tree of objects, with text as the content of a file
function fakeFolder(tree, times = new Map(), busy = new Set(), path = '') {
  return {
    kind: 'directory',
    async *entries() {
      for (const [name, value] of Object.entries(tree)) {
        const at = path ? `${path}/${name}` : name;
        if (typeof value === 'string') {
          yield [name, {
            kind: 'file',
            async getFile() {
              if (busy.has(at)) {
                throw new Error('the game has the file open');
              }
              const buffer = bytesOf(tree[name]);
              return {
                size: buffer.byteLength, lastModified: times.get(at) ?? 1,
                arrayBuffer: async () => buffer.slice(0),
              };
            },
          }];
        } else {
          yield [name, fakeFolder(value, times, busy, at)];
        }
      }
    },
  };
}

// A fake site page: the files of the account, by key, and the questions that came
function fakeSite(files = {}, { status = 200, putStatus = null } = {}) {
  const kept = new Map(Object.entries(files).map(([key, text], index) => {
    const at = key.lastIndexOf('/');
    return [key, { id: uuid(index + 1), folder: at < 0 ? '' : key.slice(0, at), name: key.slice(at + 1), text }];
  }));
  const asked = [];
  let next = kept.size + 1;
  const entry = (file) => ({
    id: file.id, folder: file.folder, name: file.name, size: bytesOf(file.text).byteLength,
    sha256: sha(bytesOf(file.text)), updated: '2026-11-15T10:00:00Z',
  });
  const ask = async (question) => {
    asked.push(question);
    if (question.op === 'list') {
      if (status !== 200) {
        return { status, body: '{}' };
      }
      const list = [...kept.values()].map(entry);
      const bytes = list.reduce((sum, file) => sum + file.size, 0);
      return {
        status: 200,
        body: JSON.stringify({ files: list, usage: { files: list.length, bytes }, limits: { files: 500, bytes: 1e9 } }),
      };
    }
    if (question.op === 'get') {
      const file = [...kept.values()].find((item) => item.id === question.file);
      return file ? { status: 200, body: '', bytes: bytesOf(file.text) } : { status: 404, body: '' };
    }
    if (question.op === 'put') {
      if (putStatus !== null) {
        return { status: putStatus, body: 'refused' };
      }
      const key = cloudKey(question.folder, question.name);
      const text = new TextDecoder().decode(question.bytes);
      const earlier = kept.get(key);
      kept.set(key, { id: earlier ? earlier.id : uuid(next++), folder: question.folder, name: question.name, text });
      return { status: earlier ? 200 : 201, body: '{}' };
    }
    return { status: 400, body: '' };
  };
  return { kept, asked, ask };
}

// A fake store of this browser for the files that wait
function fakeStore(initial = {}) {
  const items = new Map(Object.entries(initial));
  return { getItem: (key) => items.get(key) ?? null, setItem: (key, value) => items.set(key, value), items };
}

function cloudOf(site, tree, { times, busy, store } = {}) {
  const written = [];
  const lines = [];
  const cloud = new CloudSaves({
    ask: site.ask,
    profiles: fakeFolder(tree, times, busy),
    write: async (folder, name, bytes) => {
      written.push(cloudKey(folder, name));
      let at = tree;
      for (const part of folder ? folder.split('/') : []) {
        at = at[part] ??= {};
      }
      at[name] = new TextDecoder().decode(bytes);
      return true;
    },
    digest,
    log: (line) => lines.push(line),
    store,
  });
  return { cloud, written, lines };
}

check('the places of the files that the game writes are taken', () => {
  for (const [folder, name] of [
    [SLOT, '3.obsav'], ['_A_n_n/Saved Games/15/2', '15.obsav'], [SLOT, 'SaveGame.inf'], [SLOT, 'SavedCreature0'],
    [SLOT, 'Script_3.sav'], [SLOT, '3.sav'], ['Scripts/CreatureMind', 'C65d9c0ff.erc'],
    ['Scripts/CreatureMind', 'PhysiqueC65d9c0ff.erc'], ['_A_n_n/Saved Games/Pictures', 'save_game_picture_hires_1.raw'],
    [SLOT, 'screenshots_lores_0_map_0.raw'], ['_A_n_n/Current', 'screenshots_hires_3_map_0.raw'],
    ['_A_n_n', 'creature.lhp'], ['', 'Profiles.txt'],
  ]) {
    assert.ok(isCloudPlace(folder, name), `${folder}/${name}`);
  }
});

check('other places are refused', () => {
  for (const [folder, name] of [
    [SLOT, '../3.obsav'], [SLOT, 'a/3.obsav'], ['_A_n_n/Saved Games/../3', '3.obsav'], ['_A_n_n/../x', '3.obsav'],
    ['..', 'Profiles.txt'], ['_A_n_n', '3.obsav'], ['', 'helpstats.dat'], ['_A_n_n', 'helpstats.dat'],
    ['_A_n_n/html', 'Ann.html'], [SLOT, '3.obsav.tmp'], ['', ''], [null, 'x'],
  ]) {
    assert.ok(!isCloudPlace(folder, name), `${folder}/${name}`);
  }
});

check('the list of the site keeps only good entries', () => {
  const good = { id: uuid(1), folder: SLOT, name: '3.obsav', size: 8, sha256: 'a'.repeat(64) };
  const list = parseCloudList(JSON.stringify({
    files: [good, { ...good, folder: '../x' }, { ...good, id: 'x' }, { ...good, size: -1 }, { ...good, sha256: 'z' }],
    usage: { files: 5, bytes: 40 }, limits: { files: 500, bytes: 100 },
  }));
  assert.equal(list.files.length, 1);
  assert.deepEqual(list.usage, { files: 5, bytes: 40 });
  assert.equal(parseCloudList('not json'), null);
  assert.equal(parseCloudList('{"files": 3}'), null);
});

check('the plans of the files that come back and the files that go', () => {
  const remote = [{ folder: SLOT, name: '3.obsav' }, { folder: '', name: 'Profiles.txt' }];
  assert.deepEqual(planRestore(remote, (key) => key === 'Profiles.txt').map((file) => file.name), ['3.obsav']);
  const local = [
    { folder: SLOT, name: '3.obsav', size: 4, lastModified: 1 },
    { folder: SLOT, name: 'SaveGame.inf', size: 4, lastModified: 1 },
    { folder: '', name: 'Profiles.txt', size: 4, lastModified: 2 },
  ];
  const remoteByKey = new Map([[`${SLOT}/3.obsav`, {}], ['Profiles.txt', {}]]);
  const firstSeen = new Map([[`${SLOT}/3.obsav`, '4:1'], ['Profiles.txt', '4:1']]);
  // a new file goes, a file the site has and the game did not change stays, a changed file goes
  assert.deepEqual(planSends(local, remoteByKey, firstSeen, new Set()).map((file) => file.name),
    ['SaveGame.inf', 'Profiles.txt']);
  assert.deepEqual(planSends(local, remoteByKey, firstSeen, new Set([`${SLOT}/3.obsav`])).map((file) => file.name),
    ['3.obsav', 'SaveGame.inf', 'Profiles.txt']);
  assert.equal(fileStamp({ size: 4, lastModified: 9 }), '4:9');
  assert.equal(cloudHex(new Uint8Array([0, 15, 255]).buffer), '000fff');
  assert.ok(sendAgainLater(0) && sendAgainLater(429) && sendAgainLater(503));
  assert.ok(!sendAgainLater(409) && !sendAgainLater(413) && !sendAgainLater(422) && !sendAgainLater(507));
});

check('a new browser gets back the files of the site and sends nothing again', async () => {
  const site = fakeSite({ [`${SLOT}/3.obsav`]: 'OBSV world', 'Profiles.txt': 'current\tAnn\n' });
  const tree = {};
  const { cloud, written } = cloudOf(site, tree);
  assert.equal(await cloud.restore(), 2);
  assert.deepEqual(written.sort(), ['Profiles.txt', `${SLOT}/3.obsav`]);
  assert.equal(tree._A_n_n['Saved Games']['3']['3.obsav'], 'OBSV world');
  await cloud.check();
  assert.ok(!site.asked.some((question) => question.op === 'put'));
});

check('a file that the browser has is never replaced by the site', async () => {
  const site = fakeSite({ [`${SLOT}/3.obsav`]: 'OBSV site' });
  const tree = { _A_n_n: { 'Saved Games': { 3: { '3.obsav': 'OBSV local' } } } };
  const { cloud, written } = cloudOf(site, tree);
  assert.equal(await cloud.restore(), 0);
  assert.deepEqual(written, []);
  assert.equal(tree._A_n_n['Saved Games']['3']['3.obsav'], 'OBSV local');
  // the game did not write the file in this visit: the old copy of this browser does not replace the site's
  await cloud.check();
  assert.ok(!site.asked.some((question) => question.op === 'put'));
});

check('a file that the game writes goes to the site, once', async () => {
  const site = fakeSite();
  const times = new Map();
  const tree = { _A_n_n: { 'Saved Games': { 3: {} } }, Notes: { 'readme.txt': 'not a save' } };
  const { cloud } = cloudOf(site, tree, { times });
  await cloud.restore();
  tree._A_n_n['Saved Games']['3']['3.obsav'] = 'OBSV first';
  tree._A_n_n['Saved Games']['3']['SaveGame.inf'] = 'info';
  await cloud.check();
  assert.equal(site.kept.get(`${SLOT}/3.obsav`).text, 'OBSV first');
  assert.equal(site.kept.size, 2);
  const puts = () => site.asked.filter((question) => question.op === 'put').length;
  assert.equal(puts(), 2);
  await cloud.check();
  assert.equal(puts(), 2);
  // the game saves the slot again
  tree._A_n_n['Saved Games']['3']['3.obsav'] = 'OBSV second';
  times.set(`${SLOT}/3.obsav`, 2);
  await cloud.check();
  assert.equal(site.kept.get(`${SLOT}/3.obsav`).text, 'OBSV second');
  assert.equal(puts(), 3);
  assert.equal(cloud.listing.files.length, 2);
});

check('a send that fails waits, also after the page closes, and goes at a later check', async () => {
  const site = fakeSite({}, { putStatus: 0 });
  const store = fakeStore();
  const tree = { '': {} };
  delete tree[''];
  tree['Profiles.txt'] = 'current\tAnn\n';
  const { cloud, lines } = cloudOf(site, tree, { store });
  await cloud.restore();
  await cloud.check();
  assert.ok(!site.kept.has('Profiles.txt'));
  assert.deepEqual(JSON.parse(store.items.get(k_CloudWaitingKey)), ['Profiles.txt']);
  assert.ok(lines.some((line) => line.includes('tries again later')));
  // the next page of this browser
  const later = fakeSite({ 'Profiles.txt': 'current\tBob\n' });
  const next = cloudOf(later, tree, { store });
  await next.cloud.restore();
  await next.cloud.check();
  assert.equal(later.kept.get('Profiles.txt').text, 'current\tAnn\n');
  assert.deepEqual(JSON.parse(store.items.get(k_CloudWaitingKey)), []);
});

check('a refused file is not sent again until the game writes it again', async () => {
  const site = fakeSite({}, { putStatus: 422 });
  const tree = { 'Profiles.txt': 'current\tAnn\n' };
  const { cloud } = cloudOf(site, tree);
  await cloud.restore();
  await cloud.check();
  await cloud.check();
  assert.equal(site.asked.filter((question) => question.op === 'put').length, 1);
});

check('a file that the game has open waits for the next check', async () => {
  const site = fakeSite();
  const busy = new Set(['Profiles.txt']);
  const tree = { 'Profiles.txt': 'current\tAnn\n' };
  const { cloud } = cloudOf(site, tree, { busy });
  await cloud.restore();
  await cloud.check();
  assert.ok(!site.kept.has('Profiles.txt'));
  busy.clear();
  await cloud.check();
  assert.ok(site.kept.has('Profiles.txt'));
});

check('with no sign-in the page keeps the files in this browser only', async () => {
  const site = fakeSite({}, { status: 401 });
  const tree = { 'Profiles.txt': 'current\tAnn\n' };
  const { cloud } = cloudOf(site, tree);
  assert.equal(await cloud.restore(), 0);
  assert.equal(cloud.state, 'signed-out');
  await cloud.check();
  assert.ok(!site.asked.some((question) => question.op === 'put'));
});

check('a file from the site with the wrong bytes is not written', async () => {
  const site = fakeSite({ 'Profiles.txt': 'current\tAnn\n' });
  const ask = site.ask;
  site.ask = async (question) => (question.op === 'get' ? { status: 200, bytes: bytesOf('current\tEve\n') } : ask(question));
  const tree = {};
  const { cloud, written } = cloudOf(site, tree);
  assert.equal(await cloud.restore(), 0);
  assert.deepEqual(written, []);
});

check('no check runs while the files come back', async () => {
  const site = fakeSite({ 'Profiles.txt': 'current\tAnn\n' });
  const { cloud } = cloudOf(site, {});
  const restoring = cloud.restore();
  await cloud.check();
  await restoring;
  assert.ok(!site.asked.some((question) => question.op === 'put'));
});

check('the game page shows no list, Download or Upload of the files on the site', () => {
  // The account page of the site shows the list. The game page only sends the files and brings them back.
  const page = readFileSync(join(__dirname, '../web/page/index.html'), 'utf8');
  assert.ok(!page.includes('cloud-saves'));
  assert.ok(!/>\s*(Download|Upload)\s*</.test(page));
  const script = readFileSync(join(__dirname, '../web/page/launcher-cloud.js'), 'utf8');
  assert.ok(!script.includes("'Download'"));
  assert.ok(!script.includes('createElement'));
});

(async () => {
  let failed = 0;
  for (const { name, body } of tests) {
    try {
      await body();
      console.log(`ok ${name}`);
    } catch (error) {
      failed += 1;
      console.error(`FAILED ${name}\n${error.stack}`);
    }
  }
  console.log(`${tests.length - failed}/${tests.length} passed`);
  process.exit(failed ? 1 : 0);
})();
