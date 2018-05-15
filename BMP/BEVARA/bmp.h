#ifndef _BMP_H_
#define _BMP_H_

#include <emscripten/emscripten.h>

/**************************************************************

	QDBMP - Quick n' Dirty BMP

	v1.0.0 - 2007-04-07
	http://qdbmp.sourceforge.net


	The library supports the following BMP variants:
	1. Uncompressed 32 BPP (alpha values are ignored)
	2. Uncompressed 24 BPP
	3. Uncompressed 8 BPP (indexed color)

	QDBMP is free and open source software, distributed
	under the MIT licence.

	Copyright (c) 2007 Chai Braudo (braudo@users.sourceforge.net)

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in
	all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
	THE SOFTWARE.

**************************************************************/






#include <stdio.h>

// BEVARA: for debugging use when needed
// #define BEV_DEBUG 1
// extern int debugVal;
// extern char debugValChar;
// extern char debugString[50];

/* Type definitions */
#ifndef UINT
	#define UINT	unsigned long int
#endif

#ifndef USHORT
	#define USHORT	unsigned short
#endif

#ifndef UCHAR
	#define UCHAR	unsigned char
#endif


/* Version */
#define QDBMP_VERSION_MAJOR		1
#define QDBMP_VERSION_MINOR		0
#define QDBMP_VERSION_PATCH		1


/* Error codes */
typedef enum
{
	BMP_OK = 0,				/* No error */
	BMP_ERROR,				/* General error */
	BMP_OUT_OF_MEMORY,		/* Could not allocate enough memory to complete the operation */
	BMP_IO_ERROR,			/* General input/output error */
	BMP_FILE_NOT_FOUND,		/* File not found */
	BMP_FILE_NOT_SUPPORTED,	/* File is not a supported BMP variant */
	BMP_FILE_INVALID,		/* File is not a BMP image or is an invalid BMP */
	BMP_INVALID_ARGUMENT,	/* An argument is invalid or out of range */
	BMP_TYPE_MISMATCH,		/* The requested action is not compatible with the BMP's type */
	BMP_ERROR_NUM
} BMP_STATUS;




/* Type definitions */
#ifndef UINT
	#define UINT	unsigned long int
#endif

#ifndef USHORT
	#define USHORT	unsigned short
#endif

#ifndef UCHAR
	#define UCHAR	unsigned char
#endif

/* Bitmap header */
struct BMP_Header
{
	USHORT		Magic;				/* Magic identifier: "BM" */
	UINT		FileSize;			/* Size of the BMP file in bytes */
	USHORT		Reserved1;			/* Reserved */
	USHORT		Reserved2;			/* Reserved */
	UINT		DataOffset;			/* Offset of image data relative to the file's start */
	UINT		HeaderSize;			/* Size of the header in bytes */
	UINT		Width;				/* Bitmap's width */
	UINT		Height;				/* Bitmap's height */
	USHORT		Planes;				/* Number of color planes in the bitmap */
	USHORT		BitsPerPixel;		/* Number of bits per pixel */
	UINT		CompressionType;	/* Compression type */
	UINT		ImageDataSize;		/* Size of uncompressed image's data */
	UINT		HPixelsPerMeter;	/* Horizontal resolution (pixels per meter) */
	UINT		VPixelsPerMeter;	/* Vertical resolution (pixels per meter) */
	UINT		ColorsUsed;			/* Number of color indexes in the color table that are actually used by the bitmap */
	UINT		ColorsRequired;		/* Number of color indexes that are required for displaying the bitmap */
};


/* Private data structure */
struct BMP_struct
{
	struct BMP_Header	Header;
	UCHAR*		Palette;
	UCHAR*		Data;
};



/* Error description strings */
static const char* BMP_ERROR_STRING[] =
{
	"",
	"General error",
	"Could not allocate enough memory to complete the operation",
	"File input/output error",
	"File not found",
	"File is not a supported BMP variant (must be uncompressed 8, 24 or 32 BPP)",
	"File is not a valid BMP image",
	"An argument is invalid or out of range",
	"The requested action is not compatible with the BMP's type"
};


/* Size of the palette data for 8 BPP bitmaps */
#define BMP_PALETTE_SIZE	( 256 * 4 )


/* our BMP */
extern struct BMP_struct * bmp;


/*********************************** Public methods **********************************/


/* Construction/destruction */
// BEVARA: these are currently removed since they are handled externally
// int			EMSCRIPTEN_KEEPALIVE BMP_Create( UINT width, UINT height, USHORT depth );
// void		EMSCRIPTEN_KEEPALIVE BMP_Free ( );


/* I/O */
int			 EMSCRIPTEN_KEEPALIVE BMP_Decode(const char* bmp_data, const int size );
unsigned char* EMSCRIPTEN_KEEPALIVE BMP_GetImage(void);

/* Meta info */
int			EMSCRIPTEN_KEEPALIVE BMP_GetWidth  ( );
int			EMSCRIPTEN_KEEPALIVE BMP_GetHeight ( );
int			EMSCRIPTEN_KEEPALIVE BMP_GetDepth  ( );





#endif
