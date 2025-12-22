#include <WiFi.h>
#include <HTTPClient.h>
#include <M5Atom.h>


const char* WIFI_SSID = "diveintothenet";
const char* WIFI_PASS = "dtn24steffshome67L";

constexpr int INPUT_PIN = 23;              // dein Eingang
constexpr int INPUT_PIN_OVERRIDE = 39;              // dein Eingang

constexpr uint32_t DEBOUNCE_MS   = 60;
//constexpr uint32_t OFF_DELAY_MS  = 10UL * 60UL * 1000UL; // 10 Minuten
constexpr uint32_t OFF_DELAY_MS  = 10UL * 1000UL;         // 10 Sekunden (Test)

// -----------------------------------------------------------------------------
// URLs
// -----------------------------------------------------------------------------
const char* URL_TOGGLE = "http://192.168.188.44/toggle";
const char* URL_OFF    = "http://192.168.188.44/relay?state=0";

// -----------------------------------------------------------------------------
// Zustände
// -----------------------------------------------------------------------------

// Eingang 23
bool     lastState        = HIGH;
uint32_t lastChangeMs     = 0;

// Override-Button (33)
bool     overrideEnabled          = true; // false = Override AUS, true = Override EIN
bool     overrideLastState        = HIGH;
uint32_t overrideLastChangeMs     = 0;

// Timer-Logik
bool     offTimerRunning = false;
uint32_t offTimerStart   = 0;

// -----------------------------------------------------------------------------
// Hilfsfunktionen Anzeige
// -----------------------------------------------------------------------------
void clearMatrix() {
  M5.dis.clear();
}

// einfaches "I" in der Mitte
void drawI(uint32_t col) {
  // Spalte x = 2, Zeilen 0..4 [web:114]
  M5.dis.drawpix(2, 0, col);
  M5.dis.drawpix(2, 1, col);
  M5.dis.drawpix(2, 2, col);
  M5.dis.drawpix(2, 3, col);
  M5.dis.drawpix(2, 4, col);
}

// Override EIN: rotes X
void showOverrideOn() {
  clearMatrix();
  // dunkler Hintergrund
  M5.dis.fillpix(0x000000);

  uint32_t col = 0x0000FF;

  // Diagonalen für "X"
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

// Override AUS: Grundanzeige (grünes I, keine Progressbar)
void showOverrideOffBase() {
  clearMatrix();
  // hier kein vollständiges Fill, Progressbar übernimmt Zeilen unten
  drawI(0x00FF00);  // I in Grün
}

// Progressbar-Zeilen zeichnen (orange) von unten nach oben
void drawProgressBar(uint8_t filledRows) {
  // filledRows: 0..5, Zeile 4 = unten, 0 = oben
  uint32_t orange = 0xFF8000;

  // Untere 'filledRows' Zeilen füllen
  for (int y = 4; y >= 0; --y) {
    bool fillRow = (4 - y) < filledRows; // 0 -> y=4, 1 -> y=3, ...
    for (int x = 0; x < 5; ++x) {
      if (fillRow) {
        M5.dis.drawpix(x, y, orange);
      } else {
        // wenn es nicht die I-Spalte ist, auf "aus" setzen
        if (x != 2) {
          M5.dis.drawpix(x, y, 0x000000);
        }
      }
    }
  }

  // Danach I wieder drüber zeichnen (damit es nicht übermalt wird)
  drawI(0x00FF00);
}

// -----------------------------------------------------------------------------
// Netzwerk-Funktionen
// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);

  M5.begin(true, false, true);  // LED/Display an [web:114]

  pinMode(INPUT_PIN,          INPUT_PULLUP);
  pinMode(INPUT_PIN_OVERRIDE, INPUT_PULLUP);

  ensureWifi();

  lastState         = digitalRead(INPUT_PIN);
  overrideLastState = digitalRead(INPUT_PIN_OVERRIDE);

  overrideEnabled = false;
  showOverrideOffBase();
}

// -----------------------------------------------------------------------------
// Loop
// -----------------------------------------------------------------------------
void loop() {
  ensureWifi();
  uint32_t now = millis();

  // 1) Override-Taster einlesen + entprellen + toggeln
  bool ovReading = digitalRead(INPUT_PIN_OVERRIDE);
  if (ovReading != overrideLastState && (now - overrideLastChangeMs) > DEBOUNCE_MS) {
    overrideLastChangeMs = now;
    overrideLastState = ovReading;

    // Flanke HIGH -> LOW = Taster gedrückt
    if (ovReading == LOW) {
      overrideEnabled = !overrideEnabled;
      Serial.printf("Override toggled -> %s\n", overrideEnabled ? "ON" : "OFF");

      if (overrideEnabled) {
        // Override EIN: Timer abbrechen, X anzeigen, Progressbar weg
        if (offTimerRunning) {
          offTimerRunning = false;
          Serial.println("Override ON -> Timer abgebrochen");
        }
        showOverrideOn();
      } else {
        // Override AUS: zurück auf grünes I ohne Progress
        offTimerRunning = false;
        showOverrideOffBase();
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
        offTimerStart   = now;
        Serial.println("Input LOW -> Timer gestartet");
      } else {
        // Eingang wieder HIGH -> Timer abbrechen
        offTimerRunning = false;
        Serial.println("Input HIGH -> Timer abgebrochen");
        // zurück zur Basisanzeige (nur I)
        showOverrideOffBase();
      }
    }
  } else {
    // Override EIN: Sicherheitshalber Timer stoppen
    if (offTimerRunning) {
      offTimerRunning = false;
      Serial.println("Override EIN -> Timer gestoppt");
    }
  }

  // 3) Timer überwachen + Progressbar
  if (offTimerRunning) {
    uint32_t elapsed = now - offTimerStart;
    if (elapsed >= OFF_DELAY_MS) {
      offTimerRunning = false;
      Serial.println("Timer abgelaufen -> Switch AUS");
      sendOff();

      // Nach Ablauf z.B. rotes I (Schalter aus)
      clearMatrix();
      M5.dis.fillpix(0x000000);
      drawI(0x000000);
    } else {
      // Fortschritt 0.0 … 1.0
      float progress = (float)elapsed / (float)OFF_DELAY_MS;
      if (progress < 0.0f) progress = 0.0f;
      if (progress > 1.0f) progress = 1.0f;

      // 0..5 Reihen, von unten nach oben
      uint8_t filledRows = (uint8_t)(progress * 5.0f + 0.999f); // aufrunden
      if (filledRows > 5) filledRows = 5;

      // Progressbar zeichnen (orange), I bleibt grün
      drawProgressBar(filledRows);
    }
  }

  delay(5);
}
