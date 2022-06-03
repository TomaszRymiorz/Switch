#include <Arduino.h>
#include <avdweb_Switch.h>

const char device[7] = "switch";
const char smart_prefix = 'l';
const int version = 21;

const int led_pin = 16;
const int relay_pin[] = {13, 4};

Switch button1 = Switch(12);
Switch button2 = Switch(14);

bool restore_on_power_loss = false;
int fixit = 0;

struct Smart {
  bool enabled;
  String days;
  String light;
  String must_be_on;
  String must_be_off;
  int action;
  int at_time;
  int end_time;
  bool at_sunset;
  bool at_sunrise;
  bool at_dusk;
  bool at_dawn;
  bool any_required;
  uint32_t access;
};

bool light1 = false;
bool light2 = false;

int twilight_counter = 0;

bool twilight = false;
bool twilight_sensor = false;

String twin = "";
String single_button_function = "12";
String double_button_function = "0";
String long_button_function = "0";

bool readSettings(bool backup);
void saveSettings();
void saveSettings(bool log);
String getSwitchDetail();
String getValue();
void sayHelloToTheServer();
void introductionToServer();
void startServices();
void handshake();
void requestForState();
void exchangeOfBasicData();
void buttonSingle(void* b);
void buttonDouble(void* b);
void buttonLong(void* b);
void buttonFunction(String button);
void readData(String payload, bool per_wifi);
bool hasTheLightChanged();
bool automaticSettings();
bool automaticSettings(bool light_changed);
void setSmart();
void setLights(String orderer, bool put_online);
