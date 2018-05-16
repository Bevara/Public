#ifndef MUDPF_FITZ_H
#define MUDPF_FITZ_H

#include <stdio.h>
#include <stdlib.h>

#define NDEBUG 1

double polevl(double x, double coef[], int N );
double p1evl( double x, double coef[], int N );
float mysqrtf(float n);

float myatan(float x);
float myatan2(float y, float x );

#include "version.h"
#include "system.h"
#include "context.h"

#include "crypt.h"
#include "getopt.h"
#include "hash.h"
#include "math.h"
#include "string.h"
#include "tree.h"
#include "xml.h"

/* I/O */
#include "buffer.h"
#include "stream.h"
#include "compressed-buffer.h"
#include "filter.h"
#include "output.h"

/* Resources */
#include "store.h"
#include "colorspace.h"
#include "pixmap.h"
#include "glyph.h"
#include "bitmap.h"
#include "image.h"
#include "function.h"
#include "shade.h"
#include "font.h"
#include "path.h"
#include "text.h"

#include "device.h"
#include "display-list.h"
#include "structured-text.h"

#include "transition.h"
#include "glyph-cache.h"

/* Document */
#include "link.h"
#include "outline.h"
#include "document.h"
#include "annotation.h"
#include "meta.h"

#include "write-document.h"

/* Output formats */
#include "output-pnm.h"
#include "output-png.h"
/* #include "output-pwg.h" */
/* #include "output-pcl.h" */
/* #include "output-svg.h" */
/* #include "output-tga.h" */



#endif
