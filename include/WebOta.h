#pragma once
#include <ESPAsyncWebServer.h>

void setupWebOta(AsyncWebServer& server, bool filesystemMounted);
void pollWebOta();
// Called by HTTP handlers on the AsyncTCP task.
bool webOtaFilesystemAvailable();
