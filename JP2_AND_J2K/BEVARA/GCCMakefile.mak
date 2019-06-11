

TARGET = jp2_dec



CFLAGS = -Wall -g
SOURCES =  src/PlayerMain.c  src/cidx_manager.c src/color.c src/bio.c src/cio.c   src/dwt.c src/event.c src/image.c src/j2k.c   src/function_list.c src/jp2.c src/jpt.c src/mct.c src/mqc.c src/openjpeg.c src/opj_getopt.c src/pi.c  src/ppix_manager.c src/raw.c src/t1.c src/t2.c src/tcd.c src/tgt.c src/thix_manager.c src/tpix_manager.c  src/phix_manager.c


CC=gcc
LD=gcc
CFLAGS = -g -DDEBUG  -v -Wall 
MODULES = $(SOURCES:.c=.o)
ASFILE = $(TARGET).o
#
all: JP2

JP2: $(ASFILE)

$(ASFILE): $(MODULES)
	$(LD)  -o $@ $(MODULES)
	
.c.o:	
	$(CC) $(CFLAGS) -c $< -o $@

