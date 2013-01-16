TARGET:=pikeyd
PREFIX=arm-linux-gnueabihf
TOOLS=$(HOME)/RPi/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian
TOOLS_A=$(TOOLS)/$(PREFIX)
TOOLS_B=$(TOOLS)/lib/gcc/$(PREFIX)/4.7.2
TOOLS_C=$(TOOLS)/libexec/gcc/$(PREFIX)/4.7.2

SRC := $(wildcard *.c)
OBJ := $(patsubst %.c,%.o,$(SRC))
CC = $(PREFIX)-gcc -Wstrict-prototypes -Wmissing-prototypes

LDFLAGS=-L$(TOOLS_A)/lib -L$(TOOLS_A)/libc/lib -L$(TOOLS_A)/libc/lib/arm-linux-gnueabihf -L$(TOOLS_B) -L$(TOOLS_C)
LIBS=
INCLUDE=-I$(TOOLS_A)/include -I$(TOOLS_A)/libc/usr/include -I$(TOOLS_B)/include-fixed -I$(TOOLS_B)/include -I$(TOOLS_B)/finclude

CFLAGS=$(INCLUDE)
PATH += :$(TOOLS)/bin

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS) $(LIBS)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	rm -f $(TARGET) *.o *~


