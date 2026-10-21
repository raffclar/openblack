// openblack web build: code that the build puts before the game's own JavaScript. It runs in each thread of the game.
//
// The page posts the user's game folder to each worker thread before the worker starts (see post.js). The worker keeps
// it, and the game's file system reads slices of its files there. The folder is a list of local File objects: the
// browser copies the handles, not the contents, and sends nothing to a server.
if (typeof WorkerGlobalScope !== 'undefined' && self instanceof WorkerGlobalScope) {
  self.addEventListener('message', (event) => {
    if (event.data && event.data.openblackWeb) {
      globalThis.openblackWeb = event.data.openblackWeb;
    }
  });
}
