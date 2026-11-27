// openblack web build: code that the build puts before the game's own JavaScript. It runs in each thread of the game.
//
// The page posts the user's game folder to each worker thread before the worker starts (see post.js). The worker keeps
// it, and the game's file system reads slices of its files there. The folder is a list of local File objects: the
// browser copies the handles, not the contents, and sends nothing to a server. When the site sends the game files, the
// list has an empty place for each file that the page does not have yet.
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
  // A file that the site streams is not in the folder until the game first reads it. The thread asks the page for the
  // bytes that it reads and waits for them (see src/Web/UserFolderBackend.cpp). The page gets the whole file, or only
  // the blocks of a large file that has the bytes, and gives each file and block that it gets to every thread.
  const fileWaits = new Map();
  const has = (folder, file, offset, length) => {
    if (folder.files[file]) {
      return true;
    }
    const blocks = folder.blocks && folder.blocks[file];
    if (!blocks || !(folder.blockSize > 0) || !(length > 0)) {
      return false;
    }
    const first = Math.floor(offset / folder.blockSize);
    const last = Math.min(blocks.length - 1, Math.floor((offset + length - 1) / folder.blockSize));
    for (let block = first; block <= last; block += 1) {
      if (!blocks[block]) {
        return false;
      }
    }
    return true;
  };
  globalThis.openblackObtainGameFile = (file, offset, length) => {
    const folder = globalThis.openblackWeb && globalThis.openblackWeb.folder;
    if (!folder || !Number.isInteger(file) || file < 0 || file >= folder.files.length) {
      return Promise.resolve(false);
    }
    if (has(folder, file, offset, length)) {
      return Promise.resolve(true);
    }
    return new Promise((resolve) => {
      const waits = fileWaits.get(file) || [];
      waits.push({ offset, length, resolve });
      fileWaits.set(file, waits);
      postMessage({ openblackGameFileWanted: { file, offset, length } });
    });
  };
  // A file or a block arrived, or the page cannot get the file: each wait for this file that has its bytes now ends
  const answer = (folder, file, failed) => {
    const waits = fileWaits.get(file) || [];
    const open = waits.filter((wait) => {
      if (failed || has(folder, file, wait.offset, wait.length)) {
        wait.resolve(!failed);
        return false;
      }
      return true;
    });
    if (open.length > 0) {
      fileWaits.set(file, open);
    } else {
      fileWaits.delete(file);
    }
  };
  // When the page records the trace of the game files (launcher-asset-trace.js), the thread tells the page about the
  // first read of each block of each file. The page writes the time and the phase of the game for it.
  const tracedBlocks = new Set();
  const noteGameRead = (file, offset, length) => {
    const size = 1024 * 1024;
    const first = Math.floor(offset / size);
    const last = Math.floor((offset + Math.max(length, 1) - 1) / size);
    for (let block = first; block <= last; block += 1) {
      const key = `${file}:${block}`;
      if (!tracedBlocks.has(key)) {
        tracedBlocks.add(key);
        postMessage({ openblackGameRead: { file, offset: block * size, length: size } });
      }
    }
  };
  self.addEventListener('message', (event) => {
    if (event.data && event.data.openblackWeb) {
      globalThis.openblackWeb = event.data.openblackWeb;
      globalThis.openblackNoteGameRead = event.data.openblackWeb.recordReads ? noteGameRead : undefined;
    } else if (event.data && event.data.openblackGameFile) {
      const given = event.data.openblackGameFile;
      const folder = globalThis.openblackWeb && globalThis.openblackWeb.folder;
      if (folder && Number.isInteger(given.file) && given.file >= 0 && given.file < folder.files.length) {
        if (given.blob && Number.isInteger(given.block)) {
          folder.blocks[given.file][given.block] = given.blob;
        } else if (given.blob) {
          folder.files[given.file] = given.blob;
          // The page removes the blocks of a file that it keeps whole. The thread reads the whole file from now on.
          if (folder.blocks && folder.blocks[given.file]) {
            folder.blocks[given.file].fill(null);
          }
        }
        answer(folder, given.file, !given.blob);
      }
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
