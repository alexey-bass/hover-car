#include <Wire.h>
#include <Adafruit_MCP4725.h>

Adafruit_MCP4725 dac;

void setup() {
  dac.begin(0x60);
  dac.setVoltage(round((4095 * 1.3) / 3.3), true);
}

void loop() {
  delay(1000);
}
