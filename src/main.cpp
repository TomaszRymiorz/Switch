#include "core.h"

void setup() {
  Serial.begin(74880);
  while (!Serial) {}

  SPIFFS.begin();
  Wire.begin();

  writeLog("iDom Switch 2");
  Serial.print("\n Switch ID: " + WiFi.macAddress());
  Serial.printf("\n The switch is set to %s mode", offline ? "OFFLINE" : "ONLINE");
  Serial.print("\n RTC initialization " + String(RTC.begin() ? "completed" : "failed!"));

  readSettings();

  if (RTC.isrunning()) {
    start = RTC.now().unixtime() - offset;
  }


  pinMode(apds9960_pin, INPUT);
  attachInterrupt(apds9960_pin, interruptRoutine, FALLING);

  initApds();

  setupLightsPins();

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

void setupLightsPins() {
  for (int i = 0; i < 3; i++) {
    pinMode(light_pin[i], OUTPUT);
    digitalWrite(light_pin[i], HIGH);
  }
}


void readSettings() {
  File file = SPIFFS.open("/settings.txt", "r");
  if (!file) {
    writeLog("The settings file can not be read");
    return;
  }

  DynamicJsonBuffer jsonBuffer(JSON_OBJECT_SIZE(3));
  JsonObject& jsonObject = jsonBuffer.parseObject(file.readString());
  file.close();

  if (!jsonObject.success()) {
    writeLog("Settings file error");
    return;
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
  jsonObject.printTo(logs);
  writeLog("The settings file was read:\n " + logs);

  saveTheSettings();
}

void saveTheSettings() {
  DynamicJsonBuffer jsonBuffer(JSON_OBJECT_SIZE(3));
  JsonObject& jsonObject = jsonBuffer.createObject();

  jsonObject["ssid"] = ssid;
  jsonObject["password"] = password;
  jsonObject["smart"] = smartString;
  jsonObject["uprisings"] = uprisings;
  jsonObject["offset"] = offset;

  jsonObject["light"] = statesOfLights();

  if (writeObjectToFile("settings", jsonObject)) {
    String logs;
    jsonObject.printTo(logs);
    writeLog("Saving settings:\n " + logs);
  } else {
    writeLog("Saving settings failed!");
  }
}


void sayHelloToTheServer() {
  if (!offline) {
    String request = "ip=" + WiFi.localIP().toString()
    + "&deets=" + RTC.isrunning() + "," + start + "," + uprisings + "," + adps_init + "," + version;

    if (sendingError) {
      request += "&val=" + statesOfLights();

      putDataOnline("detail", request);
    } else {
      putDataOnline("rooms", request);
    }
  }
}

void startRestServer() {
  server.on("/hello", HTTP_POST, handshake);
  server.on("/set", HTTP_PUT, receivedTheData);
  server.on("/state", HTTP_GET, requestForState);
  server.on("/log", HTTP_GET, requestForLogs);
  server.on("/log", HTTP_DELETE, clearLogs);
  server.on("/deletewifisettings", HTTP_DELETE, deleteWiFiSettings);
  server.begin();
  Serial.print("\n Starting the REST server");
}

void handshake() {
  readData(server.arg("plain"), true);

  String reply = "{\"id\":\"" + WiFi.macAddress()
  + "\",\"version\":" + version
  + ",\"value\":" + statesOfLights()
  + ",\"smart\":\"" + smartString
  + "\",\"rtc\":" + RTC.isrunning()
  + ",\"active\":" + (RTC.isrunning() ? (RTC.now().unixtime() - offset) - start : 0)
  + ",\"uprisings\":" + uprisings
  + ",\"adps\":" + apds.init() + "}";

  writeLog("Shake hands");
  server.send(200, "text/plain", reply);
}

void requestForState() {
  String reply = "{\"state\":" + statesOfLights() + "}";

  server.send(200, "text/plain", reply);
}


void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    if (reconnect) {
      writeLog("Reconnection with Wi-Fi");
      if (!connectingToWifi()) {
        initiatingWPS();
      }
    } else {
      initiatingWPS();
    }
  }

  server.handleClient();

  if (isr_flag == 1) {
    detachInterrupt(apds9960_pin);
    handleGesture();
    isr_flag = 0;
    attachInterrupt(apds9960_pin, interruptRoutine, FALLING);
    return;
  }

  if (timeHasChanged()) {
    if (loopTime % 2 == 0) {
      getOnlineData();
    }
    checkSmart(false);
  }
}


String statesOfLights() {
  String result = first_light ? "1" : "";
  result += second_light ? "2" : "";
  // result += third_light ? "3" : "";
  if (result.length() == 0) {
    return "0";
  }
  return result;
}

void readData(String payload, bool perWiFi) {
  DynamicJsonBuffer jsonBuffer(JSON_OBJECT_SIZE(3));
  JsonObject& jsonObject = jsonBuffer.parseObject(payload);
  DateTime now = RTC.now();
  uint32_t newTime = 0;
  bool settingsChange = false;
  String result = "";

  if (!jsonObject.success()) {
    if (payload.length() > 0) {
      Serial.print("\n Parsing failed!");
    }
    return;
  }

  if (jsonObject.containsKey("offset")) {
    int newOffset = jsonObject["offset"].as<int>();
    if (offset != newOffset) {
      newTime = now.unixtime() - offset;
      offset = newOffset;

      if (!jsonObject.containsKey("time") && RTC.isrunning()) {
        newTime = newTime + offset;
        if (abs(newTime - now.unixtime()) > 10) {
          RTC.adjust(DateTime(newTime));
          writeLog("Adjust time");
        }
      }

      settingsChange = true;
    }
  }

  if (jsonObject.containsKey("time") && RTC.isrunning()) {
    newTime = jsonObject["time"].as<uint32_t>() + offset;
    if (abs(newTime - now.unixtime()) > 10) {
      RTC.adjust(DateTime(newTime));
      writeLog("Adjust time");
    }
  }

  if (jsonObject.containsKey("up")) {
    uint32_t newUpdateTime = jsonObject["up"].as<uint32_t>();
    if (updateTime < newUpdateTime) {
      updateTime = newUpdateTime;
    }
  }

  if (jsonObject.containsKey("twin")) {
    String newTwin = jsonObject["twin"].as<String>();
    if (twin != newTwin) {
      twin = newTwin;
    }
  }

  if (jsonObject.containsKey("smart")) {
    String newSmartString = jsonObject["smart"].as<String>();
    if (smartString != newSmartString) {
      smartString = newSmartString;
      result = "smart=" + newSmartString;
      setSmart();
      settingsChange = true;
    }
  }

  if (jsonObject.containsKey("val")) {
    String newLight = jsonObject["val"].as<String>();
    if (statesOfLights() != newLight) {
      first_light = strContains(newLight, "1") || strContains(newLight, "4");
      second_light = strContains(newLight, "2") || strContains(newLight, "4");
      // third_light = strContains(newLight, "3") || strContains(newLight, "4");
      result += "val=" + newLight;
      setLights("");
    }
  }

  if (jsonObject.containsKey("twilight")) {
    String newTwilight = jsonObject["twilight"].as<String>();
    if (strContains(newTwilight, ",")) {
      twilight = strContains(newTwilight.substring(0, newTwilight.indexOf(",")), "1");
    } else {
      twilight = strContains(newTwilight, "1");
    }
    checkSmart(true);
  }

  if (settingsChange) {
    writeLog("Received the data:\n " + payload);
    saveTheSettings();
  }
  if (perWiFi && result.length() > 0) {
    putDataOnline("detail", result);
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
    if (smart.length() > 0 && !strContains(smart, "b")) {
      onTime = strContains(smart, "_") ? smart.substring(0, smart.indexOf("_")).toInt() : -1;
      offTime = strContains(smart, "-") ? smart.substring(smart.indexOf("-") + 1, smart.length()).toInt() : -1;

      smart = strContains(smart, "_") ? smart.substring(smart.indexOf("_") + 1, smart.length()) : smart;
      smart = strContains(smart, "-") ? smart.substring(0, smart.indexOf("-")) : smart;

      lights = strContains(smart, "4") ? "123" : "";
      lights += strContains(smart, "1") ? "1" : "";
      lights += strContains(smart, "2") ? "2" : "";
      lights += strContains(smart, "3") ? "3" : "";

      days = strContains(smart, "w") ? "ouehras" : "";
      days += strContains(smart, "o") ? "o" : "";
      days += strContains(smart, "u") ? "u" : "";
      days += strContains(smart, "e") ? "e" : "";
      days += strContains(smart, "h") ? "h" : "";
      days += strContains(smart, "r") ? "r" : "";
      days += strContains(smart, "a") ? "a" : "";
      days += strContains(smart, "s") ? "s" : "";

      onAtNight = strContains(smart, "n");
      offAtDay = strContains(smart, "d");

      enabled = !strContains(smart, "/");

      smartArray[i] = (Smart) {lights, days, onAtNight, offAtDay, onTime, offTime, enabled, 0};
    }
  }
}

void checkSmart(bool lightHasChanged) {
  if (!RTC.isrunning()) {
    return;
  }

  bool result = false;
  String light;
  String newLights;
  DateTime now = RTC.now();
  int currentTime = (now.hour() * 60) + now.minute();

  int i = -1;
  while (++i < smartCount && !result) {
    if (smartArray[i].enabled) {
      light = statesOfLights();
      newLights = "";
      if (lightHasChanged) {
        if (smartArray[i].onAtNight && twilight && strContains(smartArray[i].days, daysOfTheWeek[now.dayOfTheWeek()])) {
          newLights += strContains(smartArray[i].lights, "1") ? "1" : (strContains(light, "1") ? "1" : "");
          newLights += strContains(smartArray[i].lights, "2") ? "2" : (strContains(light, "2") ? "2" : "");
          newLights += strContains(smartArray[i].lights, "3") ? "3" : (strContains(light, "3") ? "3" : "");
          if (light != newLights) {
            light = newLights;
            result = true;
            writeLog("The smart function activated the turn on at night");
          }
        }
        if (smartArray[i].offAtDay && !twilight && strContains(smartArray[i].days, daysOfTheWeek[now.dayOfTheWeek() - 1])) {
          newLights = light;
          if (strContains(smartArray[i].lights, "1")) {
            newLights.replace("1", "");
          }
          if (strContains(smartArray[i].lights, "2")) {
            newLights.replace("2", "");
          }
          if (strContains(smartArray[i].lights, "3")) {
            newLights.replace("3", "");
          }
          if (light != newLights) {
            light = newLights;
            result = true;
            writeLog("The smart function activated the turn off at day");
          }
        }
      } else {
        if (smartArray[i].access + 60 < now.unixtime() && strContains(smartArray[i].days, daysOfTheWeek[now.dayOfTheWeek()])) {
          if (smartArray[i].onTime == currentTime) {
            smartArray[i].access = now.unixtime();
            newLights += strContains(smartArray[i].lights, "1") ? "1" : (strContains(light, "1") ? "1" : "");
            newLights += strContains(smartArray[i].lights, "2") ? "2" : (strContains(light, "2") ? "2" : "");
            newLights += strContains(smartArray[i].lights, "3") ? "3" : (strContains(light, "3") ? "3" : "");
            if (light != newLights) {
              light = newLights;
              result = true;
              writeLog("The smart function activated the turn on time");
            }
          }
          if (smartArray[i].offTime == currentTime) {
            smartArray[i].access = now.unixtime();
            newLights = light;
            if (strContains(smartArray[i].lights, "1")) {
              newLights.replace("1", "");
            }
            if (strContains(smartArray[i].lights, "2")) {
              newLights.replace("2", "");
            }
            if (strContains(smartArray[i].lights, "3")) {
              newLights.replace("3", "");
            }
            if (light != newLights) {
              light = newLights;
              result = true;
              writeLog("The smart function activated the turn off time");
            }
          }
        }
      }
    }

    if (result) {
      putDataOnline("detail", "val=" + light);

      first_light = strContains(light, "1");
      second_light = strContains(light, "2");
      // third_light = strContains(light, "3");

      setLights("");
    }
  }
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
      // case DIR_LEFT:
        // gesture = "LEFT";
        // third_light = !third_light;
        // break;
      // case DIR_RIGHT:
        // gesture = "RIGHT";
        // break;
      case DIR_NEAR:
        gesture = "NEAR";
        coverage = "100";
        writeLog("Blinds changed state to 100% by a gesture to the NEAR");
        break;
      case DIR_FAR:
        gesture = "FAR";
        coverage = "0";
        writeLog("Blinds changed state to 0% by a gesture to the FAR");
        break;
      default:
        break;
    }

    if (gesture == "NEAR" || gesture == "FAR") {
      postDataToTheTwin("{\"val\":" + coverage + "}");
      putDataOnline("detail", "blinds=" + coverage);
    } else {
      setLights(gesture);
      putDataOnline("detail", "val=" + statesOfLights());
    }
  }
}

void setLights(String gesture) {
  if (digitalRead(light_pin[0]) == first_light) {
    writeLog("First light changed state to " + String(first_light));
  }
  digitalWrite(light_pin[0], first_light ? LOW : HIGH);

  if (digitalRead(light_pin[1]) == second_light) {
    writeLog("Second light changed state to " + String(second_light));
  }
  digitalWrite(light_pin[1], second_light ? LOW : HIGH);

 // if (digitalRead(light_pin[2]) == third_light) {
 //   writeLog("Third light changed state to " + String(third_light));
 // }
 // digitalWrite(light_pin[2], third_light ? LOW : HIGH);

 if (gesture.length() > 0) {
   Serial.printf(" by a gesture to the %s", gesture.c_str());
 }
}
