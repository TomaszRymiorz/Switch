#include "c_online.h"

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  SPIFFS.begin();
  Wire.begin();

  keepLog = SPIFFS.exists("/log.txt");

  note("iDom Switch 2");
  Serial.print("\nSwitch ID: " + WiFi.macAddress());
  offline = !SPIFFS.exists("/online.txt");
  Serial.printf("\nThe switch is set to %s mode", offline ? "OFFLINE" : "ONLINE");

  sprintf(hostName, "switch_%s", String(WiFi.macAddress()).c_str());
  WiFi.hostname(hostName);

  if (!readSettings(0)) {
    readSettings(1);
  }

  RTC.begin();
  if (RTC.isrunning()) {
    startTime = RTC.now().unixtime() - offset;
  }
  Serial.printf("\nRTC initialization %s", startTime != 0 ? "completed" : "failed!");


  pinMode(apds9960_pin, INPUT);
  attachInterrupt(apds9960_pin, interruptRoutine, FALLING);

  initApds();

  setLightsPins();

  if (ssid != "" && password != "") {
    connectingToWifi();
  } else {
    initiatingWPS();
  }
}

void interruptRoutine() {
  isr_flag = 1;
}

void initApds() {
  Serial.print("\n APDS-9960 initialization ");
  if (apds.init()) {
    if (apds.enableGestureSensor(true)) {
      Serial.print("complete");
      Serial.print("\n Gesture sensor is now running");
      adps_init = true;
    } else {
      Serial.print("failed");
    }
  } else {
    Serial.print("failed");
  }
}

void setLightsPins() {
  for (int i = 0; i < 3; i++) {
    pinMode(light_pin[i], OUTPUT);
    digitalWrite(light_pin[i], HIGH);
  }
}


String statesOfLights() {
  String result = first_light ? "1" : "";
  result += second_light ? "2" : "";
  result += third_light ? "3" : "";
  if (result.length() == 0) {
    return "0";
  }
  return result;
}


bool readSettings(bool backup) {
  File file = SPIFFS.open(backup ? "/backup.txt" : "/settings.txt", "r");
  if (!file) {
    note("The " + String(backup ? "backup" : "settings") + " file cannot be read");
    return false;
  }

  DynamicJsonDocument jsonObject(1024);
  deserializeJson(jsonObject, file.readString());
  file.close();

  if (jsonObject.isNull()) {
    note(String(backup ? "Backup" : "Settings") + " file error");
    return false;
  }

  if (jsonObject.containsKey("ssid")) {
    ssid = jsonObject["ssid"].as<String>();
  }
  if (jsonObject.containsKey("password")) {
    password = jsonObject["password"].as<String>();
  }

  if (jsonObject.containsKey("smart")) {
    smartString = jsonObject["smart"].as<String>();
    setSmart();
  }
  if (jsonObject.containsKey("uprisings")) {
    uprisings = jsonObject["uprisings"].as<int>() + 1;
  }
  if (jsonObject.containsKey("offset")) {
    offset = jsonObject["offset"].as<int>();
  }
  if (jsonObject.containsKey("light")) {
    String light = jsonObject["light"].as<String>();
    first_light = strContains(light, "1") || strContains(light, "4");
    second_light = strContains(light, "2") || strContains(light, "4");
    third_light = strContains(light, "3") || strContains(light, "4");
  }

  String logs;
  serializeJson(jsonObject, logs);
  note("Reading the " + String(backup ? "backup" : "settings") + " file:\n " + logs);

  saveSettings();

  return true;
}

void saveSettings() {
  DynamicJsonDocument jsonObject(1024);

  jsonObject["ssid"] = ssid;
  jsonObject["password"] = password;

  jsonObject["smart"] = smartString;
  jsonObject["uprisings"] = uprisings;
  jsonObject["offset"] = offset;

  jsonObject["light"] = statesOfLights();

  if (writeObjectToFile("settings", jsonObject)) {
    String logs;
    serializeJson(jsonObject, logs);
    note("Saving settings:\n " + logs);

    writeObjectToFile("backup", jsonObject);
  } else {
    note("Saving the settings failed!");
  }
}


void sayHelloToTheServer() {
  // This function is only available with a ready-made iDom device.
}

void introductionToServer() {
  // This function is only available with a ready-made iDom device.
}

void startServices() {
  server.on("/hello", HTTP_POST, handshake);
  server.on("/set", HTTP_PUT, receivedOfflineData);
  server.on("/state", HTTP_GET, requestForState);
  server.on("/basicdata", HTTP_GET, requestForBasicData);
  server.on("/log", HTTP_GET, requestForLogs);
  server.on("/log", HTTP_DELETE, clearTheLog);
  server.on("/admin/log", HTTP_POST, activationTheLog);
  server.on("/admin/log", HTTP_DELETE, deactivationTheLog);
  server.on("/admin/wifisettings", HTTP_DELETE, deleteWiFiSettings);
  server.begin();

  note("Launch of services. " + String(hostName) + (MDNS.begin(hostName) ? " started." : " unsuccessful!"));

  MDNS.addService("idom", "tcp", 8080);

  if (!offline) {
    prime = true;
  }
  getOfflineData(true);
}

void handshake() {
  readData(server.arg("plain"), true);

  String reply = "{\"id\":\"" + WiFi.macAddress()
  + "\",\"version\":" + version
  + ",\"value\":" + statesOfLights()
  + ",\"smart\":\"" + smartString
  + "\",\"rtc\":" + RTC.isrunning()
  + ",\"active\":" + (startTime != 0 ? RTC.now().unixtime() - offset - startTime : 0)
  + ",\"uprisings\":" + uprisings
  + ",\"offline\":" + offline
  + ",\"prime\":" + prime
  + ",\"adps\":" + apds.init() + "}";

  Serial.print("\nHandshake");
  server.send(200, "text/plain", reply);
}

void requestForState() {
  String reply = "{\"state\":" + statesOfLights() + "}";

  server.send(200, "text/plain", reply);
}

void requestForBasicData() {
  String reply = RTC.isrunning() ? ("\"time\":" + String(RTC.now().unixtime() - offset)) : "";

  server.send(200, "text/plain", "{" + reply + "}");
}


void loop() {
  server.handleClient();
  MDNS.update();

  if (isr_flag == 1) {
    detachInterrupt(apds9960_pin);
    handleGesture();
    isr_flag = 0;
    attachInterrupt(apds9960_pin, interruptRoutine, FALLING);
    return;
  }

  if (hasTimeChanged()) {
    if (!checkSmart(false) && loopTime % 2 == 0) {
      getOnlineData();
    };
  }
}

void readData(String payload, bool perWiFi) {
  DynamicJsonDocument jsonObject(1024);
  deserializeJson(jsonObject, payload);

  if (jsonObject.isNull()) {
    if (payload.length() > 0) {
      Serial.print("\n Parsing failed!");
    }
    return;
  }

  bool settingsChange = false;
  String result = "";

  uint32_t newTime = 0;
  if (jsonObject.containsKey("offset")) {
    int newOffset = jsonObject["offset"].as<int>();
    if (offset != newOffset) {
      if (RTC.isrunning()) {
        newTime = RTC.now().unixtime() - offset;
      }
      offset = newOffset;

      if (RTC.isrunning() && !jsonObject.containsKey("time")) {
        newTime = newTime + offset;
        if (abs(newTime - RTC.now().unixtime()) > 60) {
          RTC.adjust(DateTime(newTime));
          note("Adjust time");
        }
      }

      settingsChange = true;
    }
  }

  if (jsonObject.containsKey("time")) {
    newTime = jsonObject["time"].as<uint32_t>() + offset;
    if (newTime > 1546304461) {
      if (RTC.isrunning()) {
        if (abs(newTime - RTC.now().unixtime()) > 60) {
          RTC.adjust(DateTime(newTime));
          note("Adjust time");
        }
      } else {
        RTC.adjust(DateTime(newTime));
        startTime = RTC.now().unixtime() - offset;
        note("Adjust time");
        sayHelloToTheServer();
      }
    }
  }

  if (jsonObject.containsKey("up")) {
    uint32_t newUpdateTime = jsonObject["up"].as<uint32_t>();
    if (updateTime < newUpdateTime) {
      updateTime = newUpdateTime;
    }
  }

  if (jsonObject.containsKey("smart")) {
    String newSmartString = jsonObject["smart"].as<String>();
    if (smartString != newSmartString) {
      smartString = newSmartString;
      setSmart();
      result = "smart=" + newSmartString;
      settingsChange = true;
    }
  }

  if (jsonObject.containsKey("val")) {
    String newValue = jsonObject["val"].as<String>();
    if (statesOfLights() != newValue) {
      first_light = strContains(newValue, "1") || strContains(newValue, "4");
      second_light = strContains(newValue, "2") || strContains(newValue, "4");
      third_light = strContains(newValue, "3") || strContains(newValue, "4");
      result += "val=" + newValue;
      setLights("");
    }
  }

  if (jsonObject.containsKey("light")) {
    String newLight = jsonObject["light"].as<String>();
    if (twilight != strContains(newLight, "t")) {
      twilight = !twilight;
      checkSmart(true);
    }
  }

  if (jsonObject.containsKey("prime")) {
    prime = false;
  }

  if (settingsChange) {
    note("Received the data:\n " + payload);
    saveSettings();
  }
  if (perWiFi && result.length() > 0) {
    putOnlineData("detail", result);
  }
}

void setSmart() {
  if (smartString.length() < 2) {
    smartCount = 0;
    return;
  }

  String smart;
  String lights;
  String days;
  bool onAtNight;
  bool offAtDay;
  int onTime;
  int offTime;
  bool enabled;

  smartCount = 1;
  for (byte b: smartString) {
    if (b == ',') {
      smartCount++;
    }
  }

  if (smartArray != 0) {
    delete [] smartArray;
  }
  smartArray = new Smart[smartCount];

  for (int i = 0; i < smartCount; i++) {
    smart = get1Smart(i);
    if (smart.length() > 0 && strContains(smart, "l")) {
      enabled = !strContains(smart, "/");
      smart = !enabled ? smart.substring(0, smart.indexOf("/")) : smart;

      onTime = strContains(smart, "_") ? smart.substring(0, smart.indexOf("_")).toInt() : -1;
      offTime = strContains(smart, "-") ? smart.substring(smart.indexOf("-") + 1, smart.length()).toInt() : -1;

      smart = strContains(smart, "_") ? smart.substring(smart.indexOf("_") + 1, smart.length()) : smart;
      smart = strContains(smart, "-") ? smart.substring(0, smart.indexOf("-")) : smart;

      lights = strContains(smart, "4") ? "123" : "";
      lights += strContains(smart, "1") ? "1" : "";
      lights += strContains(smart, "2") ? "2" : "";
      lights += strContains(smart, "3") ? "3" : "";

      days = strContains(smart, "w") ? "w" : "";
      days += strContains(smart, "o") ? "o" : "";
      days += strContains(smart, "u") ? "u" : "";
      days += strContains(smart, "e") ? "e" : "";
      days += strContains(smart, "h") ? "h" : "";
      days += strContains(smart, "r") ? "r" : "";
      days += strContains(smart, "a") ? "a" : "";
      days += strContains(smart, "s") ? "s" : "";

      onAtNight = strContains(smart, "n");
      offAtDay = strContains(smart, "d");

      smartArray[i] = (Smart) {lights, days, onAtNight, offAtDay, onTime, offTime, enabled, 0};
    }
  }
}

bool checkSmart(bool lightChanged) {
  String newLights = statesOfLights();
  bool result = false;
  DateTime now = RTC.now();
  String log = "The smart function has activated ";

  int i = -1;
  while (++i < smartCount) {
    if (smartArray[i].enabled) {
      if (lightChanged) {
        if (twilight && smartArray[i].onAtNight
          && (strContains(smartArray[i].days, "w") || (RTC.isrunning() && strContains(smartArray[i].days, daysOfTheWeek[now.dayOfTheWeek()])))) {
          if (strContains(smartArray[i].lights, "1") && !strContains(newLights, "1")) {
            newLights += "1";
          }
          if (strContains(smartArray[i].lights, "2") && !strContains(newLights, "2")) {
            newLights += "2";
          }
          if (strContains(smartArray[i].lights, "3") && !strContains(newLights, "3")) {
            newLights += "3";
          }
          result = true;
          log += "the turn on at night";
        }
        if (!twilight && smartArray[i].offAtDay
          && (strContains(smartArray[i].days, "w") || (RTC.isrunning() && strContains(smartArray[i].days, daysOfTheWeek[now.dayOfTheWeek() != 0 ? now.dayOfTheWeek() - 1 : now.dayOfTheWeek() + 6])))) {
          if (strContains(smartArray[i].lights, "1")) {
            newLights.replace("1", "");
          }
          if (strContains(smartArray[i].lights, "2")) {
            newLights.replace("2", "");
          }
          if (strContains(smartArray[i].lights, "3")) {
            newLights.replace("3", "");
          }
          result = true;
          log += "the turn off at day";
        }
      } else {
        if (RTC.isrunning()) {
          int currentTime = (now.hour() * 60) + now.minute();
          if (smartArray[i].access + 60 < now.unixtime()) {
            if (smartArray[i].onTime == currentTime && strContains(smartArray[i].days, daysOfTheWeek[now.dayOfTheWeek()])) {
              smartArray[i].access = now.unixtime();
              if (strContains(smartArray[i].lights, "1") && !strContains(newLights, "1")) {
                newLights += "1";
              }
              if (strContains(smartArray[i].lights, "2") && !strContains(newLights, "2")) {
                newLights += "2";
              }
              if (strContains(smartArray[i].lights, "3") && !strContains(newLights, "3")) {
                newLights += "3";
              }
              result = true;
              log += "the turn on at time";
            }
            if (smartArray[i].offTime == currentTime && (strContains(smartArray[i].days, daysOfTheWeek[now.dayOfTheWeek()]) || strContains(smartArray[i].days, daysOfTheWeek[now.dayOfTheWeek() != 0 ? now.dayOfTheWeek() - 1 : now.dayOfTheWeek() + 6]))) {
              smartArray[i].access = now.unixtime();
              if (strContains(smartArray[i].lights, "1")) {
                newLights.replace("1", "");
              }
              if (strContains(smartArray[i].lights, "2")) {
                newLights.replace("2", "");
              }
              if (strContains(smartArray[i].lights, "3")) {
                newLights.replace("3", "");
              }
              result = true;
              log += "the turn off at time";
            }
          }
        }
      }
    }
  }

  if (result && newLights != statesOfLights()) {
    note(log);
    putOnlineData("detail", "val=" + newLights);

    first_light = strContains(newLights, "1");
    second_light = strContains(newLights, "2");
    third_light = strContains(newLights, "3");
    setLights("");
  } else {
    if (lightChanged) {
      note("The smart function hasn't activated anything.");
    }
  }

  return result;
}

void handleGesture() {
  if (apds.isGestureAvailable()) {
    String coverage;
    String gesture;

    switch (apds.readGesture()) {
      case DIR_UP:
        gesture = "UP";
        first_light = !first_light;
        break;
      case DIR_DOWN:
        gesture = "DOWN";
        second_light = !second_light;
        break;
      case DIR_LEFT:
        gesture = "LEFT";
        third_light = !third_light;
        break;
      // case DIR_RIGHT:
        // gesture = "RIGHT";
        // break;
      case DIR_NEAR:
        gesture = "NEAR";
        coverage = "100.100.100";
        note("Blinds changed state to 100% by a gesture to the NEAR");
        break;
      case DIR_FAR:
        gesture = "FAR";
        coverage = "0.0.0";
        note("Blinds changed state to 0% by a gesture to the FAR");
        break;
      default:
        break;
    }

    if (gesture == "NEAR" || gesture == "FAR") {
      putMultiOfflineData("{\"val\":\"" + coverage + "\"}");
      putOnlineData("detail", "blinds=" + coverage);
    } else {
      setLights(gesture);
      putOnlineData("detail", "val=" + statesOfLights());
    }
  }
}

void setLights(String gesture) {
  if (digitalRead(light_pin[0]) == first_light) {
    note("First light changed state to " + String(first_light));
  }
  digitalWrite(light_pin[0], first_light ? LOW : HIGH);

  if (digitalRead(light_pin[1]) == second_light) {
    note("Second light changed state to " + String(second_light));
  }
  digitalWrite(light_pin[1], second_light ? LOW : HIGH);

 if (digitalRead(light_pin[2]) == third_light) {
   note("Third light changed state to " + String(third_light));
 }
 digitalWrite(light_pin[2], third_light ? LOW : HIGH);

 if (gesture.length() > 0) {
   Serial.printf(" by a gesture to the %s", gesture.c_str());
 }
}
