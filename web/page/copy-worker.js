// openblack web page: copies the game data into the private storage of the site (OPFS) in this browser. The copy
// never leaves this computer. Only a worker can write a file of this storage in place in all browsers.
'use strict';

const k_Chunk = 8 * 1024 * 1024;

self.onmessage = async (event) => {
  const { entries, folder, marker } = event.data;
  try {
    const root = await navigator.storage.getDirectory();
    await root.removeEntry(folder, { recursive: true }).catch(() => {});
    const top = await root.getDirectoryHandle(folder, { create: true });
    const reader = new FileReaderSync();
    let bytes = 0;
    for (const { path, file } of entries) {
      const parts = path.split('/');
      let parent = top;
      for (const part of parts.slice(0, -1)) {
        parent = await parent.getDirectoryHandle(part, { create: true });
      }
      const handle = await parent.getFileHandle(parts.at(-1), { create: true });
      const access = await handle.createSyncAccessHandle();
      try {
        access.truncate(0);
        for (let at = 0; at < file.size; at += k_Chunk) {
          const chunk = reader.readAsArrayBuffer(file.slice(at, at + k_Chunk));
          access.write(new Uint8Array(chunk), { at });
        }
        access.flush();
      } finally {
        access.close();
      }
      bytes += file.size;
      postMessage({ bytes });
    }
    // The marker file shows that the copy is complete. A copy that stopped part of the way has no marker.
    const done = await (await top.getFileHandle(marker, { create: true })).createSyncAccessHandle();
    done.write(new TextEncoder().encode(`${entries.length} files\n`), { at: 0 });
    done.flush();
    done.close();
    postMessage({ finished: true });
  } catch (error) {
    postMessage({ finished: true, error: String(error) });
  }
};
