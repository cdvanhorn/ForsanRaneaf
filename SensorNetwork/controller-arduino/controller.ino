#include "controller_config.h"
#include "base64.hpp"

HardwareSerial infotainmentSerial(2);

struct status_packet {
  byte message_length;
  byte message_type;
  word status_flags;
  word vehicle_rpm;
};

byte buffer[6];
byte encoded_buffer[8];
char output_buffer[30];

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
  struct status_packet packet;
  packet.message_length = 6;
  packet.message_type = 1;
  packet.status_flags = 0;
  packet.vehicle_rpm = 3250;

  memcpy(buffer, &packet.message_length, 1);
  memcpy(&buffer[1], &packet.message_type, 1);
  memcpy(&buffer[2], &packet.status_flags, 2);
  memcpy(&buffer[4], &packet.vehicle_rpm, 2);

  int enclen = encode_base64(buffer, 6, encoded_buffer);

  // sprintf(output_buffer, "DEAD");
  // for(size_t i = 0; i < sizeof(buffer); i++) {
  //   sprintf(output_buffer + 4 + (i * 2), "%02x", buffer[i]);
  // }
  // sprintf(output_buffer + 4 + (sizeof(buffer) * 2), "BEEF");
  sprintf(output_buffer, "++++%s----", encoded_buffer);

  if(isGPIOSerial()) {
    infotainmentSerial.print("the sensor network controller says hello");
  } else {
    Serial.write(output_buffer);
  }
}

void setup() {
  // Serial Monitor
  Serial.begin(SERIAL_BAUD);

  Serial.println(sizeof(struct status_packet));
  
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