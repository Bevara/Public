

TARGET = progjpeg_dec

SOURCES = source/djpeg.c \
	source/wrppm.c \
	source/jmemnobs.c\
	source/jaricom.c \
	source/jcomapi.c \
	source/jdapimin.c \
	source/jdapistd.c \
	source/jdarith.c \
	source/jdatadst.c \
	source/jdatasrc.c \
	source/jdcoefct.c \
	source/jdcolor.c \
	source/jddctmgr.c \
	source/jdhuff.c \
	source/jdinput.c \
	source/jdmainct.c \
	source/jdmaster.c \
	source/jdmarker.c \
	source/jdmerge.c \
	source/jdpostct.c \
	source/jdsample.c \
	source/jerror.c \
	source/jidctint.c \
	source/jmemmgr.c \
	source/jquant1.c \
	source/jquant2.c \
	source/jutils.c 


CC=gcc
LD=gcc
CFLAGS = -g -DDEBUG  -v -Wall 
MODULES = $(SOURCES:.c=.o)
ASFILE = $(TARGET).o
#
all: JPEG

JPEG: $(ASFILE)

$(ASFILE): $(MODULES)
	$(LD)  -o $@ $(MODULES)
	
.c.o:	
	$(CC) $(CFLAGS) -c $< -o $@
