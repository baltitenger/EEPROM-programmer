#include <iostream>
#include <utility>
#include <array>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <limits>
#include <algorithm>
#include <regex>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>

#define USE_BITWISE_CRC
#include "serial.hpp"
#ifdef USE_BITWISE_CRC
#include "crcs.hpp"
namespace crc = crcs;
#else
#include "crc.hpp"
#endif

#if 0
<COMMAND> <ADDR> <LEN>
<DATA>

<COMMAND> := 'LOAD'

<DATA> := <BLOCK>*

<BLOCK> := <HEXBYTE>* <CHECKSUM>

<CHECKSUM> := <HEXBYTE> | '**'

<HEXBYTE> := <HEXDIGIT><HEXDIGIT>

<HEXDIGIT> := 0-9|A-F

RESPONSE:
OK WRITING FROM <ADDR> <LEN>
OK <BLOCKNUM> CHECKSUM | ERROR <BLOCKNUM> <CHECKSUM> <ERRORCODE>
...
DONE
#endif

using byte = unsigned char;
using std::size_t;
using uint = unsigned int;

static constexpr const uint ADDR_BITS = 32;
static constexpr const size_t ADDR_MAX = 1UL << ADDR_BITS;

static constexpr const uint BATCH_SIZE = 64;
static constexpr const uint MaxErrorCount = 3;

static const char *inputFileName = nullptr; // [-i]
static const char *serialFileName = nullptr;  // -s
static uint baudRate = 115200; // -b
static uint writeOffset = 0; // -w
static uint writeCount = 0; // -c 
static uint readOffset = 0; // -r
static bool trace = true;
static bool printOnly = false;

static void
help() {
  std::cerr << "eeprom-uploader [-q] [-p] [-s <serial>] [-b <baud-rate>] [-w <write offset>] [-c <write-count>] [-r <read-offset>] [-i] <input-file>\n";
}

template <typename T>
static void
error(const T &msg, int errorCode = -1) {
  std::cerr << msg << "\n";
  std::exit(errorCode);
}

static void
perror(const char *msg = nullptr, char opt = 0, int errorCode = -1) {
  help();
  if ((msg)) {
    std::cerr << "ERROR: ";
    if ((opt)) {
      std::cerr << "-" << opt << ": ";
    }
    std::cerr << msg << "\n";
  }
  std::exit(errorCode);
}

static const char *
getArg(int argc, const char *argv[], uint &i) {
  char opt = argv[i][1];
  if ((argv[i][2])) {
    return argv[i] + 2;
  }
  if (++i >= argc) {
    perror("missing argument", opt, -4);
  }
  return argv[i];
}

static const char *
parseArgStr(int argc, const char *argv[], uint &i) {
  return getArg(argc, argv, i);
}

static uint
parseArgUint(int argc, const char *argv[], uint &i, int base = 10) {
  char opt = argv[i][1];
  const char *arg = getArg(argc, argv, i);
  try {
    size_t processed = 0;
    auto v = std::stoul(arg, &processed, base);
    if ((arg[processed])) {
      perror("parse error (garbage after number)", opt, -6);
    }
    if (v > std::numeric_limits<uint>::max()) {
      perror("argument too large", opt, -5);
    }
    return v;
  }
  catch (...) {
    perror("parse error", opt, -6);
  }
  return 0; // never reached
}

static void
setInputFileName(const char *arg, char opt = 0) {
    if ((inputFileName)) {
      perror("multiple input-files", opt, -2);
    }
    inputFileName = arg;
}

static void
parseArgs(int argc, const char *argv[]) {
  if (argc < 2) {
    help();
    std::exit(0);
  }
  uint i = 0;
  while (++i < argc) {
    const char *arg = argv[i];
    if (!arg[0]) {
      perror("invalid argument", -1);
    }
    if (arg[0] != '-') {
      setInputFileName(arg);
    } else {
      switch (arg[1]) {
        case '\0':
          while (++i < argc) {
            setInputFileName(argv[i]);
          }
          break;
        case 'i':
          setInputFileName(parseArgStr(argc, argv, i), 'i');
          break;
        case 's':
          serialFileName = parseArgStr(argc, argv, i);
          break;
        case 'b':
          baudRate = parseArgUint(argc, argv, i);
          break;
        case 'B':
          baudRate = parseArgUint(argc, argv, i, 16);
          break;
        case 'w':
          writeOffset = parseArgUint(argc, argv, i);
          break;
        case 'W':
          writeOffset = parseArgUint(argc, argv, i, 16);
          break;
        case 'c':
          writeCount = parseArgUint(argc, argv, i);
          break;
        case 'C':
          writeCount = parseArgUint(argc, argv, i, 16);
          break;
        case 'r':
          readOffset = parseArgUint(argc, argv, i);
          break;
        case 'R':
          readOffset = parseArgUint(argc, argv, i, 16);
          break;
        case 'q':
          trace = false;
          break;
        case 'p':
          printOnly = true;
          break;
        default: {
          perror("invalid option", arg[1], -3);
        }
      }
    }
  }
  if (!inputFileName && !printOnly) {
    perror("missing input file", 0, -1);
  }
}

//--------------------------------------------------------------------------//

static const std::regex MatchNothing{};

static bool
readResp(Serial &serial, const std::regex &filter = MatchNothing, std::regex_constants::match_flag_type flags = std::regex_constants::match_default) {
  std::string resp;
  while (true) {
    if (!serial.readLine(resp)) {
      error("borken pipe", -30);
    }
    if (&filter != &MatchNothing) {
      if (std::regex_match(resp, filter, flags)) {
        std::cout << resp << "\n";
      }
    }
    boost::trim(resp);
    if (resp.empty()) {
      continue;
    }
    boost::to_upper(resp);
    if (boost::starts_with(resp, "OK")) {
      return true;
    } else if (boost::starts_with(resp, "ERROR") 
        || boost::starts_with(resp, "RESET")) {
      return false;
    }
  }
}

static bool
writeBatch(Serial &serial, const byte *buf, uint len) {
  std::stringstream line;
  for (uint i = 0; i < len; ++i) {
    line << boost::format("%1$02x") % (buf[i] & 0xff);
    if (i % 16 == 15) {
      line << '\n';
    } else {
      line << ' ';
    }
  }
  byte batchCrc = crc::crc8(buf, buf + len);
  line << boost::format("%1$02x") % (batchCrc & 0xff);
  return serial.writeLine(line.str());
}

static void
waitForReady(Serial &serial) {
  if (trace) {
    std::cout << "waiting for READY...\n";
  }
  std::string line;
  while (true) {
    if (!serial.readLine(line)) {
      error("borken pipe", -30);
    }
    boost::trim(line);
    if (line.empty()) {
      continue;
    }
    boost::to_upper(line);
    if (boost::starts_with(line, "READY")) {
      break;
    }
  }
}

static void
doPrint(Serial &serial, uint writeOffset, uint writeCount) {
  waitForReady(serial);
  static const std::regex printFilter = std::regex("^[0-9a-fA-F]{4}:.*$");

  for (uint i = 0; i < MaxErrorCount; ++i) {
    byte hdrCrc = crc::crc8be16(writeOffset, 0);
    hdrCrc = crc::crc8be16(writeCount, hdrCrc);
//    serial.writeLine(boost::format("PRINT %1$04x %2$04x %3$02x") % writeOffset % writeCount % (hdrCrc & 0xff));
    serial.writeLine(boost::format("PRINT %1$04x %2$04x") % writeOffset % writeCount);
    if (readResp(serial, (trace ? MatchNothing : printFilter))) {
      return;
    }
  }
}

static uint
doWrite(Serial &serial, std::ifstream &bin, uint writeOffset, uint writeCount) {
  waitForReady(serial);

  bin.clear();
  bin.seekg(readOffset, std::ios_base::beg);

  byte hdrCrc = crc::crc8be16(writeOffset, 0);
  hdrCrc = crc::crc8be16(writeCount, hdrCrc);
  serial.writeLine(boost::format("LOAD %1$04x %2$04x %3$02x") % writeOffset % writeCount % (hdrCrc & 0xff));
  if (!readResp(serial)) {
    return 0;
  }

  byte buf[BATCH_SIZE];
  uint wc = writeCount;
  // must end batch at the end of the page
  uint maxBatchLen = std::min(BATCH_SIZE, BATCH_SIZE - (writeOffset % BATCH_SIZE));
  while (wc > 0) {
    uint len = std::min(maxBatchLen, wc);
    maxBatchLen = BATCH_SIZE;
    if (!bin.read(static_cast<char*>(static_cast<void*>(buf)), len)) {
      error("input file read error", -40);
    }
    if (len != bin.gcount()) {
      error("unexpected EOF on input file", -41);
    }
    if (!writeBatch(serial, buf, len)) {
      break;
    }
    if (!readResp(serial)) {
      break;
    }
    wc -= len;
  }
  return writeCount - wc;
}

int
main(int argc, const char *argv[]) {
  parseArgs(argc, argv);

  Serial serial{trace};
  if (serialFileName) {
    serial.open(serialFileName);
    serial.setBaudRate(baudRate);
    if (!serial.isOpen()) {
      error("failed to open serial", -20);
    }
  } else {
    serial.setTrace(false);
  }
  serial.writeLine("RESET");

  if (printOnly) {
    doPrint(serial, writeOffset, writeCount);
    return 0;
  }

  std::ifstream bin;
  bin.open(inputFileName,std::ios::in|std::ios::binary);
  if (!bin) {
    error("failed to open input file", -20);
  }
  bin.ignore(std::numeric_limits<std::streamsize>::max());
  std::streamsize inLen = bin.gcount();
  if (!writeCount) {
    if (readOffset >= inLen) {
      error("read-offset out-of-range", -10);
    }
    writeCount = inLen - readOffset;
  } else if (readOffset + writeCount > inLen) {
    error("input file too short", -11);
  }
  if (static_cast<size_t>(readOffset) + writeCount > ADDR_MAX) {
    error("write out of range", -12);
  }

  std::string resp;
  int errorCount = 0;
  int allErrorCount = 0;
  while (writeCount > 0 && errorCount < MaxErrorCount) {
    uint numWritten = doWrite(serial, bin, writeOffset, writeCount);
    writeCount -= numWritten;
    if (writeCount > 0) {
      ++allErrorCount;
      if (numWritten > 0) {
        errorCount = 1;
      } else {
        ++errorCount;
      }
      writeOffset += numWritten;
      readOffset += numWritten;
    } else {
      errorCount = 0;
    }
  }
  if (writeCount == 0) {
    std::cerr << "SUCCESS, " << allErrorCount << " temporary write errors.\n";
  } else {
    std::cerr << "FAILURE, " << allErrorCount << " write errors\n";
  }
  return 0;
}

