'use strict';
const el = id => document.getElementById(id);
let sending = false, muted = false, pending = false, online = false;
function duration(seconds) {
  const days = Math.floor(seconds / 86400);
  return (days ? days + ' дни ' : '') + Math.floor(seconds / 3600) % 24 +
    ' ч ' + Math.floor(seconds / 60) % 60 + ' мин ' + seconds % 60 + ' сек';
}
async function api(path, options = {}) {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 5000);
  try {
    const response = await fetch(path, { ...options, cache: 'no-store', signal: controller.signal });
    if (!response.ok) throw new Error('HTTP ' + response.status);
    return await response.json();
  } finally { clearTimeout(timeout); }
}
function updateButton() { el('mute').disabled = !online || sending || muted || pending; }
async function refresh() {
  try {
    const state = await api('/status');
    online = true; muted = state.muted; pending = state.mutePending;
    el('connection').textContent = '● Устройството е свързано';
    el('connection').className = 'connected';
    el('lamp').textContent = state.lampOn ? 'Свети' : 'Не свети';
    el('lamp').className = 'value ' + (state.lampOn ? 'alert' : '');
    el('buzzer').textContent = muted ? 'Заглушен' : state.lampOn ? 'Активен' : 'Изключен';
    el('remaining').textContent = muted ? 'Още ' + state.muteRemainingSeconds + ' секунди' : 'Няма активна пауза';
    el('progress').value = state.muteRemainingSeconds;
    el('notification').textContent = state.lastNotificationAgeSeconds === null ?
      'Няма успешно изпратено известие след стартирането.' : state.lastNotificationText;
    el('notification-age').textContent = state.lastNotificationAgeSeconds === null ?
      'Историята се изчиства при рестартиране.' : 'Преди ' + duration(state.lastNotificationAgeSeconds);
    el('wifi').textContent = state.wifiConnected ? state.rssi + ' dBm' : 'Няма връзка';
    el('uptime').textContent = duration(state.uptimeSeconds);
  } catch (_) {
    online = false;
    el('connection').textContent = 'Няма връзка — показаните данни може да са остарели.';
    el('connection').className = 'alert';
  } finally { updateButton(); setTimeout(refresh, 1000); }
}
el('mute').addEventListener('click', async () => {
  sending = true; updateButton();
  el('action').textContent = 'Изпращане…';
  try {
    await api('/mute', { method: 'POST' });
    pending = true;
    el('action').textContent = 'Командата е приета. Състоянието ще се обнови автоматично.';
  } catch (_) {
    el('action').textContent = 'Няма потвърждение. Проверете състоянието преди нов опит.';
  } finally { sending = false; updateButton(); }
});
refresh();
