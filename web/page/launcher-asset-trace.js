// ******************************************************************************
// Copyright (c) 2018-2026 openblack developers
//
// For a complete list of all authors, please refer to contributors.md
// Interested in contributing? Visit https://github.com/openblack/openblack
//
// openblack is licensed under the GNU General Public License version 3.
// ******************************************************************************
// openblack web page: the trace of the game files that the game reads, for the tiers of the game files.
//
// With the page parameter recordAssetTrace, the page writes a line for the first read of each block of each game file:
// the path, the block, the time and the phase of the game. The tool tools/web-asset-tiers makes the file
// asset-tiers.json from these traces. The page then gets the game files in the order of the phases (launcher-assets.js).
//
// The phase of the game comes from the reads and from the game log:
// - boot: the start of the game, the logos and the front end,
// - pre-intro: the film of the first run starts,
// - startup: the tips screen starts while the game loads its data,
// - land-load: the game reads the landscape of a land,
// - land-soon: the land is ready, and the player plays.
// A phase never goes back to an earlier phase.
'use strict';

// The names of the phases of the game, in their order. The tiers file also has the phases "later" (the files that only
// other lands and maps read) and "background" (the files that no trace reads).
const k_AssetPhases = ['boot', 'pre-intro', 'startup', 'land-load', 'land-soon', 'later', 'background'];
// The phases that a trace of the game can record
const k_AssetTracePhases = 5;
// The size of a block of a file in the trace. It is the size of a block of the block cache of the game.
const k_AssetTraceBlockSize = 1024 * 1024;
// The format of the trace file
const k_AssetTraceFormat = 1;
// The reads of a file that start a phase, as [rule of the lower-case path, phase]
const k_AssetPhaseReads = [
  [/^data\/pre_intro\.bik$/, 1],
  [/^data\/tips\.bik$/, 2],
  [/^data\/landscape\/(.*\/)?[^/]+\.lnd$/, 3],
];
// The line of the game log that starts a phase, as [rule of the line, phase]. The game writes the time of the land
// load when the land is ready.
const k_AssetPhaseLogs = [
  [/Load land [^:]+: [\d.]+ ms in all/, 4],
];

/** The phase that starts with the read of the file at this path, or no value */
function assetPhaseOfRead(path) {
  const lower = path.toLowerCase();
  const found = k_AssetPhaseReads.find(([rule]) => rule.test(lower));
  return found ? found[1] : null;
}

/** The phase that starts with this line of the game log, or no value */
function assetPhaseOfLog(line) {
  const found = k_AssetPhaseLogs.find(([rule]) => rule.test(line));
  return found ? found[1] : null;
}

/**
 * The record of the first read of each block of each game file. The arguments are the files of the game folder in the
 * order of their numbers, each with a path and a size, and a function that gives the time in milliseconds.
 */
class AssetTraceRecorder {
  constructor(files, now) {
    this.files = files;
    this.now = now;
    this.start = now();
    this.phase = 0;
    this.seen = new Set();
    this.reads = [];
    this.marks = [];
  }

  /** Start the phase when it is after the phase now, and write the time of the start */
  enter(phase, reason) {
    if (phase !== null && phase > this.phase) {
      this.phase = phase;
      this.marks.push({ phase: k_AssetPhases[phase], t: this.time(), reason });
    }
  }

  time() {
    return Math.round((this.now() - this.start) * 10) / 10;
  }

  /** The game read the bytes of the file with this number, at the offset and with the length */
  read(number, offset, length) {
    const file = this.files[number];
    if (!file || !Number.isFinite(offset) || !Number.isFinite(length) || offset < 0) {
      return;
    }
    this.enter(assetPhaseOfRead(file.path), file.path);
    const first = Math.floor(offset / k_AssetTraceBlockSize);
    const end = Math.min(offset + Math.max(length, 1), Math.max(file.size, offset + 1));
    const last = Math.floor((end - 1) / k_AssetTraceBlockSize);
    for (let block = first; block <= last; block += 1) {
      const key = `${number}:${block}`;
      if (!this.seen.has(key)) {
        this.seen.add(key);
        this.reads.push({ path: file.path, size: file.size, block, phase: this.phase, t: this.time() });
      }
    }
  }

  /** The game wrote this line to its log */
  log(line) {
    this.enter(assetPhaseOfLog(String(line)), String(line).slice(0, 120));
  }

  /** The trace as an object for a JSON file: the scenario, the build, the phase starts and the reads */
  trace({ scenario = '', build = '' } = {}) {
    return {
      format: k_AssetTraceFormat,
      scenario,
      build,
      block_size: k_AssetTraceBlockSize,
      phases: k_AssetPhases.slice(0, k_AssetTracePhases),
      marks: this.marks,
      reads: this.reads,
    };
  }
}

if (typeof module !== 'undefined') {
  module.exports = {
    AssetTraceRecorder, assetPhaseOfRead, assetPhaseOfLog, k_AssetPhases, k_AssetTracePhases, k_AssetTraceBlockSize,
  };
}
