CXX = g++
CFLAGS = -Wall -Werror
CFLAGS += -g
#CFLAGS += -O3
#CFLAGS += -std=gnu99
LIBS    = 

CORE = rdspi
OBJS = cmd.o main.o pi2c.o rpi_pin.o si4703.o 
FILES = Makefile cmd.c main.c pi2c.h pi2c.c rpi_pin.h rpi_pin.c si4703.h si4703.c

all: $(CORE)

$(CORE): $(OBJS) $(FILES)
	$(CXX) $(CFLAGS) -o $(CORE) $(OBJS) $(LIBS)

clean:
	rm -f $(CORE)
	rm -f *.o

%.o: %.c  $(FILES)
	$(CXX) -c $(CFLAGS) $< -o $@


