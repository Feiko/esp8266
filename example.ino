#include "esp8266.h"
#include <SoftwareSerial.h>
#include <errno.h>

#define ESP_TX 3
#define ESP_RX 2
#define ESP_RESET 4

SoftwareSerial espconn(ESP_TX, ESP_RX);
ESP8266 esp(espconn, ESP_RESET);
uint8_t packetbuffer[100];

void setup() {
    Serial.begin(115200);
    espconn.begin(9600);

    Serial.println(F("Hello World"));

    if (!esp.reset()) {
        Serial.println(F("wifi reset error"));
        if (!esp.reset()) {
            Serial.println(F("wifi reset error"));
            return;
        }
    }

    if (!esp.joinAP(F("SSID"), F("PASS"))) {
        Serial.print(F("wifi join error: "));
        Serial.println(errno);
        return;
    }
    Serial.println(F("JOINED"));

    esp.putPacketBuffer(packetbuffer, sizeof(packetbuffer));

    if (!esp.tcpOpen("192.168.87.101", 6979)) {
        Serial.print(F("tcp open error: "));
        Serial.println(errno);
        return;
    }
    Serial.println(F("OPENED"));

    if (!esp.tcpSend((const uint8_t*)"1337\n", 5)) {
        Serial.print(F("tcp write error: "));
        Serial.println(errno);
        return;
    }

    Serial.println(F("WRITTEN"));
}

void loop() {
    while (true) {
        int len = esp.available();
        if (len > 0) {
            Serial.write(esp.takePacketBuffer(), len);
            esp.putPacketBuffer(packetbuffer, sizeof(packetbuffer));
            Serial.println(F("munch munch"));
        }
    }

    while (Serial.available()) {
        espconn.write(Serial.read());
    }
    while (espconn.available()) {
        Serial.write(espconn.read());
    }
}

