// vim: set filetype=cpp:
// vim: set syntax=arduino:

#include <stdarg.h>
#include "lib/crcs.hpp"

using uint = unsigned int;
using ulong = unsigned long;

static constexpr const bool PollDataTimeoutIgnored = false;
// 2 shift registers used for address pins
static constexpr const int SHIFT_LATCH = 11;
static constexpr const int SHIFT_CLK = 12;
static constexpr const int SHIFT_DATA = 13;

static constexpr const long WRITE_CYCLE_MILLIS = 15;

static constexpr const long int BAUDRATE = 115200;

// Data direction: DDRD (0 = input; 1 = output)
// Data out: PORTD
// Data in: PIND
// Write enable, Output enable, Chip enable: set PORTB, always set higher bits to 0
static constexpr const byte WRITE_EN = ~0x01;
static constexpr const byte OUTPUT_EN = ~0x02;
static constexpr const byte CHIP_EN = ~0x04;
static constexpr const byte MASK = 0x07; // PORTB

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
setAddress(uint address) {
    digitalWrite(SHIFT_LATCH, LOW);
    shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, address >> 8 & 0xFF);
    shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, address & 0xFF);
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
    PORTB = MASK & CHIP_EN & OUTPUT_EN;
    delayMicroseconds(3);
    return readIOPins();;
}

static bool
pollData(byte lastByteRead) {
    ulong pollEnd = millis() + WRITE_CYCLE_MILLIS;
//  logSerial("pollData_PORT: lastByteRead: %02x, now: %lu, pollEnd: %lu\n", lastByteRead, millis(), pollEnd);
    do {
        PORTB = MASK & CHIP_EN & OUTPUT_EN;
        if (readIOPins() == lastByteRead) {
//          logSerial("pollData_PORT: lastByteRead: %02x, data: %d, now: %lu, pollEnd: %lu, length: %lu\n", lastByteRead, readIOPins(), millis(), pollEnd, millis() - (pollEnd - WRITE_CYCLE_MILLIS));
            return true;
        }
        PORTB = MASK & CHIP_EN;
        delayMicroseconds(1);
    } while (millis() < pollEnd);
//  logSerial("pollData_PORT: lastByteRead: %02x, data: %d, now: %lu, pollEnd: %lu\n", lastByteRead, readIOPins(), millis(), pollEnd);
    return false;
}

static uint
writeBytes(uint start, const byte *data, uint len) {
    PORTB = MASK & CHIP_EN;
    uint end = min(start + len, (start / 0x40 + 1) * 0x40);
    ulong startStamp = millis();
    for (uint i = start; i != end; ++i) {
        setAddress(i);
        writeIOPins(*data++);
        PORTB = MASK & CHIP_EN & WRITE_EN;
        delayMicroseconds(1);
        PORTB = MASK & CHIP_EN;
        delayMicroseconds(1);
    }
    logSerial("write: %lu\n", millis() - startStamp);
    if (!pollData(*--data)) {
        if (PollDataTimeoutIgnored) {
            logSerial("WARN: pollData timeout.\n");
        } else {
            logSerial("ERROR: pollData timeout.\n");
            end = start; // failure
        }
    }
    logSerial("total: %lu\n", millis() - startStamp);
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
    do {
        if (start % 16 == 0) {
            logSerial(buf);
            p = buf;
            p += sprintf(p, "\r\n%04x:", start);
        } else if (start % 8 == 0) {
            *p++ = ' ';
        }
        p += sprintf(p, " %02x", readByte(start));
        ++start;
    } while (start != end);
    logSerial(buf);
    logSerial("\r\n");
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
    logSerial("\r\nOK\n");
    return true;
}

// ------------------------------------------------------------------------ //

void
setup() {
    DDRB = 0xFF; // Set pin mode to output
    PORTB = MASK & CHIP_EN; // WE = 1, OE = 1; CE = 0, leave shift register pins as 0
    Serial.begin(115200);
    logSerial("\r\nResetting...\r\n");
}

/*
flags: 3
instruction: 5
step: 4
 */

void
loop() {
    PORTB = MASK & CHIP_EN;
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

