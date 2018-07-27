using uint = unsigned int;

// pins 0 and 1 are used for serial

// 2 shift registers used for address pins and output enable
const int SHIFT_DATA = 2;
const int SHIFT_CLK = 3;
const int SHIFT_LATCH = 4;

const int WRITE_EN = 5; // Acive LOW
const int IO_START = 6;

void setAddress(uint address, bool outputEnable) {
    digitalWrite(SHIFT_LATCH, LOW);
    address <<= 1;
    address |= outputEnable; // active LOW
    shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, address >> 8 & 0xFF);
    shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, address & 0xFF);
    digitalWrite(SHIFT_LATCH, HIGH);
}

byte readByte(uint address) {
    for (int pin = IO_START + 7; pin >= IO_START; --pin) {
        pinMode(pin, INPUT);
    }
    setAddress(address, false);
    byte res;
    for (int pin = IO_START + 7; pin >= IO_START; --pin) {
        res <<= 1;
        res |= digitalRead(pin);
    }
    return res;
}

void writeByte(uint address, byte toWrite) {
    for (int pin = IO_START; pin <= IO_START + 7; ++pin) {
        pinMode(pin, OUTPUT);
    }
    setAddress(address, true);
    for (int pin = IO_START; pin <= IO_START + 7; ++pin) {
        digitalWrite(pin, toWrite & 1);
        toWrite >>=1;
    }
    digitalWrite(WRITE_EN, LOW);
    delayMicroseconds(1);
    digitalWrite(WRITE_EN, HIGH);
    delay(10);
}

void printContents(uint start, uint end) {
    start = start / 16 * 16;
    end = (end / 16 + 1) * 16;
    char buf[80];
    buf[0] = 0;
    char *p = buf;
    for (uint i = start; i < end; ++i) {
        if (i % 16 == 0) {
            Serial.println(buf);
            p = buf;
            p += sprintf(p, "%08x:", i);
        } else if (i % 8 == 0) {
            *p++ = ' ';
        }
        p += sprintf(p, " %02x", readByte(i));
    }
    Serial.println(buf);
}

void setup() {
    pinMode(SHIFT_DATA, OUTPUT);
    pinMode(SHIFT_CLK, OUTPUT);
    pinMode(SHIFT_LATCH, OUTPUT);

    pinMode(WRITE_EN, OUTPUT);
    digitalWrite(WRITE_EN, HIGH);

    Serial.begin(9600);
    Serial.println("Program start -----------------------------------------------------");


    delay(1000);
    int numBytes = 1000;
    Serial.print("Write start: ");
    Serial.print(numBytes);
    Serial.println("bytes...");
    long starttime = micros();
    for (int i = 0; i < numBytes; ++i) {
        writeByte(i, 255);
    }
    Serial.print("Write finished. It took ");
    Serial.print(micros() - starttime);
    Serial.println(" microseconds.");
    printContents(0, numBytes);

/*
    writeByte(0, 0x9A);
    delay(1);
    Serial.println(readByte(0));
 */

    Serial.println("\n");
}

/*
flags: 3
instruction: 5
step: 4
 */

void loop() {
}
