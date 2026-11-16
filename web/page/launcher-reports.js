// openblack web page: the error and crash reports of the web game to Sentry, as the desktop game sends them.
//
// The page sends no report until the game starts the reports. The game starts them only when the player turns on the
// option "Send Crash Reports" on the Options page. The profile of the player keeps the option. When the reports start,
// the page loads the Sentry browser SDK from the folder of the page. The build puts the SDK there, with a fixed version.
//
// Each report has the role tag, the release and the environment that the game gives, and the game log as an
// attachment. The reports have no page parameters and no secrets: the page removes them before a report goes. They
// have no user and no IP address, unless the lobby account of the player gave the choice "full" on the site. Then the
// game gives the page the id and the name of the account (setUser), and Sentry takes the IP address of the report.
//
// The frames of the WebAssembly code in a report have the offset of each instruction in the module. The page gives the
// build id of the module with each report, so that Sentry finds the debug file of the build and shows the function
// names and the source lines. The deploy of the build gives the debug file to Sentry. The page never loads it.
'use strict';

// The file of the Sentry browser SDK next to the page (the build copies it there)
const k_SentryScript = 'sentry-browser.min.js';
// The name of the attachment with the game log. The desktop game attaches its log file.
const k_LogAttachmentName = 'openblack.log';
// The name of the tag that tells which program sent the report
const k_RoleTagName = 'role';
// The longest time, in milliseconds, that a stop waits for the reports that are not sent
const k_CloseMilliseconds = 2000;
// The most breadcrumbs that one report keeps
const k_MostBreadcrumbs = 100;
// A frame of the WebAssembly code in a stack of the browser: the address of the module, the index of the function and
// the offset of the instruction in the module
const k_WasmFramePattern = /^(.*?):wasm-function\[\d+\]:(0x[0-9a-fA-F]+)$/;

/** The text with each secret in it replaced by a mark, as the game does */
function maskSecrets(text) {
  return String(text)
    .replace(/v4\.(public|local)\.[A-Za-z0-9_\-.=+/]+/g, '<hidden>')
    .replace(/Bearer [A-Za-z0-9_\-.=+/]+/g, '<hidden>')
    .replace(/(\w*(?:ticket|password|token|session|secret)\w*"?\s*[=:]\s*"?)([^"\s,;&]+)/gi, '$1<hidden>');
}

/** The address with no query and no fragment: the page parameters can hold developer files and arguments */
function addressWithoutParameters(address) {
  const text = String(address);
  const end = text.search(/[?#]/);
  return end < 0 ? text : text.slice(0, end);
}

/** Replace each text value in the object (and in its objects and lists) with the masked text */
function maskValues(value) {
  if (typeof value === 'string') {
    return maskSecrets(value);
  }
  if (Array.isArray(value)) {
    return value.map(maskValues);
  }
  if (value && typeof value === 'object') {
    for (const key of Object.keys(value)) {
      value[key] = maskValues(value[key]);
    }
  }
  return value;
}

/**
 * Removes the personal data from an event before it goes. The event has no user and no IP address, unless the game gave
 * the user of an account with the choice "full": then the user is only that id and name, and Sentry takes the IP
 * address. The address of the page has no parameters, the request has no referrer and no cookies, and the texts have
 * no secrets.
 */
function scrubEvent(event, user) {
  event.sdk = event.sdk || {};
  event.sdk.settings = { ...(event.sdk.settings || {}), infer_ip: user ? 'auto' : 'never' };
  if (user) {
    event.user = { id: String(user.id), username: String(user.username), ip_address: '{{auto}}' };
  } else {
    delete event.user;
  }
  if (event.request) {
    if (event.request.url) {
      event.request.url = addressWithoutParameters(event.request.url);
    }
    delete event.request.query_string;
    delete event.request.cookies;
    if (event.request.headers) {
      delete event.request.headers.Referer;
      delete event.request.headers.referer;
      delete event.request.headers.Cookie;
      delete event.request.headers.cookie;
    }
  }
  for (const part of ['message', 'logentry', 'exception', 'extra', 'tags', 'breadcrumbs']) {
    if (event[part] !== undefined) {
      event[part] = maskValues(event[part]);
    }
  }
  // The frames of a stack name the address of each file of the page
  const values = (event.exception && event.exception.values) || [];
  for (const value of values) {
    for (const frame of (value.stacktrace && value.stacktrace.frames) || []) {
      if (frame.filename) {
        frame.filename = addressWithoutParameters(frame.filename);
      }
      if (frame.abs_path) {
        frame.abs_path = addressWithoutParameters(frame.abs_path);
      }
    }
  }
  return event;
}

/**
 * Sets the choice about the IP address in each event of an envelope, just before the envelope goes. The SDK writes its
 * own choice into the events after the function scrubEvent ran, and with sendDefaultPii the SDK choice is "auto".
 * An account with the choice "full" keeps "auto". All other reports get "never", so that Sentry keeps no IP address.
 */
function applyIpChoice(envelope, user) {
  for (const [header, payload] of (envelope && envelope[1]) || []) {
    if (header && header.type === 'event' && payload && typeof payload === 'object') {
      payload.sdk = payload.sdk || {};
      payload.sdk.settings = { ...(payload.sdk.settings || {}), infer_ip: user ? 'auto' : 'never' };
    }
  }
  return envelope;
}

/** The hexadecimal text of the bytes */
function hexText(bytes) {
  return Array.from(bytes, (byte) => byte.toString(16).padStart(2, '0')).join('');
}

/**
 * The build id of a WebAssembly module and the name of its debug file, from the custom sections of the module. The
 * build id is undefined when the module has no build id.
 */
function wasmModuleInfo(module, wasm = globalThis.WebAssembly) {
  const buildIds = wasm.Module.customSections(module, 'build_id');
  const debugFiles = wasm.Module.customSections(module, 'external_debug_info');
  return {
    buildId: buildIds.length > 0 && buildIds[0].byteLength > 0 ? hexText(new Uint8Array(buildIds[0])) : undefined,
    debugFile: debugFiles.length > 0 ? new TextDecoder('utf-8').decode(new Uint8Array(debugFiles[0])) : undefined,
  };
}

/**
 * The debug image of the WebAssembly module for Sentry. Sentry finds the debug file of the build with the debug id,
 * which is the first 16 bytes of the build id (as the Sentry CLI makes it when it gets the debug file).
 */
function wasmDebugImage(buildId, codeFile) {
  return {
    type: 'wasm',
    code_id: buildId,
    code_file: codeFile,
    debug_id: `${buildId.padEnd(32, '0').slice(0, 32)}0`,
  };
}

/**
 * Gives the frames of the WebAssembly code the offset of their instruction and the debug image of the module, so that
 * Sentry can find the function and the source line of each frame. The event does not change when the build id is not
 * known or when no frame is in the WebAssembly code. The page has one module only.
 */
function addWasmDebugImage(event, buildId) {
  if (!buildId) {
    return event;
  }
  let codeFile;
  for (const value of (event.exception && event.exception.values) || []) {
    for (const frame of (value.stacktrace && value.stacktrace.frames) || []) {
      const match = k_WasmFramePattern.exec(frame.filename || '') || k_WasmFramePattern.exec(frame.abs_path || '');
      if (!match) {
        continue;
      }
      const file = addressWithoutParameters(match[1]);
      codeFile = codeFile || file;
      frame.filename = file;
      frame.abs_path = file;
      frame.instruction_addr = match[2];
      frame.addr_mode = 'rel:0';
      frame.platform = 'native';
    }
  }
  if (codeFile) {
    // The frames name the image with its place in the list: the image of the module is the first
    const images = (event.debug_meta && event.debug_meta.images) || [];
    event.debug_meta = { ...event.debug_meta, images: [wasmDebugImage(buildId, codeFile), ...images] };
  }
  return event;
}

/**
 * The options of the Sentry SDK. The SDK uses only the integrations in the list: the page reports the errors itself,
 * and makes its own breadcrumbs. The SDK makes no traces and no replays.
 */
function sentryOptions(Sentry, settings, logText, user = () => undefined, buildId = () => undefined) {
  return {
    dsn: settings.dsn,
    release: settings.release,
    environment: settings.environment,
    // The page removes the cookies, the headers and the IP address again unless the account chose "full" (scrubEvent)
    sendDefaultPii: true,
    tracesSampleRate: 0,
    maxBreadcrumbs: k_MostBreadcrumbs,
    defaultIntegrations: false,
    integrations: [Sentry.dedupeIntegration(), Sentry.linkedErrorsIntegration(), Sentry.httpContextIntegration()],
    initialScope: { tags: { [k_RoleTagName]: settings.role } },
    beforeSend: (event, hint) => {
      if (hint) {
        hint.attachments = [...(hint.attachments || []),
          { filename: k_LogAttachmentName, data: maskSecrets(logText()), contentType: 'text/plain' }];
      }
      return scrubEvent(addWasmDebugImage(event, buildId()), user());
    },
  };
}

/** An error with the name, the message and the stack of a crash of a game thread, for the stack parser of the SDK */
function crashError(type, message, stack) {
  const error = new Error(message);
  error.name = type;
  // The first line of a stack of a browser is the error itself. The frames come after it.
  const frames = String(stack || '').split('\n').filter((line) => /^\s+at\s|@/.test(line));
  error.stack = [`${type}: ${message}`, ...frames].join('\n');
  return error;
}

/** The event of a desync from the lists that the game gives: the fingerprint, the tags and the extra values */
function desyncEvent(message, level, fingerprint, tags, extras) {
  return {
    message,
    level,
    fingerprint: [...fingerprint],
    tags: Object.fromEntries(tags),
    extra: Object.fromEntries(extras),
  };
}

/**
 * The reports of the page. The calls before the SDK loads wait in a list. A stop before the SDK loads removes the list.
 * The functions of the page (load, log, the build id of the module) come from the caller, so that the tests can give
 * fakes.
 */
class ErrorReports {
  constructor({ loadSdk, logText, log, buildId = () => undefined }) {
    this.loadSdk = loadSdk;
    this.logText = logText;
    this.log = log;
    this.buildId = buildId;
    this.sentry = undefined;
    this.running = false;
    this.ready = false;
    this.starts = 0;
    this.waiting = [];
    this.pageFailureSent = false;
    // The last error of a game thread, with its stack, or undefined
    this.lastThreadError = undefined;
    // An environment that replaces the environment of the game, or undefined
    this.environment = undefined;
    // The id and the name of the account with the choice "full", or undefined: the reports then have no user
    this.user = undefined;
  }

  /** The user of the next reports ({ id, username }), or undefined for reports with no personal data */
  setUser(user) {
    this.user = user && user.id ? { id: String(user.id), username: String(user.username || '') } : undefined;
  }

  /** Start the reports with the settings of the game. Gives true when the page can send reports. */
  start(settings) {
    if (this.running) {
      return true;
    }
    if (!settings || !settings.dsn) {
      return false;
    }
    this.running = true;
    // A stop and a new start before the SDK loads make a new start. Only the newest start goes on.
    const start = ++this.starts;
    Promise.resolve(this.sentry || this.loadSdk())
      .then((Sentry) => {
        if (!this.running || start !== this.starts) {
          return;
        }
        this.sentry = Sentry;
        const environment = this.environment || settings.environment;
        Sentry.init(sentryOptions(Sentry, { ...settings, environment }, this.logText, () => this.user, this.buildId));
        const client = Sentry.getClient && Sentry.getClient();
        if (client && client.on) {
          client.on('beforeEnvelope', (envelope) => applyIpChoice(envelope, this.user));
        }
        this.ready = true;
        this.log('The error reports to Sentry started');
        const waiting = this.waiting;
        this.waiting = [];
        waiting.forEach((call) => call(Sentry));
      })
      .catch((error) => {
        if (start !== this.starts) {
          return;
        }
        this.running = false;
        this.waiting = [];
        this.log(`The error reports did not start: the page cannot load the Sentry SDK: ${error}`);
      });
    return true;
  }

  /** Stop the reports. The reports that wait go first. */
  stop() {
    if (!this.running) {
      return;
    }
    this.running = false;
    this.waiting = [];
    this.user = undefined;
    if (this.ready) {
      this.ready = false;
      this.sentry.close(k_CloseMilliseconds);
      this.log('The error reports to Sentry stopped');
    }
  }

  /** Do the call with the SDK now, or after the SDK loads. Nothing when the reports do not run. */
  send(call) {
    if (!this.running) {
      return false;
    }
    if (this.ready) {
      call(this.sentry);
    } else {
      this.waiting.push(call);
    }
    return true;
  }

  /** The crash of a game thread, after the crash handler of the game wrote its report */
  crash(type, message, report, stack) {
    const error = crashError(type, message, stack);
    return this.send((Sentry) => Sentry.captureException(error, {
      captureContext: { level: 'fatal', extra: { crash_report: report } },
    }));
  }

  /** The event of a desync, which the rule of the game allowed */
  desync(fields) {
    const event = desyncEvent(fields.message, fields.level, fields.fingerprint, fields.tags, fields.extras);
    return this.send((Sentry) => Sentry.captureEvent(event));
  }

  /** The event of a failure that the game found, such as a map download that failed */
  event(fields) {
    const event = desyncEvent(fields.message, fields.level, fields.fingerprint, fields.tags, fields.extras);
    return this.send((Sentry) => Sentry.captureEvent(event));
  }

  /**
   * An error of the browser that stopped the game: a trap of the WebAssembly code, an abort, or an error that no code
   * caught. Only the first one goes, because the game stops after it.
   */
  pageFailed(reason, error) {
    if (this.pageFailureSent || !this.running) {
      return false;
    }
    this.pageFailureSent = true;
    const failure = error instanceof Error ? error : this.lastThreadError || new Error(String(reason));
    return this.send((Sentry) => Sentry.captureException(failure, {
      captureContext: { level: 'fatal', tags: { failure: 'page' }, extra: { reason: String(reason) } },
    }));
  }

  /**
   * The name, the message and the stack of an error of a game thread. The error comes to the page next, with its
   * message only, and the report of the failure then has this stack.
   */
  threadError({ name, message, stack }) {
    this.lastThreadError = crashError(name, message, stack);
  }

  /** An error of the page that does not stop the game, such as a promise that failed with no code to take the error */
  pageError(error) {
    const failure = error instanceof Error ? error : new Error(String(error));
    return this.send((Sentry) => Sentry.captureException(failure, {
      captureContext: { level: 'error', tags: { failure: 'page' } },
    }));
  }

  /** A line of the page for the diagnostics, as a breadcrumb of the next report */
  breadcrumb(category, message) {
    return this.send((Sentry) => Sentry.addBreadcrumb({ category, message, level: 'info' }));
  }
}

/** Load the Sentry SDK from the folder of the page. Gives the Sentry object of the SDK. */
function loadSentryScript(document) {
  return new Promise((resolve, reject) => {
    if (globalThis.Sentry && globalThis.Sentry.init) {
      resolve(globalThis.Sentry);
      return;
    }
    const script = document.createElement('script');
    script.src = k_SentryScript;
    script.onload = () => (globalThis.Sentry ? resolve(globalThis.Sentry) : reject(new Error('no Sentry object')));
    script.onerror = () => reject(new Error(`the page has no ${k_SentryScript}`));
    document.head.append(script);
  });
}

if (typeof module !== 'undefined') {
  module.exports = {
    ErrorReports, scrubEvent, applyIpChoice, sentryOptions, crashError, desyncEvent, maskSecrets, addressWithoutParameters,
    wasmModuleInfo, wasmDebugImage, addWasmDebugImage, k_LogAttachmentName, k_RoleTagName,
  };
}
