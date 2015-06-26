# An ESP8266 Arduino driver

Still work in progress :)

The goal is to make a reasonably efficient driver. But also to never miss a
single message. Most libraries will silently drop "+IPD" from the esp8266,
when receiving one while doing other things.

example:
```
#include "esp8266.h"

SoftwareSerial espconn(ESP_TX, ESP_RX);
ESP8266 wifi(espconn, ESP_RESET);
uint8_t buf[100];

setup() {
    if (!wifi.joinAP("SSID", "PASS")) {
        Serial.print("wifi error: ");
        Serial.println(errno);
    }

    // setup a receive buffer, so we never have to miss a single message
    wifi.putPacketBuffer(buf, sizeof(buf));

    if (!wifi.tcpOpen("IP", PORT)) {
        Serial.print("wifi error: ");
        Serial.println(errno);
    }
}

loop() {
    // if any bytes are available, take back the buffer, copy it to serial, give it back to the wifi driver
    int len = wifi.available();
    if (len > 0) {
        wifi.takePacketBuffer();
        Serial.print("received: ");
        Serial.write(buf, len);
        wifi.putPacketBuffer(buf, sizeof(buf));
    }
}
```

