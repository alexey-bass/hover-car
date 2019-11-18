#include <ESP8266WiFi.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_MCP4725.h>
#include <SimpleTimer.h>
#include <ESP8266WebServer.h>



// ===== CONFIG =====
bool DEBUG_SERIAL  = true;

const char *HC_WIFI_SSID = "YOUR_SSID";
const char *HC_WIFI_PASSWORD = "YOUR_PASSWORD";

uint8_t HC_LED_OUTPUT_PIN    = D4;
uint8_t HC_REVERSE_INPUT_PIN = D6;
uint8_t HC_BUZZER_OUTPUT_PIN = D5; // D5, D6, D7 or D8
uint8_t HC_RELAY_OUTPUT_PIN  = D7; // D0, D1, D2, D5, D6, D7 or D8

float    HC_DAC_SUPPLY_VOLTAGE = 3.3; // set correct DAC VCC voltage
uint16_t HC_DAC_VALUE_MAX = 4095; // 0x0FFF

float HC_THROTTLE_LEVEL_MIN = 1.300;
float HC_THROTTLE_LEVEL_MAX = 3.300;
// ===== END OF CONFIG =====



Adafruit_MCP4725 HC_DAC;

SimpleTimer HC_TIMER;
uint8_t HC_BUZZER_TIMER_ID;
bool HC_BUZZER_REVERSE_SOUND_ON_PHASE = false;

ESP8266WebServer HC_SERVER(80);

bool HC_WIFI_CONNECTED = false;

bool HC_REVERSE_ENGAGED = false;
bool HC_REVERSE_DIRECTION = false;

float HC_THROTTLE_FORWARD_LEVEL = HC_THROTTLE_LEVEL_MIN; // start voltage
float HC_THROTTLE_REVERSE_LEVEL = HC_THROTTLE_LEVEL_MIN; // start voltage



void setup() {
  if (DEBUG_SERIAL) {
    Serial.begin(115200);
    Serial.println();
    Serial.println("SETUP started");
  }

  // keep wifi last thing to do on setup
  hc_WifiSetup();
  hc_ServerSetup();

  hc_LedSetup();
  hc_RelaySetup();

  // set pedal input
  pinMode(HC_REVERSE_INPUT_PIN, INPUT_PULLUP);

  hc_DacSetup();
  
  hc_BuzzerSetup();
  hc_BuzzerPlayStartup();
//  hc_BuzzerPlaySequence({1047, 1175, 1319, 1397, 1568}, 50);
  
  
//  delay(1000);
//  hc_DacSetVoltage(HC_THROTTLE_LEVEL_MIN);

  hc_TimersSetup();

  // we starting from FORWARD direction
  hc_DacSetVoltage(HC_THROTTLE_FORWARD_LEVEL);

  if (DEBUG_SERIAL) Serial.println("SETUP finished");
}

void loop() {
  HC_TIMER.run();

  if (HC_WIFI_CONNECTED) {
    HC_SERVER.handleClient();
  }

  switch (WiFi.status()) {
    case WL_CONNECTED:
      if (!HC_WIFI_CONNECTED) {
        if (DEBUG_SERIAL) {
          Serial.print("Connected to WIFI network: ");
          Serial.println(HC_WIFI_SSID);
          Serial.print("Got IP address: ");
          Serial.println(WiFi.localIP());
        }
        HC_WIFI_CONNECTED = true;
        digitalWrite(HC_LED_OUTPUT_PIN, LOW); // LED set ON
        hc_BuzzerPlayWifiConnected();
      }
      break;

//    case WL_IDLE_STATUS:
//    case WL_NO_SSID_AVAIL:
//    case WL_CONNECT_FAILED:
//    case WL_CONNECTION_LOST:
//    case WL_DISCONNECTED:
    default:
      if (HC_WIFI_CONNECTED) {
        if (DEBUG_SERIAL) {
          Serial.print("Disconnected from WIFI network: ");
          Serial.println(HC_WIFI_SSID);
        }
        HC_WIFI_CONNECTED = false;
        digitalWrite(HC_LED_OUTPUT_PIN, HIGH); // LED set OFF
        hc_BuzzerPlayWifiDisconnected();
      }
      break;
  }

  HC_REVERSE_ENGAGED = (digitalRead(HC_REVERSE_INPUT_PIN) == LOW) ? true : false;

  // TODO: when driving fast and reverse engaged - we need to stop (send brake signal)
  //       to avoid driver's injury

  if (!HC_REVERSE_ENGAGED) {
    if (HC_REVERSE_DIRECTION) {
      if (DEBUG_SERIAL) Serial.println("Change direction to FORWARD");
      HC_REVERSE_DIRECTION = false;
      HC_TIMER.disable(HC_BUZZER_TIMER_ID);
      digitalWrite(HC_RELAY_OUTPUT_PIN, LOW);
      hc_DacSetVoltage(HC_THROTTLE_FORWARD_LEVEL);
    }
  } else {
    if (!HC_REVERSE_DIRECTION) {
      if (DEBUG_SERIAL) Serial.println("Change direction to REVERSE");
      HC_REVERSE_DIRECTION = true;
      HC_TIMER.enable(HC_BUZZER_TIMER_ID);
      digitalWrite(HC_RELAY_OUTPUT_PIN, HIGH);
      hc_DacSetVoltage(HC_THROTTLE_REVERSE_LEVEL);
    }
  }

  delay(1000);
}





void hc_DacSetVoltage(float target) {
  if (DEBUG_SERIAL) {
    Serial.print("DAC new target voltage: ");
    Serial.println(target);
  }
  HC_DAC.setVoltage(round((HC_DAC_VALUE_MAX * target) / HC_DAC_SUPPLY_VOLTAGE), false);
}

void hc_LedSetup() {
  pinMode(HC_LED_OUTPUT_PIN, OUTPUT);
  digitalWrite(HC_LED_OUTPUT_PIN, HIGH); // LED set OFF
}

void hc_DacSetup() {
  // For Adafruit MCP4725A1 the address is 0x62 (default) or 0x63 (ADDR pin tied to VCC)
  // For MCP4725A0 the address is 0x60 or 0x61
  // For MCP4725A2 the address is 0x64 or 0x65
  HC_DAC.begin(0x60);
}

void hc_WifiSetup() {
//  WiFi.mode(WIFI_OFF);
//  WiFi.forceSleepBegin();
  WiFi.disconnect();
  WiFi.setAutoConnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(HC_WIFI_SSID, HC_WIFI_PASSWORD);
}

void hc_ServerHandleHome() {
  String response = "";

  response += "<html>\n";
  response += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
  response += "<body>\n";
  response += "<h3>Hover Car v2.0</h3>\n";
  response += "<pre>\n";
  response += "\nCurrent settings";
  response += "\n- FORWARD: "+ String(HC_THROTTLE_FORWARD_LEVEL) + " V";
  response += "\n- REVERSE: "+ String(HC_THROTTLE_REVERSE_LEVEL) + " V";
  response += "\n";
  response += "\nPresets";
  response += "\n- <a href='/set/?forward=1.5'>F=1.5</a>";
  response += "\n- <a href='/set/?forward=2.0'>F=2.0</a>";
  response += "\n- <a href='/set/?forward=2.5'>F=2.5</a>";
  response += "\n- <a href='/set/?forward=3.0'>F=3.0</a>";
  response += "</pre>\n";

  response += "\n</body></html>";
  
  HC_SERVER.send(200, "text/html", response);
}

float hc_ValidateThrottleLimits(float level) {
  if (level > HC_THROTTLE_LEVEL_MAX) {
    return HC_THROTTLE_LEVEL_MAX;
  } else if (level < HC_THROTTLE_LEVEL_MIN) {
    return HC_THROTTLE_LEVEL_MIN;
  }
  return level;
}

void hc_ServerHandleSet() {
  for (int i = 0; i < HC_SERVER.args(); i++) {
    if (HC_SERVER.argName(i) == "forward") {
      HC_THROTTLE_FORWARD_LEVEL = hc_ValidateThrottleLimits(HC_SERVER.arg(i).toFloat());
      if (DEBUG_SERIAL) {
        Serial.print("Set FORWARD voltage to ");
        Serial.println(HC_THROTTLE_FORWARD_LEVEL);
      }
      hc_BuzzerPlayForwardApplied();
    } else if (HC_SERVER.argName(i) == "reverse") {
      HC_THROTTLE_REVERSE_LEVEL = hc_ValidateThrottleLimits(HC_SERVER.arg(i).toFloat());
      if (DEBUG_SERIAL) {
        Serial.print("Set REVERSE voltage to ");
        Serial.println(HC_THROTTLE_REVERSE_LEVEL);
      }
      hc_BuzzerPlayReverseApplied();
    }
  } 

  if (!HC_REVERSE_DIRECTION) {
    hc_DacSetVoltage(HC_THROTTLE_FORWARD_LEVEL);
  } else {
    hc_DacSetVoltage(HC_THROTTLE_REVERSE_LEVEL);
  }

  HC_SERVER.sendHeader("Location", "/");
  HC_SERVER.send(303); 
}

void hc_ServerSetup() {
  HC_SERVER.on("/",     hc_ServerHandleHome);
  HC_SERVER.on("/set/", hc_ServerHandleSet);
  HC_SERVER.onNotFound([]() {
    HC_SERVER.send(404, "text/plain", "ERROR 404: Page Not Found");
  });
  HC_SERVER.begin();
}

void hc_RelaySetup() {
  pinMode(HC_RELAY_OUTPUT_PIN, OUTPUT);
  digitalWrite(HC_RELAY_OUTPUT_PIN, LOW);
}

void hc_BuzzerSetup() {
  pinMode(HC_BUZZER_OUTPUT_PIN, OUTPUT);
  digitalWrite(HC_BUZZER_OUTPUT_PIN, LOW);
}

void hc_TimersSetup() {
  HC_BUZZER_TIMER_ID = HC_TIMER.setInterval(500, hc_BuzzerPlayReverse);
  HC_TIMER.disable(HC_BUZZER_TIMER_ID);
}

/* TODO Make this work to use in all Play* calls
void hc_BuzzerPlaySequence(int tones[], int pause) {
  // https://github.com/wemos/D1_mini_Examples/blob/master/examples/04.Shields/Buzzer_Shield/Do_Re_Mi/Do_Re_Mi.ino
  // Note name: C6 D6 E6 F6 G6 http://newt.phys.unsw.edu.au/jw/notes.html
  uint8_t len = sizeof(tones) / sizeof(tones[0]);
  for (int i = 0; i < len; i++) {
    tone(HC_BUZZER_OUTPUT_PIN, tones[i]);
    delay(pause);
    noTone(HC_BUZZER_OUTPUT_PIN);
    if (i != len -1) { // skip last delay
      delay(pause);
    }
  }
}
*/

void hc_BuzzerPlayStartup() {
  int tones[] = {1047, 1175, 1319, 1397, 1568};
  uint8_t pause = 50;
  
  uint8_t len = sizeof(tones) / sizeof(tones[0]);
  for (int i = 0; i < len; i++) {
    tone(HC_BUZZER_OUTPUT_PIN, tones[i]);
    delay(pause);
    noTone(HC_BUZZER_OUTPUT_PIN);
    if (i != len -1) { // skip last delay
      delay(pause);
    }
  }
}

void hc_BuzzerPlayWifiConnected() {
  int tones[] = {440, 493, 523};
  uint8_t pause = 100;
  
  uint8_t len = sizeof(tones) / sizeof(tones[0]);
  for (int i = 0; i < len; i++) {
    tone(HC_BUZZER_OUTPUT_PIN, tones[i]);
    delay(pause);
    noTone(HC_BUZZER_OUTPUT_PIN);
    if (i != len -1) { // skip last delay
      delay(pause);
    }
  }
}

void hc_BuzzerPlayWifiDisconnected() {
  int tones[] = {523, 493, 440};
  uint8_t pause = 100;
  
  uint8_t len = sizeof(tones) / sizeof(tones[0]);
  for (int i = 0; i < len; i++) {
    tone(HC_BUZZER_OUTPUT_PIN, tones[i]);
    delay(pause);
    noTone(HC_BUZZER_OUTPUT_PIN);
    if (i != len -1) { // skip last delay
      delay(pause);
    }
  }
}

void hc_BuzzerPlayForwardApplied() {
  int tones[] = {523, 587, 659};
  uint8_t pause = 50;
  
  uint8_t len = sizeof(tones) / sizeof(tones[0]);
  for (int i = 0; i < len; i++) {
    tone(HC_BUZZER_OUTPUT_PIN, tones[i]);
    delay(pause);
    noTone(HC_BUZZER_OUTPUT_PIN);
    if (i != len -1) { // skip last delay
      delay(pause);
    }
  }
}

void hc_BuzzerPlayReverseApplied() {
  int tones[] = {220, 246, 261};
  uint8_t pause = 50;
  
  uint8_t len = sizeof(tones) / sizeof(tones[0]);
  for (int i = 0; i < len; i++) {
    tone(HC_BUZZER_OUTPUT_PIN, tones[i]);
    delay(pause);
    noTone(HC_BUZZER_OUTPUT_PIN);
    if (i != len -1) { // skip last delay
      delay(pause);
    }
  }
}

void hc_BuzzerPlayReverse() {
  if (!HC_BUZZER_REVERSE_SOUND_ON_PHASE) {
    tone(HC_BUZZER_OUTPUT_PIN, 2000);
    delay(500);
    noTone(HC_BUZZER_OUTPUT_PIN);
  }
  HC_BUZZER_REVERSE_SOUND_ON_PHASE = !HC_BUZZER_REVERSE_SOUND_ON_PHASE; 
}
