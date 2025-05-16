#include "controller_config.h"

HardwareSerial infotainmentSerial(2);

bool isGPIOSerial() {
  return strcmp(SERIAL_OUTPUT, SERIAL_GPIO) == 0;
}

void readSerial() {
  if(isGPIOSerial()) {
    while(infotainmentSerial.available() > 0) {
      char data = infotainmentSerial.read();
    }
  } else {
    while(Serial.available() > 0) {
      char data = Serial.read();
    }
  }
}

void writeSerial() {
  if(isGPIOSerial()) {
    infotainmentSerial.print("the sensor network controller says hello");
  } else {
    Serial.print("the sensor network controller says hello");
  }
}

void setup() {
  // Serial Monitor
  Serial.begin(SERIAL_BAUD);
  
  if(isGPIOSerial()) {
    // Start Serial 2 with the defined RX and TX pins
    infotainmentSerial.begin(SERIAL_BAUD, SERIAL_8N1, RXD2, TXD2);
    Serial.println("GPIO serial started");
  }
}

void loop() {
  writeSerial();
  readSerial();
  delay(2000);
}