// Test of the exit of the web page (web/page/launcher-exit.js). Run with node.
'use strict';

const assert = require('node:assert/strict');
const { exitToHome, exitGoesHome, homepageAddress, k_ExitMessageType } = require('../web/page/launcher-exit.js');

let count = 0;
const check = (name, body) => {
  body();
  count += 1;
  console.log(`ok ${count} ${name}`);
};

// A fake window. The replaced addresses and the posted messages go in the lists.
const fakeWindow = ({ origin = 'https://bw.zanidrak.com', framed = false, topOrigin = origin } = {}) => {
  const log = { replaced: [], topReplaced: [], posted: [] };
  const win = { location: { origin, replace: (address) => log.replaced.push(address) } };
  if (!framed) {
    win.top = win;
  } else {
    const top = {
      get location() {
        if (topOrigin !== origin) {
          throw new Error('SecurityError');
        }
        return { origin: topOrigin, replace: (address) => log.topReplaced.push(address) };
      },
    };
    win.top = top;
    win.parent = { postMessage: (message, target) => log.posted.push({ message, target }) };
  }
  return { win, log };
};

check('the homepage is the root of the same origin', () => {
  assert.equal(homepageAddress({ origin: 'https://bw.zanidrak.com' }), 'https://bw.zanidrak.com/');
});
check('a game page that is the top window moves itself', () => {
  const { win, log } = fakeWindow();
  assert.equal(exitToHome(win), 'self');
  assert.deepEqual(log.replaced, ['https://bw.zanidrak.com/']);
});
check('a framed game moves the top window of the same origin', () => {
  const { win, log } = fakeWindow({ framed: true });
  assert.equal(exitToHome(win), 'top');
  assert.deepEqual(log.topReplaced, ['https://bw.zanidrak.com/']);
  assert.deepEqual(log.replaced, []);
});
check('a framed game under another origin gives a message to the parent', () => {
  const { win, log } = fakeWindow({ framed: true, topOrigin: 'https://other.example' });
  assert.equal(exitToHome(win), 'parent');
  assert.deepEqual(log.posted, [{ message: { type: k_ExitMessageType }, target: '*' }]);
  assert.deepEqual(log.replaced, []);
});
check('a normal exit goes home', () => {
  assert.equal(exitGoesHome(0, false), true);
});
check('an exit with an error code stays', () => {
  assert.equal(exitGoesHome(1, false), false);
});
check('an exit after a kept crash stays', () => {
  assert.equal(exitGoesHome(0, true), false);
});
