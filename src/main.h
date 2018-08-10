#include <Arduino.h>
#include <SparkFun_APDS9960.h>

const String device = "switch"; //blinds

const int sd_pin = D8;
const int apds9960_pin = D3;
const int light_pin[] = {D0, D6, D7};

SparkFun_APDS9960 apds = SparkFun_APDS9960();
volatile bool isr_flag = 0;
bool adps_init;

struct Smart {
  String lights;
  String days;
  bool onAtNight;
  int onTime;
  int offTime;
  uint32_t access;
};

bool first_light = false;
bool second_light = false;
bool third_light = false;

void interruptRoutine();
void initApds(bool beginning);
void setupLightsPins();
void startRestServer();
void handshake();
void requestForStates();
void receivedTheData();
String statesOfLights();
int readData(String payload);
void setSmart();
void checkSmart();
void handleGesture();
void setLights(String gesture);
