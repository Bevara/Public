VER_MINOR = 1
VER_MAJOR = 0
TARGET =cdr2rgb_$(VER_MAJOR).$(VER_MINOR)




SOURCES = src/conv/rgb/cdr2rgb.cpp \
	src/CDRCollector.cpp \
	src/CDRContentCollector.cpp \
	src/CDRDocument.cpp \
	src/CDRInternalStream.cpp \
	src/CDROutputElementList.cpp \
	src/CDRParser.cpp \
	src/CDRPath.cpp \
	src/CDRStylesCollector.cpp \
	src/CDRTransforms.cpp \
	src/CDRTypes.cpp \
	src/CMXDocument.cpp \
	src/CMXParser.cpp \
	src/CommonParser.cpp \
	src/libcdr_utils.cpp \
	librevenge-rgb/src/RVNGBinaryData.cpp \
	librevenge-rgb/src/RVNGCSVSpreadsheetGenerator.cpp \
	librevenge-rgb/src/RVNGDirectoryStream.cpp \
	librevenge-rgb/src/RVNGHTMLTextGenerator.cpp \
	librevenge-rgb/src/RVNGHTMLTextTableStyle.cpp \
	librevenge-rgb/src/RVNGHTMLTextTextStyle.cpp \
	librevenge-rgb/src/RVNGMemoryStream.cpp \
	librevenge-rgb/src/RVNGOLEStream.cpp \
	librevenge-rgb/src/RVNGProperty.cpp \
	librevenge-rgb/src/RVNGPropertyList.cpp \
	librevenge-rgb/src/RVNGPropertyListVector.cpp \
	librevenge-rgb/src/RVNGStreamImplementation.cpp \
	librevenge-rgb/src/RVNGString.cpp \
	librevenge-rgb/src/RVNGStringVector.cpp \
	librevenge-rgb/src/RVNGSVGDrawingGenerator.cpp \
	librevenge-rgb/src/RVNGSVGPresentationGenerator.cpp \
	librevenge-rgb/src/RVNGSVGRGBDrawingGenerator.cpp \
	librevenge-rgb/src/RVNGSVGRGBPresentationGenerator.cpp \
	librevenge-rgb/src/RVNGTextDrawingGenerator.cpp \
	librevenge-rgb/src/RVNGTextPresentationGenerator.cpp \
	librevenge-rgb/src/RVNGTextSpreadsheetGenerator.cpp \
	librevenge-rgb/src/RVNGTextTextGenerator.cpp \
	librevenge-rgb/src/RVNGZipStream.cpp
	


# Uncomment for gcc version
CC = gcc
CCDEPMODE = depmode=gcc3
CFLAGS = -g -O2 -Werror -Wall -Wextra -pedantic -DHAVE_STDINT_H
CPP = gcc -E

CXXCPP = g++ -E
CXXDEPMODE = depmode=gcc3
CXXFLAGS = -g -O2 -Werror -Wall -Wextra -pedantic -Wshadow -Wunused-variable -Weffc++ -DHAVE_STDINT_H


CXX=g++
LDXX=g++
CXXINC =  -I. -Iinc  -I"librevenge-rgb/inc" -I"./cairo/cairo-1.16.0/src/" -I"./cairo/pixman-0.38.2/pixman/"
CXXFLAGS =  -O0  -std=c++11 -fPIC -v -g -O2  -Wall -Wextra -pedantic -Wshadow 
# -Werror
#-Wunused-variable 
#-Weffc++
LDFLAGS =  -Llibs  -licu_0.1
# -shared 
MODULES = $(SOURCES:.cpp=.o)
ASFILE = $(TARGET).o
ARFILE = $(TARGET).a
#
all: LIBCDR

LIBCDR: $(ASFILE)

#$(ARFILE): $(MODULES)
#	ar -s -r -o $@ $^


$(ASFILE): $(MODULES)
	$(LDXX)  -o $@ $(MODULES)   -Llibs -licu_0.1 -llcms_0.1 -lzlib_0.1 -lboost_system -lcairo -lpixman-1
	
.cpp.o:	
	$(CXX)  $(CXXFLAGS) $(CXXINC)   -c $< -o $@

	
clean:
	rm -f src/*.o $(ARFILE)

