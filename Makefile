TARGET:=pikeyd

#comment out the line below when _NOT_ cross compiling
include crosscomp.rules

ifneq ($(CROSS), yes)
CC := gcc
LDFLAGS :=
INCLUDE :=
endif

CFLAGS=-O2 -Wstrict-prototypes -Wmissing-prototypes $(INCLUDE)
SRC := $(wildcard *.c)
OBJ := $(patsubst %.c,%.o,$(SRC))
LIBS=

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -s $(OBJ) -o $@ $(LDFLAGS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) *.o *~


