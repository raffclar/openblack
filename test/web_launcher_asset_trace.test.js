// ******************************************************************************
// Copyright (c) 2018-2026 openblack developers
//
// For a complete list of all authors, please refer to contributors.md
// Interested in contributing? Visit https://github.com/openblack/openblack
//
// openblack is licensed under the GNU General Public License version 3.
// ******************************************************************************
// Test of the trace of the game files on the web page (web/page/launcher-asset-trace.js). Run with node.
'use strict';

const assert = require('node:assert/strict');
const {
  AssetTraceRecorder, assetPhaseOfRead, assetPhaseOfLog, k_AssetPhases, k_AssetTracePhases, k_AssetTraceBlockSize,
} = require('../web/page/launcher-asset-trace.js');

let count = 0;
const check = (name, body) => {
  body();
  count += 1;
  console.log(`ok ${count} ${name}`);
};

const k_Mib = k_AssetTraceBlockSize;
const k_Files = [
  { path: 'Data/logo.bik', size: 100 },
  { path: 'Data/pre_intro.bik', size: 3 * k_Mib },
  { path: 'Data/tips.bik', size: 10 },
  { path: 'Audio/Music/align/Good.sad', size: 5 * k_Mib + 1 },
  { path: 'Data/Landscape/Land1.lnd', size: 2000 },
  { path: 'Data/Landscape/Multi_Player/TwoGods.lnd', size: 2000 },
  { path: 'Scripts/Land1.txt', size: 50 },
];

check('the phases of the trace come from the reads of the films and the landscapes, and from the game log', () => {
  assert.equal(k_AssetPhases[k_AssetTracePhases - 1], 'land-soon');
  assert.equal(assetPhaseOfRead('DATA/Pre_Intro.bik'), 1);
  assert.equal(assetPhaseOfRead('Data/tips.bik'), 2);
  assert.equal(assetPhaseOfRead('Data/Landscape/Land1.lnd'), 3);
  assert.equal(assetPhaseOfRead('Data/Landscape/Multi_Player/TwoGods.lnd'), 3);
  assert.equal(assetPhaseOfRead('Data/Landscape/Land1.fot'), null);
  assert.equal(assetPhaseOfRead('Data/logo.bik'), null);
  assert.equal(assetPhaseOfLog('[game] [info] Load land Land1: 812.4 ms in all'), 4);
  assert.equal(assetPhaseOfLog('[game] [info] Load land Land1: meshes 12.0 ms'), null);
  assert.equal(assetPhaseOfLog('Load landscape Land1: 10.0 ms in all'), null);
});

check('the trace has the first read of each block of each file, with the phase and the time', () => {
  let time = 1000;
  const trace = new AssetTraceRecorder(k_Files, () => time);
  trace.read(0, 0, 100);
  time += 5;
  trace.read(0, 50, 10);
  trace.read(1, 0, 3 * k_Mib);
  time += 5;
  trace.read(2, 0, 10);
  trace.read(3, 0, 1);
  trace.read(3, 5 * k_Mib, k_Mib);
  trace.read(6, 0, 50);
  time += 1;
  trace.read(4, 0, 2000);
  trace.log('Load land Land1: 5.0 ms in all');
  trace.read(3, 2 * k_Mib, 10);
  // A number that is not a file, and a read that is not correct, give nothing
  trace.read(99, 0, 1);
  trace.read(6, -1, 1);
  const result = trace.trace({ scenario: 'land1', build: 'test' });
  assert.equal(result.format, 1);
  assert.equal(result.block_size, k_Mib);
  assert.deepEqual(result.phases, ['boot', 'pre-intro', 'startup', 'land-load', 'land-soon']);
  assert.deepEqual(result.reads.map((read) => [read.path, read.block, read.phase, read.t]), [
    ['Data/logo.bik', 0, 0, 0],
    ['Data/pre_intro.bik', 0, 1, 5],
    ['Data/pre_intro.bik', 1, 1, 5],
    ['Data/pre_intro.bik', 2, 1, 5],
    ['Data/tips.bik', 0, 2, 10],
    ['Audio/Music/align/Good.sad', 0, 2, 10],
    ['Audio/Music/align/Good.sad', 5, 2, 10],
    ['Scripts/Land1.txt', 0, 2, 10],
    ['Data/Landscape/Land1.lnd', 0, 3, 11],
    ['Audio/Music/align/Good.sad', 2, 4, 11],
  ]);
  assert.deepEqual(result.marks.map((mark) => [mark.phase, mark.t]),
    [['pre-intro', 5], ['startup', 10], ['land-load', 11], ['land-soon', 11]]);
});

check('a phase never goes back to an earlier phase', () => {
  const trace = new AssetTraceRecorder(k_Files, () => 0);
  trace.read(4, 0, 1);
  trace.read(2, 0, 1);
  trace.read(6, 0, 1);
  assert.deepEqual(trace.trace().reads.map((read) => read.phase), [3, 3, 3]);
});

console.log(`1..${count}`);
