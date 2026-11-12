// openblack web build: code that the build puts before the game's own JavaScript. It runs in each thread of the game.
//
// The page posts the user's game folder to each worker thread before the worker starts (see post.js). The worker keeps
// it, and the game's file system reads slices of its files there. The folder is a list of local File objects: the
// browser copies the handles, not the contents, and sends nothing to a server.
//
// The page also sends the answer to a question of the game thread for a lobby session from the sign-in of the site. The
// worker keeps the answer under its number until the game takes it (see src/Web/PageSiteSession.cpp).
if (typeof WorkerGlobalScope !== 'undefined' && self instanceof WorkerGlobalScope) {
  self.addEventListener('message', (event) => {
    if (event.data && event.data.openblackWeb) {
      globalThis.openblackWeb = event.data.openblackWeb;
    } else if (event.data && event.data.openblackSiteSession) {
      const reply = event.data.openblackSiteSession;
      if (Number.isInteger(reply.id)) {
        globalThis.openblackSiteSessionReplies ||= new Map();
        globalThis.openblackSiteSessionReplies.set(reply.id, { status: reply.status | 0, body: String(reply.body || '') });
      }
    }
  });
}
