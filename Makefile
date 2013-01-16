TARGET:=pikeyd

include crosscomp.rules

#comment out the line below when _NOT_ cross compiling
CROSS:=yes

ifneq ($(CROSS), yes)
CC := gcc
LDFLAGS :=
INCLUDE :=
else
PATH += :$(TOOLS)/bin
CC := $(PREFIX)-gcc -Wstrict-prototypes -Wmissing-prototypes
LDFLAGS := -L$(TOOLS_A)/lib -L$(TOOLS_A)/libc/lib -L$(TOOLS_A)/libc/lib/arm-linux-gnueabihf -L$(TOOLS_B) -L$(TOOLS_C)
INCLUDE := -I$(TOOLS_A)/include -I$(TOOLS_A)/libc/usr/include -I$(TOOLS_B)/include-fixed -I$(TOOLS_B)/include -I$(TOOLS_B)/finclude
endif

CFLAGS=$(INCLUDE)
SRC := $(wildcard *.c)
OBJ := $(patsubst %.c,%.o,$(SRC))
LIBS=

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS) $(LIBS)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	rm -f $(TARGET) *.o *~


