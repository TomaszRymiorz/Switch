#include "core.h"

void setup() {
  Serial.begin(74880);
  while (!Serial) {}
  delay(3000);

  Serial.print("\niDom Switch 2");
  Serial.printf("\nSwitch ID: %s", WiFi.macAddress().c_str());

  Serial.print("\n SD card initialization ");
  if (!SD.begin(sd_pin)) {
    Serial.print("failed!");
    while (true) {
      delay(0);
    };
  }
  Serial.print("completed");

  Serial.print("\n RTC initialization ");
  Wire.begin();
  if (RTC.begin()) {
    Serial.print("completed");
  } else {
    Serial.print("failed");
  }

  if (!RTC.isrunning()) {
    Serial.print("\n RTC is NOT running!");
  }

  pinMode(apds9960_pin, INPUT);
  attachInterrupt(apds9960_pin, interruptRoutine, FALLING);

  initApds(true);

  offline = !SD.exists("online.txt");
  Serial.printf("\n The switch is set to %s mode", offline ? "OFFLINE" : "ONLINE");

  readOffset();
  readSmart();

  if (readWiFiConfiguration()) {
    connectingToWifi();
  } else {
    initiatingWPS();
  }

  uprisingsCounter();

  setupLightsPins();

  if (RTC.isrunning()) {
    start = RTC.now().unixtime();
  }
}

void interruptRoutine() {
  isr_flag = 1;
}

void initApds(bool beginning) {
  if (beginning) {
    Serial.print("\n APDS-9960 initialization ");
  }
  if (!apds.init()) {
    if (beginning) {
      Serial.print("failed");
      Serial.print("\n Something went wrong during APDS-9960 init!");
    }
  } else {
    if (apds.enableGestureSensor(true)) {
      Serial.print("complete");
      Serial.print("\n Gesture sensor is now running");
      adps_init = true;
    } else {
      if (beginning) {
        Serial.print("failed");
        Serial.print("\n Something went wrong during gesture sensor init!");
      }
    }
  }
}

void setupLightsPins() {
  for (int i = 0; i < 3; i++) {
    pinMode(light_pin[i], OUTPUT);
    digitalWrite(light_pin[i], HIGH);
  }
}


void startRestServer() {
  server.on("/hello", HTTP_POST, handshake);
  server.on("/set", HTTP_PUT, receivedTheData);
  server.on("/state", HTTP_GET, requestForState);
  server.begin();
  Serial.print("\nStarting the REST server");
}

void handshake() {
  String reply;
  if (readData(server.arg("plain")) == -1) {
    serialPrint("Shake hands");

    uint32_t active = 0;
    if (RTC.isrunning()) {
      active = RTC.now().unixtime() - start;
    }

    reply = "{\"id\":\"" + WiFi.macAddress()
    + "\",\"light\":" + statesOfLights()
    + ",\"smart\":\"" + smartString
    + "\",\"rtc\":" + RTC.isrunning()
    + ",\"active\":" + active
    + ",\"uprisings\":" + uprisings
    + ",\"adps\":" + apds.init() + "}";
  } else {
    reply = "0";
  }
  server.send(200, "text/plain", reply);
}

void requestForState() {
  String reply =  "{\"state\":" + statesOfLights() + "}";

  server.send(200, "text/plain", reply);
}

void receivedTheData() {
  Serial.print("\n Received the data");
  if (!server.hasArg("plain")) {
    server.send(200, "text/plain", "Body not received");
    return;
  }
  server.send(200, "text/plain", "Data has received");

  readData(server.arg("plain"));
}


String statesOfLights() {
  String states = first_light ? "1" : "";
  states += second_light ? "2" : "";
  // states += third_light ? "3" : "";
  if (states.length() == 0) {
    return "0";
  }
  return states;
}


void loop() {
  server.handleClient();

  if (!adps_init) {
    initApds(false);
  }

  if (isr_flag == 1) {
    detachInterrupt(apds9960_pin);
    handleGesture();
    isr_flag = 0;
    attachInterrupt(apds9960_pin, interruptRoutine, FALLING);
    return;
  }

  if (timeHasChanged()) {
    if (!offline) {
      getOnlineData();
    }

    checkSmart(false);
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (reconnect) {
      serialPrint("Reconnection with Wi-Fi");
      if (!connectingToWifi()) {
        initiatingWPS();
      }
    } else {
      if (initiatingWPS()) {
        setupLightsPins();
      }
    }
    return;
  }
}


int readData(String payload) {
  DynamicJsonBuffer jsonBuffer(JSON_OBJECT_SIZE(3));
  JsonObject& jsonObject = jsonBuffer.parseObject(payload);

  if (!jsonObject.success()) {
    if (payload.length() > 0) {
      Serial.print("\n " + payload);
      Serial.print("\n Parsing failed!");
    }
    return 0;
  }

  if (jsonObject.containsKey("offset")) {
    if (offset != jsonObject["offset"].as<int>()) {
      offset = jsonObject["offset"].as<int>();
      writeOffset();
      setupLightsPins();
    }
  }

  if (jsonObject.containsKey("access") && RTC.isrunning()) {
    uint32_t t = jsonObject["access"].as<uint32_t>() + offset;
    DateTime now = RTC.now();
    if (t - now.unixtime() > 10) {
      RTC.adjust(DateTime(t));
      serialPrint(" Adjust time");
    }
  }

  if (jsonObject.containsKey("twin")) {
    twin = jsonObject["twin"].as<String>();
  }

  if (jsonObject.containsKey("id") && jsonObject["id"].as<String>() == "idom") {
    return -1;
  }

  if (jsonObject.containsKey("smart")) {
    if (smartString != jsonObject["smart"].as<String>()) {
      smartString = jsonObject["smart"].as<String>();
      setSmart();
      writeSmart();
      setupLightsPins();
    }
  }

  if (jsonObject.containsKey("light")) {
    first_light = strContains(jsonObject["light"].as<String>(), "1") || strContains(jsonObject["light"].as<String>(), "4");
    second_light = strContains(jsonObject["light"].as<String>(), "2") || strContains(jsonObject["light"].as<String>(), "4");
    // third_light = strContains(jsonObject["light"].as<String>(), "3") || strContains(jsonObject["light"].as<String>(), "4");
    setLights("");
  }

  if (jsonObject.containsKey("twilight")) {
    twilight = jsonObject["twilight"].as<bool>();
    checkSmart(true);
  }

  return jsonObject.success();
}

void setSmart() {
  if (smartString.length() == 0) {
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
    smart = get1Smart(smartString, i);
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


      smartArray[i] = (Smart) {lights, days, onAtNight, offAtDay, 0, onTime, offTime, 0};
    }
  }
}

void checkSmart(bool daynight) {
  if (!RTC.isrunning()) {
    return;
  }

  DateTime now = RTC.now();
  int currentTime = (now.hour() * 60) + now.minute();
  String lights;
  bool result = false;

  for (int i = 0; i < smartCount; i++) {
    result = false;
    lights = statesOfLights();

    // if (smartArray[i].lights.length() > 0) {

    if (daynight) {
      if (smartArray[i].onAtNight && twilight
        && strContains(smartArray[i].days, daysOfTheWeek[now.dayOfTheWeek()])
        && (smartArray[i].blackout + 72000) < now.unixtime()) {
          lights += strContains(smartArray[i].lights, "1") ? "1" : "";
          lights += strContains(smartArray[i].lights, "2") ? "2" : "";
          lights += strContains(smartArray[i].lights, "3") ? "3" : "";
          result = true;
          smartArray[i].blackout = now.unixtime();
          serialPrint("The smart function activated the turn on at night");
        }
      if (smartArray[i].offAtDay && !twilight
        && strContains(smartArray[i].days, daysOfTheWeek[now.dayOfTheWeek() - 1])) {
          if (strContains(smartArray[i].lights, "1")) {
            lights.replace("1", "");
          }
          if (strContains(smartArray[i].lights, "2")) {
            lights.replace("2", "");
          }
          if (strContains(smartArray[i].lights, "3")) {
            lights.replace("3", "");
          }
          result = true;
          serialPrint("The smart function activated the turn off at day");
        }
    } else {
      if (smartArray[i].access + 60 < now.unixtime()) {
        if (smartArray[i].onTime == currentTime
          && strContains(smartArray[i].days, daysOfTheWeek[now.dayOfTheWeek()])) {
            lights += strContains(smartArray[i].lights, "1") ? "1" : "";
            lights += strContains(smartArray[i].lights, "2") ? "2" : "";
            lights += strContains(smartArray[i].lights, "3") ? "3" : "";
            result = true;
            smartArray[i].access = now.unixtime();
            serialPrint("The smart function activated the turn on time");
          }
        if (smartArray[i].offTime == currentTime
          && (strContains(smartArray[i].days, daysOfTheWeek[now.dayOfTheWeek()]) || (strContains(smartArray[i].days, daysOfTheWeek[now.dayOfTheWeek() - 1]) && currentTime < 360))) {
            if (strContains(smartArray[i].lights, "1")) {
              lights.replace("1", "");
            }
            if (strContains(smartArray[i].lights, "2")) {
              lights.replace("2", "");
            }
            if (strContains(smartArray[i].lights, "3")) {
              lights.replace("3", "");
            }
            result = true;
            smartArray[i].access = now.unixtime();
            serialPrint("The smart function activated the turn off time");
          }
      }
    }

    if (result) {
      if (!offline) {
        putDataOnServer("light=" + lights);
      }

      first_light = strContains(lights, "1");
      second_light = strContains(lights, "2");
      // third_light = strContains(lights, "3");

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
        serialPrint("");
        Serial.print("Blinds changed state to 100% by a gesture to the NEAR");
        break;
      case DIR_FAR:
        gesture = "FAR";
        coverage = "0";
        serialPrint("");
        Serial.print("Blinds changed state to 0% by a gesture to the FAR");
        break;
      default:
        break;
    }

    if (gesture == "NEAR" || gesture == "FAR") {
      if (offline) {
        postToTwin("{\"coverage\":" + coverage + "}");
      } else {
        putDataOnServer("coverage=" + coverage);
      }
    } else {
      setLights(gesture);

      if (!offline) {
        putDataOnServer("light=" + statesOfLights());
      }
    }
  }
}

void setLights(String gesture) {
  if (digitalRead(light_pin[0]) == first_light) {
    serialPrint("");
    Serial.printf("First light changed state to %s", String(first_light).c_str());
  }
  digitalWrite(light_pin[0], first_light ? LOW : HIGH);

  if (digitalRead(light_pin[1]) == second_light) {
    serialPrint("");
    Serial.printf("Second light changed state to %s", String(second_light).c_str());
  }
  digitalWrite(light_pin[1], second_light ? LOW : HIGH);

 // if (digitalRead(light_pin[2]) == third_light) {
 //   serialPrint("");
 //   Serial.printf("Third light changed state to %s", String(third_light).c_str());
 // }
 // digitalWrite(light_pin[2], third_light ? LOW : HIGH);

 if (gesture.length() > 0) {
   Serial.printf(" by a gesture to the %s", gesture.c_str());
 }
}
