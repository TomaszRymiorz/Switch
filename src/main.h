#include <Arduino.h>
#include <avdweb_Switch.h>

const char device[7] = "switch";
const char smart_prefix = 'l';
const int version = 15;

const int led_pin = 16;
const int relay_pin[] = {13, 4};

Switch button1 = Switch(12);
Switch button2 = Switch(14);

bool restore_on_power_loss = false;
int fixit = 0;

struct Smart {
  bool enabled;
  String days;
  String lights;
  int target;
  int time;
  int off_time;
  bool at_night;
  bool at_night_and_time;
  bool at_day;
  bool at_day_and_time;
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
void button1Pushed(void* s);
void button2Pushed(void* s);
bool hasTheLightChanged();
void readData(String payload, bool per_wifi);
void setSmart();
bool automaticSettings();
bool automaticSettings(bool light_changed);
void handleGesture();
void setLights(String orderer, bool put_online);
