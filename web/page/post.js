// openblack web build: code that the build puts after the game's own JavaScript.
//
// On the page's main thread, it gives the game folder from the page (Module.openblackWeb) to each worker thread of the
// game: to the workers that are there now, and to each worker that starts later, before the worker loads the game.
// The game threads also get the requests array, where the page's Join Online Game button asks for the multiplayer front
// end, and the count of the page's display frames, which starts each frame of the game thread. It also shows each frame
// that the game thread sends as an image, and it changes between the image and the game canvas when the game thread
// asks (see the frame present code of the web platform). It also writes the diagnostic lines of the game threads to the
// game log (see pre.js), and it gives the stack of an error of a game thread to the error reports. When a game thread
// reads a game file that the site streams and that the page does not have yet, this code asks the page for the file.
//
// When the game thread asks for a lobby session from the sign-in of the site, this code asks the page of the site around
// the game page (the site's /play/ page), and gives the answer to that thread only. Only the parent page of the same
// origin can answer. With no such page, or with no answer in 15 seconds, the answer is status 0 and the game shows its
// login box. The answer goes only into messages.
//
// When a game thread wrote files of the profiles, it tells the page, which sends the saved games and creatures to the
// site (launcher-cloud.js).
//
// It also gives the error reports the WebAssembly module of the game, which holds the build id of the build.
//
// It also makes the audio context and the audio worklet of the sound output (see audio-output-worklet.js).
if (typeof WorkerGlobalScope === 'undefined' || !(self instanceof WorkerGlobalScope)) {
  const openblackWeb = Module['openblackWeb'];
  globalThis.openblackWeb = openblackWeb;
  // The error reports read the build id of the WebAssembly module of the game, so that Sentry can find the debug file
  // of this build. The module is there after the page compiled the game.
  globalThis.openblackWasmModule = () => (typeof wasmModule !== 'undefined' ? wasmModule : undefined);
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
  // The page gives each game file and each block of a file that it gets from the site to every worker, also to a
  // worker that did not ask for it, so that no other thread asks for it again. A new worker gets the folder with the
  // files and the blocks that the page has.
  const workers = new Set();
  const folder = openblackWeb.folder;
  const message = (file, block, blob) => ({ openblackGameFile: block === null ? { file, blob } : { file, block, blob } });
  openblackWeb.shareGameFile = (file, block, blob) => {
    if (!Number.isInteger(file) || file < 0 || file >= folder.files.length || !blob) {
      return;
    }
    if (block === null) {
      folder.files[file] = blob;
    } else if (folder.blocks && folder.blocks[file] && block >= 0 && block < folder.blocks[file].length) {
      folder.blocks[file][block] = blob;
    } else {
      return;
    }
    workers.forEach((worker) => worker.postMessage(message(file, block, blob)));
  };
  // A game thread read bytes of a game file that the page does not have yet. The page gets them from the site (see
  // launcher-assets.js), and gives them to every worker. The worker that asked then also gets what the page has of the
  // file, in case the page had it already. When the page cannot get the bytes, only the worker that asked gets the
  // answer, and the read of the game fails.
  const wantGameFile = (worker, { file, offset, length }) => {
    const refuse = (reason) => {
      if (globalThis.openblackPageLog) {
        globalThis.openblackPageLog(`The page cannot get the game file ${file} from the site: ${reason}`);
      }
      worker.postMessage(message(file, null, null));
    };
    if (!Number.isInteger(file) || file < 0 || file >= folder.files.length || !openblackWeb.obtainGameFile) {
      refuse('the page does not stream the game files');
      return;
    }
    openblackWeb.obtainGameFile(file, offset, length).then(() => {
      if (folder.files[file]) {
        worker.postMessage(message(file, null, folder.files[file]));
        return;
      }
      let given = 0;
      (folder.blocks && folder.blocks[file] || []).forEach((blob, block) => {
        if (blob) {
          worker.postMessage(message(file, block, blob));
          given += 1;
        }
      });
      // The thread must not wait for ever: with no bytes to give, the read of the game fails
      if (given === 0) {
        refuse('the page got the file, but has no bytes of it to give');
      }
    }, (error) => refuse(error.message || error));
  };
  const listened = new WeakSet();
  const show = (event, worker) => {
    if (event.data && Number.isInteger(event.data.openblackSiteSessionRequest)) {
      askSite(worker, event.data.openblackSiteSessionRequest);
    } else if (event.data && event.data.openblackGameFileWanted) {
      wantGameFile(worker, event.data.openblackGameFileWanted);
    } else if (event.data && event.data.openblackProfileWrite) {
      // The game wrote files of its profiles: the page sends the saved games and creatures to the site
      if (globalThis.openblackProfileWrite) {
        globalThis.openblackProfileWrite();
      }
    } else if (event.data && typeof event.data.openblackLog === 'string' && globalThis.openblackPageLog) {
      // A line of a game thread for the diagnostics, such as a lost device of the renderer
      globalThis.openblackPageLog(event.data.openblackLog);
    } else if (event.data && event.data.openblackThreadError && globalThis.openblackErrorReports) {
      // The name and the stack of an error of a game thread, which comes to the page next as an error event
      globalThis.openblackErrorReports.threadError(event.data.openblackThreadError);
    } else if (event.data && event.data.openblackFrame && openblackWeb.showFrame) {
      openblackWeb.showFrame(event.data.openblackFrame);
    } else if (event.data && event.data.openblackPresent && openblackWeb.setPresent) {
      openblackWeb.setPresent(event.data.openblackPresent);
    }
  };
  const give = (worker) => {
    workers.add(worker);
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

  // The game must not run more threads than the workers that the page makes at the start (PTHREAD_POOL_SIZE). A
  // thread past these workers gets a new worker, and the new worker loads the program from the site in the middle of
  // play. The game log tells each new highest count of the threads that run at one time, and each new worker.
  const poolWorkers = PThread.unusedWorkers.length + Object.keys(PThread.pthreads).length;
  let mostThreads = 0;
  const getNewWorker = PThread.getNewWorker;
  PThread.getNewWorker = () => {
    const pastPool = PThread.unusedWorkers.length === 0;
    const worker = getNewWorker();
    const threads = Object.keys(PThread.pthreads).length + 1;
    const pageLog = (text) => (globalThis.openblackPageLog ? globalThis.openblackPageLog(text) : console.log(text));
    if (threads > mostThreads) {
      mostThreads = threads;
      pageLog(`Game threads: ${threads} run at one time, the most so far (the page made ${poolWorkers} workers)`);
    }
    if (pastPool) {
      pageLog(`Game threads: a thread got a new worker past the ${poolWorkers} workers of the page`);
    }
    return worker;
  };

  // The sound output (see src/Audio/WebAudioOutput.cpp). A game thread mixes the sound into a ring in the memory of
  // the program, and an audio worklet plays the ring on the audio thread of the browser. The main thread only makes
  // the audio context and the worklet, so a busy main thread does not stop the sound. The context starts when the
  // player clicks or presses a key on the page, as the browser asks.
  const audio = { context: null, node: null, memory: null, counters: 0 };
  globalThis.openblackAudio = audio;
  Module['openblackAudioOpen'] = () => {
    if (!audio.context) {
      try {
        audio.context = new AudioContext({ latencyHint: 'interactive' });
      } catch (error) {
        console.error('openblack: the page cannot make an audio context:', error);
        return 0;
      }
      if (!audio.context.audioWorklet) {
        audio.context.close();
        audio.context = null;
        return 0;
      }
      const resume = () => {
        if (audio.context && audio.context.state === 'suspended') {
          audio.context.resume().catch(() => {});
        }
      };
      ['pointerdown', 'mousedown', 'keydown', 'touchend'].forEach((type) => {
        document.addEventListener(type, resume, { capture: true });
      });
      resume();
    }
    return audio.context.sampleRate | 0;
  };
  Module['openblackAudioConnect'] = (samples, frames, channels, counters) => {
    const context = audio.context;
    if (!context) {
      return;
    }
    audio.memory = wasmMemory.buffer;
    audio.counters = counters;
    context.audioWorklet.addModule('audio-output-worklet.js').then(() => {
      if (audio.context !== context) {
        return;
      }
      audio.node = new AudioWorkletNode(context, 'openblack-output', {
        numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [channels],
        processorOptions: { memory: audio.memory, samples, frames, channels, counters },
      });
      audio.node.connect(context.destination);
    }).catch((error) => console.error('openblack: the page cannot load the sound output:', error));
  };
  Module['openblackAudioClose'] = () => {
    if (audio.node) {
      audio.node.disconnect();
      audio.node = null;
    }
    if (audio.context) {
      audio.context.close().catch(() => {});
      audio.context = null;
    }
  };
}
