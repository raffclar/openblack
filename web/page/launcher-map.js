// Copyright (c) 2018-2026 openblack developers
//
// The map of the game files on the web page. Each part of the game is a square with an area that agrees with its
// bytes, and each file in it is a smaller square. A square shows if the browser keeps the file, gets it now, has it
// in a queue, does not need it yet, or could not get it. The pure parts (the groups, the layout, the states and the
// totals) have no DOM, so that node can test them. The view draws the map on a canvas, at most k_AssetMapTicksPerSecond
// times a second, and only when something changed.
'use strict';

// The parts of the game, in the order of their rules: the first rule that agrees with the path gives the part
const k_AssetMapGroups = [
  { id: 'films', name: 'Films', rule: /\.bik$/ },
  { id: 'music', name: 'Music', rule: /^audio\/music\// },
  { id: 'dialogue', name: 'Dialogue', rule: /^audio\/dialogue\// },
  { id: 'sounds', name: 'Sound effects', rule: /^audio\// },
  { id: 'online', name: 'Online maps', rule: /^online maps\// },
  { id: 'lands', name: 'Lands', rule: /^data\/landscape\// },
  { id: 'scripts', name: 'Scripts', rule: /^scripts\// },
  { id: 'textures', name: 'Textures and pictures', rule: /^data\/(textures|helpsprite)\/|\.(raw|bmp|tga|png|jpg|pal|256|16)$/ },
  { id: 'meshes', name: 'Meshes and animations',
    rule: /\.(g3d|anm|l3d|dan|ctr|zzz|jty|lii)$|^data\/(creaturemesh|citadel|misc|ctr|spells|handdemo)\// },
  { id: 'plugins', name: 'Plug-ins', rule: /^plug ins\// },
  { id: 'program', name: 'Program', rule: null },
  { id: 'other', name: 'Other data', rule: /./ },
];

// The states of a file or a block, in the order that the legend and a square of small files show them
const k_AssetMapStates = ['present', 'downloading', 'urgent', 'phase', 'prefetch', 'failed', 'lazy'];
const k_AssetMapStateNames = {
  present: 'In this browser',
  downloading: 'Downloading now',
  urgent: 'Queue: needed now',
  phase: 'Queue: next for the game',
  prefetch: 'Queue: for later',
  failed: 'Failed',
  lazy: 'Not needed yet',
};

// A file with a smaller square than this area in CSS pixels goes into one square of the small files of its part
const k_AssetMapSmallArea = 30;
// The height of the name of a part above its files, and the smallest part that shows its name
const k_AssetMapLabelHeight = 15;
const k_AssetMapLabelWidth = 64;
// The speed is the bytes that arrived in this time, in milliseconds
const k_AssetMapSpeedWindow = 5000;
const k_AssetMapTicksPerSecond = 10;

/** The part of the game of a file, by its path */
function assetGroupOf(path) {
  const lower = path.toLowerCase();
  return k_AssetMapGroups.find((group) => group.rule !== null && group.rule.test(lower)).id;
}

/**
 * The map of the game files: each part with its files, the largest first. A content that has two or more paths is one
 * square, with the other paths in also. The argument files are the files of the list ({ path, size, sha256, blocks }),
 * blockSize the size of their blocks, and extra more items { group, path, size, key } (the program).
 */
function buildAssetMap(files, { blockSize = 0, extra = [] } = {}) {
  const byContent = new Map();
  const items = [];
  for (const file of files) {
    const known = byContent.get(file.sha256);
    if (known) {
      known.also.push(file.path);
      continue;
    }
    const item = {
      group: assetGroupOf(file.path), path: file.path, size: file.size, key: `file ${file.sha256}`, also: [],
      blocks: file.blocks && blockSize > 0
        ? file.blocks.map((sha256, block) => ({
          key: `block ${sha256}`, size: Math.min(blockSize, file.size - block * blockSize),
        }))
        : null,
    };
    byContent.set(file.sha256, item);
    items.push(item);
  }
  for (const item of extra) {
    items.push({ also: [], blocks: null, ...item });
  }
  const groups = k_AssetMapGroups
    .map(({ id, name }) => {
      const own = items.filter((item) => item.group === id && item.size > 0).sort((a, b) => b.size - a.size);
      return { id, name, items: own, size: own.reduce((sum, item) => sum + item.size, 0) };
    })
    .filter((group) => group.items.length > 0)
    .sort((a, b) => b.size - a.size);
  return { groups, size: groups.reduce((sum, group) => sum + group.size, 0) };
}

/** The worst ratio of the long side to the short side in a row of areas along a side of this length */
function worstRatio(largest, smallest, sum, side) {
  const side2 = side * side;
  const sum2 = sum * sum;
  return Math.max((side2 * largest) / sum2, sum2 / (side2 * smallest));
}

/**
 * The squarified layout of values (positive, the largest first) in the rectangle { x, y, w, h }. Gives one rectangle
 * for each value, in the same order, with an area that agrees with the value and sides that are as near to a square as
 * possible.
 */
function squarify(values, rect) {
  const total = values.reduce((sum, value) => sum + value, 0);
  if (values.length === 0 || total <= 0 || rect.w <= 0 || rect.h <= 0) {
    return values.map(() => ({ x: rect.x, y: rect.y, w: 0, h: 0 }));
  }
  const scale = (rect.w * rect.h) / total;
  const areas = values.map((value) => value * scale);
  const rects = [];
  let { x, y, w, h } = rect;
  let first = 0;
  while (first < areas.length) {
    const side = Math.min(w, h);
    let sum = areas[first];
    let smallest = areas[first];
    let worst = worstRatio(areas[first], smallest, sum, side);
    let end = first + 1;
    while (end < areas.length) {
      const next = worstRatio(areas[first], Math.min(smallest, areas[end]), sum + areas[end], side);
      if (next > worst) {
        break;
      }
      worst = next;
      sum += areas[end];
      smallest = Math.min(smallest, areas[end]);
      end += 1;
    }
    // The last row takes all the space that is left, so that no gap stays from the errors of the float maths
    const last = end === areas.length;
    if (w >= h) {
      const thick = last ? w : sum / h;
      let at = y;
      for (let index = first; index < end; index += 1) {
        const size = index === end - 1 ? y + h - at : areas[index] / thick;
        rects.push({ x, y: at, w: thick, h: size });
        at += size;
      }
      x += thick;
      w -= thick;
    } else {
      const thick = last ? h : sum / w;
      let at = x;
      for (let index = first; index < end; index += 1) {
        const size = index === end - 1 ? x + w - at : areas[index] / thick;
        rects.push({ x: at, y, w: size, h: thick });
        at += size;
      }
      y += thick;
      h -= thick;
    }
    first = end;
  }
  return rects;
}

/**
 * The layout of the map in a canvas of width by height CSS pixels. Gives frames, one for each part with its rectangle
 * and if it shows its name, and cells: one for each file large enough to see ({ kind: 'item', item }) and one for the
 * small files of each part ({ kind: 'small', items }), each with its part and its rectangle.
 */
function layoutAssetMap(map, width, height, { labels = true, gap = 2 } = {}) {
  const frames = [];
  const cells = [];
  const groupRects = squarify(map.groups.map((group) => group.size), { x: 0, y: 0, w: width, h: height });
  map.groups.forEach((group, index) => {
    const outer = groupRects[index];
    const frame = {
      group, x: outer.x + gap / 2, y: outer.y + gap / 2, w: Math.max(0, outer.w - gap), h: Math.max(0, outer.h - gap),
    };
    frame.label = labels && frame.w >= k_AssetMapLabelWidth && frame.h >= 2 * k_AssetMapLabelHeight + 8;
    frames.push(frame);
    const inner = {
      x: frame.x + 1, y: frame.y + 1 + (frame.label ? k_AssetMapLabelHeight : 0),
      w: Math.max(0, frame.w - 2), h: Math.max(0, frame.h - 2 - (frame.label ? k_AssetMapLabelHeight : 0)),
    };
    const perByte = group.size > 0 ? (inner.w * inner.h) / group.size : 0;
    const large = group.items.filter((item) => item.size * perByte >= k_AssetMapSmallArea);
    const small = group.items.filter((item) => item.size * perByte < k_AssetMapSmallArea);
    const parts = [...large.map((item) => ({ kind: 'item', item, size: item.size }))];
    if (small.length === 1) {
      parts.push({ kind: 'item', item: small[0], size: small[0].size });
    } else if (small.length > 1) {
      parts.push({ kind: 'small', items: small, size: small.reduce((sum, item) => sum + item.size, 0) });
    }
    parts.sort((a, b) => b.size - a.size);
    const rects = squarify(parts.map((part) => part.size), inner);
    parts.forEach((part, at) => {
      const rect = rects[at];
      cells.push({ ...part, group, x: rect.x, y: rect.y, w: rect.w, h: rect.h });
    });
  });
  return { width, height, frames, cells };
}

/** The cell at the point x, y of the layout, or no value */
function assetCellAt(layout, x, y) {
  return layout.cells.find((cell) => x >= cell.x && x < cell.x + cell.w && y >= cell.y && y < cell.y + cell.h) ?? null;
}

/** A number of bytes for the player */
function formatAssetBytes(bytes) {
  const mib = 1024 * 1024;
  if (bytes < mib) {
    return `${Math.max(0, Math.round(bytes / 1024))} KB`;
  }
  if (bytes < 1024 * mib) {
    const value = bytes / mib;
    return `${value < 100 ? value.toFixed(1) : Math.round(value)} MB`;
  }
  return `${(bytes / (1024 * mib)).toFixed(2)} GB`;
}

/** A time in seconds for the player */
function formatAssetTime(seconds) {
  if (!Number.isFinite(seconds) || seconds < 0) {
    return '-';
  }
  const whole = Math.ceil(seconds);
  if (whole < 60) {
    return `${whole} s`;
  }
  if (whole < 3600) {
    return `${Math.floor(whole / 60)} min ${whole % 60} s`;
  }
  return `${Math.floor(whole / 3600)} h ${Math.floor((whole % 3600) / 60)} min`;
}

/**
 * The states of the files and the blocks of the map. It takes the events of the downloads (see safeWatch in
 * launcher-assets.js) and gives the state of each square and the totals. Each change adds one to version, so that a
 * view draws again only after a change.
 */
class AssetMapModel {
  constructor(map) {
    this.map = map;
    this.entries = new Map();
    this.version = 0;
    // The bytes of the bodies that arrived in this visit, and the bytes on the network (from the browser's timing)
    this.received = 0;
    this.wire = 0;
    this.samples = [];
    const add = (key, size) => {
      if (!this.entries.has(key)) {
        this.entries.set(key, { state: 'lazy', got: 0, size });
      }
    };
    for (const group of map.groups) {
      for (const item of group.items) {
        add(item.key, item.size);
        (item.blocks ?? []).forEach((block) => add(block.key, block.size));
      }
    }
  }

  /** Takes one event of a download. An event for a key that is not on the map changes nothing. */
  handle(event, key, detail) {
    const entry = this.entries.get(key);
    if (!entry) {
      return;
    }
    switch (event) {
    case 'present':
      entry.state = 'present';
      entry.got = entry.size;
      break;
    case 'queued':
      if (entry.state !== 'present' && entry.state !== 'downloading') {
        entry.state = { now: 'urgent', phase: 'phase' }[detail] ?? 'prefetch';
      }
      break;
    case 'started':
      entry.state = 'downloading';
      entry.got = 0;
      break;
    case 'bytes':
      entry.got = Math.min(entry.size, entry.got + detail);
      entry.counted = true;
      this.received += detail;
      break;
    case 'done':
      // A block arrives in one part, with no event for its bytes
      if (!entry.counted) {
        this.received += entry.size;
      }
      entry.state = 'present';
      entry.got = entry.size;
      break;
    case 'failed':
      entry.state = 'failed';
      break;
    default:
      return;
    }
    if (event !== 'bytes' && event !== 'started') {
      entry.counted = false;
    }
    this.version += 1;
  }

  /** Adds the bytes that one response took on the network */
  addWire(bytes) {
    if (bytes > 0) {
      this.wire += bytes;
      this.version += 1;
    }
  }

  /**
   * The parts of the square of a file: the whole file, or each block while the browser does not keep the whole file.
   * Each part has state, size and got (the bytes that arrived of it).
   */
  partsOf(item) {
    const whole = this.entries.get(item.key);
    if (!item.blocks || whole.state !== 'lazy') {
      return [whole];
    }
    return item.blocks.map((block) => this.entries.get(block.key));
  }

  /** The bytes of the file in each state */
  bytesOf(item) {
    const bytes = Object.fromEntries(k_AssetMapStates.map((state) => [state, 0]));
    for (const part of this.partsOf(item)) {
      if (part.state === 'downloading') {
        bytes.present += part.got;
        bytes.downloading += part.size - part.got;
      } else {
        bytes[part.state] += part.size;
      }
    }
    return bytes;
  }

  /** The state of a whole file for the player: the most urgent state of its parts. It is present only when all are. */
  stateOf(item) {
    const states = new Set(this.partsOf(item).map((part) => part.state));
    return ['downloading', 'urgent', 'failed', 'phase', 'prefetch', 'lazy', 'present']
      .find((state) => states.has(state));
  }

  /** Keeps the bytes that arrived until now, for the speed. The time is in milliseconds. */
  sample(now) {
    this.samples.push({ time: now, bytes: this.received });
    while (this.samples.length > 2 && now - this.samples[1].time >= k_AssetMapSpeedWindow) {
      this.samples.shift();
    }
  }

  /** The bytes a second of the last k_AssetMapSpeedWindow milliseconds, or 0 */
  speed() {
    if (this.samples.length < 2) {
      return 0;
    }
    const first = this.samples[0];
    const last = this.samples[this.samples.length - 1];
    return last.time > first.time ? ((last.bytes - first.bytes) * 1000) / (last.time - first.time) : 0;
  }

  /**
   * The totals of the map: the bytes in each state, the bytes that arrived and the bytes on the network in this visit,
   * the bytes that the queues still wait for, the number of files that download now, the speed and the time left.
   */
  totals() {
    const bytes = Object.fromEntries(k_AssetMapStates.map((state) => [state, 0]));
    let active = 0;
    for (const group of this.map.groups) {
      for (const item of group.items) {
        const own = this.bytesOf(item);
        k_AssetMapStates.forEach((state) => {
          bytes[state] += own[state];
        });
        if (this.partsOf(item).some((part) => part.state === 'downloading')) {
          active += 1;
        }
      }
    }
    const remaining = bytes.downloading + bytes.urgent + bytes.phase + bytes.prefetch;
    const speed = this.speed();
    return {
      bytes, total: this.map.size, received: this.received, wire: this.wire, remaining, active, speed,
      secondsLeft: remaining === 0 ? 0 : (speed > 0 ? remaining / speed : null),
    };
  }

  /** True while a file or a block downloads, so that its square moves */
  animating() {
    for (const entry of this.entries.values()) {
      if (entry.state === 'downloading') {
        return true;
      }
    }
    return false;
  }
}

/** The text for the player about a cell of the map: { group, title, lines } */
function describeAssetCell(cell, model) {
  if (cell.kind === 'small') {
    const bytes = Object.fromEntries(k_AssetMapStates.map((state) => [state, 0]));
    for (const item of cell.items) {
      const own = model.bytesOf(item);
      k_AssetMapStates.forEach((state) => {
        bytes[state] += own[state];
      });
    }
    return {
      group: cell.group.name,
      title: `${cell.items.length} small files, ${formatAssetBytes(cell.size)}`,
      lines: k_AssetMapStates.filter((state) => bytes[state] > 0)
        .map((state) => `${k_AssetMapStateNames[state]}: ${formatAssetBytes(bytes[state])}`),
    };
  }
  const { item } = cell;
  const lines = [`${formatAssetBytes(item.size)}, ${k_AssetMapStateNames[model.stateOf(item)]}`];
  const parts = model.partsOf(item);
  if (parts.length > 1) {
    const kept = parts.filter((part) => part.state === 'present').length;
    lines.push(`${kept} of ${parts.length} blocks in this browser`);
  } else if (parts[0].state === 'downloading' && parts[0].got > 0) {
    lines.push(`${formatAssetBytes(parts[0].got)} arrived`);
  }
  if (item.also.length > 0) {
    lines.push(`The same content as ${item.also.length === 1 ? item.also[0] : `${item.also.length} other files`}`);
  }
  return { group: cell.group.name, title: item.path, lines };
}

// ------------------------------------------------------------------------------------------------ the view (DOM)

/** The colours of the map, from the custom properties of the page (page.css) */
function assetMapColours(element) {
  const style = getComputedStyle(element);
  const value = (name) => style.getPropertyValue(name).trim();
  return {
    present: value('--map-present'), downloading: value('--map-downloading'), urgent: value('--map-urgent'),
    phase: value('--map-phase'), prefetch: value('--map-prefetch'), failed: value('--map-failed'),
    lazy: value('--map-lazy'),
    frame: value('--map-frame'), label: value('--map-label'), hover: value('--map-hover'), back: value('--map-back'),
  };
}

/** The draw of the map on one canvas. labels false is for the small map in the strip above the game. */
class AssetMapView {
  constructor(canvas, model, { labels = true, onHover = () => {} } = {}) {
    Object.assign(this, { canvas, model, labels, onHover });
    this.layout = null;
    this.drawn = -1;
    this.hover = null;
    this.size = { width: 0, height: 0, ratio: 1 };
    if (labels) {
      const point = (event) => {
        const box = canvas.getBoundingClientRect();
        this.setHover(this.layout ? assetCellAt(this.layout, event.clientX - box.left, event.clientY - box.top) : null);
      };
      canvas.addEventListener('pointermove', point);
      canvas.addEventListener('pointerdown', point);
      canvas.addEventListener('pointerleave', (event) => {
        if (event.pointerType === 'mouse') {
          this.setHover(null);
        }
      });
    }
  }

  setHover(cell) {
    if (cell !== this.hover) {
      this.hover = cell;
      this.drawn = -1;
      this.onHover(cell ? describeAssetCell(cell, this.model) : null);
    }
  }

  /** True when the canvas is on the screen */
  visible() {
    return this.canvas.isConnected && this.canvas.getClientRects().length > 0 && !document.fullscreenElement;
  }

  /** Makes the layout again when the size of the canvas changed */
  measure() {
    const width = this.canvas.clientWidth;
    const height = this.canvas.clientHeight;
    const ratio = globalThis.devicePixelRatio || 1;
    if (width === this.size.width && height === this.size.height && ratio === this.size.ratio && this.layout) {
      return;
    }
    this.size = { width, height, ratio };
    this.canvas.width = Math.round(width * ratio);
    this.canvas.height = Math.round(height * ratio);
    this.layout = layoutAssetMap(this.model.map, width, height, { labels: this.labels, gap: this.labels ? 2 : 1 });
    this.drawn = -1;
    this.hover = null;
  }

  /** Draws the map when it changed, or while a file downloads. The time is in milliseconds. */
  update(time) {
    if (!this.visible()) {
      return;
    }
    this.measure();
    if (this.drawn === this.model.version && !this.model.animating()) {
      return;
    }
    this.drawn = this.model.version;
    this.draw(time);
  }

  draw(time) {
    const context = this.canvas.getContext('2d');
    const colours = assetMapColours(this.canvas);
    const { width, height, ratio } = this.size;
    context.setTransform(ratio, 0, 0, ratio, 0, 0);
    context.fillStyle = colours.back;
    context.fillRect(0, 0, width, height);
    // A part that downloads with no count of its bytes (a block) pulses
    const pulse = 0.55 + 0.45 * Math.sin(time / 180);
    for (const cell of this.layout.cells) {
      this.drawCell(context, cell, colours, pulse);
    }
    for (const frame of this.layout.frames) {
      context.strokeStyle = colours.frame;
      context.lineWidth = 1;
      context.strokeRect(frame.x + 0.5, frame.y + 0.5, Math.max(0, frame.w - 1), Math.max(0, frame.h - 1));
      if (frame.label) {
        const present = frame.group.items.reduce((sum, item) => sum + this.model.bytesOf(item).present, 0);
        context.fillStyle = colours.label;
        context.font = '600 11px ui-sans-serif, system-ui, sans-serif';
        context.textBaseline = 'middle';
        const text = `${frame.group.name}  ${formatAssetBytes(present)} / ${formatAssetBytes(frame.group.size)}`;
        context.save();
        context.beginPath();
        context.rect(frame.x, frame.y, frame.w, k_AssetMapLabelHeight + 1);
        context.clip();
        context.fillText(text, frame.x + 5, frame.y + 1 + k_AssetMapLabelHeight / 2);
        context.restore();
      }
    }
    if (this.hover) {
      const { x, y, w, h } = this.hover;
      context.strokeStyle = colours.hover;
      context.lineWidth = 2;
      context.strokeRect(x + 1, y + 1, Math.max(0, w - 2), Math.max(0, h - 2));
    }
  }

  /** Fills a cell with its parts along its long side, each part in the colour of its state */
  drawCell(context, cell, colours, pulse) {
    const parts = cell.kind === 'small' ? this.smallParts(cell) : this.model.partsOf(cell.item);
    const total = parts.reduce((sum, part) => sum + part.size, 0);
    if (total <= 0 || cell.w <= 0 || cell.h <= 0) {
      return;
    }
    const inset = cell.w >= 4 && cell.h >= 4 ? 0.5 : 0;
    const x0 = cell.x + inset;
    const y0 = cell.y + inset;
    const w = cell.w - 2 * inset;
    const h = cell.h - 2 * inset;
    const along = w >= h;
    let at = 0;
    for (const part of parts) {
      const length = ((along ? w : h) * part.size) / total;
      const rect = along ? { x: x0 + at, y: y0, w: length, h } : { x: x0, y: y0 + at, w, h: length };
      at += length;
      if (part.state === 'downloading') {
        context.fillStyle = colours.urgent;
        context.fillRect(rect.x, rect.y, rect.w, rect.h);
        context.fillStyle = colours.downloading;
        if (part.got > 0) {
          // The fill rises from the bottom of the part as its bytes arrive
          const filled = (rect.h * part.got) / part.size;
          context.fillRect(rect.x, rect.y + rect.h - filled, rect.w, filled);
        } else {
          context.globalAlpha = pulse;
          context.fillRect(rect.x, rect.y, rect.w, rect.h);
          context.globalAlpha = 1;
        }
      } else {
        context.fillStyle = colours[part.state];
        context.fillRect(rect.x, rect.y, rect.w, rect.h);
      }
    }
    if (cell.kind === 'item' && cell.item.blocks && parts.length > 1 && w / parts.length >= 3) {
      // Thin lines between the blocks of a file
      context.fillStyle = colours.back;
      for (let block = 1; block < parts.length; block += 1) {
        const offset = (along ? w : h) * (block / parts.length);
        if (along) {
          context.fillRect(x0 + offset - 0.5, y0, 1, h);
        } else {
          context.fillRect(x0, y0 + offset - 0.5, w, 1);
        }
      }
    }
    if (this.labels && cell.kind === 'item' && cell.w >= 72 && cell.h >= 28) {
      // The name of a large file, on a plate in the colour of the map, so that it reads on each state
      context.font = '10px ui-sans-serif, system-ui, sans-serif';
      context.textBaseline = 'middle';
      const name = cell.item.path.split('/').pop();
      context.save();
      context.beginPath();
      context.rect(cell.x, cell.y, cell.w, cell.h);
      context.clip();
      context.globalAlpha = 0.8;
      context.fillStyle = colours.back;
      context.fillRect(cell.x + 3, cell.y + 3, Math.min(cell.w - 6, context.measureText(name).width + 6), 14);
      context.globalAlpha = 1;
      context.fillStyle = colours.label;
      context.fillText(name, cell.x + 6, cell.y + 10);
      context.restore();
    }
  }

  /** The parts of a cell of small files: the bytes of each state, in the order of the legend */
  smallParts(cell) {
    const bytes = Object.fromEntries(k_AssetMapStates.map((state) => [state, 0]));
    for (const item of cell.items) {
      const own = this.model.bytesOf(item);
      k_AssetMapStates.forEach((state) => {
        bytes[state] += own[state];
      });
    }
    return k_AssetMapStates.filter((state) => bytes[state] > 0)
      .map((state) => ({ state, size: bytes[state], got: bytes[state] }));
  }
}

/**
 * Draws the views of the model and calls onTick with the totals, at most k_AssetMapTicksPerSecond times a second.
 * Nothing draws while the page is not visible or the game fills the screen. Gives a function that stops the loop.
 */
function startAssetMapLoop(model, views, onTick = () => {}) {
  let told = -1;
  const tick = () => {
    if (document.hidden || document.fullscreenElement) {
      return;
    }
    const now = performance.now();
    model.sample(now);
    for (const view of views) {
      view.update(now);
    }
    // The totals change with the speed too, so they go to onTick while a file downloads
    if (told !== model.version || model.animating()) {
      told = model.version;
      onTick(model.totals());
    }
  };
  tick();
  const timer = setInterval(tick, 1000 / k_AssetMapTicksPerSecond);
  return () => clearInterval(timer);
}

/**
 * Adds the bytes on the network of each response of the browser whose address agrees with wanted to the model. The
 * browser gives them after each response ends. Gives a function that stops it.
 */
function watchAssetWire(model, wanted, onEntry = () => {}) {
  if (typeof PerformanceObserver !== 'function') {
    return () => {};
  }
  const observer = new PerformanceObserver((list) => {
    for (const entry of list.getEntries()) {
      if (wanted(entry.name)) {
        model.addWire(entry.transferSize || 0);
        onEntry(entry);
      }
    }
  });
  try {
    observer.observe({ type: 'resource', buffered: true });
  } catch {
    return () => {};
  }
  return () => observer.disconnect();
}

if (typeof module !== 'undefined') {
  module.exports = {
    k_AssetMapGroups, k_AssetMapStates, k_AssetMapStateNames, assetGroupOf, buildAssetMap, squarify, layoutAssetMap,
    assetCellAt, formatAssetBytes, formatAssetTime, AssetMapModel, describeAssetCell,
  };
}
