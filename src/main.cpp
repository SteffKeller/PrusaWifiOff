#include <WiFi.h>
#include <HTTPClient.h>
#include <M5Atom.h>


const char* WIFI_SSID = "diveintothenet";
const char* WIFI_PASS = "dtn24steffshome67L";

constexpr int INPUT_PIN = 23;              // dein Eingang
constexpr int INPUT_PIN_OVERRIDE = 39;              // dein Eingang

constexpr uint32_t DEBOUNCE_MS   = 60;
constexpr uint32_t OFF_DELAY_MS  = 10UL * 1000UL; // 10 sec

const char* URL_TOGGLE = "http://192.168.188.44/toggle";
const char* URL_OFF    = "http://192.168.188.44/relay?state=0";

// Eingang 23
bool lastState        = HIGH;
uint32_t lastChangeMs = 0;

// Override-Button (33)
bool overrideEnabled          = true;   // false = OFF, true = ON
bool overrideLastState        = HIGH;    // letzter Tasterzustand
uint32_t overrideLastChangeMs = 0;

// Timer-Logik
bool offTimerRunning = false;
uint32_t offTimerStart = 0;

void showOverrideOn();
void showOverrideOff();

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

void sendOff() {
  sendGet(URL_OFF);
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

// Anzeige: Override EIN = rotes "X"
void showOverrideOn() {
  M5.dis.clear();
  // Hintergrund leicht rot
  M5.dis.fillpix(0x000000);

  // einfaches "X" in rot auf der 5x5-Matrix
  // Koordinaten (x,y), 0..4 [web:114][web:116]
  uint32_t col = 0x0000FF;
  M5.dis.drawpix(0, 0, col);
  M5.dis.drawpix(1, 1, col);
  M5.dis.drawpix(2, 2, col);
  M5.dis.drawpix(3, 3, col);
  M5.dis.drawpix(4, 4, col);

  M5.dis.drawpix(4, 0, col);
  M5.dis.drawpix(3, 1, col);
  M5.dis.drawpix(2, 2, col);
  M5.dis.drawpix(1, 3, col);
  M5.dis.drawpix(0, 4, col);
}

// Anzeige: Override AUS = grünes "I"
void showOverrideOff() {
  M5.dis.clear();
  // Hintergrund leicht grün
  M5.dis.fillpix(0x000000);

  uint32_t col = 0x00FF00;
  // Einfaches "I" in der Mitte
  M5.dis.drawpix(2, 0, col);
  M5.dis.drawpix(2, 1, col);
  M5.dis.drawpix(2, 2, col);
  M5.dis.drawpix(2, 3, col);
  M5.dis.drawpix(2, 4, col);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  M5.begin(true, false, true);  // Display/LED an [web:116]

  pinMode(INPUT_PIN,          INPUT_PULLUP);
  pinMode(INPUT_PIN_OVERRIDE, INPUT_PULLUP);

  ensureWifi();

  lastState         = digitalRead(INPUT_PIN);
  overrideLastState = digitalRead(INPUT_PIN_OVERRIDE);

  // Startanzeige: Override AUS
  overrideEnabled = false;
  showOverrideOff();
}

void loop() {
  ensureWifi();
  uint32_t now = millis();

  // 1) Override-Taster einlesen + entprellen + toggeln
  bool ovReading = digitalRead(INPUT_PIN_OVERRIDE);
  if (ovReading != overrideLastState && (now - overrideLastChangeMs) > 10) {
    overrideLastChangeMs = now;
    overrideLastState = ovReading;
      Serial.printf("Override pressed \n" );

    // Flanke HIGH -> LOW = Button gedrückt
    if (ovReading == LOW) {
      overrideEnabled = !overrideEnabled;  // Zustand toggeln
      Serial.printf("Override toggled -> %s\n", overrideEnabled ? "ON" : "OFF");

      if (overrideEnabled) {
        // Override aktiviert: Timer abbrechen und "X" anzeigen
        if (offTimerRunning) {
          offTimerRunning = false;
          Serial.println("Override ON -> Timer abgebrochen");
        }
        showOverrideOn();
      } else {
        // Override deaktiviert: Anzeige "I"
        showOverrideOff();
      }
    }
  }

  // 2) Nur wenn Override AUS ist, Eingang 23 auswerten
  if (!overrideEnabled) {
    bool s = digitalRead(INPUT_PIN);

    // Entprellung + Flankenerkennung für Eingang 23
    if (s != lastState && (now - lastChangeMs) > DEBOUNCE_MS) {
      lastChangeMs = now;
      lastState = s;

      if (s == LOW) {
        // Eingang auf GND -> Timer starten
        offTimerRunning = true;
        offTimerStart = now;
        Serial.println("Input LOW -> Timer gestartet");
      } else {
        // Eingang wieder HIGH -> Timer abbrechen
        offTimerRunning = false;
        Serial.println("Input HIGH -> Timer abgebrochen");
      }
    }
  } else {
    // Override EIN: Sicherheitshalber Timer stoppen
    if (offTimerRunning) {
      offTimerRunning = false;
      // nur einmal loggen
      Serial.println("Override EIN -> Timer gestoppt");
    }
  }

  // 3) Timer überwachen (unabhängig von Anzeige, aber Override kann ihn schon gestoppt haben)
  if (offTimerRunning && (now - offTimerStart >= OFF_DELAY_MS)) {
    offTimerRunning = false;
    Serial.println("Timer abgelaufen -> Switch AUS");
    sendOff();
  }

  delay(5);
}
