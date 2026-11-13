// openblack web build: code that the build puts after the game's own JavaScript.
//
// On the page's main thread, it gives the game folder from the page (Module.openblackWeb) to each worker thread of the
// game: to the workers that are there now, and to each worker that starts later, before the worker loads the game.
// The game threads also get the requests array, where the page's Join Online Game button asks for the multiplayer front
// end, and the count of the page's display frames, which starts each frame of the game thread. It also shows each frame
// that the game thread sends as an image, and it changes between the image and the game canvas when the game thread
// asks (see the frame present code of the web platform). It also writes the diagnostic lines of the game threads to the
// game log (see pre.js).
//
// When the game thread asks for a lobby session from the sign-in of the site, this code asks the page of the site around
// the game page (the site's /play/ page), and gives the answer to that thread only. Only the parent page of the same
// origin can answer. With no such page, or with no answer in 15 seconds, the answer is status 0 and the game shows its
// login box. The answer goes only into messages.
if (typeof WorkerGlobalScope === 'undefined' || !(self instanceof WorkerGlobalScope)) {
  const openblackWeb = Module['openblackWeb'];
  globalThis.openblackWeb = openblackWeb;
  const siteAsks = new Map();
  let nextSiteAsk = 0;
  const answerWorker = (entry, status, body) => {
    entry.worker.postMessage({ openblackSiteSession: { id: entry.id, status, body } });
  };
  const askSite = (worker, id) => {
    const entry = { worker, id };
    if (window.parent === window) {
      answerWorker(entry, 0, '');
      return;
    }
    const ask = ++nextSiteAsk;
    entry.timer = setTimeout(() => {
      if (siteAsks.delete(ask)) answerWorker(entry, 0, '');
    }, 15000);
    siteAsks.set(ask, entry);
    window.parent.postMessage({ type: 'bw-game-session', id: ask }, window.location.origin);
  };
  window.addEventListener('message', (event) => {
    if (event.source !== window.parent || event.origin !== window.location.origin) return;
    const reply = event.data;
    if (!reply || reply.type !== 'bw-game-session-reply') return;
    const entry = siteAsks.get(reply.id);
    if (!entry) return;
    siteAsks.delete(reply.id);
    clearTimeout(entry.timer);
    answerWorker(entry, reply.status | 0, typeof reply.body === 'string' ? reply.body : '');
  });
  const listened = new WeakSet();
  const show = (event, worker) => {
    if (event.data && Number.isInteger(event.data.openblackSiteSessionRequest)) {
      askSite(worker, event.data.openblackSiteSessionRequest);
    } else if (event.data && typeof event.data.openblackLog === 'string' && globalThis.openblackPageLog) {
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
        gameFrames: openblackWeb.gameFrames,
      },
    });
    if (!listened.has(worker)) {
      listened.add(worker);
      worker.addEventListener('message', (event) => show(event, worker));
    }
  };
  PThread.unusedWorkers.forEach(give);
  Object.values(PThread.pthreads).forEach(give);
  const loadWorker = PThread.loadWasmModuleToWorker;
  PThread.loadWasmModuleToWorker = (worker) => {
    give(worker);
    return loadWorker(worker);
  };
}
