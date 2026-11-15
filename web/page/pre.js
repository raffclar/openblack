// openblack web build: code that the build puts before the game's own JavaScript. It runs in each thread of the game.
//
// The page posts the user's game folder to each worker thread before the worker starts (see post.js). The worker keeps
// it, and the game's file system reads slices of its files there. The folder is a list of local File objects: the
// browser copies the handles, not the contents, and sends nothing to a server.
//
// The page also sends the answer to a question of the game thread for a lobby session from the sign-in of the site. The
// worker keeps the answer under its number until the game takes it (see src/Web/PageSiteSession.cpp).
if (typeof WorkerGlobalScope !== 'undefined' && self instanceof WorkerGlobalScope) {
  // An error that no code of the thread catches, such as a trap of the WebAssembly code, comes to the page with its
  // message only. The thread thus also gives the page the name and the stack of the error, for the error reports.
  self.addEventListener('error', (event) => {
    const error = event.error;
    postMessage({
      openblackThreadError: {
        name: String((error && error.name) || 'Error'),
        message: String((error && error.message) || event.message || ''),
        stack: String((error && error.stack) || ''),
      },
    });
  });
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

// The renderer of the game can lose its device: the GPU stops, the driver starts again, or the browser takes the GPU
// back. The page writes each loss to the game log, for the diagnostics. The game makes its device on a game thread,
// thus this code watches the devices and the canvases of each thread.
(() => {
  const report = (text) => {
    if (typeof WorkerGlobalScope !== 'undefined' && self instanceof WorkerGlobalScope) {
      postMessage({ openblackLog: text });
    } else if (globalThis.openblackPageLog) {
      globalThis.openblackPageLog(text);
    }
  };
  if (typeof GPUAdapter !== 'undefined' && GPUAdapter.prototype.requestDevice) {
    const requestDevice = GPUAdapter.prototype.requestDevice;
    GPUAdapter.prototype.requestDevice = async function (...args) {
      const device = await requestDevice.apply(this, args);
      report('The game has a WebGPU device');
      device.lost.then((info) => report(`The WebGPU device is lost: ${info.reason}: ${info.message}`));
      return device;
    };
  }
  const watchContext = (prototype) => {
    if (!prototype || !prototype.getContext) {
      return;
    }
    const getContext = prototype.getContext;
    prototype.getContext = function (type, ...args) {
      if (!this.openblackWatched && (type === 'webgl2' || type === 'webgl')) {
        this.openblackWatched = true;
        this.addEventListener('webglcontextlost', () => report('The WebGL context is lost'));
        this.addEventListener('webglcontextrestored', () => report('The WebGL context is back'));
      }
      return getContext.call(this, type, ...args);
    };
  };
  if (typeof OffscreenCanvas !== 'undefined') {
    watchContext(OffscreenCanvas.prototype);
  }
  if (typeof HTMLCanvasElement !== 'undefined') {
    watchContext(HTMLCanvasElement.prototype);
  }
})();
