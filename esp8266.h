// author: Feiko Gorter, Onne Gorter
// license: MIT

#include <Stream.h>
typedef const __FlashStringHelper Fstr;

/// a class to manage an esp8266 wireless using AT commands
class ESP8266 {
    Stream* espconn;
    int8_t reset_pin;

    uint8_t* ipdpacket;
    int ipdlen;
    int ipdavailable;

    uint32_t state;

    int espconnread();
    int waitfor(const uint32_t* needles, int nlen, uint32_t timeout, char prompt=0, char* rec=NULL, int reclen=0);
    void receiveIPD();

    int joinAP2();
    int tcpOpen2();

public:
    /// provide a hardware or software serial in esp
    /// optionally hook up the reset pin, then hardwareReset() will work
    ESP8266(Stream& esp, int8_t reset_pin=-1);

    /// returns 0 on success, negative on error
    int hardwareReset();
    int reset();

    /// join a wireless network, given the ssid and (clear text) password
    /// returns 0 on success, negative on error
    int joinAP(const char* ssid, const char* pass);
    int joinAP(Fstr* ssid, Fstr* pass);

    /// returns 0 on success, negative on error
    int leaveAP();

    /// open a connection to a server, given the host and port
    /// returns 0 on success, -1 on timeout, -2... for other errors
    int tcpOpen(const char* adress, int port);
    int tcpOpen(Fstr* adress, int port);

    /// returns 0 on success, negative on error
    int tcpClose();

    /// returns 0 on success, negative on error
    int tcpSend(const uint8_t* data, int len);

    /// receive data from the remote peer
    /// This is the "simple" version, it is better to use putPacket/available/takePacket
    /// or there is a risk of loosing data from the peer, if we are sending at the same time.
    ///
    /// returns the size of the data received, 0 if nothing was received before timeout, or negative on error.
    int tcpReceive(uint8_t* data, int len, uint32_t timeout=0);

    /// return amount of data available, can be used to poll for data and should be called often
    /// returns the size of data available, 0 if no data is available, negative if errors have occurred.
    int available();

    /// give the driver a buffer to copy incoming data to
    void putPacketBuffer(uint8_t* packet, int len);

    /// take back the buffer from the driver, the driver will no longer use the buffer, until putPacketBuffer is called again
    /// returns the originally set packet buffer
    uint8_t* takePacketBuffer();

    /// return 0 on success, negative on error, will fill in str with esp8266 version
    int getVersion(char* str, int len);

    /// return 0 on success, negative on error, will fill in str with local ip address
    int getIP(char* str, int len);
};

