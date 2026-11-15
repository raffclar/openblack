// openblack web page: writes a saved game file or a creature file that comes back from the site into the private
// storage of the site (OPFS) in this browser. A file that is there stays as it is. Only a worker can write a file of
// this storage in place in all browsers.
'use strict';

self.onmessage = async (event) => {
  const { root, folder, name, bytes } = event.data;
  try {
    let parent = await navigator.storage.getDirectory();
    for (const part of [...root, ...(folder ? folder.split('/') : [])]) {
      parent = await parent.getDirectoryHandle(part, { create: true });
    }
    try {
      await parent.getFileHandle(name);
      postMessage({ written: false });
      return;
    } catch (error) {
      if (error.name !== 'NotFoundError') {
        throw error;
      }
    }
    const handle = await parent.getFileHandle(name, { create: true });
    const access = await handle.createSyncAccessHandle();
    try {
      access.truncate(0);
      access.write(new Uint8Array(bytes), { at: 0 });
      access.flush();
    } finally {
      access.close();
    }
    postMessage({ written: true });
  } catch (error) {
    postMessage({ written: false, error: String(error) });
  }
};
