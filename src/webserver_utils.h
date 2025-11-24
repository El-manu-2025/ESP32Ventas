#ifndef WEBSERVER_UTILS_H
#define WEBSERVER_UTILS_H

#include <WebServer.h>

extern WebServer server;

void setupWebServer();
String pageRootHTML();
void handleRoot();
void handleStatusJSON();

#endif
