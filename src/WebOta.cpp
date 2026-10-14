#include <WebOta.h>
#include <WebOtaPage.h>
#include <SPIFFS.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <atomic>

namespace {
// Upload callbacks run on the AsyncTCP task; only restartAt crosses into loop().
AsyncWebServerRequest* owner = nullptr;
size_t received = 0;
size_t expected = 0;
bool fsMounted = false;
bool filesystemUpload = false;
bool finished = false;
String failure;
String csrfToken;
std::atomic<uint32_t> restartAt{0};

void fail(const String& message) {
  failure = message;
  if (Update.isRunning()) Update.abort();
}

void releaseUpload() {
  if (Update.isRunning()) Update.abort();
  if (filesystemUpload && !fsMounted) fsMounted = SPIFFS.begin(false);
  owner = nullptr;
}

bool authorized(AsyncWebServerRequest* request) {
  return request->header("X-OTA-Token") == csrfToken;
}

void receive(AsyncWebServerRequest* request, uint8_t* data,
             size_t len, size_t index, size_t total, bool isFilesystem) {
  if (!authorized(request) || restartAt.load() != 0) return;
  if (index == 0) {
    if (owner != nullptr) return;
    owner = request;
    request->client()->setRxTimeout(30);
    received = 0;
    expected = total;
    filesystemUpload = isFilesystem;
    finished = false;
    failure = "";
    request->onDisconnect([request]() {
      if (owner == request) releaseUpload();
    });
    if (request->contentType() != "application/octet-stream" ||
        total == 0 || total != request->contentLength()) {
      fail("Невалиден или празен файл."); return;
    }
    const esp_partition_t* partition = isFilesystem
        ? esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, nullptr)
        : esp_ota_get_next_update_partition(nullptr);
    if (!partition || total > partition->size || (isFilesystem && total != partition->size)) {
      fail("Размерът на файла не съответства на flash дяла. Използвайте build от същия PlatformIO проект."); return;
    }
    if (len == 0 || (!isFilesystem && data[0] != 0xE9) || (isFilesystem && data[0] == 0xE9)) {
      fail("Грешен тип файл. Проверете избора firmware.bin / spiffs.bin."); return;
    }
    if (isFilesystem) {
      SPIFFS.end();
      fsMounted = false;
    }
    if (!Update.begin(total, isFilesystem ? U_SPIFFS : U_FLASH)) {
      fail(String("Не може да започне обновяването: ") + Update.errorString()); return;
    }
  }
  if (owner != request || failure.length() || finished) return;
  if (index != received || total != expected || len > expected - received) {
    fail("Непълен или некоректен трансфер."); return;
  }
  if (Update.write(data, len) != len) {
    fail(String("Грешка при запис: ") + Update.errorString()); return;
  }
  received += len;
  // Commit only in the request completion callback, after the full body arrives.
}

void complete(AsyncWebServerRequest* request) {
  if (!authorized(request)) {
    request->send(403, "text/plain; charset=utf-8", "Презаредете /update и опитайте отново."); return;
  }
  if (restartAt.load() != 0 || (owner && owner != request)) {
    request->send(409, "text/plain; charset=utf-8", "Вече се извършва обновяване. Изчакайте."); return;
  }
  if (owner != request) {
    request->send(400, "text/plain; charset=utf-8", "Липсва файл за обновяване."); return;
  }
  if (failure.isEmpty() && (received != expected || received == 0)) fail("Файлът не е получен изцяло.");
  if (failure.isEmpty() && !Update.end()) fail(String("Неуспешна проверка: ") + Update.errorString());
  if (failure.isEmpty() && filesystemUpload) {
    fsMounted = SPIFFS.begin(false);
    if (!fsMounted) fail("Невалиден SPIFFS образ. Качете правилния spiffs.bin през /update.");
  }
  if (failure.length()) {
    request->send(400, "text/plain; charset=utf-8", failure);
    releaseUpload();
    return;
  }
  finished = true;
  request->send(200, "text/plain; charset=utf-8", "Обновяването е успешно. ESP32 се рестартира. Страницата ще се презареди след 15 секунди.");
  // Never restart inside an AsyncTCP callback; allow time for the response.
  uint32_t deadline = millis() + 2000;
  restartAt.store(deadline ? deadline : 1);
}
}

bool webOtaFilesystemAvailable() { return fsMounted && !(owner && filesystemUpload); }

void setupWebOta(AsyncWebServer& server, bool filesystemMounted) {
  fsMounted = filesystemMounted;
  char token[33];
  snprintf(token, sizeof(token), "%08lx%08lx%08lx%08lx",
           (unsigned long)esp_random(), (unsigned long)esp_random(),
           (unsigned long)esp_random(), (unsigned long)esp_random());
  csrfToken = token;
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest* request) {
    String page = FPSTR(WEB_OTA_PAGE);
    page.replace("__OTA_TOKEN__", csrfToken);
    auto* response = request->beginResponse(200, "text/html; charset=utf-8", page);
    response->addHeader("Cache-Control", "no-store");
    response->addHeader("X-Frame-Options", "DENY");
    request->send(response);
  });
  server.on("/update/firmware", HTTP_POST, complete, nullptr,
            [](AsyncWebServerRequest* r, uint8_t* d, size_t n, size_t i, size_t t) { receive(r, d, n, i, t, false); });
  server.on("/update/spiffs", HTTP_POST, complete, nullptr,
            [](AsyncWebServerRequest* r, uint8_t* d, size_t n, size_t i, size_t t) { receive(r, d, n, i, t, true); });
}

void pollWebOta() {
  uint32_t deadline = restartAt.load();
  if (deadline && int32_t(millis() - deadline) >= 0) ESP.restart();
}
