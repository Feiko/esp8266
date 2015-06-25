
#include <SoftwareSerial.h>
#include <errno.h>
#define ESP_TX 3
#define ESP_RX 2
#define ESP_RESET 4

SoftwareSerial esp(ESP_TX, ESP_RX);

#define EFAIL 60
#define ETIMEOUT 61
#define EALREADYCONN 62

bool debug = true;
String d;

uint32_t toNum(char c1, char c2, char c3, char c4) {
  return ((uint32_t)c1 << 24) | ((uint32_t)c2 << 16) | ((uint32_t)c3 << 8) | ((uint32_t)c4 << 0);
}

const uint32_t IPD = toNum('+','I','P','D');

// just seen +IPD
void esp_readipd() {
  Serial.println("+IPD fired");
}

int esp_waitfor(uint32_t* needles, int len, uint32_t timeout, char prompt=0) {
  uint32_t start = millis();
  uint32_t state = 0;

  d.remove(0);

  while ((uint32_t)(millis() - start) < timeout) {
     if (!esp.available()) continue;
    
     uint8_t c = esp.read();
     d += (char)c;
     state = state << 8 | c;
     
     if (state == IPD) esp_readipd();
     if (prompt && (char)c == prompt) return 0;
     for (int i = 0; i < len; i++) {
        if (state == needles[i]) return i;
     }
  }
  return -1; // timeout
}

bool esp_hardwareReset() {
  digitalWrite(ESP_RESET, LOW);
  pinMode(ESP_RESET, OUTPUT);
  delay(10);
  pinMode(ESP_RESET, INPUT);
  uint32_t needles[] = {toNum('d', 'y', '\r', '\n')};
  return esp_waitfor(needles, 1, 2000) == 0;
}

bool esp_joinAP(const char* ssid, const char* pass) {
  esp.print("AT+CWJAP=\"");
  esp.print(ssid);
  esp.print("\",\"");
  esp.print(pass);
  esp.println("\"");

  uint32_t needles[] = {toNum('O','K','\r','\n'), toNum('I','L','\r','\n')};
  int status = esp_waitfor(needles, 2, 20000);
  if (status == 0) return true;
  errno = status == 1? EFAIL : ETIMEOUT;
  return false;
}

bool esp_tcpOpen(const char* adress, int port) {
  esp.print("AT+CIPSTART=\"TCP\",\"");
  esp.print(adress);
  esp.print("\",");
  esp.println(port);

  uint32_t needles[] = {toNum('O','K','\r','\n'), toNum('O','R','\r','\n'), toNum('C','T','\r','\n')};
  int status = esp_waitfor(needles, 3, 10000);
  if (status == 0) return true;
  switch (status) {
    case 1: errno = EFAIL; break;
    case 2: errno = EALREADYCONN; break;
    default: errno = ETIMEOUT;
  }
  return false;
}

bool esp_tcpClose() {
  esp.println("AT+CIPCLOSE");

  uint32_t needles[] = {toNum('O','K','\r','\n'), toNum('O','R','\r','\n')};
  int status = esp_waitfor(needles, 2, 2000);
  if (status == 0) return true;
  errno = status == 1? EFAIL : ETIMEOUT;
  return false;
}

bool esp_tcpSend(const uint8_t* data, int len) {
  esp_waitfor(NULL, 0, 1); // clear buffer
  esp.print("AT+CIPSEND=");
  esp.println(len);

  int status = esp_waitfor(NULL, 0, 2000, '>');
  if (status != 0) {
    errno = ETIMEOUT;
    return false;
  }
  esp.write(data, len);

  uint32_t needles2[] = {toNum('O','K','\r','\n'), toNum('O','R','\r','\n')};
  status = esp_waitfor(needles2, 2, 2000);
  if (status == 0) return true;
  errno = status == 1? EFAIL : ETIMEOUT;
  return false;
}

// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin 13 as an output.
  pinMode(13, OUTPUT);
  Serial.begin(9600);
  esp.begin(9600);
  Serial.println("Hello World");
  
  if (!esp_hardwareReset()) {
    Serial.println("wifi reset error");
    return;
  }
  
  if (!esp_joinAP("yourSSID", "yourPASS")) {
    Serial.print("wifi join error: ");
    Serial.println(errno);
    Serial.println(d);
    return;
  }
  Serial.println(d);
  Serial.println("talk to the esp");

  if (!esp_tcpOpen("yourIP", 6979)) {
    Serial.print("TCP open error: ");
    Serial.println(errno);
    Serial.println(d);
    return;
  }
  Serial.println(d);
  Serial.println("esp is ready");

  uint32_t needles[] ={toNum('O','K','\r','\n')}; 
  esp_waitfor(needles, 1, 1000);
  Serial.println(d);

  if (!esp_tcpSend((const uint8_t*)"hello\n", 6)) {
    Serial.print("TCP write error: ");
    Serial.println(errno);
    Serial.println(d);
    return;
  }
  Serial.println(d);
  Serial.println("tcp is written");

  
  
  if (!esp_tcpClose()) {
    Serial.print("TCP close error: ");
    Serial.println(errno);
    Serial.println(d);
    return;
  }
  Serial.println(d);
  Serial.println("tcp is closed");

  if (!esp_tcpClose()) {
    Serial.print("TCP close error: ");
    Serial.println(errno);
    Serial.println(d);
    return;
  }
  Serial.println(d);
  Serial.println("tcp is closed");
}



// the loop function runs over and over again forever
void loop() {
  while (Serial.available()) {
    esp.write(Serial.read());
  }
  while (esp.available()) {
    Serial.write(esp.read());
  }
 
  
}
