#define SSID F("your-ssid")
#define PASS F("your-password")
#define HOST F("your-host")
#define PORT 8080

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

    int s;
    if ((s = esp.reset())) {
        Serial.print(F("wifi reset error "));
        Serial.println(s);
        return;
    }

    if ((s = esp.joinAP(SSID, PASS))) {
        Serial.print(F("wifi join error"));
        Serial.println(s);
        return;
    }
    Serial.println("JOINED");

    // setup the packet buffer here, so even if the server pushes data on connect, we get to read it
    esp.putPacketBuffer(packetbuffer, sizeof(packetbuffer));

    if ((s = esp.tcpOpen(HOST, PORT))) {
        Serial.print(F("tcp open error"));
        Serial.println(s);
        return;
    }
    Serial.println("OPENED");

    if ((s = esp.tcpSend((const uint8_t*)"1337\n", 5))) {
        Serial.print(F("tcp write error"));
        Serial.println(s);
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
    } else if (len < 0) {
        Serial.print(F("tcp receive error"));
        Serial.println(len);
    }
}

