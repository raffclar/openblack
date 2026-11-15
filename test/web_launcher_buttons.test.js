// Test of the mouse buttons that the web game holds (web/page/launcher-buttons.js). Run with node.
'use strict';

const assert = require('node:assert/strict');
const { HeldButtons, buttonsAfterRelease, k_ButtonBits } = require('../web/page/launcher-buttons.js');

let count = 0;
const check = (name, body) => {
  body();
  count += 1;
  console.log(`ok ${count} ${name}`);
};

const k_Left = 0;
const k_Middle = 1;
const k_Right = 2;

check('the buttons bits are those of the mouse events', () => {
  assert.equal(k_ButtonBits[k_Left], 1);
  assert.equal(k_ButtonBits[k_Middle], 4);
  assert.equal(k_ButtonBits[k_Right], 2);
});

check('a press holds the button until its release', () => {
  const held = new HeldButtons();
  assert.equal(held.press(k_Left), true);
  assert.deepEqual(held.list(), [k_Left]);
  held.release(k_Left);
  assert.deepEqual(held.list(), []);
});

check('the game does not hold the back and forward buttons', () => {
  const held = new HeldButtons();
  assert.equal(held.press(3), false);
  assert.equal(held.press(4), false);
  assert.deepEqual(held.list(), []);
});

check('a movement with the held button down gives no release', () => {
  const held = new HeldButtons();
  held.press(k_Left);
  assert.deepEqual(held.lostReleases(1), []);
});

check('a movement with the held button up gives its release', () => {
  const held = new HeldButtons();
  held.press(k_Left);
  assert.deepEqual(held.lostReleases(0), [k_Left]);
  // The rule does not change the list: the release that the page gives removes the button
  assert.deepEqual(held.list(), [k_Left]);
});

check('a movement gives a release only for the buttons that show as up', () => {
  const held = new HeldButtons();
  held.press(k_Left);
  held.press(k_Right);
  assert.deepEqual(held.lostReleases(k_ButtonBits[k_Right]), [k_Left]);
  assert.deepEqual(held.lostReleases(k_ButtonBits[k_Left]), [k_Right]);
  assert.deepEqual(held.lostReleases(0), [k_Left, k_Right]);
});

check('the middle button has the bit of the middle button', () => {
  const held = new HeldButtons();
  held.press(k_Middle);
  assert.deepEqual(held.lostReleases(4), []);
  assert.deepEqual(held.lostReleases(2), [k_Middle]);
});

check('a lost focus gives the releases of all the held buttons', () => {
  const held = new HeldButtons();
  assert.deepEqual(held.allReleases(), []);
  held.press(k_Right);
  held.press(k_Left);
  assert.deepEqual(held.allReleases(), [k_Left, k_Right]);
});

check('a second press of a held button holds it one time', () => {
  const held = new HeldButtons();
  held.press(k_Left);
  held.press(k_Left);
  held.release(k_Left);
  assert.deepEqual(held.list(), []);
});

check('a release keeps the bits of the other held buttons', () => {
  assert.equal(buttonsAfterRelease([k_Left, k_Right], k_Left), 2);
  assert.equal(buttonsAfterRelease([k_Left, k_Middle, k_Right], k_Right), 5);
  assert.equal(buttonsAfterRelease([k_Left], k_Left), 0);
});

console.log(`1..${count}`);
