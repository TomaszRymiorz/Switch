#include <Arduino.h>
#include <avdweb_Switch.h>

const char device[7] = "switch";
const char smart_prefix = 'l';
const int version = 13;

const int led_pin = 16;
const int relay_pin[] = {13, 4};

Switch button1 = Switch(12);
Switch button2 = Switch(14);

bool restore_on_power_loss = false;

struct Smart {
  bool enabled;
  String lights;
  String days;
  bool on_at_night;
  bool off_at_day;
  int on_time;
  int off_time;
  bool on_at_night_and_time;
  bool off_at_day_and_time;
  bool react_to_cloudiness;
  uint32_t access;
};

bool light1 = false;
bool light2 = false;

int twilight_counter = 0;

bool twilight = false;
bool cloudiness = false;

bool readSettings(bool backup);
void saveSettings();
void saveSettings(bool log);
void sayHelloToTheServer();
void introductionToServer();
void startServices();
String getSwitchDetail();
String getValue();
void handshake();
void requestForState();
void exchangeOfBasicData();
void button1Single(void* s);
void button2Single(void* s);
bool hasTheLightChanged();
void readData(String payload, bool per_wifi);
void setSmart();
bool automaticSettings();
bool automaticSettings(bool light_changed);
void handleGesture();
void setLights(String orderer, bool put_online);
