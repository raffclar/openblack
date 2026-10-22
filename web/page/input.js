// openblack web build: the mouse and keyboard of the page. The build puts this code after the game's own JavaScript.
//
// The game moves its own pointer, because a web page cannot move the pointer of the system. The page locks the pointer
// to the game canvas, and the game gets only the movements of the mouse:
// - A click on the game canvas takes the pointer, from the time that the game window listens to the lock.
// - The browser frees the pointer on Escape. Thus the first Escape frees the pointer, and the second Escape goes to the
//   game. In full screen with a keyboard lock, the game gets the first Escape and asks the page to free the pointer.
// - Where the browser has it (Chromium), the lock asks for the raw movements of the mouse, without the pointer speed
//   and the acceleration of the system.
//
// The browser lets only the main thread lock the pointer and show the page on the full screen. The game thread asks
// the page through globalThis.openblackInput (see src/Web/WebPointer.cpp).
if (typeof WorkerGlobalScope === 'undefined' || !(self instanceof WorkerGlobalScope)) {
  const canvas = Module['canvas'];
  const game = canvas.parentElement;
  const say = (text) => (Module['print'] || console.log)(`Input: ${text}`);

  // Tell the game thread when the lock starts or stops. The call runs on this thread and only writes a value. Until
  // the game code is ready, the page tries again a short time later.
  const tellGame = () => {
    const locked = document.pointerLockElement === canvas;
    const changed = typeof _openblack_web_pointer_lock_changed === 'function'
      ? _openblack_web_pointer_lock_changed : Module['_openblack_web_pointer_lock_changed'];
    try {
      changed(locked ? 1 : 0, lockClick ? lockClick.x : -1, lockClick ? lockClick.y : -1);
    } catch {
      setTimeout(tellGame, 100);
    }
  };

  let rawMovement = false;
  // The place of the click that took the lock, in the pixels of the game window
  let lockClick = null;
  // The game window must listen to the pointer lock before the lock starts, else the window does not know of the lock
  let lockAllowed = false;
  const lockPointer = () => {
    if (!lockAllowed || document.pointerLockElement === canvas) {
      return;
    }
    const plain = () => {
      rawMovement = false;
      const request = canvas.requestPointerLock();
      if (request && request.catch) {
        request.catch((error) => say(`the browser did not lock the pointer: ${error.name}`));
      }
    };
    let request;
    try {
      rawMovement = true;
      request = canvas.requestPointerLock({ unadjustedMovement: true });
    } catch {
      plain();
      return;
    }
    if (request && request.catch) {
      request.catch((error) => {
        if (error.name === 'NotSupportedError') {
          plain();
        } else {
          say(`the browser did not lock the pointer: ${error.name}`);
        }
      });
    } else {
      // An older browser gives no promise and takes no options
      rawMovement = false;
    }
  };

  document.addEventListener('pointerlockchange', () => {
    const locked = document.pointerLockElement === canvas;
    tellGame();
    say(locked ? `the game has the pointer (${rawMovement ? 'raw' : 'system'} movements)` : 'the pointer is free');
  });
  document.addEventListener('pointerlockerror', () => say('the browser did not lock the pointer'));

  // A click on the game takes the pointer. The click goes to the game too, as on the desktop.
  canvas.addEventListener('mousedown', (event) => {
    if (document.pointerLockElement !== canvas) {
      const box = canvas.getBoundingClientRect();
      lockClick = { x: Math.max(0, Math.round(event.clientX - box.left)), y: Math.max(0, Math.round(event.clientY - box.top)) };
      lockPointer();
    }
  }, true);
  // The wheel turns no page zoom and no page scroll while it is over the game
  canvas.addEventListener('wheel', (event) => event.preventDefault(), { passive: false });

  // Full screen. The game asks for it after a key press. The browser lets the page start the full screen only soon
  // after a press or a click, thus a request that fails waits for the next press or click.
  let fullscreenWanted = false;
  let windowSize = null;
  const startFullscreen = () => {
    fullscreenWanted = false;
    if (document.fullscreenElement === game) {
      return;
    }
    game.requestFullscreen({ navigationUI: 'hide' }).catch(() => {
      fullscreenWanted = true;
      say('full screen starts at the next key press or click');
    });
  };
  const requestFullscreen = (fullscreen) => {
    if (fullscreen) {
      startFullscreen();
    } else if (document.fullscreenElement) {
      document.exitFullscreen().catch(() => {});
    }
  };
  const retryFullscreen = () => {
    if (fullscreenWanted) {
      startFullscreen();
    }
  };
  window.addEventListener('keyup', retryFullscreen, true);
  window.addEventListener('mouseup', retryFullscreen, true);

  // In full screen the game canvas fills the screen, and SDL takes the new size from the canvas at the resize event. In
  // Chromium the keyboard lock gives all the keys to the game: the browser then frees the pointer only when the player
  // holds Escape down, and the game gets a short press.
  document.addEventListener('fullscreenchange', () => {
    if (document.fullscreenElement === game) {
      windowSize = { width: canvas.style.width, height: canvas.style.height };
      canvas.style.width = `${window.innerWidth}px`;
      canvas.style.height = `${window.innerHeight}px`;
      if (navigator.keyboard && navigator.keyboard.lock) {
        navigator.keyboard.lock().then(() => say('keyboard lock on'), (error) => say(`no keyboard lock: ${error.name}`));
      }
      say(`full screen ${window.innerWidth}x${window.innerHeight}`);
    } else {
      if (windowSize) {
        canvas.style.width = windowSize.width;
        canvas.style.height = windowSize.height;
        windowSize = null;
      }
      if (navigator.keyboard && navigator.keyboard.unlock) {
        navigator.keyboard.unlock();
      }
      say('full screen off');
    }
    window.dispatchEvent(new Event('resize'));
  });

  // While the game has the pointer, the keys of the game do not do the browser's actions, such as Tab to the next
  // button or Alt to the menu. A key that writes a character keeps its action, because the game reads the text of
  // these keys for its text boxes. The browser does its own commands, such as Ctrl+W, before the page can stop them.
  window.addEventListener('keydown', (event) => {
    if (document.pointerLockElement !== canvas) {
      return;
    }
    const writes = event.key.length === 1 && !event.ctrlKey && !event.altKey && !event.metaKey;
    const developerTools = event.key === 'F12' || (event.ctrlKey && event.shiftKey && /^[IJC]$/i.test(event.key));
    if (!writes && !developerTools) {
      event.preventDefault();
    }
  }, true);

  globalThis.openblackInput = {
    allowLock: () => {
      lockAllowed = true;
      say('a click on the game takes the pointer');
    },
    freePointer: () => {
      if (document.pointerLockElement) {
        document.exitPointerLock();
      }
    },
    requestFullscreen,
  };
  tellGame();
}
