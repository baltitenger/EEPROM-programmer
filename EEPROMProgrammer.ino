// vim: set filetype=cpp:
// vim: set syntax=arduino:

#include <stdarg.h>
#include <SPI.h>
#include "lib/crcs.hpp"

using uint = unsigned int;
using ulong = unsigned long;

static constexpr const bool PollDataTimeoutIgnored = false;

// 2 shift registers used for address pins
static constexpr const int SHIFT_LATCH = 10;
static constexpr const int SHIFT_DATA = 11;
static constexpr const int SHIFT_CLK = 13;

static constexpr const long WRITE_CYCLE_MILLIS = 15;

static constexpr const long int BAUDRATE = 115200;

enum EepromPin {
    None = 0,
    WriteEnable = 0b01,
    OutputEnable = 0b10,
};
static constexpr const byte EepromPinMask = EepromPin::OutputEnable | EepromPin::WriteEnable;

static void
logSerial(const char *format, ...) {
    static char buf[256];
    va_list argptr;
    va_start(argptr, format);
    vsnprintf(buf, sizeof(buf), format, argptr);
    Serial.print(buf);
}

static void
waitForSerial() {
    while (!Serial.available()) {
        delayMicroseconds(1);
    }
    return;
}

static void
skipWhitespace() {
    while (true) {
        waitForSerial();
        if (isspace(Serial.peek())) {
            Serial.read();
        } else {
            return;
        }
    }
}

static int
hexToBin(const char hex) {
    if ('0' <= hex && hex <= '9') {
        return hex - '0';
    } else if ('a' <= hex && hex <= 'f') {
        return hex - 'a' + 10;
    } else if ('A' <= hex && hex <= 'F') {
        return hex - 'A' + 10;
    } else {
        return -1;
    }
}

static long
readHex(uint maxSize) {
    long res = 0;
    for (uint i = 0; i < maxSize; ++i) {
        waitForSerial();
        if (isspace(Serial.peek())) {
            break;
        }
        res <<= 4;
        int next = hexToBin(Serial.read());
        if (next == -1) {
            return -1;
        }
        res |= next;
    }
    return res;
}

static String
readWord() {
    String res = "";
    while (true) {
        waitForSerial();
        if (isspace(Serial.peek())) {
            break;
        } else {
            res.concat((char) Serial.read());
        }
    }
    return res;
}

static void
setEepromPins(EepromPin pins = EepromPin::None) {
    PORTB ^= (PORTB ^ ~pins) & EepromPinMask;
}

static void
setAddress(uint address) {
    digitalWrite(SHIFT_LATCH, LOW);
#if 0
    shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, address >> 8 & 0xFF);
    shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, address & 0xFF);
#else
    SPI.transfer16(address);
#endif
    digitalWrite(SHIFT_LATCH, HIGH);
}

static byte
readIOPins() {
    DDRC &= 0b11100111;
    DDRD &= 0b00000011;
    byte res = (PINC >> 3) & 0b11;
    res |= PIND & 0b11111100;
    return res;
}

static void
writeIOPins(byte toWrite) {
    DDRC |= 0b00011000;
    DDRD |= 0b11111100;
    byte twc = (toWrite & 0b11) << 3;
    PORTC ^= (twc ^ PORTC) & 0b00011000;
    PORTD ^= (toWrite ^ PORTD) & 0b11111100;
}

static byte
readByte(uint address) {
    setAddress(address);
    setEepromPins(EepromPin::OutputEnable);
    delayMicroseconds(3);
    return readIOPins();;
}

static bool
pollData(byte lastByteRead) {
    ulong pollEnd = millis() + WRITE_CYCLE_MILLIS;
//  logSerial("pollData_PORT: lastByteRead: %02x, now: %lu, pollEnd: %lu\n", lastByteRead, millis(), pollEnd);
    do {
        setEepromPins(EepromPin::OutputEnable);
        if (readIOPins() == lastByteRead) {
//          logSerial("pollData_PORT: lastByteRead: %02x, data: %d, now: %lu, pollEnd: %lu, length: %lu\n", lastByteRead, readIOPins(), millis(), pollEnd, millis() - (pollEnd - WRITE_CYCLE_MILLIS));
            return true;
        }
        setEepromPins();
        delayMicroseconds(1);
    } while (millis() < pollEnd);
//  logSerial("pollData_PORT: lastByteRead: %02x, data: %d, now: %lu, pollEnd: %lu\n", lastByteRead, readIOPins(), millis(), pollEnd);
    return false;
}

static uint
writeBytes(uint start, const byte *data, uint len) {
    setEepromPins();
    uint end = min(start + len, (start / 0x40 + 1) * 0x40);
    for (uint i = start; i != end; ++i) {
        setAddress(i);
        writeIOPins(*data++);
        setEepromPins(EepromPin::WriteEnable);
        delayMicroseconds(1);
        setEepromPins();
        delayMicroseconds(1);
    }
    if (!pollData(*--data)) {
        if (PollDataTimeoutIgnored) {
            logSerial("WARN: pollData timeout.\n");
        } else {
            logSerial("ERROR: pollData timeout.\n");
            end = start; // failure
        }
    }
    return end - start;
}

static void
writeBulk(uint start, const byte *data, uint len) {
    uint written = 0;
    do {
        len -= written;
        start += written;
        logSerial("Writing from %04x...\r\n", start);
    } while ((written = writeBytes(start, data, len)) < len); 
}

static void
printContents(uint start, uint len) {
    start &= ~ 0x0f;
    uint end = start + ((len + 15) & ~ 0x0f);
    char buf[80];
    buf[0] = 0;
    char *p = buf;
    byte crc = 0;
    do {
        if (start % 16 == 0) {
            logSerial(buf);
            p = buf;
            p += sprintf(p, "\r\n%04x:", start);
        } else if (start % 8 == 0) {
            *p++ = ' ';
        }
        byte b = readByte(start);
        crc = crcs::crc8(b, crc);
        p += sprintf(p, " %02x", b);
        ++start;
    } while (start != end);
    logSerial(buf);
    logSerial("\r\n\r\nOK: crc: %02x\n", crc);
}

static bool
actionLoad() {
    skipWhitespace();
    long start = readHex(4);
    if (start == -1) {
        return false;
    }
    byte ccrc = crcs::crc8be16(start);
    skipWhitespace();
    long len = readHex(4);
    if (len == -1) {
        return false;
    }
    ccrc = crcs::crc8be16(len, ccrc);
    skipWhitespace();
    byte crc = readHex(2);
    if (crc == ccrc) {
        logSerial("\r\nOK. Start address and length set.\n");
    } else {
        logSerial("\r\nError! Mismatching crc (%02x != %02x)\n", crc, ccrc);
        return false;
    }
    do {
        uint lenPage = min(len, 0x40 - (start % 0x40));
        byte page[64];
        byte *p = page;
        for (uint i = 0; i < lenPage; ++i) {
            skipWhitespace();
            int next = readHex(2);
            if (next == -1) {
                return false;
            }
            *p++ = next;
        }
        skipWhitespace();
        ccrc = crcs::crc8(page, page + lenPage);
        crc = readHex(2);
        if (crc != ccrc) {
            logSerial("\r\nError! Mismatching crc (%02x != %02x)\n", crc, ccrc);
            return false;
        }
        if (writeBytes(start, page, lenPage) == 0) {
            logSerial("\r\nError! Write timed out.\n");
            return false;
        }
        logSerial("\r\nOK, written %d bytes of data starting from %04x.\n", lenPage, start);
        len -= lenPage;
        start += lenPage;
    } while (len > 0); 
    logSerial("\r\nDone.\n");
    return true;
}

static bool
actionPrint() {
    skipWhitespace();
    long int start = readHex(4);
    if (start == -1) {
        return false;
    }
    skipWhitespace();
    long int len = readHex(4);
    if (len == -1) {
        return false;
    }
    printContents(start, len);
    return true;
}

// ------------------------------------------------------------------------ //

void
setup() {
    DDRB = 0xff; // Set pin mode to output
    setEepromPins();
    Serial.begin(115200);
    logSerial("\r\nResetting...\r\n");
    SPI.begin();
}

/*
flags: 3
instruction: 5
step: 4
 */

void
loop() {
    setEepromPins();
    logSerial("Ready\n");
    skipWhitespace();
    String action = readWord();
    if (action == "LOAD" || action == "load") {
        actionLoad();
    } else if (action == "PRINT" || action == "print") {
        actionPrint();
    } else if (action == "EXIT" || action == "exit") {
        logSerial("Bye!\n");
        Serial.end();
        exit(0);
    } else {
        logSerial("Error! Invalid action.\n");
    }
}

