CC=C:\\SysGCC\\raspberry\\bin\\arm-linux-gnueabihf-gcc.exe
CXX=C:\\SysGCC\\raspberry\\bin\\arm-linux-gnueabihf-g++.exe
LDFLAGS=
EXEC=RFID-Controler
SRC_PATH=./src
INC_PATH=-I./inc -I../include/Common -I../include/common-include -I../ncurses-6.1/include
LIB=-lpthread -lwiringPi -lwiringPiDev
#LIB=-lpthread
CFLAGS=$(INC_PATH) -ggdb -W -Wall -ansi -pedantic -std=gnu99
CXXFLAGS=$(INC_PATH) -ggdb -W -Wall -ansi -pedantic -std=c++11 -Wno-unused-parameter -Wno-unused-value -Wno-unused-variable -Wno-unused-but-set-variable

src = $(wildcard $(SRC_PATH)/*.cpp)
obj = $(src:.cpp=.o)

all: $(EXEC)
	
$(EXEC): $(obj)
	$(CXX) $(CXXFLAGS) $(LIB) -o  $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) $(EXEC)

.PHONY: mrproper
mrproper: clean
	rm -rf $(EXEC)