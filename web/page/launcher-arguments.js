// openblack web page: the arguments that the page gives the game.
//
// Every build takes the page parameters of a player, which this file lists. A page parameter that is not in the list
// does nothing. The developer parameter "args" (more arguments for the game) is open only in a build with the developer
// tools, and never goes through this list.
'use strict';

// The page parameters of a player and the game argument that each one gives. A parameter with "off" as its value
// ("false", "0", "no" or "off") is not on.
const k_PlayerParameters = [
  // The multiplayer front end opens in place of the menu, as the -MULTIPLAYER shortcut of the desktop game does.
  { parameter: 'multiplayer', argument: '-MULTIPLAYER' },
  // The own ping shows in a network game.
  { parameter: 'show-ping', argument: '--show-ping' },
];

const k_OffValues = new Set(['false', '0', 'no', 'off']);

/** True when the page parameter is in the address and its value does not turn it off */
function parameterIsOn(parameters, name) {
  return parameters.has(name) && !k_OffValues.has((parameters.get(name) || '').toLowerCase());
}

/**
 * The arguments of the game that the address of the page asks for.
 * @param {URLSearchParams} parameters the page parameters
 * @param {boolean} developer true for a build with the developer tools
 * @param {boolean} online true when the player pushed the Join Online Game button
 * @returns {string[]} the game arguments, each one one time
 */
function playerArguments(parameters, developer, online) {
  const result = developer ? (parameters.get('args') || '').split(' ').filter(Boolean) : [];
  const has = (argument) => result.some((entry) => entry.toUpperCase() === argument.toUpperCase());
  for (const { parameter, argument } of k_PlayerParameters) {
    const wanted = parameter === 'multiplayer' ? online || parameterIsOn(parameters, parameter)
      : parameterIsOn(parameters, parameter);
    if (wanted && !has(argument)) {
      result.push(argument);
    }
  }
  return result;
}

if (typeof module !== 'undefined') {
  module.exports = { playerArguments, parameterIsOn, k_PlayerParameters };
}
