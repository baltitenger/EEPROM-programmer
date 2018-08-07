#include <stdarg.h>
#include "lib/crcs.hpp"

using uint = unsigned int;

// 2 shift registers used for address pins and output enable
const int SHIFT_LATCH = 11;
const int SHIFT_CLK = 12;
const int SHIFT_DATA = 13;

// Data direction: DDRD (0 = input; 1 = output)
// Data out: PORTD
// Data in: PIND

// Write enable, Output enable, Chip enable: set PORTB, always set higher bits to 0

const int BAIL_OUT_MILLIS = 10000;
const int TOGGLE_SAFE = 5;

const long int BAUDRATE = 115200;

const byte WRITE_EN = ~0x01;
const byte OUTPUT_EN = ~0x02;
const byte CHIP_EN = ~0x04;
const byte MASK = 0x07; // PORTB

static bool isSerialOn = false;

static bool
startSerial() {
    if (isSerialOn) {
        return true;
    }
    isSerialOn = true;
    delayMicroseconds(100);
    PORTB = MASK & CHIP_EN;
    delayMicroseconds(100);
	Serial.begin(BAUDRATE);
    return false;
}

static bool
stopSerial() {
    if (!isSerialOn) {
        return false;
    }
    isSerialOn = false;
    Serial.end();
    delayMicroseconds(100);
    return true;
}

static void
logSerial(const char *format, ...) {
    static char buf[256];

    va_list argptr;
    va_start(argptr, format);
    vsnprintf(buf, sizeof(buf), format, argptr);
    bool wasSerialOn = startSerial();
    Serial.print(buf);
    if (!wasSerialOn) {
        stopSerial();
    }
}

void
waitForSerial() {
    while (!Serial.available()) {
        delayMicroseconds(1);
    }
    return;
}

void
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

int
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

int
readHex(uint maxSize) {
    int res = 0;
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

String
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

void
setAddress(uint address) {
    digitalWrite(SHIFT_LATCH, LOW);
    shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, address >> 8 & 0xFF);
    shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, address & 0xFF);
    digitalWrite(SHIFT_LATCH, HIGH);
}

byte
readByte(uint address) {
    bool wasSerialOn = stopSerial();
    DDRD = 0x00; // Set IO pins as input
    setAddress(address);
    PORTB = MASK & CHIP_EN & OUTPUT_EN;
    delayMicroseconds(3);
	if (wasSerialOn) {
        startSerial();
	}
    return PIND;
}

bool
pollData(byte data) {
    delayMicroseconds(100);
	delay(10);
	return false;
    DDRD = 0x80; // Set IO 7 as input
    // ??? pinMode(7, INPUT);
    PORTB = MASK & CHIP_EN & OUTPUT_EN;
    delayMicroseconds(100);
    long pollStart = micros();
    while (data ^ digitalRead(7)) {
        PORTB = MASK & CHIP_EN;
        delayMicroseconds(3);
        PORTB = MASK & CHIP_EN & OUTPUT_EN;
        if (micros() - pollStart > BAIL_OUT_MILLIS) {
            return true;
        }
        delayMicroseconds(3);
    }
    return false;
}

void
pollToggle() {
    DDRD = 0x40; // Set IO 6 as input
    PORTB = MASK & CHIP_EN & OUTPUT_EN;
    bool state = digitalRead(6);
    bool newState;
    int sameInARow = 0;
    while (sameInARow < TOGGLE_SAFE) {
        PORTB = MASK & CHIP_EN;
        delayMicroseconds(3);
        PORTB = MASK & CHIP_EN & OUTPUT_EN;
        delayMicroseconds(3);
        newState = digitalRead(6);
        if (state ^ newState) {
            sameInARow = 0;
        } else {
            ++sameInARow;
        }
        state = newState;
    }
}

uint
writeBytes(uint start, const byte *data, uint len) {
    bool wasSerialOn = stopSerial();
    DDRD = 0xFF;
    PORTB = MASK & CHIP_EN;
    uint end = min(start + len, ((start / 0x40) + 1) * 0x40);
    for (uint i = start; i < end; ++i) {
        setAddress(i);
        PORTD = *data++;
        PORTB = MASK & CHIP_EN & WRITE_EN;
        delayMicroseconds(3);
        PORTB = MASK & CHIP_EN;
        delayMicroseconds(3);
    }
    if (pollData(*--data & 0x80)) {
        end = start; // failure
    }
    if (wasSerialOn) {
      startSerial();
    }
    return end - start;
}

void
writeBulk(uint start, const byte *data, uint len) {
    uint written = 0;
    do {
        len -= written;
        start += written;
        logSerial("Writing from %04x...\r\n", start);
    } while ((written = writeBytes(start, data, len)) < len); 
}

void
printContents(uint start, uint end) {
    bool wasSerialOn = stopSerial();
    //start = start / 16 * 16;
    end = (end / 16 + 1) * 16;
    char buf[80];
    buf[0] = 0;
    char *p = buf;
    for (uint i = start; i < end; ++i) {
        if (i % 16 == 0) {
            logSerial(buf);
            p = buf;
            p += sprintf(p, "\r\n%08x:", i);
        } else if (i % 8 == 0) {
            *p++ = ' ';
        }
        p += sprintf(p, " %02x", readByte(i));
    }
    logSerial(buf);
    logSerial("\r\n");
	if (wasSerialOn) {
        startSerial();
	}
}

bool
actionLoad() {
    startSerial();
    skipWhitespace();
    long int start = readHex(8);
    if (start == -1) {
        return false;
    }
    byte ccrc = crcs::crc8be32(start);
    skipWhitespace();
    long int len = readHex(8);
    if (len == -1) {
        return false;
    }
    ccrc = crcs::crc8be32(len, ccrc);
    skipWhitespace();
    byte crc = readHex(2);
    if (crc == ccrc) {
        logSerial("\r\nOK. Start address and length set.\n");
    } else {
        logSerial("\r\nError! Mismatching crc (%02x != %02x)\n", crc, ccrc);
        return false;
    }
    do {
        uint lenPage = min(len, (start / 0x40 + 1) * 0x40 - start);
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
        byte ccrc = crcs::crc8(page, page + lenPage);
        byte crc = readHex(2);
        if (crc != ccrc) {
            logSerial("\r\nError! Mismatching crc (%02x != %02x)\n", crc, ccrc);
            return false;
        }
        if (writeBytes(start, page, lenPage) == 0) {
            delayMicroseconds(300);
            logSerial("Error! Write timed out.\n");
            return false;
        }
        delayMicroseconds(300);
        logSerial("\r\nOK, written %d bytes of data starting from %08x.\n", lenPage, start);
        len -= lenPage;
        start += lenPage;
    } while (len > 0); 
    logSerial("\r\nDone.\n");
    return true;
}

bool
actionPrint() {
    startSerial();
    skipWhitespace();
    long int start = readHex(8);
    if (start == -1) {
        return false;
    }
    skipWhitespace();
    long int len = readHex(8);
    if (len == -1) {
        return false;
    }
    printContents(start, start + len -1);
    return true;
}

void
setup() {
    DDRB = 0xFF; // Set pin mode to output
    PORTB = MASK & CHIP_EN; // WE = 1, OE = 1; CE = 1, leave shift register pins as 0
    startSerial();
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
    startSerial();
    logSerial("Ready\n");
    skipWhitespace();
    String action = readWord();
    if (action == "LOAD" || action == "load") {
        actionLoad();
    } else if (action == "PRINT" || action == "print") {
        actionPrint();
    } else if (action == "EXIT" || action == "exit") {
        logSerial("Bye!\n");
        stopSerial();
        exit(0);
    } else {
        logSerial("Error! Invalid action.\n");
    }
}

