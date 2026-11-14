// openblack web page: the error and crash reports of the web game to Sentry, as the desktop game sends them.
//
// The page sends no report until the game starts the reports. The game starts them only when the player turns on the
// option "Send Crash Reports" on the Options page. The profile of the player keeps the option. When the reports start,
// the page loads the Sentry browser SDK from the folder of the page. The build puts the SDK there, with a fixed version.
//
// Each report has the role tag, the release and the environment that the game gives, and the game log as an
// attachment. The reports have no user, no IP address, no page parameters and no secrets: the page removes them before
// a report goes.
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
 * Removes the personal data from an event before it goes. The event has no user and no IP address. The address of the
 * page has no parameters, the request has no referrer and no cookies, and the texts have no secrets.
 */
function scrubEvent(event) {
  delete event.user;
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
 * The options of the Sentry SDK. The SDK uses only the integrations in the list: the page reports the errors itself,
 * and makes its own breadcrumbs. The SDK makes no traces and no replays.
 */
function sentryOptions(Sentry, settings, logText) {
  return {
    dsn: settings.dsn,
    release: settings.release,
    environment: settings.environment,
    sendDefaultPii: false,
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
      return scrubEvent(event);
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
 * The functions of the page (load, log) come from the caller, so that the tests can give fakes.
 */
class ErrorReports {
  constructor({ loadSdk, logText, log }) {
    this.loadSdk = loadSdk;
    this.logText = logText;
    this.log = log;
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
        Sentry.init(sentryOptions(Sentry, { ...settings, environment }, this.logText));
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
    ErrorReports, scrubEvent, sentryOptions, crashError, desyncEvent, maskSecrets, addressWithoutParameters,
    k_LogAttachmentName, k_RoleTagName,
  };
}
