
#include <SoftwareSerial.h>
#include <errno.h>
#define ESP_TX 3
#define ESP_RX 2
#define ESP_RESET 4

#define EFAIL 60
#define ETIMEOUT 61
#define EALREADYCONN 62

bool debug = true;
String d;

uint32_t toNum(char c1, char c2, char c3, char c4) {
  return ((uint32_t)c1 << 24) | ((uint32_t)c2 << 16) | ((uint32_t)c3 << 8) | ((uint32_t)c4 << 0);
}

const uint32_t IPD = toNum('+','I','P','D');


class ESP8266 {
  public:

  uint8_t* ipdpacket;
  int ipdlen;
  int ipdavailable;
  
  Stream* espconn;
  int8_t reset_pin;

  ESP8266(Stream& esp, int8_t reset_pin=-1) : espconn(&esp), reset_pin(reset_pin), ipdpacket(NULL), ipdlen(0), ipdavailable(0) { }

  int waitfor(uint32_t* needles, int len, uint32_t timeout, char prompt=0);

  bool hardwareReset();

  bool joinAP(const char* ssid, const char* pass);

  bool tcpOpen(const char* adress, int port);

  bool tcpClose();

  bool tcpSend(const uint8_t* data, int len);

  void setPacketBuffer(uint8_t* packet, int len) {
    ipdpacket = packet;
    ipdlen = len;
    ipdavailable = 0;  
  }
  
  int available() const {
    return ipdavailable;  
  }

  uint8_t* getPacket() const {
    return ipdpacket;
  }

  void receiveIPD();
};

// just seen +IPD
void ESP8266::receiveIPD() {
  Serial.println("+IPD fired");
}

int ESP8266::waitfor(uint32_t* needles, int len, uint32_t timeout, char prompt) {
  uint32_t start = millis();
  uint32_t state = 0;

  d.remove(0);

  while ((uint32_t)(millis() - start) < timeout) {
     if (!espconn->available()) continue;
     if (!(*espconn).available()) continue;
    
     uint8_t c = espconn->read();
     d += (char)c;
     state = state << 8 | c;
     
     if (state == IPD) receiveIPD();
     if (prompt && (char)c == prompt) return 0;
     for (int i = 0; i < len; i++) {
        if (state == needles[i]) return i;
     }
  }
  return -1; // timeout
}

bool ESP8266::hardwareReset() {
  digitalWrite(ESP_RESET, LOW);
  pinMode(ESP_RESET, OUTPUT);
  delay(10);
  pinMode(ESP_RESET, INPUT);
  uint32_t needles[] = {toNum('d', 'y', '\r', '\n')};
  return waitfor(needles, 1, 2000) == 0;
}

bool ESP8266::joinAP(const char* ssid, const char* pass) {
  espconn->print("AT+CWJAP=\"");
  espconn->print(ssid);
  espconn->print("\",\"");
  espconn->print(pass);
  espconn->println("\"");

  uint32_t needles[] = {toNum('O','K','\r','\n'), toNum('I','L','\r','\n')};
  int status = waitfor(needles, 2, 20000);
  if (status == 0) return true;
  errno = status == 1? EFAIL : ETIMEOUT;
  return false;
}

bool ESP8266::tcpOpen(const char* adress, int port) {
  espconn->print("AT+CIPSTART=\"TCP\",\"");
  espconn->print(adress);
  espconn->print("\",");
  espconn->println(port);

  uint32_t needles[] = {toNum('O','K','\r','\n'), toNum('O','R','\r','\n'), toNum('C','T','\r','\n')};
  int status = waitfor(needles, 3, 10000);
  if (status == 0) return true;
  switch (status) {
    case 1: errno = EFAIL; break;
    case 2: errno = EALREADYCONN; break;
    default: errno = ETIMEOUT;
  }
  return false;
}

bool ESP8266::tcpClose() {
  espconn->println("AT+CIPCLOSE");

  uint32_t needles[] = {toNum('O','K','\r','\n'), toNum('O','R','\r','\n')};
  int status = waitfor(needles, 2, 2000);
  if (status == 0) return true;
  errno = status == 1? EFAIL : ETIMEOUT;
  return false;
}

bool ESP8266::tcpSend(const uint8_t* data, int len) {
  waitfor(NULL, 0, 1); // clear buffer
  espconn->print("AT+CIPSEND=");
  espconn->println(len);

  int status = waitfor(NULL, 0, 2000, '>');
  if (status != 0) {
    errno = ETIMEOUT;
    return false;
  }
  espconn->write(data, len);

  uint32_t needles2[] = {toNum('O','K','\r','\n'), toNum('O','R','\r','\n')};
  status = waitfor(needles2, 2, 2000);
  if (status == 0) return true;
  errno = status == 1? EFAIL : ETIMEOUT;
  return false;
}

// the setup function runs once when you press reset or power the board
SoftwareSerial espconn(ESP_TX, ESP_RX);
ESP8266 esp(espconn);

void setup() {
  // initialize digital pin 13 as an output.
  pinMode(13, OUTPUT);
  Serial.begin(9600);
  espconn.begin(9600);
  Serial.println("Hello World");
  Serial.println(sizeof(SoftwareSerial));
  Serial.println(sizeof(ESP8266));
  Serial.println((int)&espconn);
  Serial.println((int)&esp);
  
  if (!esp.hardwareReset()) {
    Serial.println("wifi reset error");
    return;
  }
  
  if (!esp.joinAP("yourSSID", "yourPASS")) {
    Serial.print("wifi join error: ");
    Serial.println(errno);
    Serial.println(d);
    return;
  }
  Serial.println(d);
  Serial.println("talk to the esp");

  if (!esp.tcpOpen("yourIP", 6979)) {
    Serial.print("TCP open error: ");
    Serial.println(errno);
    Serial.println(d);
    return;
  }
  Serial.println(d);
  Serial.println("esp is ready");

  if (!esp.tcpSend((const uint8_t*)"hello\n", 6)) {
    Serial.print("TCP write error: ");
    Serial.println(errno);
    Serial.println(d);
    return;
  }
  Serial.println(d);
  Serial.println("tcp is written");

  if (!esp.tcpClose()) {
    Serial.print("TCP close error: ");
    Serial.println(errno);
    Serial.println(d);
    return;
  }
  Serial.println(d);
  Serial.println("tcp is closed");

  if (!esp.tcpClose()) {
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
    espconn.write(Serial.read());
  }
  while (espconn.available()) {
    Serial.write(espconn.read());
  }
 
  
}
