# An ESP8266 Arduino driver

Status: works for reading/writing a single client TCP connection.

A efficient esp8266 driver for Arduino using the AT commands. Main feature is
to never miss a data packet from the server.

example:
```
#include "esp8266.h"
#include <SoftwareSerial.h>

#define ESP_TX 3
#define ESP_RX 2

SoftwareSerial espconn(ESP_TX, ESP_RX);
ESP8266 esp(espconn);
uint8_t packetbuffer[100];

void setup() {
    espconn.begin(9600);
    Serial.begin(9600);

    if (!esp.reset()) {
        Serial.println(F("wifi reset error"));
        return;
    }

    if (!esp.joinAP(SSID, PASS)) {
        Serial.println(F("wifi join error"));
        return;
    }
    Serial.println("JOINED");

    // setup the packet buffer here, so even if the server pushes data on connect, we get to read it
    esp.putPacketBuffer(packetbuffer, sizeof(packetbuffer));

    if (!esp.tcpOpen(IP, PORT)) {
        Serial.println(F("tcp open error"));
        return;
    }
    Serial.println("OPENED");

    if (!esp.tcpSend((const uint8_t*)"1337\n", 5)) {
        Serial.println(F("tcp write error"));
        return;
    }
    Serial.println("WRITTEN");
}

void loop() {
    int len = esp.available();
    if (len > 0) {
        Serial.write(esp.takePacketBuffer(), len);
        Serial.println();
        esp.putPacketBuffer(packetbuffer, sizeof(packetbuffer));
    }
}
```
