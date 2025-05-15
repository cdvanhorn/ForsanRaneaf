#define RXD2 16
#define TXD2 17
#define GPS_BAUD 115200

HardwareSerial infotainmentSerial(2);

void setup() {
  // Serial Monitor
  Serial.begin(115200);
  
  // Start Serial 2 with the defined RX and TX pins and a baud rate of 9600
  infotainmentSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);
  Serial.println("Serial 2 started at 115200 baud rate");
}

void loop() {
  // put your main code here, to run repeatedly:
  infotainmentSerial.println("the sensor network controller says hello");
  Serial.println("sent data on uart");

  Serial.println("reading uart");
  while(infotainmentSerial.available() > 0) {
    char data = infotainmentSerial.read();
    Serial.print(data);
  }

  Serial.println("\nsleeping");
  delay(1000);
}