#ifndef SERIAL_HPP_h987nx7rf91z9p8m
#define SERIAL_HPP_h987nx7rf91z9p8m

#include <sstream>
#include <iostream>
#include <boost/asio.hpp>

class Serial {
public:
  using uint = unsigned int;

private:
  static constexpr const uint BUF_SIZE = 8192;
  bool trace_;
  boost::asio::io_service io_;
  boost::asio::serial_port serial_;

  bool
  readLineSerial(std::string &line) {
    char ch;
    line.clear();
    while (true) {
      boost::asio::read(serial_, boost::asio::buffer(&ch, 1));
      switch (ch) {
        case '\r':
          break;
        case '\n':
          return true;
        default:
          line += ch;
          break;
      }
    }
  }

  bool
  readAvailableSerial(std::string &line) {
    try {
      std::array<char, BUF_SIZE> buf;
      std::size_t numRead = boost::asio::read(serial_,
          boost::asio::buffer(buf), boost::asio::transfer_at_least(0));
      line.clear();
      for (uint i = 0; i < numRead; ++i) {
        line += buf[i];
      }
      if (trace_) {
        std::cout << ">> " << line << std::endl;
      }
      return true;
    }
    catch (...) {
      return false;
    }
  }

public:
  Serial(bool trace = true) : trace_{trace}, io_{}, serial_{io_} {
  }

  void
  setBaudRate(uint baudRate) {
    serial_.set_option(boost::asio::serial_port_base::baud_rate(baudRate));
  }

  bool
  open(const std::string &fileName) {
    serial_.open(fileName);
    return serial_.is_open();
  }

  template <typename T>
  bool
  writeLine(const T &line) {
    try {
      if (isOpen()) {
        std::stringstream ss;
        ss << line << "\n";
        std::string s = ss.str();
        boost::asio::write(serial_, boost::asio::buffer(s.c_str(), s.size()));
        if (trace_) {
          std::cout << "<< " << line << std::endl;
        }
        // FIXME: flush
      } else {
        std::cout << line << std::endl;
      }
      return true;
    } catch (...) {
      return false;
    }
  }

  bool
  readLine(std::string &line) {
    if (isOpen()) {
      if (!readLineSerial(line)) {
        return false;
      }
      if (trace_) {
        std::cout << ">> " << line << std::endl;
      }
    } else {
      if (!std::getline(std::cin, line)) {
        return false;
      }
    }
    return true;
  }

  bool
  readAvailable(std::string &str) {
    if (isOpen()) {
      return readAvailableSerial(str);
    }
    else {
      std::array<char, BUF_SIZE> buf;
      uint numRead = std::cin.readsome(&buf[0], buf.size());
      str.clear();
      for (uint i = 0; i < numRead; ++i) {
        str += buf[i];
      }
      return static_cast<bool>(std::cin);
    }
  }

  bool
  isOpen() {
    return serial_.is_open();
  }

  void
  setTrace(bool trace) {
    trace_ = trace;
  }
};

#endif // SERIAL_HPP_h987nx7rf91z9p8m
