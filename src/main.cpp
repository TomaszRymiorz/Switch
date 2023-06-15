#include "core.h"

void setup() {
  pinMode(led_pin, OUTPUT);
  digitalWrite(led_pin, HIGH);

  Serial.begin(115200);
  while (!Serial) {}

  LittleFS.begin();
  Wire.begin();

  keep_log = LittleFS.exists("/log.txt");

  #ifdef physical_clock
    rtc.begin();
    note("iDom Switch " + String(version) + "." + String(core_version));
  #else
    note("iDom Switch " + String(version) + "." + String(core_version) + "wo");
  #endif

  sprintf(host_name, "switch_%s", String(WiFi.macAddress()).c_str());
  WiFi.hostname(host_name);

  for (int i = 0; i < 2; i++) {
    pinMode(relay_pin[i], OUTPUT);
    digitalWrite(relay_pin[i], LOW);
  }

  if (!readSettings(0)) {
    delay(1000);
    readSettings(1);
  }
  setLights("restore");

  if (RTCisrunning()) {
    start_u_time = rtc.now().unixtime() - offset - (dst ? 3600 : 0);
  }

  button1.setSingleClickCallback(&buttonSingle, (void*)"1");
  button2.setSingleClickCallback(&buttonSingle, (void*)"2");
  button1.setDoubleClickCallback(&buttonDouble, (void*)"1");
  button2.setDoubleClickCallback(&buttonDouble, (void*)"2");
  button1.setLongPressCallback(&buttonLong, (void*)"1");
  button2.setLongPressCallback(&buttonLong, (void*)"2");

  setupOTA();
  connectingToWifi(false);
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(led_pin, LOW);
    ArduinoOTA.handle();
    server.handleClient();
    MDNS.update();
  } else {
    digitalWrite(led_pin, loop_u_time % 2 == 0);
    if (!auto_reconnect) {
      connectingToWifi(true);
    }
  }

  button1.poll();
  button2.poll();

  if (hasTimeChanged()) {
    automation();
  }
}


bool readSettings(bool backup) {
  File file = LittleFS.open(backup ? "/backup.txt" : "/settings.txt", "r");
  if (!file) {
    note("The " + String(backup ? "backup" : "settings") + " file cannot be read");
    return false;
  }

  DynamicJsonDocument json_object(1024);
  DeserializationError deserialization_error = deserializeJson(json_object, file);

  if (deserialization_error) {
    note(String(backup ? "Backup" : "Settings") + " error: " + String(deserialization_error.f_str()));
    file.close();
    return false;
  }

  file.seek(0);
  note("Reading the " + String(backup ? "backup" : "settings") + " file:\n " + file.readString());
  file.close();

  if (json_object.containsKey("log")) {
    last_accessed_log = json_object["log"].as<int>();
  }
  if (json_object.containsKey("ssid")) {
    ssid = json_object["ssid"].as<String>();
  }
  if (json_object.containsKey("password")) {
    password = json_object["password"].as<String>();
  }
  if (json_object.containsKey("uprisings")) {
    uprisings = json_object["uprisings"].as<int>() + 1;
  }
  if (json_object.containsKey("offset")) {
    offset = json_object["offset"].as<int>();
  }
  dst = json_object.containsKey("dst");
  if (json_object.containsKey("smart")) {
    if (json_object.containsKey("ver")) {
      setSmart(json_object["smart"].as<String>());
    } else {
      setSmart(oldSmart2NewSmart(json_object["smart"].as<String>()));
    }
  }
  smart_lock = json_object.containsKey("smart_lock");
  if (json_object.containsKey("location")) {
    geo_location = json_object["location"].as<String>();
    if (geo_location.length() > 2) {
      sun.setPosition(geo_location.substring(0, geo_location.indexOf("x")).toDouble(), geo_location.substring(geo_location.indexOf("x") + 1).toDouble(), 0);
    }
  }
  if (json_object.containsKey("sunset")) {
    sunset_u_time = json_object["sunset"].as<int>();
  }
  if (json_object.containsKey("sunrise")) {
    sunrise_u_time = json_object["sunrise"].as<int>();
  }
  sensor_twilight = json_object.containsKey("sensor_twilight");
  calendar_twilight = json_object.containsKey("twilight");
  restore_on_power_loss = json_object.containsKey("restore");
  if (json_object.containsKey("fixit")) {
    fixit = json_object["fixit"].as<int>();
  }
  if (restore_on_power_loss) {
    for (int i = 0; i < 2; i++) {
      if (json_object.containsKey("light")) {
        light[i] = json_object["light"][i].as<bool>();
      } else {
        light[i] = json_object.containsKey("light" + String(i + 1));
      }
    }
  }
  if (json_object.containsKey("twin")) {
    twin_ip = json_object["twin"].as<String>();
  }
  if (json_object.containsKey("twin_mac")) {
    twin_mac = json_object["twin_mac"].as<String>();
  }
  if (json_object.containsKey("single")) {
    single_button_function = json_object["single"].as<String>();
  } else {
    if (json_object.containsKey("single_button")) {
      single_button_function = json_object["single_button"].as<String>();
    }
  }
  if (json_object.containsKey("double")) {
    double_button_function = json_object["double"].as<String>();
  } else {
    if (json_object.containsKey("double_button")) {
      double_button_function = json_object["double_button"].as<String>();
    }
  }
  if (json_object.containsKey("long")) {
    long_button_function = json_object["long"].as<String>();
  } else {
    if (json_object.containsKey("long_button")) {
      long_button_function = json_object["long_button"].as<String>();
    }
  }
  key_lock = json_object.containsKey("key_lock");

  saveSettings(false);

  return true;
}

void saveSettings() {
  saveSettings(true);
}

void saveSettings(bool log) {
  DynamicJsonDocument json_object(1024);

  json_object["ver"] = String(version) + "." + String(core_version);
  if (last_accessed_log > 0) {
    json_object["log"] = last_accessed_log;
  }
  if (ssid.length() > 0) {
    json_object["ssid"] = ssid;
  }
  if (password.length() > 0) {
    json_object["password"] = password;
  }
  json_object["uprisings"] = uprisings;
  if (offset > 0) {
    json_object["offset"] = offset;
  }
  if (dst) {
    json_object["dst"] = dst;
  }
  if (smart_count > 0) {
    json_object["smart"] = getSmartString(true);
  }
  if (smart_lock) {
    json_object["smart_lock"] = smart_lock;
  }
  if (geo_location != default_location) {
    json_object["location"] = geo_location;
  }
  if (sunset_u_time > 0) {
    json_object["sunset"] = sunset_u_time;
  }
  if (sunrise_u_time > 0) {
    json_object["sunrise"] = sunrise_u_time;
  }
  if (sensor_twilight) {
    json_object["sensor_twilight"] = sensor_twilight;
  }
  if (calendar_twilight) {
    json_object["twilight"] = calendar_twilight;
  }
  if (restore_on_power_loss) {
    json_object["restore"] = restore_on_power_loss;
  }
  if (fixit > 0) {
    json_object["fixit"] = fixit;
  }

  if (light[0] || light[1]) {
    json_object["light"][0] = light[0];
    json_object["light"][1] = light[1];
  }
  if (twin_ip.length() > 2) {
    json_object["twin"] = twin_ip;
  }
  if (twin_mac.length() > 2) {
    json_object["twin_mac"] = twin_mac;
  }
  if (single_button_function != "12") {
    json_object["single"] = single_button_function;
  }
  if (double_button_function != "0") {
    json_object["double"] = double_button_function;
  }
  if (long_button_function != "0") {
    json_object["long"] = long_button_function;
  }
  if (key_lock) {
    json_object["key_lock"] = key_lock;
  }

  if (writeObjectToFile("settings", json_object)) {
    if (log) {
      String log_text;
      serializeJson(json_object, log_text);
      note("Saving settings:\n " + log_text);
    }

    writeObjectToFile("backup", json_object);
  } else {
    note("Saving the settings failed!");
  }
}


String getValue() {
  if (!light[0] && !light[1]) {
    return "0";
  }

  return String(light[0] ? "1" : "") + (light[1] ? "2" : "");
}


void startServices() {
  server.on("/hello", HTTP_POST, handshake);
  server.on("/set", HTTP_PUT, receivedOfflineData);
  server.on("/state", HTTP_GET, requestForState);
  server.on("/basicdata", HTTP_POST, exchangeOfBasicData);
  server.on("/log", HTTP_GET, requestForLogs);
  server.on("/log", HTTP_DELETE, clearTheLog);
  server.on("/test/smartdetail", HTTP_GET, getSmartDetail);
  server.on("/test/smartdetail/raw", HTTP_GET, getRawSmartDetail);
  server.on("/admin/log", HTTP_POST, activationTheLog);
  server.on("/admin/log", HTTP_DELETE, deactivationTheLog);
  server.begin();

  note(String(host_name) + (MDNS.begin(host_name) ? " started" : " unsuccessful!"));

  MDNS.addService("idom", "tcp", 8080);

  ntpClient.begin();
  ntpClient.update();
  readData("{\"time\":" + String(ntpClient.getEpochTime()) + "}", false);
  getOfflineData();
}

void handshake() {
  if (server.hasArg("plain")) {
    readData(server.arg("plain"), true);
  }

  String reply = "\"id\":\"" + WiFi.macAddress() + "\"";
  reply += ",\"version\":" + String(version) + "." + String(core_version);
  reply += ",\"offline\":true";
  if (keep_log) {
    reply += ",\"last_accessed_log\":" + String(last_accessed_log);
  }
  if (start_u_time > 0) {
    reply += ",\"start\":" + String(start_u_time);
  } else {
    reply += ",\"active\":" + String(millis() / 1000);
  }
  reply += ",\"uprisings\":" + String(uprisings);
  if (offset > 0) {
    reply += ",\"offset\":" + String(offset);
  }
  if (dst) {
    reply += ",\"dst\":true";
  }
  if (RTCisrunning()) {
    #ifdef physical_clock
      reply += ",\"rtc\":true";
    #endif
    reply += ",\"time\":" + String(rtc.now().unixtime() - offset - (dst ? 3600 : 0));
  }
  if (smart_count > 0) {
    reply += ",\"smart\":\"" + getSmartString(true) + "\"";
  }
  if (smart_lock) {
    reply += ",\"smart_lock\":true";
  }
  if (geo_location.length() > 2) {
    reply += ",\"location\":\"" + geo_location + "\"";
  }
  if (last_sun_check > -1) {
    reply += ",\"sun_check\":" + String(last_sun_check);
  }
  if (next_sunset > -1) {
    reply += ",\"next_sunset\":" + String(next_sunset);
  }
  if (next_sunrise > -1) {
    reply += ",\"next_sunrise\":" + String(next_sunrise);
  }
  if (sunset_u_time > 0) {
    reply += ",\"sunset\":" + String(sunset_u_time);
  }
  if (sunrise_u_time > 0) {
    reply += ",\"sunrise\":" + String(sunrise_u_time);
  }
  if (light_sensor > -1) {
    reply += ",\"light\":" + String(light_sensor);
  }
  if (sensor_twilight) {
    reply += ",\"sensor_twilight\":true";
  }
  if (calendar_twilight) {
    reply += ",\"twilight\":true";
  }
  if (restore_on_power_loss) {
    reply += ",\"restore\":true";
  }
  if (fixit > 0) {
    reply += ",\"fixit\":" + String(fixit);
  }
  if (getValue() != "0") {
    reply += ",\"value\":" + getValue();
  }
  if (twin_mac.length() > 2) {
    reply += ",\"twin\":\"" + twin_mac + "\"";
  }
  if (single_button_function != "12") {
    reply += ",\"single\":\"" + single_button_function + "\"";
  }
  if (double_button_function != "0") {
    reply += ",\"double\":\"" + double_button_function + "\"";
  }
  if (long_button_function != "0") {
    reply += ",\"long\":\"" + long_button_function + "\"";
  }
  if (key_lock) {
    reply += ",\"key_lock\":true";
  }

  Serial.print("\nHandshake");
  server.send(200, "text/plain", "{" + reply + "}");
}

void requestForState() {
  String reply = "\"value\":" + getValue();

  server.send(200, "text/plain", "{" + reply + "}");
}

void exchangeOfBasicData() {
  if (server.hasArg("plain")) {
    readData(server.arg("plain"), true);
  }

  String reply = "\"ip\":\"" + WiFi.localIP().toString() + "\"" + ",\"id\":\"" + WiFi.macAddress() + "\"";

  reply += ",\"offset\":" + String(offset) + ",\"dst\":" + String(dst);

  if (RTCisrunning()) {
    reply += ",\"time\":" + String(rtc.now().unixtime() - offset - (dst ? 3600 : 0));
  }

  server.send(200, "text/plain", "{" + reply + "}");
}


void buttonSingle(void* b) {
  int button = atoi((char*)b);

  if ((fixit != button || light[0] || light[1]) && single_button_function != "0") {
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

  if (function == "s" || function == "k") {
    digitalWrite(led_pin, HIGH);
    if (function == "s") {
      smart_lock = !smart_lock;
    } else {
      key_lock = !key_lock;
    }
    saveSettings();
  }

  if (key_lock) {
    return;
  }

  if (function == "1" || function == "2") {
    if (function == "1") {
      light[0] = !light[0];
    } else {
      light[1] = !light[1];
    }
    setLights("manual");
    smartAction(function.toInt(), false);
  }
  if ((function == "3" || function == "4") && twin_ip.length() > 5) {
    putOfflineData(twin_ip, "{\"blinds\":" + String(function == "3" ? "100" : "0") + "}");
  }
  if (function == "5" || function == "6") {
    putMultiOfflineData("{\"blinds\":" + String(function == "5" ? "100" : "0") + "}");
  }
}

void readData(const String& payload, bool per_wifi) {
  DynamicJsonDocument json_object(1024);
  DeserializationError deserialization_error = deserializeJson(json_object, payload);

  if (deserialization_error) {
    note("Read data error: " + String(deserialization_error.c_str()) + "\n" + payload);
    return;
  }

  bool settings_change = false;
  bool twilight_change = false;
  bool light_change = false;

  if (json_object.containsKey("ip") && json_object.containsKey("id")) {
      for (int i = 0; i < devices_count; i++) {
        if (devices_array[i].ip == json_object["ip"].as<String>()) {
          devices_array[i].mac = json_object["id"].as<String>();
        }
      }
      if (twin_mac == json_object["id"].as<String>()) {
        twin_ip = json_object["ip"].as<String>();
        settings_change = true;
      }
  }

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
    if (dst != strContains(json_object["dst"].as<String>(), 1)) {
      dst = !dst;
      settings_change = true;
      if (RTCisrunning() && !json_object.containsKey("time")) {
        rtc.adjust(DateTime(rtc.now().unixtime() + (dst ? 3600 : -3600)));
        note(dst ? "Summer time" : "Winter time");
      }
    }
  }

  if (json_object.containsKey("time")) {
    int new_u_time = json_object["time"].as<int>() + offset + (dst ? 3600 : 0);
    if (new_u_time > 1546304461) {
      if (RTCisrunning()) {
        if (abs(new_u_time - (int)rtc.now().unixtime()) > 60) {
          rtc.adjust(DateTime(new_u_time));
          note("Adjust time");
        }
      } else {
        #ifdef physical_clock
          rtc.adjust(DateTime(new_u_time));
        #else
          rtc.begin(DateTime(new_u_time));
        #endif
        note("RTC begin");
        start_u_time = (millis() / 1000) + rtc.now().unixtime() - offset - (dst ? 3600 : 0);
      }
    }
  }

  if (json_object.containsKey("smart")) {
    if (getSmartString(true) != json_object["smart"].as<String>()) {
      setSmart(json_object["smart"].as<String>());
      settings_change = true;
    }
  }

  if (json_object.containsKey("smart_lock")) {
    if (smart_lock != strContains(json_object["smart_lock"].as<String>(), 1)) {
      smart_lock = !smart_lock;
      settings_change = true;
    }
  }

  if (json_object.containsKey("location")) {
    if (geo_location != json_object["location"].as<String>()) {
      geo_location = json_object["location"].as<String>();
      if (geo_location.length() > 2) {
        sun.setPosition(geo_location.substring(0, geo_location.indexOf("x")).toDouble(), geo_location.substring(geo_location.indexOf("x") + 1).toDouble(), 0);
      } else {
        last_sun_check = -1;
        next_sunset = -1;
        next_sunrise = -1;
        sunset_u_time = 0;
        sunrise_u_time = 0;
        calendar_twilight = false;
      }
      settings_change = true;
    }
  }

  if (json_object.containsKey("restore")) {
    if (restore_on_power_loss != strContains(json_object["restore"].as<String>(), "1")) {
      restore_on_power_loss = !restore_on_power_loss;
      settings_change = true;
    }
  }

  if (json_object.containsKey("fixit")) {
    if (fixit != json_object["fixit"].as<int>()) {
      fixit = json_object["fixit"].as<int>();
      settings_change = true;
    }
  }

  if (json_object.containsKey("twin")) {
    if (twin_ip != json_object["twin"].as<String>()) {
      twin_ip = json_object["twin"].as<String>();
      settings_change = true;
    }
  }

  if (json_object.containsKey("twin_mac")) {
    if (twin_mac != json_object["twin_mac"].as<String>()) {
      twin_mac = json_object["twin_mac"].as<String>();
      settings_change = true;
    }
  }

  if (json_object.containsKey("single")) {
    if (single_button_function != json_object["single"].as<String>()) {
      single_button_function = json_object["single"].as<String>();
      settings_change = true;
    }
  }

  if (json_object.containsKey("double")) {
    if (double_button_function != json_object["double"].as<String>()) {
      double_button_function = json_object["double"].as<String>();
      settings_change = true;
    }
  }

  if (json_object.containsKey("long")) {
    if (long_button_function != json_object["long"].as<String>()) {
      long_button_function = json_object["long"].as<String>();
      settings_change = true;
    }
  }

  if (json_object.containsKey("key_lock")) {
    if (key_lock != strContains(json_object["key_lock"].as<String>(), 1)) {
      key_lock = !key_lock;
      settings_change = true;
    }
  }

  if (json_object.containsKey("light")) {
    if (sensor_twilight != strContains(json_object["light"].as<String>(), "t")) {
      sensor_twilight = !sensor_twilight;
      twilight_change = true;
      settings_change = true;
      if (RTCisrunning()) {
        int current_time = (rtc.now().hour() * 60) + rtc.now().minute();
        if (sensor_twilight) {
          if (abs(current_time - dusk_time) > 60) {
            dusk_time = current_time;
          }
        } else {
          if (abs(current_time - dawn_time) > 60) {
            dawn_time = current_time;
          }
        }
      }
    }
    if (strContains(json_object["light"].as<String>(), "t")) {
		  light_sensor = json_object["light"].as<String>().substring(0, json_object["light"].as<String>().indexOf("t")).toInt();
    } else {
		  light_sensor = json_object["light"].as<int>();
    }
  }

  if (json_object.containsKey("val")) {
    if (getValue() != json_object["val"].as<String>()) {
      if (json_object["val"].as<String>() == "0") {
        light[0] = false;
        light[1] = false;
      }
      if (json_object["val"].as<String>() == "4") {
        light[0] = true;
        light[1] = true;
      }
      if (strContains(json_object["val"].as<String>(), "1")) {
        if (strContains(json_object["val"].as<String>(), "-1")) {
          light[0] = false;
        } else {
          light[0] = true;
        }
      }
      if (strContains(json_object["val"].as<String>(), "2")) {
        if (strContains(json_object["val"].as<String>(), "-2")) {
          light[1] = false;
        } else {
          light[1] = true;
        }
      }
      light_change = true;
    }
  }

  if (settings_change) {
    note("Received the data:\n " + payload);
    saveSettings();
  }
  if (json_object.containsKey("light")) {
    smartAction(0, twilight_change);
  }
  if (json_object.containsKey("location") && RTCisrunning()) {
    getSunriseSunset(rtc.now());
  }
  if (light_change) {
    setLights(per_wifi ? (json_object.containsKey("apk") ? "apk" : "local") : "cloud");
  }
}

void automation() {
  if (!RTCisrunning()) {
    smartAction();
    return;
  }

  DateTime now = rtc.now();
  int current_time = (now.hour() * 60) + now.minute();

  if (now.second() == 0) {
    if (current_time == 60) {
      ntpClient.update();
      readData("{\"time\":" + String(ntpClient.getEpochTime()) + "}", false);

      if (last_accessed_log++ > 14) {
        deactivationTheLog();
      }
    }
  }

  if (current_time == 120 || current_time == 180) {
    if (now.month() == 3 && now.day() > 24 && days_of_the_week[now.dayOfTheWeek()][0] == 's' && current_time == 120 && !dst) {
      int new_u_time = now.unixtime() + 3600;
      rtc.adjust(DateTime(new_u_time));
      dst = true;
      note("Setting summer time");
      saveSettings();
      getSunriseSunset(now);
    }
    if (now.month() == 10 && now.day() > 24 && days_of_the_week[now.dayOfTheWeek()][0] == 's' && current_time == 180 && dst) {
      int new_u_time = now.unixtime() - 3600;
      rtc.adjust(DateTime(new_u_time));
      dst = false;
      note("Setting winter time");
      saveSettings();
      getSunriseSunset(now);
    }
  }

  if (geo_location.length() < 2) {
    if (current_time == 181) {
      smart_lock = false;
      saveSettings();
    }
  } else {
    if (now.second() == 0 && ((current_time > 181 && last_sun_check != now.day()) || next_sunset == -1 || next_sunrise == -1)) {
      getSunriseSunset(now);
    }

    if (next_sunset > -1 && next_sunrise > -1) {
      if ((!calendar_twilight && current_time == next_sunset) || (calendar_twilight && current_time == next_sunrise)) {
        if (current_time == next_sunset) {
          calendar_twilight = true;
          sunset_u_time = now.unixtime() - offset - (dst ? 3600 : 0);
        }
        if (current_time == next_sunrise) {
          calendar_twilight = false;
          sunrise_u_time = now.unixtime() - offset - (dst ? 3600 : 0);
        }
        smart_lock = false;
        saveSettings();
      }
    }
  }

  smartAction();
}

void smartAction() {
  smartAction(-1, false);
}


void setLights(String orderer) {
  String log_text = "";

  for (int i = 0; i < 2; i++) {
    if (digitalRead(relay_pin[i]) != light[i]) {
      digitalWrite(relay_pin[i], light[i]);
      log_text += "\n " + String(i + 1) + " to " + String(light[i]);
    }
  }

  if (log_text.length() > 0) {
    note("Switch (" + orderer + "): " + log_text);
    saveSettings();
  }
}
