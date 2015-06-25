# An ESP8266 Arduino driver

Still work in progress :)

The goal is to make a reasonably efficient driver. But also to never miss a
single message. Most libraries will silently drop "+IPD" from the esp8266,
when receiving one while doing other things.

End goal example:
```
#include "esp8266.h"

ESP8266 wifi(ESP_PIN_RX, ESP_PIN_TX, ESP_PIN_RESET);
uint8_t receive_buf[100];

setup() {
  if (!wifi.joinAP("SSID", "PASS")) {
    Serial.print("wifi error: ");
    Serial.println(errno);
  }
  
  // setup a receive buffer, so we never have to miss a single message
  wifi.setReceiveBuffer(buf, sizeof(buf));
  
  if (!wifi.tcpOpen("IP", PORT)) {
    Serial.print("wifi error: ");
    Serial.println(errno);
  }
  
  int len = wifi.tcpReceive();
  if (len < 0) {
    Serial.print("wifi error: ");
    Serial.prinln(errno);
  }
  if (len > 0) {
    Serial.print("received: ");
    Serial.write(buf, len);
  }
}
```
