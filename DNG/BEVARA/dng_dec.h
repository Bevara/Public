

#ifndef DNG_DEC_H
#define DNG_DEC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <emscripten/emscripten.h>

//general error value, used in verification process
#define BEV_NOT_SUPPORTED -4
#define ushort unsigned short 


//global params
#define MAX_NUM_THUMBNAILS 5
#define MAX_NUM_IFDS 10
#define MAX_NUM_COMPONENTS 20  // components struct includes IFDS (main, thumbs, XMPs)
#define MAX_METADATA_LENGTH 10000

  // These enums do not correspond to the DNG compression types
  // e.g., baseline and lossless JPEG share comp flag = 7 in 
  // DNG. These are returned as a flag from the DNG parser for 
  // the demuxer to choose the accessor. 

enum DNG_COMPRESSION_TYPE {
  COMP_UNKNOWN   = 0,            /* placeholder when we don't know type */  
  NO_COMPRESSION = 1,           /* no compression */
  UNCOMPRESSED   = 2,		/* uncompressed */
  PACKED_DNG     = 3,           /* packed DNG, uncompressed or Huffman */
  LOSSLESS_JPEG  = 4,		/* lossless JPEG */
  BASELINE_JPEG  = 5,		/* baseline JPEG or lossless JPEG */
  ZIP            = 6,           /* ZIP */
  LOSSY_JPEG     = 7		/* Lossy JPEG */
};

enum DNG_IFD_TYPE {
    TYPE_UNKNOWN = 0,
    TYPE_MAIN_IMAGE = 1,
    TYPE_THUMBNAIL = 2,
    TYPE_XMP = 3,
	TYPE_METADATA = 4
};

  enum DNG_UNICODE_TYPE {  /* used for XMP */
    UNICODE_UNKNOWN =0,
    UNICODE8 = 1,
    UNICODE16 = 2,
    UNICODE32 =3
  };

typedef struct tiff_ifd_ {
  int type;
  int dng_comp; /* note different index from comp below */
 
  long XMP_start; /* handle XMP data within an ifd */
  long XMP_stop;
  int XMP_coding;

  int width, height, bps, comp, phint, offset, flip, samples, bytes, endoffset;
  int tile_width, tile_length;
  float shutter;
} TIFF_IFD;

extern TIFF_IFD tiff_ifd[MAX_NUM_IFDS];


// we need to track each component, since multiple can be embedded in an
// IFD separate out and link back to host IFD
typedef struct component_ {
  int type;
  int ifd_loc; 
  int width;
  int height;
  int size;
} COMPONENT;

extern COMPONENT component[MAX_NUM_COMPONENTS];

extern int outwidth, outheight;
extern unsigned char* outbuf;

extern int EMSCRIPTEN_KEEPALIVE imgSetup();

extern int EMSCRIPTEN_KEEPALIVE getNumComponents(char *inbuf, int insize );

extern int EMSCRIPTEN_KEEPALIVE getWidth(int idx);
extern int EMSCRIPTEN_KEEPALIVE getHeight(int idx);
extern int EMSCRIPTEN_KEEPALIVE getSize(int idx);
extern int EMSCRIPTEN_KEEPALIVE getComponentType(int idx);
extern unsigned char*  EMSCRIPTEN_KEEPALIVE getData(int idx);
extern int EMSCRIPTEN_KEEPALIVE imgShutdown();

#ifdef __cplusplus
}
#endif


#endif
