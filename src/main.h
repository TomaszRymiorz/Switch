#include <Arduino.h>
#include <SparkFun_APDS9960.h>

const String device = "switch";

const int apds9960_pin = D3;
const int light_pin[] = {D0, D6, D7};

SparkFun_APDS9960 apds = SparkFun_APDS9960();
volatile bool isr_flag = 0;
bool adps_init;

struct Smart {
  String lights;
  String days;
  bool onAtNight;
  bool offAtDay;
  int onTime;
  int offTime;
  bool enabled;
  uint32_t access;
};

bool first_light = false;
bool second_light = false;
bool third_light = false;

void interruptRoutine();
void initApds();
void setupLightsPins();
void readSettings();
void saveTheSettings();
void sayHelloToTheServer() ;
void startRestServer();
void handshake();
void requestForState();
void readData(String payload, bool perWiFi);
void setSmart();
void checkSmart(bool lightHasChanged);
String statesOfLights();
void handleGesture();
void setLights(String gesture);
