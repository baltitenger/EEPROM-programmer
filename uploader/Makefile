CXX=clang++
CXXFLAGS=-std=gnu++17 -stdlib=libc++ -Wall -O2
LDFLAGS=-stdlib=libc++ -lc++abi -lboost_system -pthread 
LD=clang++
BUILD_DIR=build

all: $(BUILD_DIR)/eeprom-uploader

$(BUILD_DIR)/eeprom-uploader: main.cpp
	@mkdir -p $(BUILD_DIR)
	$(LD) $(LDFLAGS) -o $@ $<

clean:
	rm -f *.o
	rm -f eeprom-uploader
