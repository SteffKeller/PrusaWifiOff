#pragma once
#include <WebServer.h>

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
extern const uint32_t OFF_DELAY_MS;

void startWebServer();
