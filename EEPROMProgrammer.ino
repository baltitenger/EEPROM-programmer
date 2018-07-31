#include <stdarg.h>

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

void
logSerial(const char *format, ...) {
    delayMicroseconds(100);
    PORTB = MASK & CHIP_EN;
    static char buf[256];
    va_list argptr;
    va_start(argptr, format);
    vsnprintf(buf, sizeof(buf), format, argptr);
    Serial.begin(BAUDRATE);
    Serial.print(buf);
    Serial.end();
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
    DDRD = 0x00; // Set IO pins as input
    setAddress(address);
    PORTB = MASK & CHIP_EN & OUTPUT_EN;
    delayMicroseconds(3);
    return PIND;
}

bool
pollData(byte data) {
    DDRD = 0x00; // Set IO pins as input
    PORTB = MASK & CHIP_EN & OUTPUT_EN;
    long pollStart = micros();
    while (data ^ (PIND & 0x80)) {
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
    DDRD = 0x00; // Set IO pins as input
    PORTB = MASK & CHIP_EN & OUTPUT_EN;
    bool state = PIND & 0x40;
    bool newState;
    int sameInARow = 0;
    while (sameInARow < TOGGLE_SAFE) {
        PORTB = MASK & CHIP_EN;
        delayMicroseconds(3);
        PORTB = MASK & CHIP_EN & OUTPUT_EN;
        delayMicroseconds(3);
        newState = PIND & 0x40;
        if (state ^ newState) {
            sameInARow = 0;
        } else {
            ++sameInARow;
        }
        state = newState;
    }
}

void
writeByte(uint address, byte toWrite) {
    DDRD = 0xFF;
    PORTB = MASK & CHIP_EN;
    setAddress(address);
    PORTD = toWrite;
    PORTB = MASK & CHIP_EN & WRITE_EN;
    delayMicroseconds(3);
    PORTB = MASK & CHIP_EN;
    delayMicroseconds(3);
    if (pollData(toWrite & 0x80)) {
        logSerial("Bailing out of writing to %04x\r\n", address);
    }
}

uint
writeBytes(uint start, const byte *data, uint len) {
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
        logSerial("Error");
        return 0
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
}

void
setup() {
    DDRB = 0xFF; // Set pin mode to output
    PORTB = MASK; // WE = 1, OE = 1; CE = 1, leave shift register pins as 0

    logSerial("Program start -----------------------------------------------------\r\n");

    int numBytes = 256;
    byte data[numBytes];
    byte *p = data;
    for (int i = 0; i < numBytes; ++i) {
        *p++ = i;
        // writeByte(i, i + 2);
    }

    logSerial("Write start: %u bytes...\r\n", numBytes);
    long starttime = micros();
    writeBulk(0, data, numBytes);
    logSerial("Write took %u microseconds.\r\n", micros() - starttime);

    logSerial("\r\n");
    printContents(0, numBytes - 1);
    logSerial("\r\n");
}

/*
flags: 3
instruction: 5
step: 4
 */

void
loop() {
}

