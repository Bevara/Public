

TARGET = pgm_dec


SOURCES =  src/pgm_accessor.c	

CC=gcc
LD=gcc
CFLAGS = -g -DDEBUG  -v -Wall 
MODULES = $(SOURCES:.c=.o)
ASFILE = $(TARGET).o
#
all: PGM

PGM: $(ASFILE)

$(ASFILE): $(MODULES)
	$(LD)  -o $@ $(MODULES)
	
.c.o:	
	$(CC) $(CFLAGS) -c $< -o $@

