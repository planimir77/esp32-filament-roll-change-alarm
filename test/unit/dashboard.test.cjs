const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '../../data/index.js'), 'utf8');

async function panel() {
  const nodes = {}, requests = [], timers = new Map();
  let timerId = 0;
  const h = { state: { lampOn: false, muted: false, mutePending: false,
    muteRemainingSeconds: 0, wifiConnected: true, rssi: -61,
    uptimeSeconds: 65, lastNotificationAgeSeconds: null }, failure: false, status: 200 };
  const context = vm.createContext({
    document: { getElementById(id) { return nodes[id] ||= { textContent: '',
      addEventListener(event, fn) { this[event] = fn; } }; } },
    AbortController,
    setTimeout(fn, ms) { timers.set(++timerId, {fn, ms}); return timerId; },
    clearTimeout(id) { timers.delete(id); },
    fetch: async (url, options) => {
      requests.push({url, options});
      if (h.failure) throw Error('offline');
      return { ok: h.status >= 200 && h.status < 300, status: h.status,
        json: async () => url === '/status' ? h.state : {accepted:true} };
    }
  });
  vm.runInContext(source, context);
  await new Promise(setImmediate);
  return Object.assign(h, {nodes, requests, timers,
    refresh: () => vm.runInContext('refresh()', context),
    duration: seconds => vm.runInContext('duration(' + seconds + ')', context)});
}
test('initial lamp off, buzzer off, no previous notification', async () => {
  const h = await panel();
  assert.equal(h.nodes.lamp.textContent, 'Не свети');
  assert.equal(h.nodes.buzzer.textContent, 'Изключен');
  assert.match(h.nodes.notification.textContent, /Няма успешно/);
  assert.equal(h.nodes.mute.disabled, false);
  assert.equal(h.nodes.wifi.textContent, '-61 dBm');
});
test('lamp on activates the buzzer display', async () => {
  const h = await panel(); h.state.lampOn = true; await h.refresh();
  assert.equal(h.nodes.lamp.textContent, 'Свети');
  assert.equal(h.nodes.buzzer.textContent, 'Активен');
});
test('mute countdown and expiry while lamp remains on', async () => {
  const h = await panel();
  Object.assign(h.state, {lampOn:true, muted:true, muteRemainingSeconds:23});
  await h.refresh();
  assert.equal(h.nodes.buzzer.textContent, 'Заглушен');
  assert.equal(h.nodes.remaining.textContent, 'Още 23 секунди');
  assert.equal(h.nodes.progress.value, 23);
  assert.equal(h.nodes.mute.disabled, true);
  Object.assign(h.state, {muted:false, muteRemainingSeconds:0});
  await h.refresh();
  assert.equal(h.nodes.buzzer.textContent, 'Активен');
  assert.equal(h.nodes.mute.disabled, false);
});
test('lamp going out during mute stays off after expiry', async () => {
  const h = await panel();
  Object.assign(h.state, {muted:true, muteRemainingSeconds:1});
  await h.refresh(); assert.equal(h.nodes.buzzer.textContent, 'Заглушен');
  Object.assign(h.state, {muted:false, muteRemainingSeconds:0});
  await h.refresh(); assert.equal(h.nodes.buzzer.textContent, 'Изключен');
});
test('mute button POST and pending state', async () => {
  const h = await panel(); await h.nodes.mute.click();
  const request = h.requests.find(r => r.url === '/mute');
  assert.equal(request.options.method, 'POST');
  assert.equal(h.nodes.mute.disabled, true);
  assert.match(h.nodes.action.textContent, /приета/);
});
test('HTTP failure does not claim mute succeeded', async () => {
  const h = await panel(); h.status = 503; await h.nodes.mute.click();
  assert.match(h.nodes.action.textContent, /Няма потвърждение/);
});
test('network failure disables control and recovery restores it', async () => {
  const h = await panel(); h.failure = true; await h.refresh();
  assert.equal(h.nodes.mute.disabled, true);
  assert.match(h.nodes.connection.textContent, /остарели/);
  h.failure = false; await h.refresh(); assert.equal(h.nodes.mute.disabled, false);
});
test('HTTP status failure disables control', async () => {
  const h = await panel(); h.status = 500; await h.refresh();
  assert.equal(h.nodes.mute.disabled, true);
});
test('notification text is rendered as text, with age', async () => {
  const h = await panel();
  Object.assign(h.state, {lastNotificationAgeSeconds:65, lastNotificationText:'<b>Тест</b>'});
  await h.refresh();
  assert.equal(h.nodes.notification.textContent, '<b>Тест</b>');
  assert.equal(h.nodes['notification-age'].textContent, 'Преди 0 ч 1 мин 5 сек');
});
test('uptime formatting covers seconds, hours, days', async () => {
  const h = await panel();
  assert.equal(h.duration(0), '0 ч 0 мин 0 сек');
  assert.equal(h.duration(3661), '1 ч 1 мин 1 сек');
  assert.equal(h.duration(90061), '1 дни 1 ч 1 мин 1 сек');
});
test('poll schedules next refresh and clears request timeout', async () => {
  const h = await panel();
  assert.equal([...h.timers.values()].filter(t => t.ms === 1000).length, 1);
  assert.equal([...h.timers.values()].filter(t => t.ms === 5000).length, 0);
  assert.equal(h.requests[0].options.cache, 'no-store');
});
