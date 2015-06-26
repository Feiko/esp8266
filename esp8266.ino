#include <Stream.h>
#include <errno.h>

#define EFAIL 60
#define ETIMEOUT 61
#define EALREADYCONN 62

uint32_t toNum(char c1, char c2, char c3, char c4) {
  return ((uint32_t)c1 << 24) | ((uint32_t)c2 << 16) | ((uint32_t)c3 << 8) | ((uint32_t)c4 << 0);
}

class ESP8266 {
    Stream* espconn;
    int8_t reset_pin;

    uint8_t* ipdpacket;
    int ipdlen;
    int ipdavailable;
    uint32_t state;

    int espconnread();
    int waitfor(const uint32_t* needles, int len, uint32_t timeout, char prompt=0);
    void receiveIPD();

public:

    ESP8266(Stream& esp, int8_t reset_pin=-1) :
        espconn(&esp), reset_pin(reset_pin), ipdpacket(NULL), ipdlen(0), ipdavailable(0), state(0) { }

    bool hardwareReset();
    bool joinAP(const char* ssid, const char* pass);

    bool tcpOpen(const char* adress, int port);
    bool tcpClose();

    bool tcpSend(const uint8_t* data, int len);
    int tcpReceive(uint8_t* data, int len, uint32_t timeout=0);

    void putPacketBuffer(uint8_t* packet, int len) {
        ipdpacket = packet;
        ipdlen = len;
        ipdavailable = 0;
    }

    int available() {
        waitfor(NULL, 0, 1);
        return ipdavailable;
    }

    uint8_t* takePacketBuffer() {
        uint8_t* ret = ipdpacket;
        ipdpacket = NULL;
        ipdlen = 0;
        ipdavailable = 0;

        return ret;
    }
};

int ESP8266::espconnread() {
    int r = espconn->read();
    if (r != -1) return r;

    uint32_t start = millis();
    do {
        int r = espconn->read();
        if (r != -1) return r;
    } while ((uint32_t)(millis() - start) < 100);
    return -1;
}

// just seen +IPD
void ESP8266::receiveIPD() {
    int c = espconnread();
    if (c != ',') return;

    int len = 0;

    while (true) {
        int n = espconnread();
        if (n >= '0' && n <= '9') {
            len = len * 10 + n - '0';
            continue;
        }
        if (n == ':') break;
        return;
    }

    for (int i = 0; i < len; i++) {
        int b = espconnread();
        if (b == -1) return;
        if (!ipdpacket) continue;
        if (ipdavailable >= ipdlen) continue;

        ipdpacket[ipdavailable] = b;
        ipdavailable += 1;
    }
    Serial.print(F("+IPD: "));
    Serial.print(len);
    Serial.print(F(" "));
    Serial.println(ipdavailable);
}

int ESP8266::waitfor(const uint32_t* needles, int len, uint32_t timeout, char prompt) {
    uint32_t start = millis();
    const uint32_t IPD = toNum('+','I','P','D');

    do {
        int len = espconn->available();
        for(int i = 0; i < len; i++) {
            uint8_t c = espconn->read();
            state = state << 8 | c;
            //d += (char)c;

            if (state == IPD) receiveIPD();
            if (prompt && (char)c == prompt) return 0;
            for (int i = 0; i < len; i++) {
                if (state == needles[i]) return i;
            }
        }
    } while ((uint32_t)(millis() - start) < timeout +80);
    return -1; // timeout
}

bool ESP8266::hardwareReset() {
    digitalWrite(reset_pin, LOW);
    pinMode(reset_pin, OUTPUT);
    delay(10);
    pinMode(reset_pin, INPUT);
    const uint32_t needles[] = {toNum('d', 'y', '\r', '\n')};
    if (waitfor(needles, 1, 2000) < 0) return false;

    espconn->println(F("ATE0"));
    const uint32_t needles2[] = {toNum('O','K','\r','\n')};
    if (waitfor(needles2, 1, 200) < 0) return false;

    espconn->println(F("AT+CIPMUX=0"));
    const uint32_t needles3[] = {toNum('O','K','\r','\n'), toNum('n','g','e','\r')};
    if (waitfor(needles3, 2, 200) < 0) return false;

    return true;
}

bool ESP8266::joinAP(const char* ssid, const char* pass) {
    espconn->print(F("AT+CWJAP=\""));
    espconn->print(ssid);
    espconn->print(F("\",\""));
    espconn->print(pass);
    espconn->println("\"");

    static uint32_t const needles[] = {toNum('O','K','\r','\n'), toNum('I','L','\r','\n')};
    int status = waitfor(needles, 2, 20000);
    if (status == 0) return true;
    errno = status == 1? EFAIL : ETIMEOUT;
    return false;
}

bool ESP8266::tcpOpen(const char* adress, int port) {
    espconn->print(F("AT+CIPSTART=\"TCP\",\""));
    espconn->print(adress);
    espconn->print(F("\","));
    espconn->println(port);

    const uint32_t needles[] = {toNum('O','K','\r','\n'), toNum('O','R','\r','\n'), toNum('C','T','\r','\n')};
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
    espconn->println(F("AT+CIPCLOSE"));

    const uint32_t needles[] = {toNum('O','K','\r','\n'), toNum('O','R','\r','\n')};
    int status = waitfor(needles, 2, 2000);
    if (status == 0) return true;
    errno = status == 1? EFAIL : ETIMEOUT;
    return false;
}

bool ESP8266::tcpSend(const uint8_t* data, int len) {
    waitfor(NULL, 0, 1); // clear buffer
    espconn->print(F("AT+CIPSEND="));
    espconn->println(len);

    int status = waitfor(NULL, 0, 2000, '>');
    if (status != 0) {
        errno = ETIMEOUT;
        return false;
    }
    espconn->write(data, len);

    const uint32_t needles2[] = {toNum('O','K','\r','\n'), toNum('O','R','\r','\n')};
    status = waitfor(needles2, 2, 2000);
    if (status == 0) return true;
    errno = status == 1? EFAIL : ETIMEOUT;
    return false;
}

int ESP8266::tcpReceive(uint8_t* data, int len, uint32_t timeout) {
    uint32_t start = millis();
    putPacketBuffer(data, len);
    do {
        int res = available();
        if (res) {
            takePacketBuffer();
            return res;
        }
    } while ((uint32_t)(millis() - start) < timeout);
    takePacketBuffer();
    return 0;
}

#include <SoftwareSerial.h>
#define ESP_TX 3
#define ESP_RX 2
#define ESP_RESET 4

SoftwareSerial espconn(ESP_TX, ESP_RX);
ESP8266 esp(espconn, ESP_RESET);
uint8_t packetbuffer[100];

void setup() {
    Serial.begin(9600);
    espconn.begin(9600);
    Serial.println(F("Hello World"));

    if (!esp.hardwareReset()) {
        Serial.println(F("wifi reset error"));
        return;
    }

    if (!esp.joinAP("SSID", "PASS")) {
        Serial.print(F("wifi join error: "));
        Serial.println(errno);
        return;
    }
    Serial.println(F("talk to the esp"));

    esp.putPacketBuffer(packetbuffer, sizeof(packetbuffer));

    if (!esp.tcpOpen("192.168.87.101", 6979)) {
        Serial.print(F("TCP open error: "));
        Serial.println(errno);
        return;
    }
    Serial.println(F("esp is ready"));

    if (!esp.tcpSend((const uint8_t*)"1337\n", 5)) {
        Serial.print(F("TCP write error: "));
        Serial.println(errno);
        return;
    }

    Serial.println(F("tcp is written"));
}

// the loop function runs over and over again forever
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

