
#include "ESP8266WiFi.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MCP4725.h>

bool DEBUG_CONSOLE  = false;

// SCL GPIO5
// SDA GPIO4
#define OLED_RESET 0  // GPIO0
Adafruit_SSD1306 hcDisplay(OLED_RESET);

Adafruit_MCP4725 hcDac;

bool HC_PEDAL_ENGAGED_CURRENT = false;
bool HC_PEDAL_ENGAGED_PREV = false;

// ===== CONFIG =====
uint8_t HC_LED_OUTPUT_PIN  = D4;
uint8_t HC_PEDAL_INPUT_PIN = D6;
uint8_t HC_ANALOG_IN_PIN   = A0;

float    HC_DAC_SUPPLY_VOLTAGE = 3.3; // set correct DAC VCC voltage
uint16_t HC_DAC_VALUE_MAX = 4095; // 0x0FFF

// diff real value measured and set, should be 0.0
// measure actual on Vout with multimeter when MIN is set diff here
float    HC_DAC_ACTUAL_VOLTAGE_CORRECTION = +0.000; 

float HC_THROTTLE_OUTPUT_LEVEL_CURRENT = 0.000; // start voltage
float HC_THROTTLE_OUTPUT_LEVEL_MIN     = 1.100;
float HC_THROTTLE_OUTPUT_LEVEL_MAX     = 1.500;
float HC_THROTTLE_OUTPUT_LEVEL_STEP_UP = 0.050;
float HC_THROTTLE_OUTPUT_LEVEL_STEP_DN = 0.100; // brake faster then accelerate
// ===== END OF CONFIG =====

void displaySetup() {
  hcDisplay.begin(SSD1306_SWITCHCAPVCC, 0x3C); // 0x3C or 0x3D
  hcDisplay.clearDisplay();
  hcDisplay.display();
  hcDisplay.setTextSize(1); // 6 rows, 10 cols
  hcDisplay.setTextColor(WHITE);
  hcDisplay.display();
}

void displayPrepare() {
  hcDisplay.clearDisplay();
//  hcDisplay.display();
  hcDisplay.setCursor(0, 0);
//  hcDisplay.print("Hover-Car");
}

int adcInput = 0;
float voltageIn = 0;
float MCP4725_reading;

void displayUpdate() {
  displayPrepare();
  String pedalStatus = (HC_PEDAL_ENGAGED_CURRENT) ? "YES" : "NO";
  hcDisplay.printf("DIR: %s", (true) ? "F=>" : "<=B");
  hcDisplay.printf("\nGAS: %s", (HC_PEDAL_ENGAGED_CURRENT) ? "YES" : "NO");
  hcDisplay.printf("\nBRK: %s", "NO");

  adcInput = analogRead(HC_ANALOG_IN_PIN);
  voltageIn = (adcInput * HC_DAC_SUPPLY_VOLTAGE) / 1023.0;
  hcDisplay.print("\nVtg: ");
  hcDisplay.print(HC_THROTTLE_OUTPUT_LEVEL_CURRENT, 3);
  hcDisplay.print("\nVou: ");
  hcDisplay.print(voltageIn, 3);
  
  hcDisplay.printf("\n%d", millis() / 100);
}

void setDacVoltage(float target) {
  target += HC_DAC_ACTUAL_VOLTAGE_CORRECTION;
  hcDac.setVoltage(round((HC_DAC_VALUE_MAX * target) / HC_DAC_SUPPLY_VOLTAGE), false);
} 

void setup() {
  if (DEBUG_CONSOLE) {
    Serial.begin(9600);
  }

  // disable wifi
  //WiFi.mode(WIFI_OFF);
  //WiFi.forceSleepBegin();

  // setup LED
  pinMode(HC_LED_OUTPUT_PIN, OUTPUT);
  digitalWrite(HC_LED_OUTPUT_PIN, HIGH); // LED set OFF

  // set pedal input
  pinMode(HC_PEDAL_INPUT_PIN, INPUT_PULLUP);

  // setup display
  displaySetup();

  // setup DAC
  hcDac.begin(0x60);
  setDacVoltage(HC_THROTTLE_OUTPUT_LEVEL_MIN);
  HC_THROTTLE_OUTPUT_LEVEL_CURRENT = HC_THROTTLE_OUTPUT_LEVEL_MIN;

  displayUpdate();
}

void loop() {
  HC_PEDAL_ENGAGED_CURRENT = (digitalRead(HC_PEDAL_INPUT_PIN) == LOW) ? true : false;

  // if pedal state changed
  if (HC_PEDAL_ENGAGED_PREV != HC_PEDAL_ENGAGED_CURRENT) {

    if (DEBUG_CONSOLE) {
      Serial.print("Pedal: ");
      Serial.print((HC_PEDAL_ENGAGED_CURRENT) ? "YES" : "NO");
      Serial.print(", was ");
      Serial.print((HC_PEDAL_ENGAGED_PREV) ? "YES" : "NO");
      Serial.println();
    }

    if (HC_PEDAL_ENGAGED_CURRENT) {
      digitalWrite(HC_LED_OUTPUT_PIN, LOW); // LED set ON
    } else {
      digitalWrite(HC_LED_OUTPUT_PIN, HIGH); // LED set OFF
    }
    
    HC_PEDAL_ENGAGED_PREV = HC_PEDAL_ENGAGED_CURRENT;
    
    displayUpdate();
  }

  if (HC_PEDAL_ENGAGED_CURRENT && HC_THROTTLE_OUTPUT_LEVEL_CURRENT < HC_THROTTLE_OUTPUT_LEVEL_MAX) {
    HC_THROTTLE_OUTPUT_LEVEL_CURRENT += HC_THROTTLE_OUTPUT_LEVEL_STEP_UP;
    if (HC_THROTTLE_OUTPUT_LEVEL_CURRENT > HC_THROTTLE_OUTPUT_LEVEL_MAX) { // if exeed MAX
      HC_THROTTLE_OUTPUT_LEVEL_CURRENT = HC_THROTTLE_OUTPUT_LEVEL_MAX;
    }
    
    setDacVoltage(HC_THROTTLE_OUTPUT_LEVEL_CURRENT);
    if (DEBUG_CONSOLE) {
      //MCP4725_reading = (HC_DAC_SUPPLY_VOLTAGE / 4096.0) * HC_THROTTLE_OUTPUT_LEVEL_CURRENT; //3.3 is your supply voltage
      Serial.print("Expected Voltage: ");
      Serial.println(HC_THROTTLE_OUTPUT_LEVEL_CURRENT);
      //Serial.print("Current Voltage: ");
      //Serial.println(voltageIn);
      //Serial.println(MCP4725_reading, 3);
    }
    
    displayUpdate();
    
  } else if (!HC_PEDAL_ENGAGED_CURRENT && HC_THROTTLE_OUTPUT_LEVEL_CURRENT > HC_THROTTLE_OUTPUT_LEVEL_MIN) {
    HC_THROTTLE_OUTPUT_LEVEL_CURRENT -= HC_THROTTLE_OUTPUT_LEVEL_STEP_DN;
    if (HC_THROTTLE_OUTPUT_LEVEL_CURRENT < HC_THROTTLE_OUTPUT_LEVEL_MIN) { // if we went negative
      HC_THROTTLE_OUTPUT_LEVEL_CURRENT = HC_THROTTLE_OUTPUT_LEVEL_MIN;
    }
    
    setDacVoltage(HC_THROTTLE_OUTPUT_LEVEL_CURRENT);
    if (DEBUG_CONSOLE) {
      //MCP4725_reading = (3.3/4096.0) * HC_THROTTLE_OUTPUT_LEVEL_CURRENT; //3.3 is your supply voltage
      Serial.print("Expected Voltage: ");
      Serial.println(HC_THROTTLE_OUTPUT_LEVEL_CURRENT);
      //Serial.print("Current Voltage: ");
      //Serial.println(voltageIn);
      //Serial.println(MCP4725_reading, 3);
    }

    displayUpdate();
  }

  // finally, update display
  hcDisplay.display();
  
  delay(100);
}
