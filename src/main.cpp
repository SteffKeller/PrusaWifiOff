#include <WiFi.h>
#include <HTTPClient.h>

const char* WIFI_SSID = "diveintothenet";
const char* WIFI_PASS = "dtn24steffshome67L";

constexpr int INPUT_PIN = 23;              // dein Eingang
constexpr int INPUT_PIN_OVERRIDE = 33;              // dein Eingang

constexpr uint32_t DEBOUNCE_MS = 60;
//constexpr uint32_t OFF_DELAY_MS = 10UL * 60UL * 1000UL; // 10 Minuten
constexpr uint32_t OFF_DELAY_MS = 10UL * 1000UL; // 10 sec


const char* URL_TOGGLE = "http://192.168.188.44/toggle";
// Falls es für AUS einen anderen Endpunkt gibt, hier eintragen:
const char* URL_OFF = "http://192.168.188.44/relay?state=0";

bool lastState = HIGH;
uint32_t lastChangeMs = 0;

// Timer-Logik
bool offTimerRunning = false;
uint32_t offTimerStart = 0;

void sendGet(const char* url) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(url);
  int code = http.GET();
  Serial.printf("GET %s -> HTTP %d\n", url, code);
  if (code > 0) {
    Serial.println(http.getString());
  }
  http.end();
}

void sendToggle() {
  sendGet(URL_TOGGLE);
}

// Wenn dein Switch wirklich nur /toggle kennt, kannst du hier auch einfach sendToggle() aufrufen.
// Dann wird nach 10 min erneut getoggelt (also aus, wenn er vorher ein war).
void sendOff() {
  // Variante 1: eigener OFF-Endpunkt
  sendGet(URL_OFF);

  // Variante 2: erneut toggle (falls der Schalter dann sicher „aus“ ist)
  // sendToggle();
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
  delay(200);

  pinMode(INPUT_PIN, INPUT_PULLUP);
  pinMode(INPUT_PIN_OVERRIDE, INPUT_PULLUP);


  ensureWifi();
  lastState = digitalRead(INPUT_PIN);
}

void loop() {
  ensureWifi();

  bool s = digitalRead(INPUT_PIN);
  uint32_t now = millis();

  if(digitalRead(INPUT_PIN_OVERRIDE) == LOW) {

    
    // Entprellung + Flankenerkennung
  if (s != lastState && (now - lastChangeMs) > DEBOUNCE_MS) {
    lastChangeMs = now;
    lastState = s;
    
    if (s == LOW) {
      // Eingang wurde auf GND gezogen -> Timer starten
      offTimerRunning = true;
      offTimerStart = now;
      Serial.println("Input LOW -> 10min-Timer gestartet");
    } else { 
      // Eingang wieder HIGH -> Timer abbrechen
      offTimerRunning = false;
      Serial.println("Input HIGH -> Timer abgebrochen");
    }
  }
  } else {
    // Override aktiv, Timer zurücksetzen
    if (offTimerRunning) {
      offTimerRunning = false;
      Serial.println("Override aktiv -> Timer abgebrochen");
    }
  }

  // Timer überwachen, ohne delay() zu blockieren [web:71][web:76]
  if (offTimerRunning && (now - offTimerStart >= OFF_DELAY_MS)) {
    offTimerRunning = false;
    Serial.println("10min abgelaufen -> Switch AUS");
    sendOff();
  }

  delay(5);
}
