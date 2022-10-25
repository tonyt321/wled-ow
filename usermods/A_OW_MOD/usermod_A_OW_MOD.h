#pragma once

#include "wled.h"

//Pin defaults for QuinLed Dig-Uno (A0)
#define PHOTORESISTOR_PIN A0

// the frequency to check photoresistor, 5 seconds
#ifndef USERMOD_SN_PHOTORESISTOR_MEASUREMENT_INTERVAL
#define USERMOD_SN_PHOTORESISTOR_MEASUREMENT_INTERVAL 5000
#endif

// how many seconds after boot to take first measurement, 7 seconds
#ifndef USERMOD_SN_PHOTORESISTOR_FIRST_MEASUREMENT_AT
#define USERMOD_SN_PHOTORESISTOR_FIRST_MEASUREMENT_AT 7000
#endif

// supplied voltage
#ifndef USERMOD_SN_PHOTORESISTOR_REFERENCE_VOLTAGE
#define USERMOD_SN_PHOTORESISTOR_REFERENCE_VOLTAGE 5
#endif

// 10 bits
#ifndef USERMOD_SN_PHOTORESISTOR_ADC_PRECISION
#define USERMOD_SN_PHOTORESISTOR_ADC_PRECISION 1024.0
#endif

// resistor size 10K hms
#ifndef USERMOD_SN_PHOTORESISTOR_RESISTOR_VALUE
#define USERMOD_SN_PHOTORESISTOR_RESISTOR_VALUE 10000.0
#endif

// only report if differance grater than offset value
#ifndef USERMOD_SN_PHOTORESISTOR_OFFSET_VALUE
#define USERMOD_SN_PHOTORESISTOR_OFFSET_VALUE 5
#endif

class Usermod_A_OW_MOD : public Usermod
{
private:
  float referenceVoltage = USERMOD_SN_PHOTORESISTOR_REFERENCE_VOLTAGE;
  float resistorValue = USERMOD_SN_PHOTORESISTOR_RESISTOR_VALUE;
  float adcPrecision = USERMOD_SN_PHOTORESISTOR_ADC_PRECISION;
  int8_t offset = USERMOD_SN_PHOTORESISTOR_OFFSET_VALUE;

  unsigned long readingInterval = USERMOD_SN_PHOTORESISTOR_MEASUREMENT_INTERVAL;
  // set last reading as "40 sec before boot", so first reading is taken after 20 sec
  unsigned long lastMeasurement = UINT32_MAX - (USERMOD_SN_PHOTORESISTOR_MEASUREMENT_INTERVAL - USERMOD_SN_PHOTORESISTOR_FIRST_MEASUREMENT_AT);
  // flag to indicate we have finished the first getTemperature call
  // allows this library to report to the user how long until the first
  // measurement
  bool getLuminanceComplete = false;
  uint16_t lastLDRValue = -1000;

  // flag set at startup
  bool disabled = false;
  bool disabled1 = false;


  // strings to reduce flash memory usage (used more than twice)
  static const char _name[];
  static const char _Status_bar[];
  static const char _battery_bar[];
  static const char _readInterval[];
  static const char _referenceVoltage[];
  static const char _resistorValue[];
  static const char _adcPrecision[];
  static const char _offset[];

  bool checkBoundSensor(float newValue, float prevValue, float maxDiff)
  {
    return isnan(prevValue) || newValue <= prevValue - maxDiff || newValue >= prevValue + maxDiff;
  }

  uint16_t getLuminance()
  {
    // http://forum.arduino.cc/index.php?topic=37555.0
    // https://forum.arduino.cc/index.php?topic=185158.0
    float volts = analogRead(PHOTORESISTOR_PIN) * (referenceVoltage / adcPrecision);
    float amps = volts / resistorValue;
    float lux = amps * 1000000 * 2.0;

    lastMeasurement = millis();
    getLuminanceComplete = true;
    return uint16_t(lux);
  }

public:
  void setup()
  {
    // set pinmode
    pinMode(PHOTORESISTOR_PIN, INPUT);
  }

  void loop()
  {
    if (disabled || strip.isUpdating())
      return;

    unsigned long now = millis();

    // check to see if we are due for taking a measurement
    // lastMeasurement will not be updated until the conversion
    // is complete the the reading is finished
    if (now - lastMeasurement < readingInterval)
    {
      return;
    }

    uint16_t currentLDRValue = getLuminance();
    if (checkBoundSensor(currentLDRValue, lastLDRValue, offset))
    {
      lastLDRValue = currentLDRValue;

      if (WLED_MQTT_CONNECTED)
      {
        char subuf[45];
        strcpy(subuf, mqttDeviceTopic);
        strcat_P(subuf, PSTR("/luminance"));
        mqtt->publish(subuf, 0, true, String(lastLDRValue).c_str());
      }
      else
      {
        DEBUG_PRINTLN("Missing MQTT connection. Not publishing data");
      }
    }
  }

  void addToJsonInfo(JsonObject &root)
  {
    JsonObject user = root[F("u")];
    if (user.isNull())
      user = root.createNestedObject(F("u"));

    JsonArray lux = user.createNestedArray(F("Luminance"));

    if (!getLuminanceComplete)
    {
      // if we haven't read the sensor yet, let the user know
      // that we are still waiting for the first measurement
      lux.add((USERMOD_SN_PHOTORESISTOR_FIRST_MEASUREMENT_AT - millis()) / 1000);
      lux.add(F(" sec until read"));
      return;
    }

    lux.add(lastLDRValue);
    lux.add(F(" lux"));
  }

  uint16_t getId()
  {
    return USERMOD_ID_A_OW_MOD;
  }

  /**
     * addToConfig() (called from set.cpp) stores persistent properties to cfg.json
     */
  void addToConfig(JsonObject &root)
  {
    // we add JSON object.
    JsonObject top = root.createNestedObject(FPSTR(_name)); // usermodname
    top[FPSTR(_Status_bar)] = !disabled;
    top[FPSTR(_battery_bar)] = !disabled1;
    top[FPSTR(_readInterval)] = readingInterval / 1000;
    top[FPSTR(_referenceVoltage)] = referenceVoltage;
    top[FPSTR(_resistorValue)] = resistorValue;
    top[FPSTR(_adcPrecision)] = adcPrecision;
    top[FPSTR(_offset)] = offset;

    DEBUG_PRINTLN(F("A OW MOD config saved."));
  }

  /**
  * readFromConfig() is called before setup() to populate properties from values stored in cfg.json
  */
  bool readFromConfig(JsonObject &root)
  {
    // we look for JSON object.
    JsonObject top = root[FPSTR(_name)];
    if (top.isNull()) {
      DEBUG_PRINT(FPSTR(_name));
      DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
      return false;
    }

    disabled         = !(top[FPSTR(_Status_bar)] | !disabled);
    disabled1         = !(top[FPSTR(_battery_bar)] | !disabled1);
    readingInterval  = (top[FPSTR(_readInterval)] | readingInterval/1000) * 1000; // convert to ms
    referenceVoltage = top[FPSTR(_referenceVoltage)] | referenceVoltage;
    resistorValue    = top[FPSTR(_resistorValue)] | resistorValue;
    adcPrecision     = top[FPSTR(_adcPrecision)] | adcPrecision;
    offset           = top[FPSTR(_offset)] | offset;
    DEBUG_PRINT(FPSTR(_name));
    DEBUG_PRINTLN(F(" config (re)loaded."));

    // use "return !top["newestParameter"].isNull();" when updating Usermod with new features
    return true;
  }
};

// strings to reduce flash memory usage (used more than twice)
//                           _veriable         "what it says on the webpage"
const char Usermod_A_OW_MOD::_name[] PROGMEM = "Enabled Features";
const char Usermod_A_OW_MOD::_Status_bar[] PROGMEM = "Mirror Status bar error";
const char Usermod_A_OW_MOD::_battery_bar[] PROGMEM = "Display battery on dismount";
const char Usermod_A_OW_MOD::_readInterval[] PROGMEM = "read-interval-s";
const char Usermod_A_OW_MOD::_referenceVoltage[] PROGMEM = "supplied-voltage";
const char Usermod_A_OW_MOD::_resistorValue[] PROGMEM = "resistor-value";
const char Usermod_A_OW_MOD::_adcPrecision[] PROGMEM = "adc-precision";
const char Usermod_A_OW_MOD::_offset[] PROGMEM = "offset";