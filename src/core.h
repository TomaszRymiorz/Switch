#include <Wire.h>
#include <SPI.h>
#include <FS.h>
#include <RTClib.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include "main.h"

RTC_DS1307 RTC;
ESP8266WebServer server(80);
HTTPClient HTTP;

const int version = 9;
bool offline = true; // set by "main.cpp / setup()"
const String baseURL = "";
const bool keepLog = false;

const char daysOfTheWeek[7][12] = {"s", "o", "u", "e", "h", "r", "a"};
char hostName[16] = {0};

String ssid = "";
String password = "";

uint32_t startTime = 0;
uint32_t loopTime = 0;
uint32_t updateTime = 0;
int uprisings = 1;
int offset = 0;

String smartString = "0";
Smart *smartArray;
int smartCount = 0;

bool twilight = false;
bool sendingError = false;
bool blockGetOnlineData = false;

bool strContains(String text, String value);
bool hasTimeChanged();
void note(String text);
bool writeObjectToFile(String name, JsonObject& jsonObject);
void connectingToWifi();
void initiatingWPS();
void receivedTheData();
void requestForLogs();
void clearLogs();
void deleteWiFiSettings();
void onlineSwitch();
String get1Smart(int index);
void getOnlineData();
void putOnlineData(String variant, String values);
void putOfflineData(String values);
void getOfflineData();


bool strContains(String text, String value) {
  return text.indexOf(value) != -1;
}

bool hasTimeChanged() {
  uint32_t currentTime = RTC.isrunning() ? RTC.now().unixtime() : millis() / 1000;
  if (abs(currentTime - loopTime) >= 1) {
    loopTime = currentTime;
    return true;
  }
  return false;
}


void note(String text) {
  if (text == "" || !keepLog) {
    return;
  }

  String logs = strContains(text, "iDom") ? "\n[" : "[";
  if (RTC.isrunning()) {
    DateTime now = RTC.now();
    logs += now.day();
    logs += ".";
    logs += now.month();
    logs += ".";
    logs += String(now.year()).substring(2, 4);
    logs += " ";
    logs += now.hour();
    logs += ":";
    logs += now.minute();
    logs += ":";
    logs += now.second();
  } else {
    logs += millis() / 1000;
  }
  logs += "] " + text;

  File file = SPIFFS.open("/log.txt", "a");
  if (file) {
    file.println(logs);
    file.close();
  }
  Serial.print("\n" + logs);
}

bool writeObjectToFile(String name, JsonObject& jsonObject) {
  name = "/" + name + ".txt";

  File file = SPIFFS.open(name, "w");
  if (file) {
    jsonObject.printTo(file);
    file.close();
    return true;
  }
  return false;
}


void connectingToWifi() {
  String logs = "Connecting to Wi-Fi";
  Serial.print("\n" + logs);


  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  WiFi.begin(ssid.c_str(), password.c_str());
  int timeout = 0;
  while (timeout++ < 20 && WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
    delay(250);
  }

  bool result = WiFi.status() == WL_CONNECTED;

  if (result) {
    logs += " finished";
    logs += "\n Connected to " + WiFi.SSID();
    logs += "\n IP address: " + WiFi.localIP().toString();
  } else {
    logs += " timed out";
  }
  note(logs);

  if (result) {
    WiFi.setAutoReconnect(true);

    startServices();
    sayHelloToTheServer();
  } else {
    initiatingWPS();
  }
}

void initiatingWPS() {
  String logs = "Initiating WPS";
  Serial.print("\n" + logs);


  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);

  WiFi.begin();
  while (WiFi.status() != WL_CONNECTED) {
    WiFi.beginWPSConfig();
    delay(250);
    Serial.print(".");
    delay(250);
  }

  ssid = WiFi.SSID();
  password = WiFi.psk();

  logs += " finished";
  logs += "\n Connected to " + WiFi.SSID();
  logs += "\n IP address: " + WiFi.localIP().toString();
  note(logs);
  saveSettings();

  WiFi.setAutoReconnect(true);

  startServices();
  sayHelloToTheServer();
}


void receivedTheData() {
  if (server.hasArg("plain")) {
    server.send(200, "text/plain", "Data has received");
    readData(server.arg("plain"), true);
    return;
  }

  server.send(200, "text/plain", "Body not received");
}

void requestForLogs() {
  File file = SPIFFS.open("/log.txt", "r");
  if (!file) {
    server.send(404, "text/plain", "No log file");
    return;
  }
  Serial.print("\nA log file was requested");

  server.setContentLength(file.size() + String("Log file\nHTTP/1.1 200 OK").length());
  server.send (200, "text/html", "Log file\n");
  while (file.available()) {
    server.sendContent(String(char(file.read())));
  }
  file.close();
  server.send (200, "text/html", "Done");
}

void clearLogs() {
  if (SPIFFS.exists("/log.txt")) {
    SPIFFS.remove("/log.txt");
    server.send(200, "text/plain", "Done");
    Serial.print("\nThe log file was cleared");
  } else {
    server.send(404, "text/plain", "Failed!");
  }
}

void deleteWiFiSettings() {
  ssid = "";
  password = "";
  saveSettings();
  note("Wi-Fi settings have been removed");
  server.send(200, "text/plain", "Done");
}

void onlineSwitch() {
  if (SPIFFS.exists("/online.txt")) {
    SPIFFS.remove("/online.txt");
  } else {
    File file = SPIFFS.open("/online.txt", "a");
    if (file) {
      file.println();
      file.close();
    }
  }
  offline = !SPIFFS.exists("/online.txt");

  String logs = "The device has been set to " + String(offline ? "OFFLINE" : "ONLINE") + " mode";
  server.send(200, "text/plain", logs);
  note(logs);
}


String get1Smart(int index) {
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = smartString.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (smartString.charAt(i) == ',' || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? smartString.substring(strIndex[0], strIndex[1]) : "";
}


void getOfflineData() {
  if (WiFi.status() != WL_CONNECTED || !offline) {
    return;
  }

  int n = MDNS.queryService("idom", "tcp");
  if (n > 0) {
    String ip;
    String logs;

    for (int i = 0; i < n; ++i) {
      ip = String(MDNS.IP(i)[0]) + '.' + String(MDNS.IP(i)[1]) + '.' + String(MDNS.IP(i)[2]) + '.' + String(MDNS.IP(i)[3]);
      logs = "Get basic data from " + ip;
      HTTP.begin("http://" + ip + "/basicdata");
      HTTP.addHeader("Content-Type", "text/plain");
      int httpCode = HTTP.GET();

      if (httpCode > 0 && httpCode == HTTP_CODE_OK) {
        String data = HTTP.getString();
        logs = "Received basic data from " + ip + ": " + data;
        readData(data, true);
      } else {
        logs += " failed!";
      }

      HTTP.end();
      note(logs);
    }
  }
}

void putOfflineData(String values) {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  int n = MDNS.queryService("idom", "tcp");
  if (n == 0) {
    return;
  }

  String ip;
  String logs;

  for (int i = 0; i < n; ++i) {
    ip = String(MDNS.IP(i)[0]) + '.' + String(MDNS.IP(i)[1]) + '.' + String(MDNS.IP(i)[2]) + '.' + String(MDNS.IP(i)[3]);

    HTTP.begin("http://" + ip + "/set");
    HTTP.addHeader("Content-Type", "text/plain");
    int httpCode = HTTP.PUT(values);
    if (httpCode > 0) {
      logs += "\nhttp://" + ip + "/set" + values;
    } else {
      logs += "\nError sending data to " + ip;
    }
    HTTP.end();
  }

  if (logs != "") {
    note("Data transfer between devices (" + String(n) + "): " + logs + "");
  }
}

void getOnlineData() {
  // if (WiFi.status() != WL_CONNECTED || offline || blockGetOnlineData) {
  //   return;
  // }
  //
  // if (sendingError) {
  //   sayHelloToTheServer();
  // }
  //
  // blockGetOnlineData = true;
  //
  // HTTP.begin(baseURL + "/detail/" + device + "/?id=" + WiFi.macAddress() + "&up=" + updateTime);
  // int httpCode = HTTP.GET();
  //
  // if (httpCode > 0) {
  //   if (httpCode == HTTP_CODE_OK) {
  //     readData(HTTP.getString(), false);
  //   }
  //   blockGetOnlineData = false;
  // } else {
  //   Serial.print(".");
  //   blockGetOnlineData = false;
  // }
  //
  // HTTP.end();
}

void putOnlineData(String variant, String values) {
  // if (offline) {
  //   return;
  // }
  //
  // if (WiFi.status() != WL_CONNECTED) {
  //   sendingError = true;
  //   return;
  // }
  //
  // HTTP.begin(baseURL + "/" + variant + "/" + device + "/?id=" + WiFi.macAddress() + "&" + values);
  // int httpCode = HTTP.PUT("");
  //
  // if (httpCode > 0) {
  //   if (httpCode == HTTP_CODE_OK) {
  //     note("Data sent to the server:\n {" + values + "}");
  //     readData(HTTP.getString(), false);
  //     sendingError = false;
  //   }
  // } else {
  //   if (!sendingError) {
  //     note("Failure to send data to the server:\n {" + values + "}");
  //   }
  //   sendingError = true;
  // }
  //
  // HTTP.end();
}
