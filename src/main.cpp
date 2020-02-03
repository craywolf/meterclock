#include <Arduino.h>
#include <RTClib.h>

// #define DEBUG

/* define the pins used for the meters
 * must use pwm pins
 *
 * the meters are calibrated through use of trim pots
 * to ensure the full 0-255 range is used
 */

const byte VU_SEC_PIN  = 11;   // atmega328p pin 17
const byte VU_MIN_PIN  = 10;   // atmega328p pin 16
const byte VU_HOUR_PIN = 9;    // atmega328p pin 15

/* buttons used for setting time and
 * accessing other functions (like calibration)
 */

const byte BTN_HOUR = A1;   // atmega328p pin 24
const byte BTN_MIN  = A2;   // atmega328p pin 25
const byte BTN_SET  = A3;   // atmega328p pin 26

/*
 * defining these values in a lookup table helps the meter
 * be more precise than doing it with map()
 *
 * second meter sweeps, so we'll use map() for that one
 */
const byte hourVals[12] PROGMEM   = { 0, 22, 44, 67, 92, 117, 142, 166, 189, 212, 233, 255 };
const byte minuteVals[60] PROGMEM = { 0,   4,   9,   13,  17,  20,  24,  29,  33,  37,  41,  45,  49,  53,  57,
                                      62,  66,  71,  75,  79,  83,  87,  92,  96,  100, 105, 109, 114, 118, 123,
                                      127, 131, 136, 140, 144, 149, 153, 157, 162, 166, 170, 175, 179, 184, 188,
                                      193, 198, 202, 206, 210, 214, 219, 223, 227, 231, 235, 240, 244, 248, 251 };

unsigned long lastSecond = 0;   // time of last update to curSec

// create the real time clock instance
RTC_DS1307 rtc;

// define a variable to hold the current time
DateTime curTime;

/* function declarations
 */

void updateMeter(byte &, byte, byte);
void testMeter(byte);
void getRTCTime(DateTime &);
void displayTime(void);

void setup() {
#ifdef DEBUG
  Serial.begin(115200);
#endif

  // set up pwm pins used for meters
  pinMode(VU_SEC_PIN, OUTPUT);
  analogWrite(VU_SEC_PIN, 0);

  pinMode(VU_MIN_PIN, OUTPUT);
  analogWrite(VU_MIN_PIN, 0);

  pinMode(VU_HOUR_PIN, OUTPUT);
  analogWrite(VU_HOUR_PIN, 0);

  // no point in continuing if we can't communicate with the rtc
  if (!rtc.begin()) {
#ifdef DEBUG
    Serial.println(F("Couldn't find RTC"));
#endif
    // indicate the no-rtc condition by setting all meters to midpoint
    analogWrite(VU_SEC_PIN, 128);
    analogWrite(VU_MIN_PIN, 128);
    analogWrite(VU_HOUR_PIN, 128);
    while (1) {
    };
  }

  // if the rtc doesn't have a time stored, store one
  if (!rtc.isrunning()) {
#ifdef DEBUG
    Serial.println(F("RTC lost power, setting time to default"));
#endif
    // set time to 12:00 when resetting for minimal confusion
    rtc.adjust(DateTime(2020, 01, 01, 0, 0, 0));
  }

#ifndef DEBUG
  // show a little animation on startup
  testMeter(VU_HOUR_PIN);
  testMeter(VU_MIN_PIN);
  testMeter(VU_SEC_PIN);
#endif
}

void loop() {
  displayTime();
}

void displayTime() {
  static byte sec, min, hour = 0;        // keep track of last meter setting
  byte        newSec, newMin, newHour;   // new pwm values to compare against last set values

  // fetch time from rtc into global var
  getRTCTime(curTime);

  // convert hour from 24h to 12h
  byte h = (curTime.hour() == 0 || curTime.hour() == 12) ? 12 : curTime.hour() % 12;

  /*
   * calculate our meter values
   */

  // second hand sweeps
  unsigned long curSecMillis = (curTime.second() * 1000UL) + (unsigned long)(millis() - lastSecond);

  newSec  = map(curSecMillis, 0, 60000, 0, 255);
  newMin  = pgm_read_byte(&minuteVals[curTime.minute()]);
  newHour = pgm_read_byte(&hourVals[h - 1]);

  // update the meters
  updateMeter(sec, newSec, VU_SEC_PIN);
  updateMeter(min, newMin, VU_MIN_PIN);
  updateMeter(hour, newHour, VU_HOUR_PIN);
}

// Calculate new PWM value based on current and target
// Avoids writing to pin if value isn't changing

/*
 * update displays
 *
 * avoid calling analogWrite() if the value hasn't changed since
 * last time
 *
 * does a slow sweep downwards instead of letting the needle
 * slam against the stop
 */

void updateMeter(byte &curVal, byte targetVal, byte VU_PIN) {
  const unsigned long  updateInterval = 100;        // for downward sweep: ms between updates
  const unsigned long  sweepLength    = 750;        // total ms downward sweep should last
  static unsigned long lastUpdate     = millis();   // track the last time we updated the sweep

  // skip update if no change
  if (curVal != targetVal) {
    // no special handling needed for going up
    if (targetVal > curVal) {
      curVal = targetVal;
    }

    // if we're going down, sweep down instead of letting needle slam to 0
    if (targetVal < curVal) {
      byte interval = 255 / (sweepLength / (float)updateInterval);

      if (interval > curVal) {
        // don't drop below 0
        curVal = 0;
      } else {
        if ((unsigned long)(millis() - lastUpdate) > updateInterval) {
          curVal -= interval;
          lastUpdate = millis();
        }
      }
    }

    analogWrite(VU_PIN, curVal);
  }
}

/*
 * sweep a meter from min to max to min
 */
void testMeter(byte pin) {
  for (byte i = 0; i < 255; i++) {
    analogWrite(pin, i);
    delay(3);
  }
  delay(5);
  for (byte i = 255; i > 0; i--) {
    analogWrite(pin, i);
    delay(3);
  }
}

/*
 * fetch time from rtc into passed variable
 *
 * keeps track of last internal timestamp when the seconds
 * changed, for calculating second hand sweep
 */
void getRTCTime(DateTime &curTime) {
  byte oldSec = curTime.second();

  curTime = rtc.now();

  if (oldSec < curTime.second()) {
    lastSecond = millis();
  }

#ifdef DEBUG
  Serial.print(F("fetched time: "));
  Serial.print(curTime.hour());
  Serial.print(F(":"));
  Serial.print(curTime.minute());
  Serial.print(F(":"));
  Serial.println(curTime.second());
#endif
}