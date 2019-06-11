

TARGET = JBIG2_dec


SOURCES = src/jbig2dec.c  \
	src/jbig2_arith.c src/jbig2_arith_int.c \
	src/jbig2_arith_iaid.c \
	src/jbig2_huffman.c src/jbig2_segment.c \
	src/jbig2_page.c src/jbig2_symbol_dict.c \
	src/jbig2_text.c src/jbig2_halftone.c \
	src/jbig2_generic.c src/jbig2_refinement.c \
	src/jbig2_mmr.c src/jbig2_image.c \
	src/jbig2_metadata.c src/jbig2.c 


CC=gcc
LD=gcc
CFLAGS = -g -DDEBUG  -v -Wall 
MODULES = $(SOURCES:.c=.o)
ASFILE = $(TARGET).o
#
all: JBIG

JBIG: $(ASFILE)

$(ASFILE): $(MODULES)
	$(LD)  -o $@ $(MODULES)
	
.c.o:	
	$(CC) $(CFLAGS) -c $< -o $@

