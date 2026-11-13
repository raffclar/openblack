// openblack web page: the end of a normal game run sends the browser to the homepage of the site.
//
// A crash or an error does not come here: the page keeps its crash message, with the buttons to copy the diagnostics,
// to load the page again and to close the message.
'use strict';

// The type of the message that a framed game gives to the page around it, when it cannot move the top window itself
const k_ExitMessageType = 'openblack-exit-to-home';

// The address of the homepage: the root of the same origin as the game page
function homepageAddress(location) {
  return `${location.origin}/`;
}

// True when the page can use the top window: the top window has the same origin as the game page. A page of another
// origin refuses the read of its location.
function topWindowIsOurs(win) {
  try {
    return win.top.location.origin === win.location.origin;
  } catch {
    return false;
  }
}

// Sends the browser to the homepage. A game that runs inside a frame moves the top window, when it has the same origin.
// If it cannot, it gives a message to the page around it, which can then move itself. Gives the way that it took.
function exitToHome(win) {
  const address = homepageAddress(win.location);
  if (win.top === win) {
    win.location.replace(address);
    return 'self';
  }
  if (topWindowIsOurs(win)) {
    win.top.location.replace(address);
    return 'top';
  }
  win.parent.postMessage({ type: k_ExitMessageType }, '*');
  return 'parent';
}

// True when the end of the program is a normal exit that sends the player to the homepage: the code is zero, and the
// page did not keep a crash.
function exitGoesHome(code, crashKept) {
  return code === 0 && !crashKept;
}

if (typeof module !== 'undefined') {
  module.exports = { exitToHome, exitGoesHome, homepageAddress, topWindowIsOurs, k_ExitMessageType };
}
