// openblack web build: code that the build puts after the game's own JavaScript.
//
// On the page's main thread, it gives the game folder from the page (Module.openblackWeb) to each worker thread of the
// game: to the workers that are there now, and to each worker that starts later, before the worker loads the game.
// The game threads also get the requests array, where the page's Join Online Game button asks for the multiplayer front
// end, and the count of the page's display frames, which starts each frame of the game thread. It also shows each frame
// that the game thread sends as an image, and it changes between the image and the game canvas when the game thread
// asks (see the frame present code of the web platform). It also writes the diagnostic lines of the game threads to the
// game log (see pre.js).
if (typeof WorkerGlobalScope === 'undefined' || !(self instanceof WorkerGlobalScope)) {
  const openblackWeb = Module['openblackWeb'];
  globalThis.openblackWeb = openblackWeb;
  const show = (event) => {
    if (event.data && typeof event.data.openblackLog === 'string' && globalThis.openblackPageLog) {
      // A line of a game thread for the diagnostics, such as a lost device of the renderer
      globalThis.openblackPageLog(event.data.openblackLog);
    } else if (event.data && event.data.openblackFrame && openblackWeb.showFrame) {
      openblackWeb.showFrame(event.data.openblackFrame);
    } else if (event.data && event.data.openblackPresent && openblackWeb.setPresent) {
      openblackWeb.setPresent(event.data.openblackPresent);
    }
  };
  const give = (worker) => {
    worker.postMessage({
      openblackWeb: {
        folder: openblackWeb.folder, storage: openblackWeb.storage, version: openblackWeb.version,
        requests: openblackWeb.requests, displayFrames: openblackWeb.displayFrames,
      },
    });
    worker.addEventListener('message', show);
  };
  PThread.unusedWorkers.forEach(give);
  Object.values(PThread.pthreads).forEach(give);
  const loadWorker = PThread.loadWasmModuleToWorker;
  PThread.loadWasmModuleToWorker = (worker) => {
    give(worker);
    return loadWorker(worker);
  };
}
