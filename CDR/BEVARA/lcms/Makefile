

VER_MINOR = 1
VER_MAJOR = 0
TARGET =lcms_$(VER_MAJOR).$(VER_MINOR)




SOURCES = src/cmsalpha.c \
	src/cmscam02.c \
	src/cmscgats.c \
	src/cmscnvrt.c \
	src/cmserr.c \
	src/cmsgamma.c \
	src/cmsgmt.c \
	src/cmshalf.c \
	src/cmsintrp.c \
	src/cmsio0.c \
	src/cmsio1.c \
	src/cmslut.c \
	src/cmsmd5.c \
	src/cmsmtrx.c \
	src/cmsnamed.c \
	src/cmsopt.c \
	src/cmspack.c \
	src/cmspcs.c \
	src/cmsplugin.c \
	src/cmsps2.c \
	src/cmssamp.c \
	src/cmssm.c \
	src/cmstypes.c \
	src/cmsvirt.c \
	src/cmswtpnt.c \
	src/cmsxform.c 


# Uncomment for gcc version
CC = gcc
CCDEPMODE = depmode=gcc3
CFLAGS = -g -O2  -Wall -Wextra -pedantic  #-Werror
CDEPSA = -DPACKAGE_NAME=\"lcms2\" -DPACKAGE_TARNAME=\"lcms2\" -DPACKAGE_VERSION=\"2.9\" -DPACKAGE_STRING=\"lcms2\ 2.9\" 
CDEPSB =  -DSTDC_HEADERS=1 -DHAVE_SYS_TYPES_H=1 -DHAVE_SYS_STAT_H=1  -DHAVE_STDLIB_H=1  -DHAVE_STRING_H=1
CDEPSC = -DHAVE_MEMORY_H=1 -DHAVE_STRINGS_H=1 -DHAVE_INTTYPES_H=1 -DHAVE_STDINT_H=1  -DHAVE_UNISTD_H=1 -DHAVE_DLFCN_H=1
CDEPSD =   -DHAVE_FUNC_ATTRIBUTE_VISIBILITY=1   -DHAVE_PTHREAD=1 -DHasTHREADS=1 -DHasZLIB=1
 
CDEPS = $(CDEPSA)  $(CDEPSB) $(CDEPSC) $(CDEPSD)
  
CPP = gcc -E

CXX=g++
LD=g++
CXXINC =  -I. -Iinc  
CXXFLAGS =  -O0  -std=c++11 -fPIC -v -g -O2 -Werror -Wall -Wextra -pedantic -Wshadow -Wunused-variable -Weffc++  
LDFLAGS =   
# -shared 
MODULES = $(SOURCES:.c=.o)
ASFILE = $(TARGET).o
ARFILE = $(TARGET).a
#
all: LIBR

LIBR: $(ARFILE)

$(ARFILE): $(MODULES)
	ar -s -r -o $@ $^


#$(ASFILE): $(MODULES)
#	$(LD) $(LDFLAGS) -o $@ $(MODULES)
	
.c.o:	
	$(CC) $(CFLAGS) $(CXXINC) $(CDEPS)  -c $< -o $@

	
clean:
	rm -f src/*.o $(ARFILE)


