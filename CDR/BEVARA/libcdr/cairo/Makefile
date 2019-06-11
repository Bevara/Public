VER_MINOR = 1
VER_MAJOR = 0
TARGET =cdr2rgb_$(VER_MAJOR).$(VER_MINOR)




SOURCES = hello.c 


# Uncomment for gcc version
CC = gcc
CCDEPMODE = depmode=gcc3
CFLAGS = -g -O2 -Wall -Wextra -pedantic -DHAVE_STDINT_H
CPP = gcc -E
CINC =  -I. -I"./cairo-1.16.0/src/" -I"./pixman-0.38.2/pixman/"

CXXCPP = g++ -E
CXXDEPMODE = depmode=gcc3
CXXFLAGS = -g -O2 -Werror -Wall -Wextra -pedantic -Wshadow -Wunused-variable -Weffc++


CXX=g++
LDXX=g++
CXXINC =  -I. -Iinc  -Ilibcairo
CXXFLAGS =  -O0  -std=c++11 -fPIC -v -g -O2  -Wall -Wextra -pedantic -Wshadow 
# -Werror
#-Wunused-variable 
#-Weffc++
LDFLAGS =  -Llibs -lrevenge -licu_0.1
# -shared 
MODULES = $(SOURCES:.c=.o)
ASFILE = $(TARGET).o
ARFILE = $(TARGET).a
#
all: LIBCDR

LIBCDR: $(ASFILE)

#$(ARFILE): $(MODULES)
#	ar -s -r -o $@ $^


$(ASFILE): $(MODULES)
	$(CC)  -o $@ $(MODULES)   -L"/usr/local/lib/" -lcairo
	
.c.o:	
	$(CC)  $(CFLAGS) $(CINC)   -c $< -o $@

	
clean:
	rm -f src/*.o $(ARFILE)

