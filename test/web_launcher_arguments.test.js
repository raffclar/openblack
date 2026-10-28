// Test of the arguments that the web page gives the game (web/page/launcher-arguments.js). Run with node.
'use strict';

const assert = require('node:assert/strict');
const { playerArguments } = require('../web/page/launcher-arguments.js');

const query = (text) => new URLSearchParams(text);
let count = 0;
const check = (name, body) => {
  body();
  count += 1;
  console.log(`ok ${count} ${name}`);
};

check('no parameter gives no argument', () => {
  assert.deepEqual(playerArguments(query(''), false, false), []);
});
check('multiplayer reaches a shipping build', () => {
  assert.deepEqual(playerArguments(query('multiplayer'), false, false), ['-MULTIPLAYER']);
});
check('show-ping reaches a shipping build', () => {
  assert.deepEqual(playerArguments(query('show-ping'), false, false), ['--show-ping']);
});
check('both together', () => {
  assert.deepEqual(playerArguments(query('multiplayer&show-ping=1'), false, false), ['-MULTIPLAYER', '--show-ping']);
});
check('an off value turns the parameter off', () => {
  assert.deepEqual(playerArguments(query('multiplayer=false&show-ping=0'), false, false), []);
});
check('the Join Online Game button gives the multiplayer switch', () => {
  assert.deepEqual(playerArguments(query(''), false, true), ['-MULTIPLAYER']);
});
check('a shipping build drops args, even with a player parameter beside it', () => {
  assert.deepEqual(playerArguments(query('args=--testbed%20--frame-stats%20100&show-ping'), false, false),
    ['--show-ping']);
});
check('a parameter that is not in the list gives nothing', () => {
  assert.deepEqual(playerArguments(query('testbed&scenario=x&seed=1&-n=5'), false, false), []);
});
check('a developer build takes args and adds each switch one time', () => {
  assert.deepEqual(playerArguments(query('args=--testbed%20-multiplayer&multiplayer&show-ping'), true, false),
    ['--testbed', '-multiplayer', '--show-ping']);
});
console.log(`${count} checks passed`);
