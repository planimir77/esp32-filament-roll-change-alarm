#pragma once
#include <Arduino.h>

static const char WEB_OTA_PAGE[] PROGMEM = R"OTA(<!doctype html>
<html lang="bg"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Обновяване на ESP32</title>
<style>
body{font:17px system-ui,sans-serif;background:#101820;color:#eef4fa;margin:0;padding:24px}
main{max-width:600px;margin:auto}section{background:#203040;padding:20px;border-radius:12px;margin:20px 0}
h1{font-size:26px}h2{font-size:20px}input,button{font:inherit;max-width:100%;margin:10px 0}
button{padding:12px;background:#8ed8ff;border:0;border-radius:8px}button:disabled{opacity:.5}
progress{width:100%}a{color:#8ed8ff}#status{white-space:pre-wrap;overflow-wrap:anywhere}
</style><main><h1>Обновяване на ESP32</h1>
<p>Изберете файла за това устройство. Останете в същата Wi-Fi мрежа и не изключвайте захранването до приключване.</p>
<section><h2>Обновяване на програмата</h2>
<label for="firmware">Файл firmware.bin</label><br>
<input id="firmware" type="file" accept=".bin,application/octet-stream"><br>
<button data-target="firmware">Обнови програмата</button></section>
<section><h2>Обновяване на интерфейса</h2>
<label for="spiffs">Файл spiffs.bin</label><br>
<input id="spiffs" type="file" accept=".bin,application/octet-stream"><br>
<button data-target="spiffs">Обнови интерфейса</button>
<p>Заменя всички файлове на интерфейса и данните в SPIFFS.</p></section>
<progress id="progress" max="100" value="0"></progress>
<p id="status" role="status" aria-live="polite">Готово за избор на файл.</p>
<p><a href="/">Към контролния панел</a> · <a href="/update">Презареди страницата</a></p>
<script>
'use strict';
const token = '__OTA_TOKEN__';
const status = document.getElementById('status');
const progress = document.getElementById('progress');
function lock(value) { document.querySelectorAll('button,input').forEach(el => el.disabled = value); }
document.querySelectorAll('button').forEach(button => button.addEventListener('click', () => {
  const target = button.dataset.target;
  const file = document.getElementById(target).files[0];
  if (!file || file.name.toLowerCase() !== target + '.bin' || !file.size) {
    status.textContent = 'Изберете непразен файл ' + target + '.bin'; return;
  }
  lock(true); progress.value = 0; status.textContent = 'Качване…';
  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/update/' + target);
  xhr.setRequestHeader('Content-Type', 'application/octet-stream');
  xhr.setRequestHeader('X-OTA-Token', token);
  xhr.timeout = 180000;
  xhr.upload.onprogress = event => {
    if (event.lengthComputable) {
      progress.value = Math.round(event.loaded * 100 / event.total);
      status.textContent = progress.value < 100 ? 'Качване: ' + progress.value + '%' : 'Проверка и записване…';
    }
  };
  xhr.onload = () => {
    status.textContent = xhr.responseText || 'Неуспешно обновяване.';
    if (xhr.status === 200) {
      progress.value = 100;
      // Give the device time to reboot before offering another update.
      setTimeout(() => { location.href = '/update'; }, 15000);
    } else { lock(false); }
  };
  xhr.onerror = xhr.ontimeout = xhr.onabort = () => {
    status.textContent = 'Връзката прекъсна. Резултатът не е потвърден. Проверете Wi-Fi и презаредете /update преди нов опит.';
    lock(false);
  };
  xhr.send(file);
}));
</script></main></html>)OTA";
