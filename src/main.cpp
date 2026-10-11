/*
  Light-triggered alarm with ESP32
  - Reads signal from K2055 sensor module (D0)
  - Pulses a local buzzer (replaces the 555 timer chip)
  - Sends a push notification to the phone via ntfy.sh when light is detected
  - Includes retry logic for weak/slow internet connections

  Wiring:
  - K2055 D0  -> GPIO 13  (input, dark=HIGH, light=LOW)
  - Buzzer "+" -> GPIO 5  (output, through a 1kΩ resistor to the base of BC547
                           if the buzzer draws more current than the GPIO pin can supply;
                           ESP32 GPIO handles ~12mA, small buzzers under 12mA
                           can be connected directly to the GPIO)
*/

#include <Arduino.h>
#include "WiFi.h"
#include <HTTPClient.h>
#include <Wire.h>
#include <AsyncTCP.h>
#include "FS.h"
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include <USSensor.h>
#include <HttpGETRequest.h>
#include <stdio.h>
#include <time.h>
#include <esp_timer.h>
#include <MuteTimer.h>
#include <MuteButton.h>
#include <NimBLEDevice.h>
#include <secrets.h>

const char* NTFY_TOPIC = "rast3d-pla"; // pick a unique name, subscribe to it in the ntfy app
//const char* NTFY_TOPIC = "rast3d-pla-test";

// ==== PINS ====
const int SENSOR_PIN = 13;   // D0 from K2055
const int BUZZER_PIN = 5;   // to the buzzer circuit
const int MUTE_PIN = 27;    // apply 3.3 V to temporarily silence the buzzer

// ==== BUZZER PULSE TIMING ====
const int BEEP_MS        = 100;   // how long each beep sounds
const int SHORT_PAUSE_MS = 150;   // short pause after the first beep
const int LONG_PAUSE_MS  = 2000;   // long pause after the second beep, before the pattern repeats

// ==== INTERNAL STATE ====
bool lampWasOn = false;        // whether the lamp was on in the previous loop cycle
unsigned long lastStepChange = 0;
bool isNotificationSent = false;

// Pattern steps: 0 = beep, 1 = short pause, 2 = beep, 3 = long pause (then repeats)
int patternStep = 0;
const bool  STEP_IS_ON[4]     = { true, false, true, false };
const unsigned long STEP_DURATION_MS[4] = { BEEP_MS, SHORT_PAUSE_MS, BEEP_MS, LONG_PAUSE_MS };

// ==== WIFI RECONNECT ====
unsigned long lastWifiReconnectAttempt = 0;
const unsigned long WIFI_RECONNECT_INTERVAL_MS = 10000;

AsyncWebServer server(80);

volatile bool muteRequested = false;
portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;
MuteTimer muteTimer;
MuteButton muteButton;
int64_t lastNotificationUs = -1;
char lastNotificationText[256] = "";
QueueHandle_t notificationQueue = nullptr;
struct Notification {
  char message[256];
  bool lowPriority;
};
bool wifiIpNotificationPending = false;

// ==== BLUETOOTH LOW ENERGY ====
const char* BLE_SERVICE_UUID =
    "7b3e0001-7d1f-4f6a-9d2b-3c4e5f607182";

const char* BLE_STATUS_UUID =
    "7b3e0002-7d1f-4f6a-9d2b-3c4e5f607182";

const char* BLE_EVENT_UUID =
    "7b3e0003-7d1f-4f6a-9d2b-3c4e5f607182";

NimBLEServer* bleServer = nullptr;
NimBLECharacteristic* bleStatusCharacteristic = nullptr;
NimBLECharacteristic* bleEventCharacteristic = nullptr;

unsigned long lastBleStatusUpdate = 0;

bool bleWifiStateKnown = false;
bool lastBleWifiConnected = false;

void onWifiGotIP(WiFiEvent_t) {
  portENTER_CRITICAL(&stateMux);
  wifiIpNotificationPending = true;
  portEXIT_CRITICAL(&stateMux);
}

void requestMute() {
  portENTER_CRITICAL(&stateMux);
  muteRequested = true;
  portEXIT_CRITICAL(&stateMux);
}

String statusJson() {
  char message[256];
  portENTER_CRITICAL(&stateMux);
  unsigned long remaining = muteTimer.remainingSeconds(millis());
  bool pending = muteRequested;
  int64_t sentAt = lastNotificationUs;
  memcpy(message, lastNotificationText, sizeof(message));
  portEXIT_CRITICAL(&stateMux);
  bool lampOn = digitalRead(SENSOR_PIN) == LOW;
  String escaped;
  for (const char* p = message; *p; ++p) {
    if (*p == '"' || *p == '\\') escaped += '\\';
    if ((unsigned char)*p >= 32) escaped += *p;
  }
  String json = "{\"lampOn\":" + String(lampOn ? "true" : "false");
  json += ",\"muted\":" + String(remaining > 0 ? "true" : "false");
  json += ",\"mutePending\":" + String(pending ? "true" : "false");
  json += ",\"muteRemainingSeconds\":" + String(remaining);
  json += ",\"wifiConnected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false");
  json += ",\"rssi\":" + String(WiFi.RSSI());
  json += ",\"uptimeSeconds\":" + String((unsigned long)(esp_timer_get_time() / 1000000));
  json += ",\"lastNotificationAgeSeconds\":";
  json += sentAt < 0 ? "null" : String((unsigned long)((esp_timer_get_time() - sentAt) / 1000000));
  json += ",\"lastNotificationText\":\"" + escaped + "\"}";
  return json;
}

void connectWiFi() {
  Serial.print("Connecting to WiFi");

  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected. IP address: " + WiFi.localIP().toString());

  } else {
    Serial.println("\nFailed to connect to WiFi — will retry later.");
  }
}

void notFound(AsyncWebServerRequest *request)
{
  request->send(404, "text/plain", "Not found");
}

String readFile(fs::FS &fs, const char *path)
{
  Serial.printf("Reading file from path: %s\r\n", path);

  File file = fs.open(path, FILE_READ);

  if (!file)
  {
    Serial.println("No file");
  }

  if (!file || file.isDirectory())
  {
    Serial.println("- empty file or failed to open file");
    return String();
  }
  Serial.println("- read from file:");

  String fileContent;
  while (file.available())
  {
    fileContent += String((char)file.read());
  }
  Serial.println("reading done");
  file.close();
  return fileContent;
}

void writeFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Writing file: %s\r\n", path);
  File file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println("- failed to open file for writing");
    return;
  }
  if (file.print(message))
  {
    Serial.println("- file written");
  }
  else
  {
    Serial.println("- write failed");
  }
  file.close();
}
void listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
  Serial.printf("Listing directory: %s\r\n", dirname);

  File root = fs.open(dirname);
  if (!root)
  {
    Serial.println("− failed to open directory");
    return;
  }
  if (!root.isDirectory())
  {
    Serial.println(" − not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file)
  {
    if (file.isDirectory())
    {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels)
      {
        listDir(fs, file.name(), levels - 1);
      }
    }
    else
    {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("\tSIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void sendPushNotification(const char* message, bool lowPriority = false);

// Copy text into the queue so temporary IP strings remain safe to send later.
bool queueNotification(const char* message, bool lowPriority = false) {
  Notification notification = {};
  snprintf(notification.message, sizeof(notification.message), "%s", message);
  notification.lowPriority = lowPriority;
  return notificationQueue && xQueueSend(notificationQueue, &notification, 0) == pdTRUE;
}

void notificationWorker(void*) {
  Notification notification;
  for (;;) {
    if (xQueueReceive(notificationQueue, &notification, portMAX_DELAY) == pdTRUE) {
      sendPushNotification(notification.message, notification.lowPriority);
    }
  }
}

void sendBleEvent(const char* eventText) {
  if (bleEventCharacteristic == nullptr) {
    return;
  }

  bleEventCharacteristic->setValue(eventText);

  if (bleServer != nullptr &&
      bleServer->getConnectedCount() > 0) {
    bleEventCharacteristic->notify();
  }

  Serial.print("BLE event: ");
  Serial.println(eventText);
}


void updateBleStatus(unsigned long now,
                     bool lampOn,
                     bool muted) {
  if (bleStatusCharacteristic == nullptr) {
    return;
  }

  bool wifiConnected = WiFi.status() == WL_CONNECTED;
  int rssi = wifiConnected ? WiFi.RSSI() : 0;
  bool buzzerActive = lampOn && !muted;

  char statusText[160];

  snprintf(
      statusText,
      sizeof(statusText),
      "UP=%lu;WIFI=%d;RSSI=%d;HEAP=%u;LAMP=%d;BUZZER=%d;MUTED=%d",
      now / 1000UL,
      wifiConnected ? 1 : 0,
      rssi,
      (unsigned int)ESP.getFreeHeap(),
      lampOn ? 1 : 0,
      buzzerActive ? 1 : 0,
      muted ? 1 : 0
  );

  bleStatusCharacteristic->setValue(statusText);
}


void setupBle() {
  NimBLEDevice::init("FilamentAlarm");

  bleServer = NimBLEDevice::createServer();

  // След прекъсване телефонът отново ще може да намери ESP32.
  bleServer->advertiseOnDisconnect(true);

  NimBLEService* service =
      bleServer->createService(BLE_SERVICE_UUID);

  bleStatusCharacteristic =
      service->createCharacteristic(
          BLE_STATUS_UUID,
          NIMBLE_PROPERTY::READ
      );

  bleEventCharacteristic =
      service->createCharacteristic(
          BLE_EVENT_UUID,
          NIMBLE_PROPERTY::READ |
          NIMBLE_PROPERTY::NOTIFY
      );

  bleStatusCharacteristic->setValue("STARTING");
  bleEventCharacteristic->setValue("BOOT");

  service->start();

  NimBLEAdvertising* advertising =
      NimBLEDevice::getAdvertising();

  advertising->setName("FilamentAlarm");
  advertising->addServiceUUID(BLE_SERVICE_UUID);
  advertising->enableScanResponse(true);
  advertising->start();

  Serial.println("BLE started: FilamentAlarm");
}

void setup()
{
  Wire.begin();
  Serial.begin(9600);
  Serial.println("Starting...");
  pinMode(SENSOR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(MUTE_PIN, INPUT_PULLDOWN);
  digitalWrite(BUZZER_PIN, LOW);

  setupBle();

  // Initialize SPIFFS
  if (!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.onEvent(onWifiGotIP, ARDUINO_EVENT_WIFI_STA_GOT_IP);

  connectWiFi();

  notificationQueue = xQueueCreate(4, sizeof(Notification));
  if (notificationQueue && xTaskCreate(notificationWorker, "notifications", 8192,
                                       nullptr, 1, nullptr) != pdPASS) {
    vQueueDelete(notificationQueue);
    notificationQueue = nullptr;
  }
  if (!notificationQueue) Serial.println("Notification worker unavailable");

  if (digitalRead(SENSOR_PIN) == LOW) {
    queueNotification("Ако лампата в момента не свети, значи има проблем с датчика.");
  }

  // Send web page with input fields to client
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/index.html", "text/html; charset=utf-8"); });
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse* response = request->beginResponse(200, "application/json; charset=utf-8", statusJson());
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
  });
  server.on("/mute", HTTP_POST, [](AsyncWebServerRequest *request) {
    requestMute();
    request->send(202, "application/json", "{\"accepted\":true}");
  });

  // Send Css
  server.on("/stylesheets/main.css", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/css", readFile(SPIFFS, "/stylesheets/main.css")); });
  // Send Javascript
  server.on("/index.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/javascript", readFile(SPIFFS, "/index.js")); });
  server.on("/getDiameter", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/plain", readFile(SPIFFS, "/inputDiameter.txt")); });
  server.onNotFound(notFound);
  server.begin();
}


// Generates a pulsed sound (beep-pause-beep-pause) without blocking with delay()
void pulseBuzzer() {
  unsigned long now = millis();

  if (now - lastStepChange >= STEP_DURATION_MS[patternStep]) {
    patternStep = (patternStep + 1) % 4;   // advance to the next step, wrap around after step 3
    digitalWrite(BUZZER_PIN, STEP_IS_ON[patternStep] ? HIGH : LOW);
    lastStepChange = now;
  }
}

// Sends a notification via ntfy.sh with up to 3 retries on failure/timeout
void sendPushNotification(const char* message, bool lowPriority) {
  // If WiFi dropped, try to reconnect before sending
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("No WiFi — notification will not be sent.");
      return;
    }
  }

  const int maxAttempts = 3;
  const int timeoutMs = 5000; // wait up to 5 sec for a server response

  for (int attempt = 1; attempt <= maxAttempts; attempt++) {
    HTTPClient http;
    String url = "https://ntfy.sh/" + String(NTFY_TOPIC);

    http.begin(url);
    http.setTimeout(timeoutMs);
    http.addHeader("Title", "PLA Extruder"); // title of the notification
    http.addHeader("Priority", lowPriority ? "low" : "high");
    http.addHeader("Tags", lowPriority ? "information_source" : "warning");

    int httpCode = http.POST(String(message));
    http.end();

    if (httpCode >= 200 && httpCode < 300) {
      portENTER_CRITICAL(&stateMux);
      snprintf(lastNotificationText, sizeof(lastNotificationText), "%s", message);
      lastNotificationUs = esp_timer_get_time();
      portEXIT_CRITICAL(&stateMux);
      Serial.println("Notification sent successfully (attempt " + String(attempt) + ")");
      return; // success — stop retrying
    } else {
      Serial.println("Attempt " + String(attempt) + " failed (code: " + String(httpCode) + "), retrying...");
      delay(1000); // short pause before next attempt
    }
  }

  Serial.println("Notification not delivered after " + String(maxAttempts) + " attempts.");
}


void loop()
{
  unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED &&
      now - lastWifiReconnectAttempt >= WIFI_RECONNECT_INTERVAL_MS) {

    lastWifiReconnectAttempt = now;
    Serial.println("WiFi disconnected — reconnecting...");
    WiFi.reconnect();
  }
  portENTER_CRITICAL(&stateMux);
  bool announceIP = wifiIpNotificationPending;
  wifiIpNotificationPending = false;
  portEXIT_CRITICAL(&stateMux);
  if (announceIP) {
    String ip = WiFi.localIP().toString();
    if (WiFi.status() == WL_CONNECTED && queueNotification(ip.c_str(), true)) {
      Serial.println("IP notification queued (low priority): " + ip);
    } else {
      portENTER_CRITICAL(&stateMux);
      wifiIpNotificationPending = true;
      portEXIT_CRITICAL(&stateMux);
    }
  }
  
  bool buttonPressed = muteButton.update(now, digitalRead(MUTE_PIN) == HIGH);
  portENTER_CRITICAL(&stateMux);
  bool webRequested = muteRequested;
  bool shouldMute = webRequested || buttonPressed;
  muteRequested = false;
  bool wasMuted = muteTimer.active();
  muteTimer.update(now, shouldMute);
  bool muted = muteTimer.active();
  portEXIT_CRITICAL(&stateMux);
  if (!wasMuted && muted) {
    Serial.println(webRequested ? "Mute started: web button" : "Mute started: GPIO 27 stable HIGH");
  }
  // D0 is HIGH when dark, LOW when the lamp is on
  bool lampIsOn = (digitalRead(SENSOR_PIN) == LOW);

  // Следене за промяна в Wi-Fi връзката
  bool wifiConnected = WiFi.status() == WL_CONNECTED;

  if (!bleWifiStateKnown) {
    bleWifiStateKnown = true;
    lastBleWifiConnected = wifiConnected;
  }
  else if (wifiConnected != lastBleWifiConnected) {
    lastBleWifiConnected = wifiConnected;

    sendBleEvent(
        wifiConnected ? "WIFI_OK" : "WIFI_LOST"
    );
  }

  // Събитие при промяна на лампата
  if (lampIsOn != lampWasOn) {
    sendBleEvent(
        lampIsOn ? "LAMP_ON" : "LAMP_OFF"
    );
  }

  // Обновяване на четимия статус веднъж в секунда
  if (now - lastBleStatusUpdate >= 1000) {
    lastBleStatusUpdate = now;
    updateBleStatus(now, lampIsOn, muted);
  }

  if (lampIsOn) {
    if (muted) {
      digitalWrite(BUZZER_PIN, LOW);
      patternStep = 3;
      lastStepChange = now - STEP_DURATION_MS[3];
    } else {
      pulseBuzzer();
    }
    if (!isNotificationSent) {
      isNotificationSent = queueNotification("Предстои смяна на ролката.");
    }
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    isNotificationSent = false; // reset notification flag when lamp is off 
  }

  lampWasOn = lampIsOn;
  delay(20); // short pause for reading stability
}
