// Test of the search for the game pointer in a screenshot (web/scripts/png.mjs), which the web input checks use. Run
// with node.
import assert from 'node:assert/strict';

import { findPointer } from '../web/scripts/png.mjs';

let count = 0;
const check = (name, body) => {
  body();
  count += 1;
  console.log(`ok ${count} ${name}`);
};

const k_Teal = [40, 180, 180];
const k_Grey = [100, 100, 100];

// A grey picture of 1280x1024 with teal rectangles [left, top, width, height]
const picture = (rectangles) => {
  const width = 1280;
  const height = 1024;
  const channels = 3;
  const pixels = new Uint8Array(width * height * channels);
  for (let i = 0; i < width * height; ++i) {
    pixels.set(k_Grey, i * channels);
  }
  for (const [left, top, w, h] of rectangles) {
    for (let y = top; y < top + h; ++y) {
      for (let x = left; x < left + w; ++x) {
        pixels.set(k_Teal, (y * width + x) * channels);
      }
    }
  }
  return { width, height, channels, pixels };
};

// The pointer picture at the OK button of the name box, about 160 pixels
const k_Pointer = [320, 710, 16, 10];

check('a picture with no teal has no pointer', () => {
  assert.deepEqual(findPointer(picture([])), { x: -1, y: -1, count: 0 });
});

check('the pointer alone is found at its middle', () => {
  assert.deepEqual(findPointer(picture([k_Pointer])), { x: 328, y: 715, count: 160 });
});

check('teal buttons in the strip above the canvas do not move the pointer', () => {
  // The strip of the page above the canvas has buttons with teal pixels (55 pixels in a real screenshot)
  const seen = findPointer(picture([k_Pointer, [1050, 5, 11, 5]]));
  assert.deepEqual(seen, { x: 328, y: 715, count: 160 });
});

check('the box keeps the search out of the strip above the canvas', () => {
  // Only the strip has teal: with the box of the canvas, the picture has no pointer
  assert.equal(findPointer(picture([[1050, 5, 11, 5]]), [0, 22, 1280, 1024]).count, 0);
});

check('a far patch of teal in the background does not move the pointer', () => {
  const seen = findPointer(picture([k_Pointer, [1100, 300, 8, 8]]), [0, 22, 1280, 1024]);
  assert.deepEqual(seen, { x: 328, y: 715, count: 160 });
});
