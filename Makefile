CXX = g++
CFLAGS = -Wall -Werror
CFLAGS += -g
#CFLAGS += -O3
LIBS    = -lbcm2835

CORE = rdspi
OBJS = main.o rpi_pin.o si4703.o cmd.o
FILES = Makefile rpi_i2c.h rpi_pin.h rpi_pin.c si4703.h si4703.c cmd.c main.c

all: $(CORE)

$(CORE): $(OBJS) $(FILES)
	$(CXX) $(CFLAGS) -o $(CORE) $(OBJS) $(LIBS)

clean:
	rm -f $(CORE)
	rm -f *.o

%.o: %.c  $(FILES)
	$(CXX) -c $(CFLAGS) $< -o $@


