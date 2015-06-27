// author: Feiko Gorter, Onne Gorter
// license: MIT

#include <Stream.h>

#define EFAIL 60
#define ETIMEOUT 61
#define EALREADYCONN 62

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
    ESP8266(Stream& esp, int8_t reset_pin=-1);

    bool hardwareReset();
    bool reset();

    bool joinAP(const char* ssid, const char* pass);
    bool leaveAP();

    bool tcpOpen(const char* adress, int port);
    bool tcpClose();

    bool tcpSend(const uint8_t* data, int len);
    int tcpReceive(uint8_t* data, int len, uint32_t timeout=0);

    int available();
    void putPacketBuffer(uint8_t* packet, int len);
    uint8_t* takePacketBuffer();
};

