#include <Arduino.h>
#include <avdweb_Switch.h>

#define light_switch

const char device[7] = "switch";
const char smart_prefix = 'l';
const uint8_t version = 23;

const int led_pin = 16;
const int relay_pin[] = {13, 4};

Switch button1 = Switch(12);
Switch button2 = Switch(14);

bool restore_on_power_loss = false;
int fixit = 0;

bool light[] = {false, false};

String twin_ip = "";
String twin_mac = "";
String single_button_function = "12";
String double_button_function = "0";
String long_button_function = "0";
bool key_lock = false;

bool readSettings(bool backup);
void saveSettings();
void saveSettings(bool log);
String getValue();
void startServices();
void handshake();
void requestForState();
void exchangeOfBasicData();
void buttonSingle(void* b);
void buttonDouble(void* b);
void buttonLong(void* b);
void buttonFunction(String button);
void readData(const String& payload, bool per_wifi);
void automation();
void smartAction();
void setLights(String orderer);
