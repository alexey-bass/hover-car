#include <Wire.h>
#include <Adafruit_MCP4725.h>

// ===== CONFIG =====
uint8_t DAC_ADDRESS = 0x60;
float DAC_STARTUP_VOLTAGE = 1.800;
float DAC_SUPPLY_VOLTAGE  = 5.000;
uint16_t HC_DAC_VALUE_MAX = 4095; // 0x0FFF
// ===== END OF CONFIG =====

Adafruit_MCP4725 dac;

void setup() {
  dac.begin(DAC_ADDRESS);
  dac.setVoltage(round((HC_DAC_VALUE_MAX * DAC_STARTUP_VOLTAGE) / DAC_SUPPLY_VOLTAGE), true);
}

void loop() {
  delay(1000);
}
