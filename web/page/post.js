// openblack web build: code that the build puts after the game's own JavaScript.
//
// On the page's main thread, it gives the game folder from the page (Module.openblackWeb) to each worker thread of the
// game: to the workers that are there now, and to each worker that starts later, before the worker loads the game.
// It also shows each frame that the game thread sends as an image, and it changes between the image and the game
// canvas when the game thread asks (see the frame present code of the web platform).
if (typeof WorkerGlobalScope === 'undefined' || !(self instanceof WorkerGlobalScope)) {
  const openblackWeb = Module['openblackWeb'];
  globalThis.openblackWeb = openblackWeb;
  const show = (event) => {
    if (event.data && event.data.openblackFrame && openblackWeb.showFrame) {
      openblackWeb.showFrame(event.data.openblackFrame);
    } else if (event.data && event.data.openblackPresent && openblackWeb.setPresent) {
      openblackWeb.setPresent(event.data.openblackPresent);
    }
  };
  const give = (worker) => {
    worker.postMessage({
      openblackWeb: { folder: openblackWeb.folder, storage: openblackWeb.storage, version: openblackWeb.version },
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
