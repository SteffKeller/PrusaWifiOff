#pragma once
#include <WebServer.h>
#include <Preferences.h>

// von main bereitgestellt
extern WebServer server;
extern bool autoPowerOffEnabled;
extern bool offTimerRunning;
extern bool reportValid;
extern bool reportRelay;
extern float reportPower;
extern float reportWs;
extern float reportTemperature;
extern String reportBootId;
extern float reportEnergyBoot;
extern uint32_t reportTimeBoot;
extern uint32_t offDelayMs ;
extern uint32_t offTimerStart;
extern Preferences prefs;

void startWebServer();
