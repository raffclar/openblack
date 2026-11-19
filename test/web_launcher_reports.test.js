// Test of the error reports of the web page (web/page/launcher-reports.js). Run with node.
'use strict';

const assert = require('node:assert/strict');
const {
  ErrorReports, scrubEvent, applyIpChoice, sentryOptions, crashError, desyncEvent, maskSecrets, addressWithoutParameters,
  wasmModuleInfo, addWasmDebugImage, k_LogAttachmentName, isThreadLoadFailure, k_ThreadLoadFailedText, hideEmails,
  hideNames,
} = require('../web/page/launcher-reports.js');

let count = 0;
const tests = [];
const check = (name, body) => tests.push({ name, body });

// A fake Sentry SDK. It keeps each call in a list.
const fakeSentry = () => {
  const calls = [];
  const sentry = {
    calls,
    init: (options) => calls.push(['init', options]),
    close: (timeout) => calls.push(['close', timeout]),
    captureException: (error, hint) => calls.push(['exception', error, hint]),
    captureEvent: (event) => calls.push(['event', event]),
    addBreadcrumb: (crumb) => calls.push(['breadcrumb', crumb]),
    dedupeIntegration: () => 'dedupe',
    linkedErrorsIntegration: () => 'linked',
    httpContextIntegration: () => 'http',
  };
  return sentry;
};

const settings = { dsn: 'https://key@o1.ingest.example.org/2', release: 'openblack@0.1.0+abc', environment: 'production',
  role: 'client' };

// The reports with a fake SDK that loads at once, or with a load that the test ends
const makeReports = (sentry = fakeSentry(), load) => {
  const lines = [];
  const reports = new ErrorReports({
    loadSdk: load || (() => Promise.resolve(sentry)),
    logText: () => 'line 1\ntoken=abc123',
    log: (text) => lines.push(text),
  });
  return { reports, sentry, lines };
};
const settle = () => new Promise((resolve) => setImmediate(resolve));

check('nothing goes before the game starts the reports', async () => {
  let loads = 0;
  const { reports, sentry } = makeReports(fakeSentry(), () => {
    loads += 1;
    return Promise.resolve(sentry);
  });
  assert.equal(reports.crash('T', 'm', 'r', ''), false);
  assert.equal(reports.desync({ message: 'Desync: A', level: 'warning', fingerprint: [], tags: [], extras: [] }), false);
  assert.equal(reports.pageFailed('trap', new Error('trap')), false);
  assert.equal(reports.breadcrumb('page', 'x'), false);
  await settle();
  assert.equal(loads, 0, 'the page does not load the SDK');
  assert.equal(sentry.calls.length, 0);
});
check('the reports do not start with no DSN', async () => {
  const { reports, sentry } = makeReports();
  assert.equal(reports.start({ ...settings, dsn: '' }), false);
  await settle();
  assert.equal(sentry.calls.length, 0);
});
check('the calls before the SDK loads go after the start, in order', async () => {
  let finish;
  const sentry = fakeSentry();
  const { reports } = makeReports(sentry, () => new Promise((resolve) => {
    finish = () => resolve(sentry);
  }));
  assert.equal(reports.start(settings), true);
  reports.breadcrumb('page', 'one');
  reports.crash('class std::runtime_error', 'Sentry test error 1', 'report', '');
  assert.equal(sentry.calls.length, 0);
  finish();
  await settle();
  assert.deepEqual(sentry.calls.map((call) => call[0]), ['init', 'breadcrumb', 'exception']);
  // Later calls go at once
  reports.breadcrumb('page', 'two');
  assert.equal(sentry.calls.at(-1)[0], 'breadcrumb');
});
check('a stop before the SDK loads sends nothing', async () => {
  let finish;
  const sentry = fakeSentry();
  const { reports } = makeReports(sentry, () => new Promise((resolve) => {
    finish = () => resolve(sentry);
  }));
  reports.start(settings);
  reports.crash('T', 'm', 'r', '');
  reports.stop();
  finish();
  await settle();
  assert.equal(sentry.calls.length, 0);
});
check('a stop and a new start before the SDK loads start the SDK one time', async () => {
  const finishes = [];
  const sentry = fakeSentry();
  const { reports } = makeReports(sentry, () => new Promise((resolve) => finishes.push(() => resolve(sentry))));
  reports.start(settings);
  reports.stop();
  reports.start(settings);
  finishes.forEach((finish) => finish());
  await settle();
  assert.deepEqual(sentry.calls.map((call) => call[0]), ['init']);
});
check('a stop closes the SDK, and a new start starts it again', async () => {
  const { reports, sentry } = makeReports();
  reports.start(settings);
  await settle();
  reports.stop();
  assert.deepEqual(sentry.calls.map((call) => call[0]), ['init', 'close']);
  assert.equal(reports.breadcrumb('page', 'x'), false);
  reports.start(settings);
  await settle();
  assert.deepEqual(sentry.calls.map((call) => call[0]), ['init', 'close', 'init']);
});
check('the SDK gets the settings of the game, the role tag and no default integrations', async () => {
  const { reports, sentry } = makeReports();
  reports.start(settings);
  await settle();
  const options = sentry.calls[0][1];
  assert.equal(options.dsn, settings.dsn);
  assert.equal(options.release, 'openblack@0.1.0+abc');
  assert.equal(options.environment, 'production');
  assert.equal(options.sendDefaultPii, true);
  assert.equal(options.tracesSampleRate, 0);
  assert.equal(options.defaultIntegrations, false);
  assert.deepEqual(options.integrations, ['dedupe', 'linked', 'http']);
  assert.deepEqual(options.initialScope.tags, { role: 'client' });
});
check('a developer environment replaces the environment of the game', async () => {
  const { reports, sentry } = makeReports();
  reports.environment = 'lane-test';
  reports.start(settings);
  await settle();
  assert.equal(sentry.calls[0][1].environment, 'lane-test');
});
check('each event gets the game log with no secrets as an attachment', () => {
  const options = sentryOptions(fakeSentry(), settings, () => 'line 1\npassword=hunter2');
  const hint = {};
  options.beforeSend({ message: 'x' }, hint);
  assert.equal(hint.attachments.length, 1);
  assert.equal(hint.attachments[0].filename, k_LogAttachmentName);
  assert.equal(hint.attachments[0].data, 'line 1\npassword=<hidden>');
});
check('the event has no user, no page parameters, no referrer and no secrets', () => {
  const event = scrubEvent({
    user: { ip_address: '1.2.3.4' },
    request: { url: 'https://bw.example.org/play/index.html?args=x&tool-file=y#z', query_string: 'args=x',
      headers: { 'User-Agent': 'Chrome', Referer: 'https://bw.example.org/play/?a=1' } },
    message: 'Bearer abc.def',
    extra: { crash_report: 'ticket=12345 and more', list: ['token: zz'] },
    exception: { values: [{ value: 'v4.public.abcdef', stacktrace: { frames: [
      { filename: 'https://bw.example.org/play/openblack.js?x=1', abs_path: 'https://bw.example.org/play/a.js#q' }] } }] },
  });
  assert.equal(event.user, undefined);
  assert.equal(event.request.url, 'https://bw.example.org/play/index.html');
  assert.equal(event.request.query_string, undefined);
  assert.deepEqual(event.request.headers, { 'User-Agent': 'Chrome' });
  assert.equal(event.message, '<hidden>');
  assert.equal(event.extra.crash_report, 'ticket=<hidden> and more');
  assert.deepEqual(event.extra.list, ['token: <hidden>']);
  assert.equal(event.exception.values[0].value, '<hidden>');
  assert.equal(event.exception.values[0].stacktrace.frames[0].filename, 'https://bw.example.org/play/openblack.js');
  assert.equal(event.exception.values[0].stacktrace.frames[0].abs_path, 'https://bw.example.org/play/a.js');
});
check('without consent the event asks Sentry to keep no IP address', () => {
  const event = scrubEvent({ user: { id: '7', username: 'matt', ip_address: '{{auto}}' },
    sdk: { name: 'sentry.javascript.browser', settings: { infer_ip: 'auto' } } });
  assert.equal(event.user, undefined);
  assert.equal(event.sdk.settings.infer_ip, 'never');
  assert.equal(event.sdk.name, 'sentry.javascript.browser');
  assert.doesNotMatch(JSON.stringify(event), /matt/);
});
check('the envelope takes the IP choice after the SDK wrote its own', () => {
  const make = () => [{}, [[{ type: 'event' }, { sdk: { settings: { infer_ip: 'auto' } } }],
    [{ type: 'attachment' }, 'text']]];
  assert.equal(applyIpChoice(make(), undefined)[1][0][1].sdk.settings.infer_ip, 'never');
  assert.equal(applyIpChoice(make(), { id: 7, username: 'matt' })[1][0][1].sdk.settings.infer_ip, 'auto');
  assert.equal(applyIpChoice(make(), undefined)[1][1][1], 'text');
});
check('with the choice full the event names only the id and the name of the account', () => {
  const event = scrubEvent({ user: { email: 'a@b.c', segment: 'x' } }, { id: 7, username: 'matt' });
  assert.deepEqual(event.user, { id: '7', username: 'matt', ip_address: '{{auto}}' });
  assert.equal(event.sdk.settings.infer_ip, 'auto');
});
check('the reports name the user that the game gives, and a stop forgets it', async () => {
  const { reports, sentry } = makeReports();
  reports.start(settings);
  await settle();
  const options = sentry.calls.find((call) => call[0] === 'init')[1];
  assert.equal(options.sendDefaultPii, true);
  assert.equal(options.beforeSend({ user: { ip_address: '1.2.3.4' } }, {}).user, undefined);
  reports.setUser({ id: 7, username: 'matt' });
  assert.equal(options.beforeSend({}, {}).user.username, 'matt');
  reports.setUser(undefined);
  assert.equal(options.beforeSend({}, {}).user, undefined);
  reports.setUser({ id: 7, username: 'matt' });
  reports.stop();
  assert.equal(reports.user, undefined);
});
check('the crash error has the type, the message and only the frames of the stack', () => {
  const stack = 'Error\n    at openblack_reports_stack (https://h/openblack.js:1:2)\n' +
    '    at openblack.wasm.main (https://h/openblack.wasm:wasm-function[12]:0x34)';
  const error = crashError('class std::runtime_error', 'Sentry test error 5', stack);
  assert.equal(error.name, 'class std::runtime_error');
  assert.equal(error.message, 'Sentry test error 5');
  assert.equal(error.stack.split('\n')[0], 'class std::runtime_error: Sentry test error 5');
  assert.equal(error.stack.split('\n').length, 3);
  assert.match(error.stack, /wasm-function\[12\]/);
});
check('a crash goes as a fatal exception with the crash report', async () => {
  const { reports, sentry } = makeReports();
  reports.start(settings);
  await settle();
  reports.crash('T', 'm', 'the report', '');
  const [, error, hint] = sentry.calls.at(-1);
  assert.equal(error.message, 'm');
  assert.equal(hint.captureContext.level, 'fatal');
  assert.equal(hint.captureContext.extra.crash_report, 'the report');
});
check('the desync event keeps the fields of the rule of the game', () => {
  const event = desyncEvent('Desync: Animal', 'warning', ['desync', 'Animal'], [['key', 'Animal'], ['slot', '1']],
    [['turn', '300']]);
  assert.deepEqual(event, { message: 'Desync: Animal', level: 'warning', fingerprint: ['desync', 'Animal'],
    tags: { key: 'Animal', slot: '1' }, extra: { turn: '300' } });
});
check('only the first failure of the page goes, as a fatal exception', async () => {
  const { reports, sentry } = makeReports();
  reports.start(settings);
  await settle();
  assert.equal(reports.pageFailed('RuntimeError: unreachable', new Error('unreachable')), true);
  assert.equal(reports.pageFailed('second', undefined), false);
  const exceptions = sentry.calls.filter((call) => call[0] === 'exception');
  assert.equal(exceptions.length, 1);
  assert.equal(exceptions[0][2].captureContext.level, 'fatal');
});
check('a worker that could not load the program is a load failure, an error of the code is not', () => {
  assert.equal(isThreadLoadFailure(new Event('error')), true);
  assert.equal(isThreadLoadFailure(new Error('unreachable')), false);
  assert.equal(isThreadLoadFailure(undefined), false);
  assert.equal(isThreadLoadFailure('worker sent an error!'), false);
});
check('a load failure of a thread is one warning with a tag of its own, and no failure of the page follows', async () => {
  const { reports, sentry } = makeReports();
  reports.start(settings);
  await settle();
  assert.equal(reports.threadLoadFailed(), true);
  assert.equal(reports.threadLoadFailed(), false);
  assert.equal(reports.pageFailed('second', undefined), false);
  const exceptions = sentry.calls.filter((call) => call[0] === 'exception');
  assert.equal(exceptions.length, 1);
  assert.equal(exceptions[0][1].message, k_ThreadLoadFailedText);
  assert.equal(exceptions[0][2].captureContext.level, 'warning');
  assert.equal(exceptions[0][2].captureContext.tags.failure, 'thread-load');
});
check('the failure of the page has the stack of the error of the game thread', async () => {
  const { reports, sentry } = makeReports();
  reports.start(settings);
  await settle();
  reports.threadError({ name: 'RuntimeError', message: 'unreachable',
    stack: 'RuntimeError: unreachable\n    at openblack.wasm.f (https://h/openblack.wasm:wasm-function[7]:0x9)' });
  reports.pageFailed('Uncaught RuntimeError: unreachable', 'not an error');
  const [, error] = sentry.calls.at(-1);
  assert.equal(error.name, 'RuntimeError');
  assert.match(error.stack, /wasm-function\[7\]/);
});
check('a failed load of the SDK stops the reports and writes a line', async () => {
  const { reports, lines } = makeReports(undefined, () => Promise.reject(new Error('no file')));
  reports.start(settings);
  await settle();
  assert.equal(reports.running, false);
  assert.match(lines.at(-1), /did not start/);
});
check('the address loses its parameters and the masks find the secrets', () => {
  assert.equal(addressWithoutParameters('https://h/p?a=1'), 'https://h/p');
  assert.equal(addressWithoutParameters('https://h/p'), 'https://h/p');
  assert.equal(maskSecrets('session_id=abc'), 'session_id=<hidden>');
});
check('the event of a failure of the game goes to Sentry with its fingerprint, tags and extras', async () => {
  const { reports, sentry } = makeReports();
  reports.start(settings);
  await settle();
  reports.event({
    message: 'The map download failed: piece', level: 'warning', fingerprint: ['map-download', 'piece'],
    tags: [['map_step', 'piece']], extras: [['map_offset', '0']],
  });
  await settle();
  const event = sentry.calls.find((call) => call[0] === 'event')[1];
  assert.equal(event.message, 'The map download failed: piece');
  assert.equal(event.level, 'warning');
  assert.deepEqual(event.fingerprint, ['map-download', 'piece']);
  assert.deepEqual(event.tags, { map_step: 'piece' });
  assert.deepEqual(event.extra, { map_offset: '0' });
});

// A WebAssembly module with a build id and the name of a debug file in its custom sections
const customSection = (name, bytes) => {
  const nameBytes = Buffer.from(name, 'utf-8');
  const body = Buffer.concat([Buffer.from([nameBytes.length]), nameBytes, Buffer.from(bytes)]);
  return Buffer.concat([Buffer.from([0, body.length]), body]);
};
const k_BuildId = [0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe,
  0xaa, 0xbb, 0xcc, 0xdd];
const wasmModule = (sections) => new WebAssembly.Module(Buffer.concat([
  Buffer.from([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]), ...sections]));

check('the page reads the build id and the debug file of the module', () => {
  const info = wasmModuleInfo(wasmModule([customSection('build_id', k_BuildId),
    customSection('external_debug_info', Buffer.from('openblack.debug.wasm'))]));
  assert.equal(info.buildId, '0123456789abcdef1032547698badcfeaabbccdd');
  assert.equal(info.debugFile, 'openblack.debug.wasm');
  assert.deepEqual(wasmModuleInfo(wasmModule([])), { buildId: undefined, debugFile: undefined });
});
const wasmEvent = () => ({
  exception: { values: [{ type: 'std::runtime_error', stacktrace: { frames: [
    { filename: 'https://example.org/play/game/openblack.js', function: 'callMain', lineno: 4, colno: 9 },
    { filename: 'https://example.org/play/game/openblack.wasm:wasm-function[1234]:0x5a6b7', function: 'main' },
    { abs_path: 'https://example.org/play/game/openblack.wasm:wasm-function[77]:0x1f', function: 'abort' },
  ] } }] },
});
check('the wasm frames of a report name the instruction and the debug image of the module', () => {
  const event = addWasmDebugImage(wasmEvent(), '0123456789abcdef1032547698badcfeaabbccdd');
  const [jsFrame, mainFrame, abortFrame] = event.exception.values[0].stacktrace.frames;
  assert.equal(jsFrame.instruction_addr, undefined, 'the frames of the JavaScript code do not change');
  assert.equal(jsFrame.lineno, 4);
  assert.deepEqual(mainFrame, { filename: 'https://example.org/play/game/openblack.wasm',
    abs_path: 'https://example.org/play/game/openblack.wasm', function: 'main', instruction_addr: '0x5a6b7',
    addr_mode: 'rel:0', platform: 'native' });
  assert.equal(abortFrame.instruction_addr, '0x1f');
  assert.deepEqual(event.debug_meta.images, [{ type: 'wasm', code_id: '0123456789abcdef1032547698badcfeaabbccdd',
    code_file: 'https://example.org/play/game/openblack.wasm', debug_id: '0123456789abcdef1032547698badcfe0' }]);
});
check('a report keeps its frames when the build id is not known or no frame is in the wasm code', () => {
  assert.deepEqual(addWasmDebugImage(wasmEvent(), undefined), wasmEvent());
  const event = { exception: { values: [{ stacktrace: { frames: [{ filename: 'https://example.org/a.js' }] } }] } };
  assert.equal(addWasmDebugImage(event, 'abcd').debug_meta, undefined);
  assert.deepEqual(addWasmDebugImage({ message: 'Desync: A' }, 'abcd'), { message: 'Desync: A' });
});
check('the options of the SDK add the debug image and remove the personal data', () => {
  const options = sentryOptions(fakeSentry(), settings, () => '', () => undefined, () => 'abcd');
  const event = options.beforeSend({ user: { id: 'x' }, exception: { values: [{ stacktrace: { frames: [
    { filename: 'https://example.org/openblack.wasm?args=x:wasm-function[1]:0x10' }] } }] } }, {});
  assert.equal(event.user, undefined);
  assert.equal(event.debug_meta.images[0].code_file, 'https://example.org/openblack.wasm');
  assert.equal(event.exception.values[0].stacktrace.frames[0].instruction_addr, '0x10');
});

check('the email addresses get a mark, and other texts with an "@" stay the same', () => {
  assert.equal(hideEmails('Mail bob.smith+tag@example.co.uk now.'), 'Mail <email> now.');
  assert.equal(hideEmails('turn 5 @ 3, a@b, @home'), 'turn 5 @ 3, a@b, @home');
});
check('a name matches as a full word with no difference for upper case', () => {
  assert.equal(hideNames('slot 1 (Player2), PLAYER2 left, player2x stays', ['Player2']),
    'slot 1 (<name>), <name> left, player2x stays');
  assert.equal(hideNames('Bob Smith and Bob joined', ['Bob', 'Bob Smith']), '<name> and <name> joined');
  // The characters of a name are not a pattern
  assert.equal(hideNames('a.b axb joined', ['a.b']), '<name> axb joined');
});
check('the names of digits and short names stay, so the log keeps its turns and keys', () => {
  const line = 'Desync report: key Animal of game 4f2a at turn 1200';
  assert.equal(hideNames(line, ['1200', 'A', '']), line);
});
check('the log and the texts of each event have no names that the game gave and no email addresses', async () => {
  const { reports, sentry } = makeReports();
  reports.logText = () => 'slot 1 (Mira) joined, mail mira@mail.example\npassword=hunter2';
  reports.start(settings);
  await settle();
  const options = sentry.calls.find((call) => call[0] === 'init')[1];
  reports.hideName('Mira');
  reports.hideName('7');
  const hint = {};
  const event = options.beforeSend({ message: 'Mira left', tags: { who: 'MIRA' }, extra: { turn: '7' } }, hint);
  assert.equal(hint.attachments[0].data, 'slot 1 (<name>) joined, mail <email>\npassword=<hidden>');
  assert.equal(event.message, '<name> left');
  assert.equal(event.tags.who, '<name>');
  assert.equal(event.extra.turn, '7');
  reports.stop();
});

(async () => {
  for (const { name, body } of tests) {
    await body();
    count += 1;
    console.log(`ok ${count} ${name}`);
  }
})().catch((error) => {
  console.error(error);
  process.exit(1);
});
