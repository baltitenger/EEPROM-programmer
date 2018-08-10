CXX=clang++
CXXFLAGS=-std=gnu++17 -stdlib=libc++ -Wall -O2
LDFLAGS=-stdlib=libc++ -lc++abi -lboost_system -pthread 
LD=clang++

all: eeprom-uploader

eeprom-uploader: eeprom-uploader.o
	$(LD) $(LDFLAGS) -o $@ $<

clean:
	rm -f *.o
	rm -f eeprom-uploader
