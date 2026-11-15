// openblack web page: the mouse buttons that the game holds, and the releases that the browser did not give.
//
// The game gets a press and a release of each mouse button from the events of the page. The browser does not always
// give the release: the player can let go of the button in another window after Alt+Tab, or while the page is hidden.
// Then the game holds the button, and the drag of the land or of the hand does not stop. This file keeps the buttons
// that the game holds, and tells which buttons must get a release from the page.
'use strict';

// The game reads the left, the middle and the right button. The other buttons do nothing in the game.
const k_GameButtons = [0, 1, 2];

// The bit of each button in the "buttons" value of a mouse event. The middle and the right button have their bits in
// the other order.
const k_ButtonBits = [1, 4, 2];

// The buttons that the game holds, from the presses and the releases that it got
class HeldButtons {
  constructor() {
    this.held = new Set();
  }

  // A press of a button. Gives true when the game reads this button.
  press(button) {
    if (!k_GameButtons.includes(button)) {
      return false;
    }
    this.held.add(button);
    return true;
  }

  // A release of a button
  release(button) {
    this.held.delete(button);
  }

  // The buttons that the game holds, in the order of their numbers
  list() {
    return [...this.held].sort((a, b) => a - b);
  }

  // The held buttons that the "buttons" value of a mouse event shows as up. Their releases did not come to the page.
  // The value does not change the list: the page gives the releases, and these releases remove the buttons.
  lostReleases(buttons) {
    return this.list().filter((button) => (buttons & k_ButtonBits[button]) === 0);
  }

  // All the held buttons. The page gives their releases when it loses the focus, because the browser then gives the
  // releases to another window.
  allReleases() {
    return this.list();
  }
}

// The "buttons" value of a release of this button, when the other held buttons stay down
function buttonsAfterRelease(held, button) {
  return held.filter((other) => other !== button).reduce((bits, other) => bits | k_ButtonBits[other], 0);
}

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { HeldButtons, buttonsAfterRelease, k_ButtonBits, k_GameButtons };
} else {
  globalThis.openblackButtons = { HeldButtons, buttonsAfterRelease };
}
