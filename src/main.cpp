#include <c_online.h>

void setup() {
  pinMode(led_pin, OUTPUT);
  digitalWrite(led_pin, HIGH);

  Serial.begin(115200);
  while (!Serial) {}

  LittleFS.begin();
  Wire.begin();

  keep_log = LittleFS.exists("/log.txt");

  #ifdef RTC_DS1307
    rtc.begin();
  #endif

  note("iDom Switch " + String(version) + "." + String(core_version));

  offline = !LittleFS.exists("/online.txt");
  Serial.print(offline ? " OFFLINE" : " ONLINE");

  sprintf(host_name, "switch_%s", String(WiFi.macAddress()).c_str());
  WiFi.hostname(host_name);

  for (int i = 0; i < 2; i++) {
    pinMode(relay_pin[i], OUTPUT);
    digitalWrite(relay_pin[i], LOW);
  }

  if (!readSettings(0)) {
    readSettings(1);
  }
  setLights("restore", false);

  if (RTCisrunning()) {
    start_time = rtc.now().unixtime() - offset - (dst ? 3600 : 0);
  }

  button1.setSingleClickCallback(&buttonSingle, (void*)"1");
  button2.setSingleClickCallback(&buttonSingle, (void*)"2");
  button1.setDoubleClickCallback(&buttonDouble, (void*)"1");
  button2.setDoubleClickCallback(&buttonDouble, (void*)"2");
  button1.setLongPressCallback(&buttonLong, (void*)"1");
  button2.setLongPressCallback(&buttonLong, (void*)"2");

  setupOTA();

  if (ssid != "" && password != "") {
    connectingToWifi();
  } else {
    initiatingWPS();
  }
}


bool readSettings(bool backup) {
  File file = LittleFS.open(backup ? "/backup.txt" : "/settings.txt", "r");
  if (!file) {
    note("The " + String(backup ? "backup" : "settings") + " file cannot be read");
    return false;
  }

  DynamicJsonDocument json_object(1024);
  deserializeJson(json_object, file.readString());

  if (json_object.isNull() || json_object.size() < 5) {
    note(String(backup ? "Backup" : "Settings") + " file error");
    file.close();
    return false;
  }

  file.seek(0);
  note("Reading the " + String(backup ? "backup" : "settings") + " file:\n " + file.readString());
  file.close();

  if (json_object.containsKey("ssid")) {
    ssid = json_object["ssid"].as<String>();
  }
  if (json_object.containsKey("password")) {
    password = json_object["password"].as<String>();
  }

  if (json_object.containsKey("smart")) {
    smart_string = json_object["smart"].as<String>();
    setSmart();
  }
  if (json_object.containsKey("uprisings")) {
    uprisings = json_object["uprisings"].as<int>() + 1;
  }
  if (json_object.containsKey("offset")) {
    offset = json_object["offset"].as<int>();
  }
  if (json_object.containsKey("dst")) {
    dst = json_object["dst"].as<bool>();
  }
  if (json_object.containsKey("restore")) {
    restore_on_power_loss = json_object["restore"].as<bool>();
  }
  if (json_object.containsKey("twilight")) {
    twilight = json_object["twilight"].as<bool>();
  }
  if (json_object.containsKey("cloudiness")) {
    cloudiness = json_object["cloudiness"].as<bool>();
  }

  if (restore_on_power_loss) {
    if (json_object.containsKey("light1")) {
      light1 = json_object["light1"].as<bool>();
    }
    if (json_object.containsKey("light2")) {
      light2 = json_object["light2"].as<bool>();
    }
  }

  if (json_object.containsKey("fixit")) {
    fixit = json_object["fixit"].as<int>();
  }
  if (json_object.containsKey("location")) {
    geo_location = json_object["location"].as<String>();
  }
  if (json_object.containsKey("sensors")) {
    also_sensors = json_object["sensors"].as<bool>();
  }
  if (json_object.containsKey("dawn_delay")) {
    dawn_delay = json_object["dawn_delay"].as<int>();
  }
  if (json_object.containsKey("dusk_delay")) {
    dusk_delay = json_object["dusk_delay"].as<int>();
  }

  if (json_object.containsKey("twin")) {
    twin = json_object["twin"].as<String>();
  }
  if (json_object.containsKey("single_button")) {
    single_button_function = json_object["single_button"].as<String>();
  }
  if (json_object.containsKey("double_button")) {
    double_button_function = json_object["double_button"].as<String>();
  }
  if (json_object.containsKey("long_button")) {
    long_button_function = json_object["long_button"].as<String>();
  }

  saveSettings(false);

  return true;
}

void saveSettings() {
  saveSettings(true);
}

void saveSettings(bool log) {
  DynamicJsonDocument json_object(1024);

  json_object["ssid"] = ssid;
  json_object["password"] = password;

  if (smart_string.length() > 2) {
    json_object["smart"] = smart_string;
  }
  json_object["uprisings"] = uprisings;
  if (offset > 0) {
    json_object["offset"] = offset;
  }
  if (dst) {
    json_object["dst"] = dst;
  }
  if (restore_on_power_loss) {
    json_object["restore"] = restore_on_power_loss;
  }
  if (twilight) {
    json_object["twilight"] = twilight;
  }
  if (cloudiness) {
    json_object["cloudiness"] = cloudiness;
  }
  if (fixit > 0) {
    json_object["fixit"] = fixit;
  }
  if (geo_location.length() > 2) {
    json_object["location"] = geo_location;
  }
  if (also_sensors) {
    json_object["sensors"] = also_sensors;
  }
  if (dusk_delay != 0) {
    json_object["dusk_delay"] = dusk_delay;
  }
  if (dawn_delay != 0) {
    json_object["dawn_delay"] = dawn_delay;
  }
  if (twin.length() > 2) {
    json_object["twin"] = twin;
  }
  if (single_button_function != "12") {
    json_object["single_button"] = single_button_function;
  }
  if (double_button_function != "0") {
    json_object["double_button"] = double_button_function;
  }
  if (long_button_function != "0") {
    json_object["long_button"] = long_button_function;
  }
  if (light1) {
    json_object["light1"] = light1;
  }
  if (light2) {
    json_object["light2"] = light2;
  }

  if (writeObjectToFile("settings", json_object)) {
    if (log) {
      String logs;
      serializeJson(json_object, logs);
      note("Saving settings:\n " + logs);
    }

    writeObjectToFile("backup", json_object);
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
  server.on("/basicdata", HTTP_POST, exchangeOfBasicData);
  server.on("/log", HTTP_GET, requestForLogs);
  server.on("/log", HTTP_DELETE, clearTheLog);
  server.on("/admin/update", HTTP_POST, manualUpdate);
  server.on("/admin/log", HTTP_POST, activationTheLog);
  server.on("/admin/log", HTTP_DELETE, deactivationTheLog);
  server.begin();

  note(String(host_name) + (MDNS.begin(host_name) ? " started" : " unsuccessful!"));

  MDNS.addService("idom", "tcp", 8080);

  getTime();
  getOfflineData();
}

String getSwitchDetail() {
  return "";
    // This function is only available with a ready-made iDom device.
}

String getValue() {
  if (!light1 && !light2) {
    return "0";
  }

  return String(light1 ? "1" : "") + (light2 ? "2" : "");
}

void handshake() {
  if (server.hasArg("plain")) {
    readData(server.arg("plain"), true);
  }

  String reply = "\"id\":\"" + WiFi.macAddress()
  + "\",\"value\":" + getValue()
  + ",\"twilight\":" + twilight
  + ",\"cloudiness\":" + cloudiness
  + ",\"next_sunset\":" + next_sunset
  + ",\"next_sunrise\":" + next_sunrise
  + ",\"sun_check\":" + last_sun_check
  + ",\"restore\":" + restore_on_power_loss
  + ",\"dusk_delay\":" + dusk_delay
  + ",\"dawn_delay\":" + dawn_delay
  + ",\"fixit\":" + fixit
  + ",\"location\":\"" + geo_location
  + "\",\"sensors\":" + also_sensors
  + ",\"version\":" + version
  + ",\"smart\":\"" + smart_string
  + "\",\"twin\":\"" + twin
  + "\",\"single_button\":\"" + single_button_function
  + "\",\"double_button\":\"" + double_button_function
  + "\",\"long_button\":\"" + long_button_function
  + "\",\"rtc\":" + RTCisrunning()
  + ",\"dst\":" + dst
  + ",\"offset\":" + offset
  + ",\"time\":" + (RTCisrunning() ? String(rtc.now().unixtime() - offset - (dst ? 3600 : 0)) : "0")
  + ",\"active\":" + (RTCisrunning() ? String((rtc.now().unixtime() - offset - (dst ? 3600 : 0)) - start_time) : "0")
  + ",\"uprisings\":" + uprisings
  + ",\"offline\":" + offline;

  Serial.print("\nHandshake");
  server.send(200, "text/plain", "{" + reply + "}");
}

void requestForState() {
  String reply = "\"state\":" + getValue();

  server.send(200, "text/plain", "{" + reply + "}");
}

void exchangeOfBasicData() {
  if (server.hasArg("plain")) {
    readData(server.arg("plain"), true);
  }

  String reply = "\"offset\":" + String(offset) + ",\"dst\":" + String(dst);

  if (RTCisrunning()) {
    reply += ",\"time\":" + String(rtc.now().unixtime() - offset - (dst ? 3600 : 0));
  }

  server.send(200, "text/plain", "{" + reply + "}");
}


void buttonSingle(void* b) {
  int button = atoi((char*)b);

  if ((fixit != button || light1 || light2) && single_button_function != "0") {
    buttonFunction(String(single_button_function[button - 1]));
  }
}

void buttonDouble(void* b) {
  int button = atoi((char*)b);

  if (fixit != button && double_button_function != "0") {
    buttonFunction(String(double_button_function[button - 1]));
  }
}

void buttonLong(void* b) {
  int button = atoi((char*)b);

  if (fixit != button && long_button_function != "0") {
    buttonFunction(String(long_button_function[button - 1]));
  }
}

void buttonFunction(String function) {
  if (function == "0") {
    return;
  }

  if (function == "1" || function == "2") {
    if (function == "1") {
      light1 = !light1;
    } else {
      light2 = !light2;
    }
    setLights("manual", true);
  }
  if ((function == "3" || function == "4") && twin.length() > 5) {
    putOfflineData(twin, "{\"val\":\"" + String(function == "3" ? "100.100.100" : "0.0.0") + "\",\"blinds\":1}");
  }
  if (function == "5" || function == "6") {
    putMultiOfflineData("{\"val\":\"" + String(function == "5" ? "100.100.100" : "0.0.0") + "\",\"blinds\":1}");
  }
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(led_pin, LOW);

    ArduinoOTA.handle();
    server.handleClient();
    MDNS.update();
  } else {
    digitalWrite(led_pin, loop_time % 2 == 0);
    if (!sending_error) {
      note("Wi-Fi connection lost");
    }
    sending_error = true;
  }

  button1.poll();
  button2.poll();

  if (hasTimeChanged()) {
    getOnlineData();
    if (light_delay > 0) {
      if (--light_delay == 0) {
        automaticSettings(true);
        return;
      }
    }
    automaticSettings();
  }
}


bool hasTheLightChanged() {
  return false;
}

void readData(String payload, bool per_wifi) {
  DynamicJsonDocument json_object(1024);
  deserializeJson(json_object, payload);

  if (json_object.isNull()) {
    if (payload.length() > 0) {
      note("Parsing failed!");
    }
    return;
  }

  bool settings_change = false;
  bool details_change = false;
  String result = "";

  if (json_object.containsKey("offset")) {
    if (offset != json_object["offset"].as<int>()) {
      if (RTCisrunning() && !json_object.containsKey("time")) {
        rtc.adjust(DateTime((rtc.now().unixtime() - offset) + json_object["offset"].as<int>()));
        note("Time zone change");
      }

      offset = json_object["offset"].as<int>();
      settings_change = true;
    }
  }

  if (json_object.containsKey("dst")) {
    if (dst != strContains(json_object["dst"].as<String>(), "1")) {
      dst = !dst;
      settings_change = true;

      if (RTCisrunning() && !json_object.containsKey("time")) {
        rtc.adjust(DateTime(rtc.now().unixtime() + (dst ? 3600 : -3600)));
        note(dst ? "Summer time" : "Winter time");
      }
    }
  }

  if (json_object.containsKey("time")) {
    int new_time = json_object["time"].as<uint32_t>() + offset + (dst ? 3600 : 0);
    if (new_time > 1546304461) {
      if (RTCisrunning()) {
        if (abs(new_time - (int)rtc.now().unixtime()) > 60) {
          rtc.adjust(DateTime(new_time));
          note("Adjust time");
        }
      } else {
        rtc.adjust(DateTime(new_time));
        note("RTC begin");
        start_time = rtc.now().unixtime() - offset - (dst ? 3600 : 0);
        if (RTCisrunning()) {
          details_change = true;
        }
      }
    }
  }

  if (json_object.containsKey("smart")) {
    if (smart_string != json_object["smart"].as<String>()) {
      smart_string = json_object["smart"].as<String>();
      setSmart();
      if (per_wifi) {
        result += String(result.length() > 0 ? "&" : "") + "smart=" + getSmartString();
      }
      settings_change = true;
    }
  }

  if (json_object.containsKey("val") && !json_object.containsKey("blinds")) {
    String newValue = json_object["val"].as<String>();
    if (getValue() != newValue) {
      light1 = strContains(newValue, "1") || strContains(newValue, "4");
      light2 = strContains(newValue, "2") || strContains(newValue, "4");
      setLights(per_wifi ? (json_object.containsKey("apk") ? "apk" : "local") : "cloud", false);
      if (per_wifi) {
        result += String(result.length() > 0 ? "&" : "") + "val=" + getValue();
      }
    }
  }

  if (json_object.containsKey("restore")) {
    if (restore_on_power_loss != strContains(json_object["restore"].as<String>(), "1")) {
      restore_on_power_loss = !restore_on_power_loss;
      details_change = true;
    }
  }

  if (json_object.containsKey("fixit")) {
    if (fixit != json_object["fixit"].as<int>()) {
      fixit = json_object["fixit"].as<int>();
      details_change = true;
    }
  }

  if (json_object.containsKey("dusk_delay")) {
    if (dusk_delay != json_object["dusk_delay"].as<int>()) {
      dusk_delay = json_object["dusk_delay"].as<int>();
      details_change = true;
    }
  }

  if (json_object.containsKey("dawn_delay")) {
    if (dawn_delay != json_object["dawn_delay"].as<int>()) {
      dawn_delay = json_object["dawn_delay"].as<int>();
      details_change = true;
    }
  }

  if (json_object.containsKey("location")) {
    if (geo_location != json_object["location"].as<String>()) {
      geo_location = json_object["location"].as<String>();
      getSunriseSunset(rtc.now().day());
      details_change = true;
    }
  }

  if (json_object.containsKey("sensors")) {
    if (also_sensors != strContains(json_object["sensors"].as<String>(), "1")) {
      also_sensors = !also_sensors;
      details_change = true;
    }
  }

  if (json_object.containsKey("light")) {
    if (((geo_location.length() < 2 || also_sensors) && twilight != strContains(json_object["light"].as<String>(), "t"))
    || (geo_location.length() > 2 && !also_sensors && cloudiness != strContains(json_object["light"].as<String>(), "t"))) {
      if (geo_location.length() < 2) {
        twilight = !twilight;
      } else {
        if (also_sensors) {
          twilight = !twilight;
        } else {
          cloudiness = !cloudiness;
        }
      }
      settings_change = true;

      if (twilight ? dusk_delay != 0 : dawn_delay != 0) {
        light_delay = ((twilight ? dusk_delay : dawn_delay) * ((twilight ? dusk_delay : dawn_delay) < 0 ? -1 : 1)) * 60;
      } else {
        automaticSettings(true);
      }
    }
  }

  if (json_object.containsKey("twin")) {
    if (twin != json_object["twin"].as<String>()) {
      twin = json_object["twin"].as<String>();
      settings_change = true;
    }
  }

  if (json_object.containsKey("single_button")) {
    if (single_button_function != json_object["single_button"].as<String>()) {
      single_button_function = json_object["single_button"].as<String>();
      details_change = true;
    }
  }

  if (json_object.containsKey("double_button")) {
    if (double_button_function != json_object["double_button"].as<String>()) {
      double_button_function = json_object["double_button"].as<String>();
      details_change = true;
    }
  }

  if (json_object.containsKey("long_button")) {
    if (long_button_function != json_object["long_button"].as<String>()) {
      long_button_function = json_object["long_button"].as<String>();
      details_change = true;
    }
  }

  if (settings_change || details_change) {
    note("Received the data:\n " + payload);
    saveSettings();
  }
  if (!offline && (result.length() > 0 || details_change)) {
    if (details_change) {
      result += String(result.length() > 0 ? "&" : "") + "detail=" + getSwitchDetail();
    }
    putOnlineData(result, true);
  }
}

void setSmart() {
  if (smart_string.length() < 2) {
    smart_count = 0;
    return;
  }

  int count = 1;
  smart_count = 1;
  for (char b: smart_string) {
    if (b == ',') {
      count++;
    }
    if (b == smart_prefix) {
      smart_count++;
    }
  }

  if (smart_array != 0) {
    delete [] smart_array;
  }
  smart_array = new Smart[smart_count];
  smart_count = 0;

  String single_smart_string;

  for (int i = 0; i < count; i++) {
    single_smart_string = get1(smart_string, i);
    if (strContains(single_smart_string, String(smart_prefix))) {

      if (strContains(single_smart_string, "/")) {
        smart_array[smart_count].enabled = false;
        single_smart_string = single_smart_string.substring(1);
      } else {
        smart_array[smart_count].enabled = true;
      }

      if (strContains(single_smart_string, "_")) {
        smart_array[smart_count].time = single_smart_string.substring(0, single_smart_string.indexOf("_")).toInt();
        single_smart_string = single_smart_string.substring(single_smart_string.indexOf("_") + 1);
      } else {
        smart_array[smart_count].time = -1;
      }

      if (strContains(single_smart_string, "-")) {
        smart_array[smart_count].off_time = single_smart_string.substring(single_smart_string.indexOf("-") + 1).toInt();
        single_smart_string = single_smart_string.substring(0, single_smart_string.indexOf("-"));
      } else {
        smart_array[smart_count].off_time = -1;
      }

      if (isStringDigit(single_smart_string.substring(0, single_smart_string.indexOf(String(smart_prefix))))) {
        smart_array[smart_count].target = single_smart_string.substring(0, single_smart_string.indexOf(String(smart_prefix))).toInt();
        single_smart_string = single_smart_string.substring(single_smart_string.indexOf(String(smart_prefix)));
      } else {
        smart_array[smart_count].target = -1;
      }

      if (strContains(single_smart_string, "4")) {
        smart_array[smart_count].lights = "12";
      } else {
        smart_array[smart_count].lights = strContains(single_smart_string, "1") ? "1" : "";
        smart_array[smart_count].lights += strContains(single_smart_string, "2") ? "2" : "";
        if (smart_array[smart_count].lights == "") {
          smart_array[smart_count].lights = "12";
        }
      }

      if (strContains(single_smart_string, "w")) {
        smart_array[smart_count].days = "w";
      } else {
        smart_array[smart_count].days = strContains(single_smart_string, "o") ? "o" : "";
        smart_array[smart_count].days += strContains(single_smart_string, "u") ? "u" : "";
        smart_array[smart_count].days += strContains(single_smart_string, "e") ? "e" : "";
        smart_array[smart_count].days += strContains(single_smart_string, "h") ? "h" : "";
        smart_array[smart_count].days += strContains(single_smart_string, "r") ? "r" : "";
        smart_array[smart_count].days += strContains(single_smart_string, "a") ? "a" : "";
        smart_array[smart_count].days += strContains(single_smart_string, "s") ? "s" : "";
      }

      smart_array[smart_count].at_night = strContains(single_smart_string, "n");
      smart_array[smart_count].at_night_and_time = smart_array[smart_count].time > -1 && (strContains(single_smart_string, String(smart_prefix) + "&") || strContains(single_smart_string, "n&"));
      smart_array[smart_count].at_day = strContains(single_smart_string, "d");
      smart_array[smart_count].at_day_and_time = smart_array[smart_count].off_time > -1 && (strContains(single_smart_string, String(smart_prefix) + "&") || strContains(single_smart_string, "d&"));
      smart_array[smart_count].react_to_cloudiness = strContains(single_smart_string, "z");
      if (smart_array[smart_count].react_to_cloudiness && !smart_array[smart_count].at_night && !smart_array[smart_count].at_day) {
        smart_array[smart_count].at_night = true;
        smart_array[smart_count].at_day = true;
      }
      smart_array[smart_count].access = 0;

      smart_count++;
    }
  }
  note("Smart contains " + String(smart_count) + " of " + String(smart_prefix));
}

bool automaticSettings() {
  return automaticSettings(hasTheLightChanged());
}

bool automaticSettings(bool light_changed) {
  bool result = false;
  DateTime now = rtc.now();
  String log = "Smart ";
  int current_time = -1;

  if (RTCisrunning()) {
    current_time = (now.hour() * 60) + now.minute();

    if (geo_location.length() > 2) {
      if ((current_time > 61 && last_sun_check != now.day()) || next_sunset == -1 || next_sunrise == -1) {
        getSunriseSunset(now.day());
      }

      if (next_sunset > -1 && next_sunrise > -1) {
        if (((current_time == (next_sunset + dusk_delay)) || (current_time == (next_sunrise + dawn_delay))) && now.second() == 0) {
          if (twilight && current_time == (next_sunrise + dawn_delay)) {
            twilight = false;
          }
          if (!twilight && current_time == (next_sunset + dusk_delay)) {
            twilight = true;
          }
          cloudiness = false;
          light_changed = false;
          saveSettings();
        }
      }
    }

    if (current_time == 120 || current_time == 180) {
      if (now.month() == 3 && now.day() > 24 && days_of_the_week[now.dayOfTheWeek()][0] == 's' && current_time == 120 && !dst) {
        int new_time = now.unixtime() + 3600;
        rtc.adjust(DateTime(new_time));
        dst = true;
        note("Smart set to summer time");
        saveSettings();
        getSunriseSunset(now.day());
      }
      if (now.month() == 10 && now.day() > 24 && days_of_the_week[now.dayOfTheWeek()][0] == 's' && current_time == 180 && dst) {
        int new_time = now.unixtime() - 3600;
        rtc.adjust(DateTime(new_time));
        dst = false;
        note("Smart set to winter time");
        saveSettings();
        getSunriseSunset(now.day());
      }
    }

    if (current_time == (WiFi.localIP()[3] / 2) && now.second() == 0) {
      checkForUpdate(false);
    }
  }

  int i = -1;
  String old_state = getValue();
  while (++i < smart_count) {
    if (smart_array[i].enabled && (strContains(smart_array[i].days, "w") || (RTCisrunning() && strContains(smart_array[i].days, days_of_the_week[now.dayOfTheWeek()])))) {
      if (smart_array[i].at_night) {
        if (light_changed) {
          result = also_sensors ? twilight : smart_array[i].react_to_cloudiness && cloudiness;
        } else {
          result = twilight && current_time == (next_sunset + dusk_delay) && current_time > -1 && smart_array[i].access + 60 < now.unixtime();
        }
        result &= !smart_array[i].at_night_and_time || (smart_array[i].at_night_and_time && smart_array[i].time < current_time && current_time > -1);

        if (result) {
          if (strContains(smart_array[i].lights, "1")) {
            light1 = smart_array[i].target > -1 ? strContains(smart_array[i].target, "1") : true;
          }
          if (strContains(smart_array[i].lights, "2")) {
            light2 = smart_array[i].target > -1 ? strContains(smart_array[i].target, "1") : true;
          }
          log += smart_array[i].target == -1 ? "on" : String(smart_array[i].target);
          log += " at ";
          log += light_changed && smart_array[i].react_to_cloudiness && cloudiness ? "cloudiness" : String(light_changed ? "sensor " : "") + "dusk";
          if (smart_array[i].at_night_and_time && twilight) {
            log += " and time";
          }
        }
      }

      if (!result && smart_array[i].at_day) {
        if (light_changed) {
          result = also_sensors ? !twilight : smart_array[i].react_to_cloudiness && !cloudiness;
        } else {
          result = !twilight && current_time == (next_sunrise + dawn_delay) && current_time > -1 && smart_array[i].access + 60 < now.unixtime();
        }
        result &= smart_array[i].time == -1 || (smart_array[i].time > current_time && current_time > -1);
        result &= !smart_array[i].at_day_and_time || (smart_array[i].at_day_and_time && smart_array[i].off_time < current_time && current_time > -1);

        if (result) {
          if (strContains(smart_array[i].lights, "1")) {
            light1 = smart_array[i].target > -1 ? strContains(smart_array[i].target, "1") : false;
          }
          if (strContains(smart_array[i].lights, "2")) {
            light2 = smart_array[i].target > -1 ? strContains(smart_array[i].target, "1") : false;
          }
          log += smart_array[i].target == -1 ? "off" : String(smart_array[i].target);
          log += " at ";
          log += light_changed && smart_array[i].react_to_cloudiness && !cloudiness ? "sunshine" : String(light_changed ? "sensor " : "") + "dawn";
          if (smart_array[i].at_day_and_time && !twilight) {
            log += " and time";
          }
        }
      }

      if (!result && current_time > -1 && smart_array[i].access + 60 < now.unixtime()) {
        result = smart_array[i].time == current_time;
        result &= !smart_array[i].at_night_and_time || (smart_array[i].at_night_and_time && twilight);

        if (result) {
          if (strContains(smart_array[i].lights, "1")) {
            light1 = smart_array[i].target > -1 ? strContains(smart_array[i].target, "1") : true;
          }
          if (strContains(smart_array[i].lights, "2")) {
            light2 = smart_array[i].target > -1 ? strContains(smart_array[i].target, "1") : true;
          }
          log += smart_array[i].target == -1 ? "on" : String(smart_array[i].target);
          log += " at time";
          if (smart_array[i].at_night_and_time) {
            log += " and dusk";
          }
        }

        if (!result) {
          result = smart_array[i].off_time == current_time;
          result &= !smart_array[i].at_day_and_time || (smart_array[i].at_day_and_time && !twilight);

          if (result) {
            if (strContains(smart_array[i].lights, "1")) {
              light1 = smart_array[i].target > -1 ? strContains(smart_array[i].target, "1") : false;
            }
            if (strContains(smart_array[i].lights, "2")) {
              light2 = smart_array[i].target > -1 ? strContains(smart_array[i].target, "1") : false;
            }
            log += smart_array[i].target == -1 ? "off" : String(smart_array[i].target);
            log += " at time";
            if (smart_array[i].at_day_and_time) {
              log += " and dawn";
            }
          }
        }
      }

      if (result) {
        smart_array[i].access = now.unixtime();
      }
    }
  }

  if (result && old_state != getValue()) {
    note(log);
    setLights("smart", true);
  }
  return result;
}

void setLights(String orderer, bool put_online) {
  String logs = "";
  if (digitalRead(relay_pin[0]) != light1) {
    logs += "\n 1 to " + String(light1);
  }
  digitalWrite(relay_pin[0], light1);

  if (digitalRead(relay_pin[1]) != light2) {
    logs += "\n 2 to " + String(light2);
  }
  digitalWrite(relay_pin[1], light2);

  if (logs.length() > 0) {
    note("Switch (" + orderer + "): " + logs);
    if (put_online) {
      putOnlineData("val=" + getValue());
    }
    saveSettings();
  }
}
