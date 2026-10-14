const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const page = fs.readFileSync(path.join(__dirname, '../../include/WebOtaPage.h'), 'utf8');
const script = page.match(/<script>([\s\S]*?)<\/script>/)[1].replace('__OTA_TOKEN__', 'test-token');

function panel() {
  const nodes = { status: {}, progress: {}, firmware: {files: []}, spiffs: {files: []} };
  const buttons = ['firmware', 'spiffs'].map(target => ({
    dataset: {target}, addEventListener(event, fn) { this[event] = fn; }
  }));
  const requests = [], timers = [], location = {};
  class XHR {
    constructor() { this.upload = {}; this.headers = {}; requests.push(this); }
    open(method, url) { this.method = method; this.url = url; }
    setRequestHeader(key, value) { this.headers[key] = value; }
    send(body) { this.body = body; }
  }
  vm.runInNewContext(script, {
    document: { getElementById: id => nodes[id],
      querySelectorAll: selector => selector === 'button' ? buttons : [...buttons, nodes.firmware, nodes.spiffs] },
    XMLHttpRequest: XHR, location, setTimeout: (fn, ms) => timers.push({fn, ms})
  });
  return {nodes, buttons, requests, timers, location};
}

test('reject missing, empty and swapped images before upload', () => {
  for (const file of [undefined, {name:'firmware.bin', size:0}, {name:'spiffs.bin', size:100}]) {
    const h = panel(); h.nodes.firmware.files = file ? [file] : [];
    h.buttons[0].click(); assert.equal(h.requests.length, 0);
  }
});
for (const [index, target] of ['firmware', 'spiffs'].entries()) {
  test(`${target}: sends binary to its own endpoint and waits for server confirmation`, () => {
    const h = panel(); const file = {name:target + '.bin', size:1234};
    h.nodes[target].files = [file]; h.buttons[index].click();
    const xhr = h.requests[0];
    assert.equal(xhr.url, '/update/' + target); assert.equal(xhr.method, 'POST');
    assert.equal(xhr.body, file); assert.equal(xhr.headers['X-OTA-Token'], 'test-token');
    assert.equal(xhr.headers['Content-Type'], 'application/octet-stream');
    assert.ok(h.buttons.every(b => b.disabled));
    xhr.upload.onprogress({lengthComputable:true, loaded:1234, total:1234});
    assert.equal(h.timers.length, 0); // 100% upload is not a successful flash.
    xhr.status = 200; xhr.responseText = 'OK'; xhr.onload();
    assert.equal(h.nodes.status.textContent, 'OK');
    assert.ok(h.buttons.every(b => b.disabled));
    assert.equal(h.timers[0].ms, 15000); h.timers[0].fn();
    assert.equal(h.location.href, '/update');
  });
}
test('server rejection and network failure permit retry without claiming success', () => {
  for (const outcome of ['rejected', 'onerror', 'ontimeout', 'onabort']) {
    const h = panel(); h.nodes.firmware.files = [{name:'firmware.bin', size:1234}];
    h.buttons[0].click(); const xhr = h.requests[0];
    if (outcome === 'rejected') { xhr.status = 400; xhr.responseText = 'Invalid image'; xhr.onload(); }
    else xhr[outcome]();
    assert.ok(h.buttons.every(b => !b.disabled)); assert.equal(h.timers.length, 0);
    assert.ok(h.nodes.status.textContent.length > 0);
  }
});
