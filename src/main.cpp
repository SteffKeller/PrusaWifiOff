#include <WiFi.h>
#include <HTTPClient.h>

const char* WIFI_SSID = "diveintothenet";
const char* WIFI_PASS = "dtn24steffshome67L";

// Nimm einen freien GPIO, der bei deinem M5Stack auch wirklich herausgeführt ist.
constexpr int INPUT_PIN = 23;   // Beispiel: GPIO26
constexpr uint32_t DEBOUNCE_MS = 5;

const char* URL = "http://192.168.188.44/toggle";

bool lastState = HIGH;
uint32_t lastChangeMs = 0;

void sendTogglePost() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(URL);

  // Optional, falls dein Server Content-Type erwartet:
  http.addHeader("Content-Type", "text/plain");

  // Leerer Body reicht oft für "/toggle"
  int code = http.GET();


  Serial.printf("GET %s -> HTTP %d\n", "http://192.168.188.44/toggle", code);
  if (code > 0) {
    Serial.println(http.getString());  // Debug-Ausgabe
  }
  http.end();
}

void ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
    delay(200);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi OK, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connect timeout.");
  }
}

void setup() {
  Serial.begin(115200);
  delay(00);

  pinMode(INPUT_PIN, INPUT_PULLUP);  // Taster/Schalter: Pin -> GND [web:17]

  ensureWifi();
  lastState = digitalRead(INPUT_PIN);

      sendTogglePost();  // HTTPClient POST [web:15]

}

void loop() {
  ensureWifi();

  bool s = digitalRead(INPUT_PIN);
  uint32_t now = millis();
  
  // einfache Entprellung + Flankenerkennung (HIGH -> LOW)
  if (s != lastState && (now - lastChangeMs) > DEBOUNCE_MS) {
    lastChangeMs = now;
    lastState = s;
    
    if (s == LOW) {
      sendTogglePost();  // HTTPClient POST [web:15]
    }
  }

  delay(5);
}
