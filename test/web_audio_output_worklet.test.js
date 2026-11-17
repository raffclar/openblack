// Test of the sound output worklet of the web page (web/page/audio-output-worklet.js). Run with node.
'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

let count = 0;
const check = (name, body) => {
  body();
  count += 1;
  console.log(`ok ${count} ${name}`);
};

// Loads the worklet with the two globals of an audio worklet scope, and gives its processor class
const loadProcessor = () => {
  const registered = {};
  const scope = {
    AudioWorkletProcessor: class {},
    registerProcessor: (name, processor) => {
      registered[name] = processor;
    },
    Atomics, Float32Array, Int32Array, Math,
  };
  const file = path.join(__dirname, '..', 'web', 'page', 'audio-output-worklet.js');
  vm.runInNewContext(fs.readFileSync(file, 'utf8'), scope);
  return registered['openblack-output'];
};

const k_Frames = 8;
const k_Channels = 2;
const k_Samples = 64;
const k_Counters = 0;

// A memory with the counters at the start and a ring of k_Frames stereo frames after them
const makeOutput = () => {
  const Processor = loadProcessor();
  const memory = new SharedArrayBuffer(k_Samples + k_Frames * k_Channels * 4);
  const processor = new Processor({
    processorOptions: { memory, samples: k_Samples, frames: k_Frames, channels: k_Channels, counters: k_Counters },
  });
  const counters = new Int32Array(memory, k_Counters, 3);
  const samples = new Float32Array(memory, k_Samples, k_Frames * k_Channels);
  return { processor, counters, samples };
};

// One block of the browser with this many frames
const block = (length) => [[new Float32Array(length).fill(9), new Float32Array(length).fill(9)]];

check('the worklet registers its processor', () => {
  assert.equal(typeof loadProcessor(), 'function');
});

check('the worklet plays the frames in the ring for each channel and counts them as read', () => {
  const { processor, counters, samples } = makeOutput();
  for (let i = 0; i < k_Frames * k_Channels; i += 1) {
    samples[i] = i;
  }
  counters[0] = 4;
  const outputs = block(4);
  assert.equal(processor.process([], outputs), true);
  assert.deepEqual([...outputs[0][0]], [0, 2, 4, 6]);
  assert.deepEqual([...outputs[0][1]], [1, 3, 5, 7]);
  assert.equal(counters[1], 4);
  assert.equal(counters[2], 0);
});

check('the frames wrap round at the end of the ring', () => {
  const { processor, counters, samples } = makeOutput();
  for (let i = 0; i < k_Frames * k_Channels; i += 1) {
    samples[i] = i;
  }
  counters[1] = 6;
  counters[0] = 10;
  const outputs = block(4);
  processor.process([], outputs);
  assert.deepEqual([...outputs[0][0]], [12, 14, 0, 2]);
  assert.equal(counters[1], 10);
});

check('the counts can wrap round past the largest integer', () => {
  const { processor, counters, samples } = makeOutput();
  samples.fill(1);
  counters[1] = 0x7ffffffe;
  counters[0] = (0x7ffffffe + 4) | 0;
  const outputs = block(4);
  processor.process([], outputs);
  assert.deepEqual([...outputs[0][0]], [1, 1, 1, 1]);
  assert.equal(counters[1], (0x7ffffffe + 4) | 0);
});

check('a block with too few frames ends in silence and counts once the sound started', () => {
  const { processor, counters, samples } = makeOutput();
  samples.fill(1);
  // Before the first sound, an empty ring is no fault
  processor.process([], block(4));
  assert.equal(counters[2], 0);
  counters[0] = 4;
  processor.process([], block(4));
  assert.equal(counters[2], 0);
  counters[0] = 6;
  const outputs = block(4);
  processor.process([], outputs);
  assert.deepEqual([...outputs[0][0]], [1, 1, 0, 0]);
  assert.equal(counters[2], 1);
  processor.process([], block(4));
  assert.equal(counters[2], 2);
});
