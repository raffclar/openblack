// A small reader for the screenshots of Chrome (8 bits a channel, RGB or RGBA, no interlace), and the finders that the
// web checks use on them.
import { readFileSync } from 'node:fs';
import { inflateSync } from 'node:zlib';

export function decodePng(data) {
  let offset = 8;
  let width = 0;
  let height = 0;
  let channels = 0;
  const parts = [];
  while (offset < data.length) {
    const length = data.readUInt32BE(offset);
    const type = data.toString('ascii', offset + 4, offset + 8);
    const body = data.subarray(offset + 8, offset + 8 + length);
    if (type === 'IHDR') {
      width = body.readUInt32BE(0);
      height = body.readUInt32BE(4);
      channels = { 2: 3, 6: 4 }[body[9]];
      if (body[8] !== 8 || !channels || body[12] !== 0) {
        throw new Error('not an 8-bit RGB or RGBA PNG without interlace');
      }
    } else if (type === 'IDAT') {
      parts.push(body);
    }
    offset += length + 12;
  }
  const raw = inflateSync(Buffer.concat(parts));
  const stride = width * channels;
  const pixels = Buffer.alloc(stride * height);
  for (let y = 0; y < height; ++y) {
    const filter = raw[y * (stride + 1)];
    const line = raw.subarray(y * (stride + 1) + 1, (y + 1) * (stride + 1));
    for (let x = 0; x < stride; ++x) {
      const left = x >= channels ? pixels[y * stride + x - channels] : 0;
      const up = y > 0 ? pixels[(y - 1) * stride + x] : 0;
      const upLeft = x >= channels && y > 0 ? pixels[(y - 1) * stride + x - channels] : 0;
      let value = line[x];
      if (filter === 1) {
        value += left;
      } else if (filter === 2) {
        value += up;
      } else if (filter === 3) {
        value += (left + up) >> 1;
      } else if (filter === 4) {
        const p = left + up - upLeft;
        const pa = Math.abs(p - left);
        const pb = Math.abs(p - up);
        const pc = Math.abs(p - upLeft);
        value += pa <= pb && pa <= pc ? left : pb <= pc ? up : upLeft;
      }
      pixels[y * stride + x] = value & 0xff;
    }
  }
  return { width, height, channels, pixels };
}

export const readPng = (file) => decodePng(readFileSync(file));

// The place of the game's pointer in a screenshot: the middle of the pixels with the teal of the pointer, and how many
// there are. The count is 0 when the picture has no pointer.
export function findPointer(picture) {
  const { width, height, channels, pixels } = picture;
  let count = 0;
  let sumX = 0;
  let sumY = 0;
  for (let y = 0; y < height; ++y) {
    for (let x = 0; x < width; ++x) {
      const i = (y * width + x) * channels;
      const [r, g, b] = [pixels[i], pixels[i + 1], pixels[i + 2]];
      if (r < 110 && g > 130 && b > 130 && Math.abs(g - b) < 40 && g - r > 60) {
        ++count;
        sumX += x;
        sumY += y;
      }
    }
  }
  return count > 0 ? { x: Math.round(sumX / count), y: Math.round(sumY / count), count } : { x: -1, y: -1, count };
}
