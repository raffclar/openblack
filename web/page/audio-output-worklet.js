// openblack web page: the sound output on the audio thread of the browser.
//
// A game thread mixes the sound into a ring of stereo frames in the shared memory of the program (see
// src/Audio/WebAudioOutput.cpp). The game thread counts the frames that it wrote, and this worklet counts the frames
// that it read. Both counts wrap round, and the ring has a power of two frames, so the place of a count stays correct.
// When the ring has too few frames, the worklet plays silence for the rest of the block and counts the block.
'use strict';

class OpenblackOutput extends AudioWorkletProcessor {
  constructor(options) {
    super();
    const { memory, samples, frames, channels, counters } = options.processorOptions;
    this.frames = frames;
    this.channels = channels;
    this.samples = new Float32Array(memory, samples, frames * channels);
    // The frames written, the frames read and the blocks with too few frames
    this.counters = new Int32Array(memory, counters, 3);
    this.started = false;
  }

  process(inputs, outputs) {
    const output = outputs[0];
    const length = output[0].length;
    const written = Atomics.load(this.counters, 0);
    const read = Atomics.load(this.counters, 1);
    const count = Math.min((written - read) >>> 0, length);
    const used = Math.min(output.length, this.channels);
    for (let channel = 0; channel < used; channel += 1) {
      const target = output[channel];
      for (let frame = 0; frame < count; frame += 1) {
        target[frame] = this.samples[(((read + frame) >>> 0) % this.frames) * this.channels + channel];
      }
      target.fill(0, count);
    }
    Atomics.store(this.counters, 1, (read + count) | 0);
    if (count < length && this.started) {
      Atomics.add(this.counters, 2, 1);
    }
    this.started = this.started || count > 0;
    return true;
  }
}

registerProcessor('openblack-output', OpenblackOutput);
