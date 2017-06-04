#include <math.h>

// pins
#define VOLTAGE_PIN 0
#define TEMP_PIN 1
#define RELAY_LOAD 3
#define RELAY_CHARGE 2
#define CHARGE_NOT_DONE_PIN 4

// params
const int POLL_MS = 1000;
const int CELL_COUNT = 3;
const float MIN_CELL_VOLTAGE = 3.4;
const float THERM_A1 = 3.354016E-3;
const float THERM_B1 = 2.569850E-4;
const float THERM_C1 = 2.620131E-6;
const float THERM_D1 = 6.383091E-8;

// when the battery is disconnected or done, pin 4 is on

void setup() {
  pinMode(RELAY_LOAD, OUTPUT);
  pinMode(RELAY_CHARGE, OUTPUT);
  pinMode(CHARGE_NOT_DONE_PIN, INPUT);

  Serial.begin(115200);
  firstRow();
}

float getPinVoltage(int pin) {
  return analogRead(pin) / 1024.0f * 5;
}

float getTemp() {
  // voltage
  float v = getPinVoltage(TEMP_PIN);

  float rt = 10000; // resistance of therm @ 25C
  float r = (rt * (5 - v)) / v; // resistance now

  float t = 1.0f / ( // temperature of therm by steinhart-hart
    THERM_A1 +
    THERM_B1 * log(r/rt) +
    THERM_C1 * pow(log(r/rt), 2) +
    THERM_D1 * pow(log(r/rt), 3)
  );

  return t - 273.15; // we return C, for "cience"
}

bool isConnected() {
  return getVoltage() > 1; // why not 1
}

bool isFull() {
  return isConnected() && !digitalRead(CHARGE_NOT_DONE_PIN);
}

float getVoltage() {
  return getPinVoltage(VOLTAGE_PIN) * 2.8f;
}

bool isEmpty() {
  return getVoltage() < (CELL_COUNT * MIN_CELL_VOLTAGE);
}

struct BatteryState {
  bool isCharging;
  int waitMs;
} bs;

void loop() {
  delay(POLL_MS);

  logData();

  doStateMachine();
}

// this could be nicer, but not without some serious preproc abuse
void firstRow() {
  Serial.print("getTemp()");
  Serial.print(",");
  Serial.print("getVoltage()");
  Serial.print(",");
  Serial.print("bs.isCharging");
  Serial.print(",");
  Serial.print("isFull()");
  Serial.println();
}

void logData() {
  // getTemp, getVoltage, isCharging, isFull
  Serial.print(getTemp());
  Serial.print(",");
  Serial.print(getVoltage());
  Serial.print(",");
  Serial.print(bs.isCharging);
  Serial.print(",");
  Serial.print(isFull());
  Serial.println();
}

enum ChargeState { STOP, DISCHARGE, CHARGE };

void charge(ChargeState cs); // arduino is literal garbage.
void charge(ChargeState cs) {
  digitalWrite(RELAY_LOAD, cs == ChargeState::DISCHARGE);
  digitalWrite(RELAY_CHARGE, cs == ChargeState::CHARGE);
}

void doStateMachine() {
  // section: early exit conditions
  // please don't change any state here

  if (!isConnected())
    // no sense in running the harness if there's no battery...
    // TODO: should we reset the harness until the battery's up?
    return;

  // this condition accounts for the delay happening first.
  if (bs.waitMs > POLL_MS) {
    // something has put the sensors into an undefined state;
    // the state machine should sleep until the sensors are okay
    bs.waitMs -= POLL_MS;
    return;
  }

  bool isChargingLast = bs.isCharging;
  // you can change state after this line

  if (getTemp() > 35) {
    charge(ChargeState::STOP);
  }
    
  if (bs.isCharging) {
    if (!isFull())
      charge(ChargeState::CHARGE);
    else
      bs.isCharging = false;
  } else {
    if (!isEmpty())
      charge(ChargeState::DISCHARGE);
    else
      bs.isCharging = true;
  }

  // section: detect state machine changes and react to them
  if (isChargingLast != bs.isCharging)
    bs.waitMs = 5000;
}
