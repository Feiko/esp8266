// author: Feiko Gorter, Onne Gorter
// license: MIT

// comment in to debug the module
//#define DEBUG 1

#include "esp8266.h"
#include <Arduino.h>
#include <errno.h>

// This esp8266 implementation reads from the esp8266 serial, and matches its
// output 4 bytes at the time (uint32_t ESP8266::state), not bothering to
// store the full response in a buffer somewhere. This saves memory and is
// fast. See ESP8266::waitfor

// Also, it can deal with receiving data at any point in while processing
// commands, as long as the user has given a PacketBuffer to the driver.

// It would have been better if the esp8266 wouldn't push +IPD, but instead has
// the other side poll for it. That way the TCP window would shrink to the
// speed at which your program can process data.

// these are the bytes sequences we are looking for as the various reponses
// from the esp.
#define NL_IPD     n(0x2B495044UL) // "+IPD"
#define NL_READY   n(0x64790D0AUL) // "dy\r\n"
#define NL_OK      n(0x4F4B0D0AUL) // "OK\r\n"
#define NL_CHANGE  n(0x6E67650DUL) // "nge\r"
#define NL_FAIL    n(0x494C0D0AUL) // "IL\r\n"
#define NL_ERROR   n(0x4F520D0AUL) // "OR\r\n"
#define NL_CONNECT n(0x43540D0AUL) // "CT\r\n"

// this n function forces to keep these numbers in .text section, not sure why
static inline uint32_t n(uint32_t n) { return n; }


ESP8266::ESP8266(Stream& esp, int8_t reset_pin) : espconn(&esp), reset_pin(reset_pin), ipdpacket(NULL), ipdlen(0), ipdavailable(0), state(0) { }

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

#ifdef DEBUG
    Serial.print(F("+IPD: "));
    Serial.print(len);
    Serial.print(F(" "));
    Serial.println(ipdavailable);
#endif
}

int ESP8266::waitfor(const uint32_t* needles, int len, uint32_t timeout, char prompt) {
    uint32_t start = millis();

#ifdef DEBUG
    if (timeout) {
        Serial.print("waitfor: ");
        Serial.print(len);
        Serial.print(" ");
        Serial.print(prompt);
        Serial.print(" t=");
        Serial.println(timeout);
    }
#endif

    do {
        int alen = espconn->available();
        for (int i = 0; i < alen; i++) {
            uint8_t c = espconn->read();
            state = state << 8 | c;
#ifdef DEBUG
            Serial.write(c);
#endif

            if (state == NL_IPD) receiveIPD();
            if (prompt && (uint8_t)prompt == c) return len;
            for (int i = 0; i < len; i++) {
                if (state == needles[i]) return i;
            }
        }
    } while ((uint32_t)(millis() - start) < timeout);

    return -1; // timeout
}

void ESP8266::putPacketBuffer(uint8_t* packet, int len) {
    ipdpacket = packet;
    ipdlen = len;
    ipdavailable = 0;
}

int ESP8266::available() {
    if (!ipdavailable) waitfor(NULL, 0, 0);
    return ipdavailable;
}

uint8_t* ESP8266::takePacketBuffer() {
    uint8_t* ret = ipdpacket;
    ipdpacket = NULL;
    ipdlen = 0;
    ipdavailable = 0;

    return ret;
}


bool ESP8266::hardwareReset() {
    waitfor(NULL, 0, 0);
    digitalWrite(reset_pin, LOW);
    pinMode(reset_pin, OUTPUT);
    delay(10);
    pinMode(reset_pin, INPUT);
    const uint32_t needles[] = {NL_READY};
    if (waitfor(needles, 1, 2000) < 0) return false;

#ifndef DEBUG
    espconn->println(F("ATE0"));
    const uint32_t needles2[] = {NL_OK};
    if (waitfor(needles2, 1, 200) < 0) return false;
#endif

    espconn->println(F("AT+CIPMUX=0"));
    const uint32_t needles3[] = {NL_OK, NL_CHANGE};
    if (waitfor(needles3, 2, 200) < 0) return false;

    return true;
}

bool ESP8266::reset() {
    waitfor(NULL, 0, 0);
    delay(1000);
    espconn->println(F("AT+RST"));
    const uint32_t needles[] = {NL_READY};
    if (waitfor(needles, 1, 3000) < 0) return false;

#ifndef DEBUG
    espconn->println(F("ATE0"));
    const uint32_t needles2[] = {NL_OK};
    if (waitfor(needles2, 1, 200) < 0) return false;
#endif

    espconn->println(F("AT+CIPMUX=0"));
    const uint32_t needles3[] = {NL_OK, NL_CHANGE};
    if (waitfor(needles3, 2, 200) < 0) return false;

    return true;
}

bool ESP8266::joinAP2() {
    const uint32_t needles[] = {NL_OK, NL_FAIL};
    int status = waitfor(needles, 2, 20000);
    if (status == 0) return true;
    errno = status == 1? EFAIL : ETIMEOUT;
    return false;
}

bool ESP8266::joinAP(const char* ssid, const char* pass) {
    waitfor(NULL, 0, 0);
    espconn->print(F("AT+CWJAP=\""));
    espconn->print(ssid);
    espconn->print(F("\",\""));
    espconn->print(pass);
    espconn->println("\"");
    return joinAP2();
}

bool ESP8266::joinAP(Fstr* ssid, Fstr* pass) {
    waitfor(NULL, 0, 0);
    espconn->print(F("AT+CWJAP=\""));
    espconn->print(ssid);
    espconn->print(F("\",\""));
    espconn->print(pass);
    espconn->println("\"");
    return joinAP2();
}

bool ESP8266::leaveAP() {
    waitfor(NULL, 0, 0);
    espconn->print(F("AT+CWQAP"));
    const uint32_t needles[] = {NL_OK, NL_ERROR};
    int status = waitfor(needles, 3, 2000);
    return status == 0;
}

bool ESP8266::tcpOpen2() {
    const uint32_t needles[] = {NL_OK, NL_ERROR, NL_CONNECT};
    int status = waitfor(needles, 3, 2000);
    if (status != 0) {
        switch (status) {
            case 1: errno = EFAIL; break;
            case 2: errno = EALREADYCONN; break;
            default: errno = ETIMEOUT;
        }
        return false;
    }

    const uint32_t needles2[] = {NL_OK, NL_ERROR};
    status = waitfor(needles2, 2, 20000);
    if (status == 0) return true;
    errno = EFAIL;
    return false;
}

bool ESP8266::tcpOpen(const char* adress, int port) {
    waitfor(NULL, 0, 0);
    espconn->print(F("AT+CIPSTART=\"TCP\",\""));
    espconn->print(adress);
    espconn->print(F("\","));
    espconn->println(port);
    return tcpOpen2();
}

bool ESP8266::tcpOpen(Fstr* adress, int port) {
    waitfor(NULL, 0, 0);
    espconn->print(F("AT+CIPSTART=\"TCP\",\""));
    espconn->print(adress);
    espconn->print(F("\","));
    espconn->println(port);
    return tcpOpen2();
}

bool ESP8266::tcpClose() {
    waitfor(NULL, 0, 0);
    espconn->println(F("AT+CIPCLOSE"));

    const uint32_t needles[] = {NL_OK, NL_ERROR};
    int status = waitfor(needles, 2, 5000);
    if (status == 0) return true;
    errno = status == 1? EFAIL : ETIMEOUT;
    return false;
}

bool ESP8266::tcpSend(const uint8_t* data, int len) {
    waitfor(NULL, 0, 0);
    espconn->print(F("AT+CIPSEND="));
    espconn->println(len);

    int status = waitfor(NULL, 0, 5000, '>');
    if (status != 0) {
        errno = ETIMEOUT;
        return false;
    }
    espconn->write(data, len);

    const uint32_t needles2[] = {NL_OK, NL_ERROR};
    status = waitfor(needles2, 2, 5000);
    if (status == 0) return true;
    errno = status == 1? EFAIL : ETIMEOUT;
    return false;
}

int ESP8266::tcpReceive(uint8_t* data, int len, uint32_t timeout) {
    waitfor(NULL, 0, 0);
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

