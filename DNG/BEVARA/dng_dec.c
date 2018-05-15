/*
   dcraw.c -- Dave Coffin's raw photo decoder
   Copyright 1997-2015 by Dave Coffin, dcoffin a cybercom o net

   This is a command-line ANSI C program to convert raw photos from
   any digital camera on any computer running any operating system.

   No license is required to download and use dcraw.c.  However,
   to lawfully redistribute dcraw, you must either (a) offer, at
   no extra charge, full source code* for all executable files
   containing RESTRICTED functions, (b) distribute this code under
   the GPL Version 2 or later, (c) remove all RESTRICTED functions,
   re-implement them, or copy them from an earlier, unrestricted
   Revision of dcraw.c, or (d) purchase a license from the author.

   The functions that process Foveon images have been RESTRICTED
   since Revision 1.237.  All other code remains free for all uses.

   *If you have not modified dcraw.c in any way, a link to my
   homepage qualifies as "full source code".

   $Revision: 1.476 $
   $Date: 2015/05/25 02:29:14 $
 */

/***********************************************************************
 ** 
 ** BEVARA 2015
 **
 ** Revised by Bevara 10/2015
 **    cut down to just Canon case
 **    Foveon image functions removed
 **    RAW handling removed
 **    code generally trimmed
 **    mem and string lib fns implemented locally
 **    incorporated a baseline JPEG decoder
 **
 ** Note: bad_pixel files not handled
 ** 
 ** Various features are left in but commented out if not widely used, in
 ** order to save space, or if not supported. Feel free to uncomment, but
 ** make sure to test them.
 ** 
 ******************************** ***************************************/

#define DCRAW_VERSION "9.26"


//#include <emscripten/emscripten.h>
#define EMSCRIPTEN_KEEPALIVE 

//#include <ctype.h>
//#include <errno.h>
//#include <fcntl.h>
//#include <float.h>
//#include <limits.h>
#include <math.h>
//#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
//#include <time.h>
//#include <sys/types.h>
 #include <stdio.h>
//#include <ctype.h>

#include "dng_dec.h"

#define BEV_OK 0 // this is returned if all is going well
#define BEV_NOT_OK 1 // this is returned on a failure

#define GF_OK 0 // these are legavy values
#define GF_NOT_SUPPORTED -4

// BEVARA: we need some globals, since we're importing/exporting
// data and decoded data/info
// We have to expand on the components and IFDs; see the header file
int numComponents; // this stores the number of thumbs+mains+XMPs
int outwidth, outheight, outsize;
unsigned char* outbuf;
TIFF_IFD tiff_ifd[MAX_NUM_IFDS];
COMPONENT component[MAX_NUM_COMPONENTS];
char* globalInbuf;
int globalInsize;

//#include <ieeefp.h>
// pulled from IEEE DBL MAX POW 2
#define DBL_MAX	((double)(1L << (32 - 2)) * ((1L << (32-11)) - 32 + 1))

#define UINT_MAX  4294967295U
#define INT_MAX 2147483647


typedef long long INT64;
typedef unsigned int UINT32;
typedef unsigned long long UINT64;


/* handle the endianess */
const int endtest = 1;
#define is_bigendian() ( (*(char*)&endtest) == 0 )

void myswab(ushort *pixelsrc, ushort *pixeldst, int count) /*by def count is even, since fed in as even */
{
  if (count == 2)
    {
      *pixeldst = 0;
      *pixeldst = ((*pixelsrc >> 8) & 0x00FF) + ((*pixelsrc & 0x00FF) << 8);
     }
  else 
    printf("problem with myswab() -- more than 16bits being swapped\n");

}

UINT32 myswabint(UINT32 a)
{
  return (  ((a >> 24) & 0x000000FF) +  ( (a >> 8 & 0x0000FF00) ) + ((a & 0x000000FF) << 24) + ((a & 0x0000FF00) << 8) );
}



#if !defined(uchar)
#define uchar unsigned char
#endif
#if !defined(ushort)
#define ushort unsigned short
#endif

/* general new globals for entry fn, moved from original main() */
int thm = -1;
 int status=0, /* quality, */ i, c;
  int identify_only=0;
  /* int user_qual=-1, user_black=-1, user_sat=-1; */
  char *ofname;
long DATALOC;
long DATALEN;

#define EOF (-1)

/*****
BEVARA needs some local implementations
*********/

void *mymemset (void *s, int c, int n)
{
  const unsigned char uc = c;
  unsigned char *su;

  for (su= (unsigned char *) s; 0<n; ++su, --n)
    *su = uc;

  return (s);

}

/* string implementations from Plauger "The Standard C Library" */
int mystrlen (const char *s)
{
  const char *sc;
  
  for (sc=s; *sc != '\0'; ++sc);

  return (sc-s);
}

int mystrncmp (const char *s1, const char *s2, int n)
{
  for (; 0<n; ++s1, ++s2, --n)
    if (*s1 != *s2)
      return ((*(unsigned char *)s1 < *(unsigned char *)s2) ? -1: +1);
 else if (*s1 == '\0')
   return (0);
return (0);
}

int mystrcmp (const char *s1, const char *s2)
{
  for (; *s1 == *s2; ++s1, ++s2)
    {
    if (*s1 == '\0')
      { return (0); }
    }
  return ((*(unsigned char*)s1 < *(unsigned char *)s2) ? -1 : +1);
}

char *mystrcpy (char *s1, const char *s2)
{
  char *s=s1;
  
  for (s=s1; (*s++ = *s2++) != '\0'; )  ;
  return(s1);
}

/* needed for adobe_coeff, but not otherwise */
/* char *mystrprintf(char *sout, const char *s1, const char *s2) */
/* { */
/*   char *s=sout; */
  
/*   for (s=sout; (*s++ = *s1++) != '\0'; )  ; */
/*   *s++ = ' '; */
/*   for ( ; (*s++ == *s2++) != '\0'; ) ; */

/*   return(sout); */

/* } */


int myfgetc(char *buf)
{
  if (DATALOC >= DATALEN)
    return EOF;
char tmp;
int rettmp = 255;  /* tmp gets forced to 0xffffffxx */
tmp = *(buf+DATALOC);  ++DATALOC;
return  rettmp & tmp;

}

char *myfgets(char *s,  int num, char *buf)
{

     
     if (DATALOC >= DATALEN)
       {
	 printf("error in data read...exiting...\n"); exit(0);
       /* return EOF; */
       }


     int i;
     for (i=0; i<num; ++i)
       {
        *(s+i) = *(buf+DATALOC); ++DATALOC;
        if (  (*(s+i) == '\n') ||  (DATALOC>DATALEN))
               return s;

       }

     return s; 

}

/* really K&R's basic itoa() -- doesn't handle extremes so call with care */
/* needed for populating metadata structures */
/* void myreverse(char s[]) */
/*  { */
/*      int i, j; */
/*      char c; */
 
/*      for (i = 0, j = mystrlen(s)-1; i<j; i++, j--) { */
/*          c = s[i]; */
/*          s[i] = s[j]; */
/*          s[j] = c; */
/*      } */
/*  } */


/* void myitoa(int n, char s[]) */
/*  { */
/*      int i, sign; */
 
/*      if ((sign = n) < 0)  /\* record sign *\/ */
/*          n = -n;          /\* make n positive *\/ */
/*      i = 0; */
/*      do {       /\* generate digits in reverse order *\/ */
/*          s[i++] = n % 10 + '0';   /\* get next digit *\/ */
/*      } while ((n /= 10) > 0);     /\* delete it *\/ */
/*      if (sign < 0) */
/*          s[i++] = '-'; */
/*      s[i] = '\0'; */
/*      myreverse(s); */
/*  } */



/*
   All global variables are defined here, and all functions that
   access them are prefixed with "CLASS".  Note that a thread-safe
   C++ class cannot have non-const static local variables.
*/
FILE *ifp, *ofp;  /* for debugging */
short order;


/* for Bevara capabilities output */
int out_size;

char *meta_data, xtrans[6][6], xtrans_abs[6][6];
char cdesc[5], desc[512], make[64], model[64], model2[64], artist[64];
float flash_used, canon_ev, iso_speed, shutter, aperture, focal_len;

off_t strip_offset, data_offset;
off_t thumb_offset, meta_offset, profile_offset;
unsigned shot_order, /* kodak_cbpp, */ exif_cfa, unique_id;
unsigned thumb_length, meta_length, profile_length;
UINT32 thumb_misc, *oprof, fuji_layout, shot_select=0;
/* unsigned  multi_out=0; */
unsigned tiff_nifds, tiff_samples, tiff_bps, tiff_compress;
unsigned black, maximum, mix_green, raw_color/* , zero_is_bad */ ;
unsigned zero_after_ff, is_raw, dng_version, is_foveon, data_error;
unsigned tile_width, tile_length, gpsdata[32], load_flags;
unsigned flip, tiff_flip, filters, colors;
ushort raw_height, raw_width, height, width, top_margin, left_margin;
ushort shrink, iheight, iwidth, /* fuji_width, */ thumb_width, thumb_height;
ushort *raw_image, (*image)[4], cblack[4102];
ushort white[8][8], curve[0x10000], cr2_slice[3], sraw_mul[4];
double pixel_aspect, aber[4]={1,1,1,1}, gamm[6]={ 0.45,4.5,0,0,0,0 };
float bright=1, /* user_mul[4]={0,0,0,0}, */ threshold=0;
int mask[8][4];
int half_size=0, four_color_rgb=0, /* document_mode=0, */ highlight=0;


int /* use_auto_wb=0, */ /* use_camera_wb=0, */ use_camera_matrix=1;
int output_color=1, output_bps=8  /* , output_tiff=0 *//* , med_passes=0 */;
/* int no_auto_bright=0; */
/* unsigned greybox[4] = { 0, 0, UINT_MAX, UINT_MAX }; */
float cam_mul[4], pre_mul[4], cmatrix[3][4], rgb_cam[3][4];
const double xyz_rgb[3][3] = {			/* XYZ from RGB */
  { 0.412453, 0.357580, 0.180423 },
  { 0.212671, 0.715160, 0.072169 },
  { 0.019334, 0.119193, 0.950227 } };
const float d65_white[3] = { 0.950456, 1, 1.088754 };
int histogram[4][0x2000];

void (*load_raw)(char *buf), (*thumb_load_raw)(char *buf);
void (*load_thumb)(char *buf);



/* BEVARA: the TAG/VALUE storage is in here for debugging and also as a
   backup to any embedded XMP; add it back in if you want to see the 
   tags  */

/* #define MAXTAGNAMELEN 100  /\*max length of name describing TAG *\/ */
/* #define MAXTABLELEN 500  /\* max number of TAG entries we'd want to output to user *\/ */
/* int TAGTBLNUM; */
/* struct metaentry { */
/*   char tagname[MAXTAGNAMELEN]; */
/*   char *tagdata; */
/* } meta_table[MAXTABLELEN]; */


int parse_xmp(char *buf, int ifd_num)
{
  int UTF = 0;
  int saveDATALOC = DATALOC;
  long XMPLEN = 0;

	printf("XMP at IFD=%d\n",ifd_num);

  tiff_ifd[ifd_num].XMP_start = DATALOC;

  /*determine UTF type 8,16,32 (UTF-8, UTF-16/UCS-2, UTF-32/UCS-4*/
  if ( (*(buf+DATALOC) == 0x3C) && (*(buf+DATALOC+1) == 0x3F))
    {
      UTF = 8;
    }
  else if ((*(buf+DATALOC) == 0x3C) && (*(buf+DATALOC+1) == 0x00))
  {
    if (*(buf+DATALOC+1) == 0x3F)
      UTF = 16;
    else if ((*(buf+DATALOC+1) == 0x00)  && (*(buf+DATALOC+2) == 0x00)  && (*(buf+DATALOC+3) == 0x00) )
      UTF = 32;
    else 
      {
	return 0;
      }
  }
 else
   {
     printf("error parsing XMP\n");
     return 0;
   }



  if (UTF == 8)
    {
      while ( !((*(buf+DATALOC) == 'e') && (*(buf+DATALOC+1) == 'n') && (*(buf+DATALOC+2) == 'd')))
	{
	  /* printf("%c",*(buf+DATALOC)); */
	  ++DATALOC;
	  if (DATALOC > DATALEN)
	    {
	      DATALOC = saveDATALOC;
	      return 0;
	    }
	  ++XMPLEN;
	}
      while ( (*(buf+DATALOC) != '>') )
	{
	  ++DATALOC;
	  if (DATALOC > DATALEN)
	    {
	      DATALOC = saveDATALOC;
	      return 0;
	    }
	  ++XMPLEN;
	}
    }
  else if( (UTF ==16) || (UTF ==32))
    {
      printf("UTF-16/32 XMP not yet parsed. Contact Bevara for support.\n");
      return BEV_NOT_SUPPORTED;
    }


  tiff_ifd[ifd_num].XMP_stop = saveDATALOC+XMPLEN;
  
  // found a valid XMP, add to components
  component[numComponents].type = TYPE_XMP;
  component[numComponents].ifd_loc = ifd_num;
	++numComponents;
 
  return 1; /* success */
}



#define FORC(cnt) for (c=0; c < cnt; c++)
#define FORC3 FORC(3)
#define FORC4 FORC(4)
#define FORCC FORC(colors)

#define SQR(x) ((x)*(x))
/* BEVARA: comment out if using in a system that has these */
#define ABS(x) (((int)(x) ^ ((int)(x) >> 31)) - ((int)(x) >> 31))
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define LIM(x,min,max) MAX(min,MIN(x,max))
#define ULIM(x,y,z) ((y) < (z) ? LIM(x,y,z) : LIM(x,z,y))
#define CLIP(x) LIM((int)(x),0,65535)
#define SWAP(a,b) { a=a+b; b=a-b; a=a-b; }

/*
   In order to inline this calculation, I make the risky
   assumption that all filter patterns can be described
   by a repeating pattern of eight rows and two columns

   Do not use the FC or BAYER macros with the Leaf CatchLight,
   because its pattern is 16x16, not 2x8.

   Return values are either 0/1/2/3 = G/M/C/Y or 0/1/2/3 = R/G1/B/G2

	PowerShot 600	PowerShot A50	PowerShot Pro70	Pro90 & G1
	0xe1e4e1e4:	0x1b4e4b1e:	0x1e4b4e1b:	0xb4b4b4b4:

	  0 1 2 3 4 5	  0 1 2 3 4 5	  0 1 2 3 4 5	  0 1 2 3 4 5
	0 G M G M G M	0 C Y C Y C Y	0 Y C Y C Y C	0 G M G M G M
	1 C Y C Y C Y	1 M G M G M G	1 M G M G M G	1 Y C Y C Y C
	2 M G M G M G	2 Y C Y C Y C	2 C Y C Y C Y
	3 C Y C Y C Y	3 G M G M G M	3 G M G M G M
			4 C Y C Y C Y	4 Y C Y C Y C
	PowerShot A5	5 G M G M G M	5 G M G M G M
	0x1e4e1e4e:	6 Y C Y C Y C	6 C Y C Y C Y
			7 M G M G M G	7 M G M G M G
	  0 1 2 3 4 5
	0 C Y C Y C Y
	1 G M G M G M
	2 C Y C Y C Y
	3 M G M G M G

   All RGB cameras use one of these Bayer grids:

	0x16161616:	0x61616161:	0x49494949:	0x94949494:

	  0 1 2 3 4 5	  0 1 2 3 4 5	  0 1 2 3 4 5	  0 1 2 3 4 5
	0 B G B G B G	0 G R G R G R	0 G B G B G B	0 R G R G R G
	1 G R G R G R	1 B G B G B G	1 R G R G R G	1 G B G B G B
	2 B G B G B G	2 G R G R G R	2 G B G B G B	2 R G R G R G
	3 G R G R G R	3 B G B G B G	3 R G R G R G	3 G B G B G B
 */

#define RAW(row,col) \
	raw_image[(row)*raw_width+(col)]

#define FC(row,col) \
	(filters >> ((((row) << 1 & 14) + ((col) & 1)) << 1) & 3)

#define BAYER(row,col) \
	image[((row) >> shrink)*iwidth + ((col) >> shrink)][FC(row,col)]

#define BAYER2(row,col) \
	image[((row) >> shrink)*iwidth + ((col) >> shrink)][fcol(row,col)]

int   fcol (int row, int col)
{
  static const char filter[16][16] =
  { { 2,1,1,3,2,3,2,0,3,2,3,0,1,2,1,0 },
    { 0,3,0,2,0,1,3,1,0,1,1,2,0,3,3,2 },
    { 2,3,3,2,3,1,1,3,3,1,2,1,2,0,0,3 },
    { 0,1,0,1,0,2,0,2,2,0,3,0,1,3,2,1 },
    { 3,1,1,2,0,1,0,2,1,3,1,3,0,1,3,0 },
    { 2,0,0,3,3,2,3,1,2,0,2,0,3,2,2,1 },
    { 2,3,3,1,2,1,2,1,2,1,1,2,3,0,0,1 },
    { 1,0,0,2,3,0,0,3,0,3,0,3,2,1,2,3 },
    { 2,3,3,1,1,2,1,0,3,2,3,0,2,3,1,3 },
    { 1,0,2,0,3,0,3,2,0,1,1,2,0,1,0,2 },
    { 0,1,1,3,3,2,2,1,1,3,3,0,2,1,3,2 },
    { 2,3,2,0,0,1,3,0,2,0,1,2,3,0,1,0 },
    { 1,3,1,2,3,2,3,2,0,2,0,1,1,0,3,0 },
    { 0,2,0,3,1,0,0,1,1,3,3,2,3,2,2,1 },
    { 2,1,3,2,3,1,2,1,0,3,0,2,0,2,0,2 },
    { 0,3,1,0,0,2,0,3,2,1,3,1,1,3,1,3 } };


  if (filters == 1) return filter[(row+top_margin)&15][(col+left_margin)&15];
  if (filters == 9) return xtrans[(row+6) % 6][(col+6) % 6];

  return FC(row,col);
}


char *my_memmem (char *haystack, size_t haystacklen,
	      char *needle, size_t needlelen)
{
  char *c;
  for (c = haystack; c <= haystack + haystacklen - needlelen; c++)
    if (!memcmp (c, needle, needlelen))
      return c;
  return 0;
}
#define memmem my_memmem


static const short tolow_tab[257] = {-1,
 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
 0x40,  'a',  'b',  'c',  'd',  'e',  'f',  'g',
  'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o',
  'p',  'q',  'r',  's',  't',  'u',  'v',  'w',
  'x',  'y',  'z', 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
 0x60,  'a',  'b',  'c',  'd',  'e',  'f',  'g',
  'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o',
  'p',  'q',  'r',  's',  't',  'u',  'v',  'w',
  'x',  'y',  'z', 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
  0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff};

const short *_Tolower = &tolow_tab[1];


int my_tolower(int c)
{
	return (_Tolower[c]);

}

#define strncasecmp my_strncasecmp

int my_strncasecmp(const char *s1, const char *s2, int n)
{
  if (n == 0)
    return 0;

  while (n-- != 0 && (my_tolower(*s1) == my_tolower(*s2)))
    {
      if (n == 0 || *s1 == '\0' || *s2 == '\0')
    	  break;
      s1++;
      s2++;
    }

  return my_tolower(*(unsigned char *) s1) - my_tolower(*(unsigned char *) s2);
}




char *my_strcasestr (char *haystack, const char *needle)
{
  char *c;
  for (c = haystack; *c; c++)
    if (!strncasecmp(c, needle, mystrlen(needle)))
      return c;
  return 0;
}
#define strcasestr my_strcasestr


void   merror (void *ptr, const char *where)
{
  if (ptr) return;
  printf ("Out of memory in %s...exiting\n", where);
  exit(0);
}


ushort   sget2 (uchar *s)
{
  if (order == 0x4949)		/* "II" means little-endian */
    {
    return s[0] | s[1] << 8;
    }
  else				/* "MM" means big-endian */
    {
      return s[0] << 8 | s[1];
    }
}
unsigned   sget4 (uchar *s)
{
  if (order == 0x4949)
    return s[0] | s[1] << 8 | s[2] << 16 | s[3] << 24;
  else
    return s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3];
}
#define sget4(s) sget4((uchar *)s)


/* if data and system endian don't match, then switch */
ushort   myget2(char *inbuf)
{
  uchar str[2] = { 0xff,0xff };
  
  str[0] = (uchar) *(inbuf+DATALOC); ++DATALOC; 
  str[1] = (uchar) *(inbuf+DATALOC); ++DATALOC;
  return sget2(str);
}



unsigned   myget4(char *inbuf)
{
  uchar str[4] = { 0xff,0xff,0xff,0xff };
  /* fread (str, 1, 4, ifp); */
  str[0] = *(inbuf+DATALOC); ++DATALOC; 
  str[1] = *(inbuf+DATALOC); ++DATALOC;
  str[2] = *(inbuf+DATALOC); ++DATALOC; 
  str[3] = *(inbuf+DATALOC); ++DATALOC;
  return sget4(str);
}


unsigned   getint (int type, char *buf)
{
  return type == 3 ? myget2(buf) : myget4(buf);
}

float   int_to_float (int i)
{
  union { int i; float f; } u;
  u.i = i;
  return u.f;
}

double   getreal (int type, char *buf)
{
  union { char c[8]; double d; } u;
  int i, rev;

  switch (type) {
    case 3: return (unsigned short) myget2(buf);
    case 4: return (unsigned int) myget4(buf);
    case 5:  u.d = (unsigned int) myget4(buf);
      return u.d / (unsigned int) myget4(buf);
    case 8: return (signed short) myget2(buf);
    case 9: return (signed int) myget4(buf);
    case 10: u.d = (signed int) myget4(buf);
      return u.d / (signed int) myget4(buf);
    case 11: return int_to_float (myget4(buf));
    case 12:
      /* BEVARA: local impl of byte-swapping */
      /* order 0x4949 is little-endian */
      /* ntohs(0x1234) == 0x1234) checks for big-endian */
      /* rev = 7 * ((order == 0x4949) == (ntohs(0x1234) == 0x1234)); */
      rev = 7 * ((order == 0x4949) == (is_bigendian()));
      for (i=0; i < 8; i++)
	/* u.c[i ^ rev] = fgetc(ifp); */
	u.c[i ^ rev] = myfgetc(buf);
      return u.d;
    default: 
      /* return fgetc(ifp); */
      return myfgetc(buf);
  }
}

void   read_shorts (ushort *pixel, int count, char *buf)
{
  int i;
  
  for (i=0;i<count;++i)
    {
      *(pixel+i) = (*(buf + DATALOC) << 8) && *(buf+DATALOC+1);
      DATALOC = DATALOC +2;
      if (DATALOC > DATALEN)
	{
	  printf("reading a ushort, buffer exceeded...exiting....\n");
	  exit(0);

	}
    }

 
  /* BEVARA: local implementation of byte-swapping, ntohs is not ANSI */
  /* if ((order == 0x4949) == (ntohs(0x1234) == 0x1234)) */
  /*   swab (pixel, pixel, count*2); */
  ushort tmppixel;
  tmppixel = *pixel;
  if ((order == 0x4949) && is_bigendian())
    {
      printf("pixel before myswab 0x%04x, ",tmppixel);
      myswab(&tmppixel, pixel, count*2);
      printf("pixel after myswab 0x%04x, ",*pixel);
    }

  for (i=0;i<count;++i)
    printf("pixel after ushort= 0x%08x\n",*(pixel+i)); exit(0);
}






unsigned   getbithuff (int nbits, ushort *huff, char *buf)
{
  static unsigned bitbuf=0;
  static int vbits=0, reset=0;
  unsigned c;

  if (nbits > 25) return 0;
  if (nbits < 0)
    return bitbuf = vbits = reset = 0;
  if (nbits == 0 || vbits < 0) return 0;
  /* while (!reset && vbits < nbits && (c = fgetc(ifp)) != EOF && */
  /*   !(reset = zero_after_ff && c == 0xff && fgetc(ifp))) { */

  while (!reset && vbits < nbits && (c = myfgetc(buf)) != EOF &&
    !(reset = zero_after_ff && c == 0xff && myfgetc(buf))) {
    bitbuf = (bitbuf << 8) + (uchar) c;
    vbits += 8;
  }
  c = bitbuf << (32-vbits) >> (32-nbits);
  if (huff) {
    vbits -= huff[c] >> 8;
    c = (uchar) huff[c];
  } else
    vbits -= nbits;
  if (vbits < 0) 
    {
      printf("error in the data....exiting...\n"); exit(0);
    };
  return c;
}

/* #define getbits(n,buf) getbithuff(n,0,buf) */
#define gethuff(h,buf) getbithuff(*h,h+1,buf)

/*
   Construct a decode tree according the specification in *source.
   The first 16 bytes specify how many codes should be 1-bit, 2-bit
   3-bit, etc.  Bytes after that are the leaf values.

   For example, if the source is

    { 0,1,4,2,3,1,2,0,0,0,0,0,0,0,0,0,
      0x04,0x03,0x05,0x06,0x02,0x07,0x01,0x08,0x09,0x00,0x0a,0x0b,0xff  },

   then the code is

	00		0x04
	010		0x03
	011		0x05
	100		0x06
	101		0x02
	1100		0x07
	1101		0x01
	11100		0x08
	11101		0x09
	11110		0x00
	111110		0x0a
	1111110		0x0b
	1111111		0xff
 */
ushort *   make_decoder_ref (const uchar **source)
{
  int max, len, h, i, j;
  const uchar *count;
  ushort *huff;

  count = (*source += 16) - 17;
  for (max=16; max && !count[max]; max--);
  huff = (ushort *) calloc (1 + (1 << max), sizeof *huff);
  merror (huff, "make_decoder()");
  huff[0] = max;
  for (h=len=1; len <= max; len++)
    for (i=0; i < count[len]; i++, ++*source)
      for (j=0; j < 1 << (max-len); j++)
	if (h <= 1 << max)
	  huff[h++] = len << 8 | **source;
  return huff;
}

ushort *   make_decoder (const uchar *source)
{
  return make_decoder_ref (&source);
}


/*
   Return 0 if the image starts with compressed data,
   1 if it starts with uncompressed low-order bits.

   In Canon compressed data, 0xff is always followed by 0x00.
 */
/* int   canon_has_lowbits() */
/* { */
/*   printf("not supporting Canon at the moment...exiting...]n"); exit(0); */
/* } */

/* void   canon_load_raw() */
/* { */

/*   printf("raw Canon files not handled. Contact Bevara for support\n"); */
/*   exit(0); */

/* } */

struct jhead {
  int algo, bits, high, wide, clrs, sraw, psv, restart, vpred[6];
  ushort quant[64], idct[64], *huff[20], *free[20], *row;
};

int   ljpeg_start (struct jhead *jh, int info_only, char *buf)
{
  ushort c, tag, len;
  uchar data[0xA00];
  const uchar *dp;
  //int ii;

  mymemset (jh, 0, sizeof *jh);
  jh->restart = INT_MAX;

  if ((myfgetc(buf),myfgetc(buf)) != 0xd8)
    {
      printf("concat myfgetc checked, returning 0\n");
      return 0;
    }



  do {

    data[0]=*(buf+DATALOC); ++DATALOC;
    data[1]=*(buf+DATALOC); ++DATALOC;
    data[2]=*(buf+DATALOC); ++DATALOC; 
    data[3]=*(buf+DATALOC); ++DATALOC;

    if (DATALOC > DATALEN) 
      { 
	printf("buffer read error\n");
	return 0;
      }
    tag =  data[0] << 8 | data[1];
    /* printf("\t in ljpeg_start tag = 0x%04x ",tag); */
    len = (data[2] << 8 | data[3]) - 2;
    /* printf("\t len = 0x%04x\n",len); */
    if (tag <= 0xff00) return 0;

    int ii;

    for (ii=0;ii<len;++ii)
      {
	data[ii] = *(buf+DATALOC); ++DATALOC;
      }

    /* printf("tag in ljpeg_start = 0x%04x\n",tag); */

    switch (tag) {
      case 0xffc3:
	jh->sraw = ((data[7] >> 4) * (data[7] & 15) - 1) & 3;
      case 0xffc1:
      case 0xffc0:
	jh->algo = tag & 0xff;
	jh->bits = data[0];
	jh->high = data[1] << 8 | data[2];
	jh->wide = data[3] << 8 | data[4];
	jh->clrs = data[5] + jh->sraw;
	if (len == 9 && !dng_version) myfgetc(buf);
	break;
      case 0xffc4:
	if (info_only) break;
	/* printf("in c4....going to len = %d\n",len); */
	for (dp = data; dp < data+len && !((c = *dp++) & -20); )
	  jh->free[c] = jh->huff[c] = make_decoder_ref (&dp);
	break;
      case 0xffda:
	jh->psv = data[1+data[0]*2];
	jh->bits -= data[3+data[0]*2] & 15;
	break;
      case 0xffdb:
	FORC(64) jh->quant[c] = data[c*2+1] << 8 | data[c*2+2];
	break;
      case 0xffdd:
	jh->restart = data[0] << 8 | data[1];
    }
  } while (tag != 0xffda);
  if (info_only) return 1;
  if (jh->clrs > 6 || !jh->huff[0]) return 0;
  FORC(19) if (!jh->huff[c+1]) jh->huff[c+1] = jh->huff[c];
  if (jh->sraw) {
    FORC(4)        jh->huff[2+c] = jh->huff[1];
    FORC(jh->sraw) jh->huff[1+c] = jh->huff[0];
  }
  jh->row = (ushort *) calloc (jh->wide*jh->clrs, 4);
  merror (jh->row, "ljpeg_start()");
  return zero_after_ff = 1;
}

void   ljpeg_end (struct jhead *jh)
{
  int c;
  FORC4 if (jh->free[c]) free (jh->free[c]);
  free (jh->row);
}

int   ljpeg_diff (ushort *huff, char *buf)
{
  int len, diff;

  len = gethuff(huff,buf);
  if (len == 16 && (!dng_version || dng_version >= 0x1010000))
    return -32768;
  diff = getbithuff(len,0,buf);
  if ((diff & (1 << (len-1))) == 0)
    diff -= (1 << len) - 1;
  return diff;
}

ushort *   ljpeg_row (int jrow, struct jhead *jh, char *buf)
{
  int col, c, diff, pred, spred=0;
  ushort mark=0, *row[3];

  if (jrow * jh->wide % jh->restart == 0) {
    FORC(6) jh->vpred[c] = 1 << (jh->bits-1);
    if (jrow) {
      /* fseek (ifp, -2, SEEK_CUR); */
      DATALOC -= 2;
      if (DATALOC < 0) {printf("invalid loc ptr in ljpeg_row...exiting...\n"); exit(0);}

      /* do mark = (mark << 8) + (c = fgetc(ifp)); */
      do mark = (mark << 8) + (c = myfgetc(buf));

      while (c != EOF && mark >> 4 != 0xffd);
    }
    /* getbits(-1); */
    getbithuff(-1,0,buf);
  }
  FORC3 row[c] = jh->row + jh->wide*jh->clrs*((jrow+c) & 1);
  for (col=0; col < jh->wide; col++)
    FORC(jh->clrs) {
      diff = ljpeg_diff (jh->huff[c],buf);
      if (jh->sraw && c <= jh->sraw && (col | c))
		    pred = spred;
      else if (col) pred = row[0][-jh->clrs];
      else	    pred = (jh->vpred[c] += diff) - diff;
      if (jrow && col) switch (jh->psv) {
	case 1:	break;
	case 2: pred = row[1][0];					break;
	case 3: pred = row[1][-jh->clrs];				break;
	case 4: pred = pred +   row[1][0] - row[1][-jh->clrs];		break;
	case 5: pred = pred + ((row[1][0] - row[1][-jh->clrs]) >> 1);	break;
	case 6: pred = row[1][0] + ((pred - row[1][-jh->clrs]) >> 1);	break;
	case 7: pred = (pred + row[1][0]) >> 1;				break;
	default: pred = 0;
      }
      if ((**row = pred + diff) >> jh->bits)
	{
	  printf("error in the data...exiting...\n"); exit(0);
	}
      if (c <= jh->sraw) spred = **row;
      row[0]++; row[1]++;
    }
  return row[2];
}

void   lossless_jpeg_load_raw()
{

  printf("in lossless jpeg load raw....exiting...\n"); exit(0);
}


void   adobe_copy_pixel (unsigned row, unsigned col, ushort **rp)
{
  int c;

  if (tiff_samples == 2 && shot_select) (*rp)++;
  if (raw_image) {
    if (row < raw_height && col < raw_width)
      {

      RAW(row,col) = curve[**rp];
      }
    *rp += tiff_samples;
  } else {
    if (row < height && col < width)
      FORC(tiff_samples)
	image[row*width+col][c] = curve[(*rp)[c]];
    *rp += tiff_samples;
  }
  if (tiff_samples == 2 && shot_select) (*rp)--;
}

void   ljpeg_idct (struct jhead *jh, char *buf)
{
  int c, i, j, len, skip, coef;
  float work[3][8][8];
  static float cs[106] = { 0 };
  static const uchar zigzag[80] =
  {  0, 1, 8,16, 9, 2, 3,10,17,24,32,25,18,11, 4, 5,12,19,26,33,
    40,48,41,34,27,20,13, 6, 7,14,21,28,35,42,49,56,57,50,43,36,
    29,22,15,23,30,37,44,51,58,59,52,45,38,31,39,46,53,60,61,54,
    47,55,62,63,63,63,63,63,63,63,63,63,63,63,63,63,63,63,63,63 };

  if (!cs[0])
    FORC(106) cs[c] = cos((c & 31)*M_PI/16)/2;
  mymemset (work, 0, sizeof work);
  work[0][0][0] = jh->vpred[0] += ljpeg_diff (jh->huff[0],buf) * jh->quant[0];
  for (i=1; i < 64; i++ ) {
    len = gethuff (jh->huff[16],buf);
    i += skip = len >> 4;
    if (!(len &= 15) && skip < 15) break;
    /* coef = getbits(len); */
    coef = getbithuff(len,0,buf);
    if ((coef & (1 << (len-1))) == 0)
      coef -= (1 << len) - 1;
    ((float *)work)[zigzag[i]] = coef * jh->quant[i];
  }
  FORC(8) work[0][0][c] *= M_SQRT1_2;
  FORC(8) work[0][c][0] *= M_SQRT1_2;
  for (i=0; i < 8; i++)
    for (j=0; j < 8; j++)
      FORC(8) work[1][i][j] += work[0][i][c] * cs[(j*2+1)*c];
  for (i=0; i < 8; i++)
    for (j=0; j < 8; j++)
      FORC(8) work[2][i][j] += work[1][c][j] * cs[(i*2+1)*c];

  FORC(64) jh->idct[c] = CLIP(((float *)work[2])[c]+0.5);
}

void   lossless_dng_load_raw(char *buf)
{
  unsigned save, trow=0, tcol=0, jwide, jrow, jcol, row, col, i, j;
  struct jhead jh;
  ushort *rp;
  /* int ii,jj; */


  printf("in lossless dng load raw, trow = %d,raw_height=%d\n",trow,raw_height);
  printf("\t\t DATALOC = %li\n",DATALOC);

 

  while (trow < raw_height) {
   
    save = DATALOC;
    if (tile_length < INT_MAX) 
      {
      DATALOC =  myget4(buf);

      /* printf("\t\t\t tile location  for row %d is %d\n",trow,DATALOC); */
      }
    if (!ljpeg_start (&jh, 0, buf)) 
      {
    	printf("breaking in ljpeg_start check\n");
    	break;
      }
    jwide = jh.wide;
    if (filters) jwide *= jh.clrs;
    jwide /= MIN (is_raw, tiff_samples);
    switch (jh.algo) {
      case 0xc1:
	jh.vpred[0] = 16384;
	getbithuff(-1,0,buf);
	for (jrow=0; jrow+7 < jh.high; jrow += 8) {
	  for (jcol=0; jcol+7 < jh.wide; jcol += 8) {
	    ljpeg_idct (&jh,buf);
	    rp = jh.idct;
	    row = trow + jcol/tile_width + jrow*2;
	    col = tcol + jcol%tile_width;


	    for (i=0; i < 16; i+=2)
	      for (j=0; j < 8; j++)
		{
		adobe_copy_pixel (row+i, col+j, &rp);
       
		}

	  }
	}
	break;
      case 0xc3:
	for (row=col=jrow=0; jrow < jh.high; jrow++) {
	  rp = ljpeg_row (jrow, &jh, buf);
	  for (jcol=0; jcol < jwide; jcol++) {
	    adobe_copy_pixel (trow+row, tcol+col, &rp);
	    if (++col >= tile_width || col >= raw_width)
	      row += 1 + (col = 0);
	  }
	}
	break;
    default:
      break;
    }

 

    DATALOC =  save+4;

    if ((tcol += tile_width) >= raw_width)
      {
	trow += tile_length + (tcol = 0);
	/* printf("new trow = %d\n",trow); */
      }
    ljpeg_end (&jh);
  } /* end of loop over raw_height */


  

  printf("DATALOC at end of lossless_dng_load_raw = %ld\n",DATALOC);

}

void   packed_dng_load_raw(char *buf)
{
  ushort *pixel, *rp;
  int row, col;
  
  //printf("in packed dng load raw...exiting...\n"); exit(0);

  pixel = (ushort *) calloc (raw_width, tiff_samples*sizeof *pixel);
  /* merror (pixel, "packed_dng_load_raw()"); */
   for (row=0; row < raw_height; row++) {
     if (tiff_bps == 16)
       read_shorts (pixel, raw_width * tiff_samples, buf);
     else {
       /* getbits(-1); */
       getbithuff(-1,0,buf);
       for (col=0; col < raw_width * tiff_samples; col++)
   	/* pixel[col] = getbits(tiff_bps); */
   	pixel[col] = getbithuff(tiff_bps,0,buf);
     }
     for (rp=pixel, col=0; col < raw_width; col++)
       adobe_copy_pixel (row, col, &rp);
   }
   free (pixel);
}





/* int   raw (unsigned row, unsigned col) */
/* { */
/*   return (row < raw_height && col < raw_width) ? RAW(row,col) : 0; */
/* } */






/**** BEVARA - not handling full JPEG currently, add these fns back in if 
support needed ****/
/* void   kodak_jpeg_load_raw() {} */
/* void   lossy_dng_load_raw() {} */



void   crop_masked_pixels(char *buf)
{
  int row, col;
  unsigned /* r, */ c, m, mblack[8], zero, val;

  /* if (load_raw == &  phase_one_load_raw || */
  /*     load_raw == &  phase_one_load_raw_c) */
  /*   phase_one_correct(buf); */
  /* if (fuji_width) { */
  /*   for (row=0; row < raw_height-top_margin*2; row++) { */
  /*     for (col=0; col < fuji_width << !fuji_layout; col++) { */
  /* 	if (fuji_layout) { */
  /* 	  r = fuji_width - 1 - col + (row >> 1); */
  /* 	  c = col + ((row+1) >> 1); */
  /* 	} else { */
  /* 	  r = fuji_width - 1 + row - (col >> 1); */
  /* 	  c = row + ((col+1) >> 1); */
  /* 	} */
  /* 	if (r < height && c < width) */
  /* 	  BAYER(r,c) = RAW(row+top_margin,col+left_margin); */
  /*     } */
  /*   } */
  /* } else { */

    for (row=0; row < height; row++)
      {
      for (col=0; col < width; col++)
	{
	  BAYER2(row,col) = RAW(row+top_margin,col+left_margin);
	}
      }

  /* } */

  if (mask[0][3] > 0) goto mask_set;
  /* if (/\* (load_raw == &  canon_load_raw || *\/ */
  /*     load_raw == &  lossless_jpeg_load_raw) { */
  /*   mask[0][1] = mask[1][1] += 2; */
  /*   mask[0][3] -= 2; */
  /*   goto sides; */
  /* } */
 /*  if (/\* load_raw == &  canon_600_load_raw || *\/ */
/*       load_raw == &  sony_load_raw || */
/*      (load_raw == &  eight_bit_load_raw && mystrncmp(model,"DC2",3)) || */
/*       load_raw == &  kodak_262_load_raw || */
/*      (load_raw == &  packed_load_raw && (load_flags & 32))) { */
/* sides: */
/*     mask[0][0] = mask[1][0] = top_margin; */
/*     mask[0][2] = mask[1][2] = top_margin+height; */
/*     mask[0][3] += left_margin; */
/*     mask[1][1] += left_margin+width; */
/*     mask[1][3] += raw_width; */
/*   } */
/*   if (load_raw == &  nokia_load_raw) { */
/*     mask[0][2] = top_margin; */
/*     mask[0][3] = width; */
/*   } */
mask_set:
  mymemset (mblack, 0, sizeof mblack);
  for (zero=m=0; m < 8; m++)
    for (row=MAX(mask[m][0],0); row < MIN(mask[m][2],raw_height); row++)
      for (col=MAX(mask[m][1],0); col < MIN(mask[m][3],raw_width); col++) {
	c = FC(row-top_margin,col-left_margin);
	mblack[c] += val = RAW(row,col);
	mblack[4+c]++;
	zero += !val;
      }
  /* if (load_raw == &  canon_600_load_raw && width < raw_width) { */
  /*   black = (mblack[0]+mblack[1]+mblack[2]+mblack[3]) / */
  /* 	    (mblack[4]+mblack[5]+mblack[6]+mblack[7]) - 4; */
  /*   canon_600_correct(); */
  /* } else */ if (zero < mblack[4] && mblack[5] && mblack[6] && mblack[7]) {
    FORC4 cblack[c] = mblack[c] / mblack[4+c];
    cblack[4] = cblack[5] = cblack[6] = 0;
  }
}

/* void   remove_zeroes() */
/* { */
/*   unsigned row, col, tot, n, r, c; */


/*   for (row=0; row < height; row++) */
/*     for (col=0; col < width; col++) */
/*       if (BAYER(row,col) == 0) { */
/* 	tot = n = 0; */
/* 	for (r = row-2; r <= row+2; r++) */
/* 	  for (c = col-2; c <= col+2; c++) */
/* 	    if (r < height && c < width && */
/* 		FC(r,c) == FC(row,col) && BAYER(r,c)) */
/* 	      tot += (n++,BAYER(r,c)); */
/* 	if (n) BAYER(row,col) = tot/n; */
/*       } */
/* } */



void   gamma_curve (double pwr, double ts, int mode, int imax)
{
  int i;
  double g[6], bnd[2]={0,0}, r;

  g[0] = pwr;
  g[1] = ts;
  g[2] = g[3] = g[4] = 0;
  bnd[g[1] >= 1] = 1;
  if (g[1] && (g[1]-1)*(g[0]-1) <= 0) {
    for (i=0; i < 48; i++) {
      g[2] = (bnd[0] + bnd[1])/2;
      if (g[0]) bnd[(pow(g[2]/g[1],-g[0]) - 1)/g[0] - 1/g[2] > -1] = g[2];
      else	bnd[g[2]/exp(1-1/g[2]) < g[1]] = g[2];
    }
    g[3] = g[2] / g[1];
    if (g[0]) g[4] = g[2] * (1/g[0] - 1);
  }
  if (g[0]) g[5] = 1 / (g[1]*SQR(g[3])/2 - g[4]*(1 - g[3]) +
		(1 - pow(g[3],1+g[0]))*(1 + g[4])/(1 + g[0])) - 1;
  else      g[5] = 1 / (g[1]*SQR(g[3])/2 + 1
		- g[2] - g[3] -	g[2]*g[3]*(log(g[3]) - 1)) - 1;
  if (!mode--) {
    memcpy (gamm, g, sizeof gamm);
    return;
  }
  for (i=0; i < 0x10000; i++) {
    curve[i] = 0xffff;
    if ((r = (double) i / imax) < 1)
      curve[i] = 0x10000 * ( mode
	? (r < g[3] ? r*g[1] : (g[0] ? pow( r,g[0])*(1+g[4])-g[4]    : log(r)*g[2]+1))
	: (r < g[2] ? r/g[1] : (g[0] ? pow((r+g[4])/(1+g[4]),1/g[0]) : exp((r-1)/g[2]))));
  }
}

void   pseudoinverse (double (*in)[3], double (*out)[3], int size)
{
  double work[3][6], num;
  int i, j, k;

  for (i=0; i < 3; i++) {
    for (j=0; j < 6; j++)
      work[i][j] = j == i+3;
    for (j=0; j < 3; j++)
      for (k=0; k < size; k++)
	work[i][j] += in[k][i] * in[k][j];
  }
  for (i=0; i < 3; i++) {
    num = work[i][i];
    for (j=0; j < 6; j++)
      work[i][j] /= num;
    for (k=0; k < 3; k++) {
      if (k==i) continue;
      num = work[k][i];
      for (j=0; j < 6; j++)
	work[k][j] -= work[i][j] * num;
    }
  }
  for (i=0; i < size; i++)
    for (j=0; j < 3; j++)
      for (out[i][j]=k=0; k < 3; k++)
	out[i][j] += work[j][k+3] * in[i][k];
}

void   cam_xyz_coeff (float rgb_cam[3][4], double cam_xyz[4][3])
{
  double cam_rgb[4][3], inverse[4][3], num;
  int i, j, k;

  for (i=0; i < colors; i++)		/* Multiply out XYZ colorspace */
    for (j=0; j < 3; j++)
      for (cam_rgb[i][j] = k=0; k < 3; k++)
	cam_rgb[i][j] += cam_xyz[i][k] * xyz_rgb[k][j];

  for (i=0; i < colors; i++) {		/* Normalize cam_rgb so that */
    for (num=j=0; j < 3; j++)		/* cam_rgb * (1,1,1) is (1,1,1,1) */
      num += cam_rgb[i][j];
    for (j=0; j < 3; j++)
      cam_rgb[i][j] /= num;
    pre_mul[i] = 1 / num;
  }
  pseudoinverse (cam_rgb, inverse, colors);
  for (i=0; i < 3; i++)
    for (j=0; j < colors; j++)
      rgb_cam[i][j] = inverse[j][i];
}





void   scale_colors()
{
  unsigned /* bottom, right, */ size, row, col, ur, uc, i, /* x, y, */ c   /* , sum[8] */;
  int val, dark, sat;
  double /* dsum[8], */ dmin, dmax;
  float scale_mul[4], fr, fc;
  ushort *img=0, *pix;

 


/*   if (/\* use_auto_wb || *\/ (use_camera_wb && cam_mul[0] == -1)) { */

/*     printf("in use camera wb\n"); */

/*     mymemset (dsum, 0, sizeof dsum); */
/*     bottom = MIN (greybox[1]+greybox[3], height); */
/*     right  = MIN (greybox[0]+greybox[2], width); */
/*     for (row=greybox[1]; row < bottom; row += 8) */
/*       for (col=greybox[0]; col < right; col += 8) { */
/* 	mymemset (sum, 0, sizeof sum); */
/* 	for (y=row; y < row+8 && y < bottom; y++) */
/* 	  for (x=col; x < col+8 && x < right; x++) */
/* 	    FORC4 { */
/* 	      if (filters) { */
/* 		c = fcol(y,x); */
/* 		val = BAYER2(y,x); */
/* 	      } else */
/* 		val = image[y*width+x][c]; */
/* 	      if (val > maximum-25) goto skip_block; */
/* 	      if ((val -= cblack[c]) < 0) val = 0; */
/* 	      sum[c] += val; */
/* 	      sum[c+4]++; */
/* 	      if (filters) break; */
/* 	    } */
/* 	FORC(8) dsum[c] += sum[c]; */
/* skip_block: ; */
/*       } */
/*     FORC4 if (dsum[c]) pre_mul[c] = dsum[c+4] / dsum[c]; */
/*   } */


  /* if (use_camera_wb && cam_mul[0] != -1) { */
  /*   mymemset (sum, 0, sizeof sum); */
  /*   for (row=0; row < 8; row++) */
  /*     for (col=0; col < 8; col++) { */
  /* 	c = FC(row,col); */
  /* 	if ((val = white[row][col] - cblack[c]) > 0) */
  /* 	  sum[c] += val; */
  /* 	sum[c+4]++; */
  /*     } */
   
  /*   if (sum[0] && sum[1] && sum[2] && sum[3]) */
  /*     FORC4 pre_mul[c] = (float) sum[c+4] / sum[c]; */
  /*   else if (cam_mul[0] && cam_mul[2]) */
  /*     memcpy (pre_mul, cam_mul, sizeof pre_mul); */
  /*   else */
  /*     printf ("Cannot use camera white balance.\n"); */
  /* } */


   printf("intermed premul = ");
   for (i=0;i<4;++i) printf("%f  ",pre_mul[i]);
      printf("\n");


  if (pre_mul[1] == 0) pre_mul[1] = 1;
  if (pre_mul[3] == 0) pre_mul[3] = colors < 4 ? pre_mul[1] : 1;
  dark = black;
  sat = maximum;
  /* Bevara: no wavelet denoising supported */
  /* if (threshold) { */
  /*   printf("going to wavelet denoise\n"); */
  /*   wavelet_denoise(); */
  /* } */
  maximum -= black;
  for (dmin=DBL_MAX, dmax=c=0; c < 4; c++) {
    if (dmin > pre_mul[c])
	dmin = pre_mul[c];
    if (dmax < pre_mul[c])
	dmax = pre_mul[c];
  }
  if (!highlight) dmax = dmin;
  FORC4 scale_mul[c] = (pre_mul[c] /= dmax) * 65535.0 / maximum;
 
    //printf ("Scaling with darkness %d, saturation %d, and\nmultipliers", dark, sat);

 for (i=0;i<4;++i) printf("%f  ",pre_mul[i]);
      printf("\n");


   
    

  if (filters > 1000 && (cblack[4]+1)/2 == 1 && (cblack[5]+1)/2 == 1) {
    FORC4 cblack[FC(c/2,c%2)] +=
	cblack[6 + c/2 % cblack[4] * cblack[5] + c%2 % cblack[5]];
    cblack[4] = cblack[5] = 0;
  }
  size = iheight*iwidth;
  for (i=0; i < size*4; i++) {
    if (!(val = ((ushort *)image)[i])) continue;
    if (cblack[4] && cblack[5])
      val -= cblack[6 + i/4 / iwidth % cblack[4] * cblack[5] +
			i/4 % iwidth % cblack[5]];
    val -= cblack[i & 3];
    val *= scale_mul[i & 3];
    ((ushort *)image)[i] = CLIP(val);
  }
  if ((aber[0] != 1 || aber[2] != 1) && colors == 3) {
   
      printf ("Correcting chromatic aberration...\n");
    for (c=0; c < 4; c+=2) {
      if (aber[c] == 1) continue;
      img = (ushort *) malloc (size * sizeof *img);
      merror (img, "scale_colors()");
      for (i=0; i < size; i++)
	{
	img[i] = image[i][c];
	/* printf("image i=%d, val = %d\n",i,img[i]); */
	}

      for (row=0; row < iheight; row++) {
	ur = fr = (row - iheight*0.5) * aber[c] + iheight*0.5;
	if (ur > iheight-2) continue;
	fr -= ur;
	for (col=0; col < iwidth; col++) {
	  uc = fc = (col - iwidth*0.5) * aber[c] + iwidth*0.5;
	  if (uc > iwidth-2) continue;
	  fc -= uc;
	  pix = img + ur*iwidth + uc;
	  image[row*iwidth+col][c] =
	    (pix[     0]*(1-fc) + pix[       1]*fc) * (1-fr) +
	    (pix[iwidth]*(1-fc) + pix[iwidth+1]*fc) * fr;
	}
      }
      free(img);
    }
  }
}

void   pre_interpolate()
{
  ushort (*img)[4];
  int row, col, c;

  if (shrink) {
    if (half_size) {
      height = iheight;
      width  = iwidth;
      if (filters == 9) {
	for (row=0; row < 3; row++)
	  for (col=1; col < 4; col++)
	    if (!(image[row*width+col][0] | image[row*width+col][2]))
	      goto break2;  break2:
	for ( ; row < height; row+=3)
	  for (col=(col-1)%3+1; col < width-1; col+=3) {
	    img = image + row*width+col;
	    for (c=0; c < 3; c+=2)
	      img[0][c] = (img[-1][c] + img[1][c]) >> 1;
	  }
      }
    } else {
      img = (ushort (*)[4]) calloc (height, width*sizeof *img);
      merror (img, "pre_interpolate()");
      for (row=0; row < height; row++)
	for (col=0; col < width; col++) {
	  c = fcol(row,col);
	  img[row*width+col][c] = image[(row >> 1)*iwidth+(col >> 1)][c];
	}
      free (image);
      image = img;
      shrink = 0;
    }
  }
  if (filters > 1000 && colors == 3) {
    mix_green = four_color_rgb ^ half_size;
    if (four_color_rgb | half_size) colors++;
    else {
      for (row = FC(1,0) >> 1; row < height; row+=2)
	for (col = FC(row,1) & 1; col < width; col+=2)
	  image[row*width+col][1] = image[row*width+col][3];
     
      filters &= ~((filters & 0x55555555) << 1);
           
    }
  }
  if (half_size) filters = 0;
}

void   border_interpolate (int border)
{
  unsigned row, col, y, x, f, c, sum[8];

  for (row=0; row < height; row++)
    for (col=0; col < width; col++) {
      if (col==border && row >= border && row < height-border)
	col = width-border;
      mymemset (sum, 0, sizeof sum);
      for (y=row-1; y != row+2; y++)
	for (x=col-1; x != col+2; x++)
	  if (y < height && x < width) {
	    f = fcol(y,x);
	    sum[f] += image[y*width+x][f];
	    sum[f+4]++;
	  }
      f = fcol(row,col);
      FORCC if (c != f && sum[c+4])
	image[row*width+col][c] = sum[c] / sum[c+4];
    }
}




void   cielab (ushort rgb[3], short lab[3])
{
  int c, i, j, k;
  float r, xyz[3];
  static float cbrt[0x10000], xyz_cam[3][4];

  if (!rgb) {
    for (i=0; i < 0x10000; i++) {
      r = i / 65535.0;
      cbrt[i] = r > 0.008856 ? pow(r,1/3.0) : 7.787*r + 16/116.0;
    }
    for (i=0; i < 3; i++)
      for (j=0; j < colors; j++)
	for (xyz_cam[i][j] = k=0; k < 3; k++)
	  xyz_cam[i][j] += xyz_rgb[i][k] * rgb_cam[k][j] / d65_white[i];
    return;
  }
  xyz[0] = xyz[1] = xyz[2] = 0.5;
  FORCC {
    xyz[0] += xyz_cam[0][c] * rgb[c];
    xyz[1] += xyz_cam[1][c] * rgb[c];
    xyz[2] += xyz_cam[2][c] * rgb[c];
  }
  xyz[0] = cbrt[CLIP((int) xyz[0])];
  xyz[1] = cbrt[CLIP((int) xyz[1])];
  xyz[2] = cbrt[CLIP((int) xyz[2])];
  lab[0] = 64 * (116 * xyz[1] - 16);
  lab[1] = 64 * 500 * (xyz[0] - xyz[1]);
  lab[2] = 64 * 200 * (xyz[1] - xyz[2]);
}



#define TS 512		/* Tile Size */

/*
   Adaptive Homogeneity-Directed interpolation is based on
   the work of Keigo Hirakawa, Thomas Parks, and Paul Lee.
 */
void   ahd_interpolate()
{
  int i, j, top, left, row, col, tr, tc, c, d, val, hm[2];
  static const int dir[4] = { -1, 1, -TS, TS };
  unsigned ldiff[2][4], abdiff[2][4], leps, abeps;
  ushort (*rgb)[TS][TS][3], (*rix)[3], (*pix)[4];
   short (*lab)[TS][TS][3], (*lix)[3];
   char (*homo)[TS][TS], *buffer;


  cielab (0,0);
  border_interpolate(5);
  buffer = (char *) malloc (26*TS*TS);
  merror (buffer, "ahd_interpolate()");
  rgb  = (ushort(*)[TS][TS][3]) buffer;
  lab  = (short (*)[TS][TS][3])(buffer + 12*TS*TS);
  homo = (char  (*)[TS][TS])   (buffer + 24*TS*TS);


  for (top=2; top < height-5; top += TS-6)
    for (left=2; left < width-5; left += TS-6) {

/*  Interpolate green horizontally and vertically:		*/
      for (row=top; row < top+TS && row < height-2; row++) {
	col = left + (FC(row,left) & 1);

	for (c = FC(row,col); col < left+TS && col < width-2; col+=2) {
	  pix = image + row*width+col;
	 
	  val = ((pix[-1][1] + pix[0][c] + pix[1][1]) * 2
		- pix[-2][c] - pix[2][c]) >> 2;
	  rgb[0][row-top][col-left][1] = ULIM(val,pix[-1][1],pix[1][1]);
	  val = ((pix[-width][1] + pix[0][c] + pix[width][1]) * 2
		- pix[-2*width][c] - pix[2*width][c]) >> 2;
	  rgb[1][row-top][col-left][1] = ULIM(val,pix[-width][1],pix[width][1]);
	}
      }
/*  Interpolate red and blue, and convert to CIELab:		*/
      for (d=0; d < 2; d++)
	for (row=top+1; row < top+TS-1 && row < height-3; row++)
	  for (col=left+1; col < left+TS-1 && col < width-3; col++) {
	    pix = image + row*width+col;
	    rix = &rgb[d][row-top][col-left];
	    lix = &lab[d][row-top][col-left];
	    if ((c = 2 - FC(row,col)) == 1) {
	      c = FC(row+1,col);
	      val = pix[0][1] + (( pix[-1][2-c] + pix[1][2-c]
				 - rix[-1][1] - rix[1][1] ) >> 1);
	      rix[0][2-c] = CLIP(val);
	      val = pix[0][1] + (( pix[-width][c] + pix[width][c]
				 - rix[-TS][1] - rix[TS][1] ) >> 1);
	    } else
	      val = rix[0][1] + (( pix[-width-1][c] + pix[-width+1][c]
				 + pix[+width-1][c] + pix[+width+1][c]
				 - rix[-TS-1][1] - rix[-TS+1][1]
				 - rix[+TS-1][1] - rix[+TS+1][1] + 1) >> 2);
	    rix[0][c] = CLIP(val);
	    c = FC(row,col);
	    rix[0][c] = pix[0][c];
	    cielab (rix[0],lix[0]);
	  }
/*  Build homogeneity maps from the CIELab images:		*/
      mymemset (homo, 0, 2*TS*TS);
      for (row=top+2; row < top+TS-2 && row < height-4; row++) {
	tr = row-top;
	for (col=left+2; col < left+TS-2 && col < width-4; col++) {
	  tc = col-left;
	  for (d=0; d < 2; d++) {
	    lix = &lab[d][tr][tc];
	    for (i=0; i < 4; i++) {
	       ldiff[d][i] = ABS(lix[0][0]-lix[dir[i]][0]);
	      abdiff[d][i] = SQR(lix[0][1]-lix[dir[i]][1])
			   + SQR(lix[0][2]-lix[dir[i]][2]);
	    }
	  }
	  leps = MIN(MAX(ldiff[0][0],ldiff[0][1]),
		     MAX(ldiff[1][2],ldiff[1][3]));
	  abeps = MIN(MAX(abdiff[0][0],abdiff[0][1]),
		      MAX(abdiff[1][2],abdiff[1][3]));
	  for (d=0; d < 2; d++)
	    for (i=0; i < 4; i++)
	      if (ldiff[d][i] <= leps && abdiff[d][i] <= abeps)
		homo[d][tr][tc]++;
	}
      }
/*  Combine the most homogenous pixels for the final result:	*/
      for (row=top+3; row < top+TS-3 && row < height-5; row++) {
	tr = row-top;
	for (col=left+3; col < left+TS-3 && col < width-5; col++) {
	  tc = col-left;
	  for (d=0; d < 2; d++)
	    for (hm[d]=0, i=tr-1; i <= tr+1; i++)
	      for (j=tc-1; j <= tc+1; j++)
		hm[d] += homo[d][i][j];
	  if (hm[0] != hm[1])
	    FORC3 image[row*width+col][c] = rgb[hm[1] > hm[0]][tr][tc][c];
	  else
	    FORC3 image[row*width+col][c] =
		(rgb[0][tr][tc][c] + rgb[1][tr][tc][c]) >> 1;
	}
      }
    }

 
  free (buffer);
}
#undef TS



void   tiff_get (unsigned int base,
		     unsigned int *tag, unsigned int *type, unsigned int *len, unsigned int *save, char* buf)
{
  
  
  *tag  = myget2(buf);
  *type = myget2(buf);
  *len  = myget4(buf);


  *save = /*ftell(ifp) + 4;*/ DATALOC+4;


  /* printf("in tiff_get tag = 0x%08x, type = %d, len = %d, save = %d\n",*tag,*type,*len,*save); */

  if (*len * ("11124811248484"[*type < 14 ? *type:0]-'0') > 4)
    {
     
    /* fseek (ifp, myget4(buf)+base, SEEK_SET); */
    DATALOC = myget4(buf)+base;
    /* printf("in len check set loc to %d\n",DATALOC); */
    }
}

void   parse_thumb_note (int base, unsigned toff, unsigned tlen, char *buf)
{
  unsigned entries, tag, type, len, save;


  entries = myget2(buf);
  while (entries--) {
    tiff_get (base, &tag, &type, &len, &save, buf);
    if (tag == toff) thumb_offset = myget4(buf)+base;
    if (tag == tlen) thumb_length = myget4(buf);
    //fseek (ifp, save, SEEK_SET);
	DATALOC = save;
  }
}

int myisdigit(char c)
{
  if ( c=='0' || c=='1' || c== '2' || c== '3' || c=='4' || c=='5' || c=='6' || c=='7' || c=='8' ||c=='9')
    return 1;
  return 0;
}

int   parse_tiff_ifd (int base, char *buf);

void   parse_makernote (int base, int uptag, char *buf)
{
    static const uchar xlat[2][256] = {
  { 0xc1,0xbf,0x6d,0x0d,0x59,0xc5,0x13,0x9d,0x83,0x61,0x6b,0x4f,0xc7,0x7f,0x3d,0x3d,
    0x53,0x59,0xe3,0xc7,0xe9,0x2f,0x95,0xa7,0x95,0x1f,0xdf,0x7f,0x2b,0x29,0xc7,0x0d,
    0xdf,0x07,0xef,0x71,0x89,0x3d,0x13,0x3d,0x3b,0x13,0xfb,0x0d,0x89,0xc1,0x65,0x1f,
    0xb3,0x0d,0x6b,0x29,0xe3,0xfb,0xef,0xa3,0x6b,0x47,0x7f,0x95,0x35,0xa7,0x47,0x4f,
    0xc7,0xf1,0x59,0x95,0x35,0x11,0x29,0x61,0xf1,0x3d,0xb3,0x2b,0x0d,0x43,0x89,0xc1,
    0x9d,0x9d,0x89,0x65,0xf1,0xe9,0xdf,0xbf,0x3d,0x7f,0x53,0x97,0xe5,0xe9,0x95,0x17,
    0x1d,0x3d,0x8b,0xfb,0xc7,0xe3,0x67,0xa7,0x07,0xf1,0x71,0xa7,0x53,0xb5,0x29,0x89,
    0xe5,0x2b,0xa7,0x17,0x29,0xe9,0x4f,0xc5,0x65,0x6d,0x6b,0xef,0x0d,0x89,0x49,0x2f,
    0xb3,0x43,0x53,0x65,0x1d,0x49,0xa3,0x13,0x89,0x59,0xef,0x6b,0xef,0x65,0x1d,0x0b,
    0x59,0x13,0xe3,0x4f,0x9d,0xb3,0x29,0x43,0x2b,0x07,0x1d,0x95,0x59,0x59,0x47,0xfb,
    0xe5,0xe9,0x61,0x47,0x2f,0x35,0x7f,0x17,0x7f,0xef,0x7f,0x95,0x95,0x71,0xd3,0xa3,
    0x0b,0x71,0xa3,0xad,0x0b,0x3b,0xb5,0xfb,0xa3,0xbf,0x4f,0x83,0x1d,0xad,0xe9,0x2f,
    0x71,0x65,0xa3,0xe5,0x07,0x35,0x3d,0x0d,0xb5,0xe9,0xe5,0x47,0x3b,0x9d,0xef,0x35,
    0xa3,0xbf,0xb3,0xdf,0x53,0xd3,0x97,0x53,0x49,0x71,0x07,0x35,0x61,0x71,0x2f,0x43,
    0x2f,0x11,0xdf,0x17,0x97,0xfb,0x95,0x3b,0x7f,0x6b,0xd3,0x25,0xbf,0xad,0xc7,0xc5,
    0xc5,0xb5,0x8b,0xef,0x2f,0xd3,0x07,0x6b,0x25,0x49,0x95,0x25,0x49,0x6d,0x71,0xc7 },
  { 0xa7,0xbc,0xc9,0xad,0x91,0xdf,0x85,0xe5,0xd4,0x78,0xd5,0x17,0x46,0x7c,0x29,0x4c,
    0x4d,0x03,0xe9,0x25,0x68,0x11,0x86,0xb3,0xbd,0xf7,0x6f,0x61,0x22,0xa2,0x26,0x34,
    0x2a,0xbe,0x1e,0x46,0x14,0x68,0x9d,0x44,0x18,0xc2,0x40,0xf4,0x7e,0x5f,0x1b,0xad,
    0x0b,0x94,0xb6,0x67,0xb4,0x0b,0xe1,0xea,0x95,0x9c,0x66,0xdc,0xe7,0x5d,0x6c,0x05,
    0xda,0xd5,0xdf,0x7a,0xef,0xf6,0xdb,0x1f,0x82,0x4c,0xc0,0x68,0x47,0xa1,0xbd,0xee,
    0x39,0x50,0x56,0x4a,0xdd,0xdf,0xa5,0xf8,0xc6,0xda,0xca,0x90,0xca,0x01,0x42,0x9d,
    0x8b,0x0c,0x73,0x43,0x75,0x05,0x94,0xde,0x24,0xb3,0x80,0x34,0xe5,0x2c,0xdc,0x9b,
    0x3f,0xca,0x33,0x45,0xd0,0xdb,0x5f,0xf5,0x52,0xc3,0x21,0xda,0xe2,0x22,0x72,0x6b,
    0x3e,0xd0,0x5b,0xa8,0x87,0x8c,0x06,0x5d,0x0f,0xdd,0x09,0x19,0x93,0xd0,0xb9,0xfc,
    0x8b,0x0f,0x84,0x60,0x33,0x1c,0x9b,0x45,0xf1,0xf0,0xa3,0x94,0x3a,0x12,0x77,0x33,
    0x4d,0x44,0x78,0x28,0x3c,0x9e,0xfd,0x65,0x57,0x16,0x94,0x6b,0xfb,0x59,0xd0,0xc8,
    0x22,0x36,0xdb,0xd2,0x63,0x98,0x43,0xa1,0x04,0x87,0x86,0xf7,0xa6,0x26,0xbb,0xd6,
    0x59,0x4d,0xbf,0x6a,0x2e,0xaa,0x2b,0xef,0xe6,0x78,0xb6,0x4e,0xe0,0x2f,0xdc,0x7c,
    0xbe,0x57,0x19,0x32,0x7e,0x2a,0xd0,0xb8,0xba,0x29,0x00,0x3c,0x52,0x7d,0xa8,0x49,
    0x3b,0x2d,0xeb,0x25,0x49,0xfa,0xa3,0xaa,0x39,0xa7,0xc5,0xa7,0x50,0x11,0x36,0xfb,
    0xc6,0x67,0x4a,0xf5,0xa5,0x12,0x65,0x7e,0xb0,0xdf,0xaf,0x4e,0xb3,0x61,0x7f,0x2f } };
  unsigned offset=0, entries, tag, type, len, save, c;
  unsigned ver97=0, serial=0, i, wbi=0, wb[4]={0,0,0,0};
  uchar buf97[324], ci, cj, ck;
  short morder, sorder=order;
  char locbuf[10];
  int iii;
/*
   The MakerNote might have its own TIFF header (possibly with
   its own byte-order!), or it might just be a table.
 */
  if (!mystrcmp(make,"Nokia")) return;
  //fread (buf, 1, 10, ifp);
  for (iii=0; iii<10; ++iii)
	{
		*(locbuf+iii) = *(buf+DATALOC); ++DATALOC;
		if (DATALOC > DATALEN)
		{
			printf("buffer length exceeded...exiting...."); exit(0);
		}
	}
  
  if (!mystrncmp (buf,"KDK" ,3) ||	/* these aren't TIFF tables */
      !mystrncmp (buf,"VER" ,3) ||
      !mystrncmp (buf,"IIII",4) ||
      !mystrncmp (buf,"MMMM",4)) return;
  if (!mystrncmp (buf,"KC"  ,2) ||	/* Konica KD-400Z, KD-510Z */
      !mystrncmp (buf,"MLY" ,3)) {	/* Minolta DiMAGE G series */
    order = 0x4d4d;
	
    //while ((i=ftell(ifp)) < data_offset && i < 16384) {
	while ((i=DATALOC) < data_offset && i < 16384) {
      wb[0] = wb[2];  wb[2] = wb[1];  wb[1] = wb[3];
      //wb[3] = get2();
	  wb[3] = myget2(buf);
      if (wb[1] == 256 && wb[3] == 256 &&
	  wb[0] > 256 && wb[0] < 640 && wb[2] > 256 && wb[2] < 640)
	FORC4 cam_mul[c] = wb[c];
    }
    goto quit;
  }
  if (!mystrcmp (buf,"Nikon")) {
    base = DATALOC; // ftell(ifp);
    order = myget2(buf); //get2();
    //if (get2() != 42) goto quit;
	if (myget2(buf) != 42) goto quit;
    offset = myget4(buf); //get4();
    //fseek (ifp, offset-8, SEEK_CUR);
	DATALOC += offset-8;
  } else if (!mystrcmp (buf,"OLYMPUS") ||
             !mystrcmp (buf,"PENTAX ")) {
    //base = ftell(ifp)-10;
	base = DATALOC -10;
    // fseek (ifp, -2, SEEK_CUR);
	DATALOC -= 2;
    order = myget2(buf);
    if (buf[0] == 'O') myget2(buf);
  } else if (!mystrncmp (buf,"SONY",4) ||
	     !mystrcmp  (buf,"Panasonic")) {
    goto nf;
  } else if (!mystrncmp (buf,"FUJIFILM",8)) {
    //base = ftell(ifp)-10;
	base = DATALOC -10;
nf: order = 0x4949;
    //fseek (ifp,  2, SEEK_CUR);
	DATALOC += 2;
  } else if (!mystrcmp (buf,"OLYMP") ||
	     !mystrcmp (buf,"LEICA") ||
	     !mystrcmp (buf,"Ricoh") ||
	     !mystrcmp (buf,"EPSON"))
    //fseek (ifp, -2, SEEK_CUR);
	DATALOC -= 2;
  else if (!mystrcmp (buf,"AOC") ||
	   !mystrcmp (buf,"QVC"))
    //fseek (ifp, -4, SEEK_CUR);
	DATALOC -=4;
  else {
    //fseek (ifp, -10, SEEK_CUR);
	DATALOC -=10;
    if (!mystrncmp(make,"SAMSUNG",7))
      //base = ftell(ifp);
		base = DATALOC;
  }
  entries = myget2(buf);
  if (entries > 1000) return;
  morder = order;
  while (entries--) {
    order = morder;
    tiff_get (base, &tag, &type, &len, &save, buf);
    tag |= uptag << 16;
    if (tag == 2 && strstr(make,"NIKON") && !iso_speed)
      iso_speed = (myget2(buf),myget2(buf));
    if (tag == 4 && len > 26 && len < 35) {
      if ((i=(myget4(buf),myget2(buf))) != 0x7fff && !iso_speed)
	iso_speed = 50 * pow (2, i/32.0 - 4);
      if ((i=(myget2(buf),myget2(buf))) != 0x7fff && !aperture)
	aperture = pow (2, i/64.0);
      if ((i=myget2(buf)) != 0xffff && !shutter)
	shutter = pow (2, (short) i/-32.0);
      wbi = (myget2(buf),myget2(buf));
      shot_order = (myget2(buf),myget2(buf));
    }
    if ((tag == 4 || tag == 0x114) && !mystrncmp(make,"KONICA",6)) {
      //fseek (ifp, tag == 4 ? 140:160, SEEK_CUR);
	  if (tag == 4)
		  DATALOC +=140;
	  else DATALOC += 160;
      switch (myget2(buf)) {
	case 72:  flip = 0;  break;
	case 76:  flip = 6;  break;
	case 82:  flip = 5;  break;
      }
    }
    if (tag == 7 && type == 2 && len > 20)
      //fgets (model2, 64, ifp);
		myfgets(model2, 64, buf);
    if (tag == 8 && type == 4)
      shot_order = myget4(buf);
    if (tag == 9 && !mystrcmp(make,"Canon"))
      //fread (artist, 64, 1, ifp);
	  {
		for (iii=0; iii<64; ++iii)
		{
			*(artist+iii) = *(buf+DATALOC); ++DATALOC;
			if (DATALOC > DATALEN)
			{
				printf("buffer length exceeded...exiting...."); exit(0);
			}
		}	
	  }
    if (tag == 0xc && len == 4)
      FORC3 cam_mul[(c << 1 | c >> 1) & 3] = getreal(type,buf);
    if (tag == 0xd && type == 7 && myget2(buf) == 0xaaaa) {
      for (c=i=2; (ushort) c != 0xbbbb && i < len; i++)
	c = c << 8 | myfgetc(buf);
      while ((i+=4) < len-5)
	if (myget4(buf) == 257 && (i=len) && (c = (myget4(buf),myfgetc(buf))) < 3)
	  flip = "065"[c]-'0';
    }
    if (tag == 0x10 && type == 4)
      unique_id = myget4(buf);
    if (tag == 0x11 && is_raw && !mystrncmp(make,"NIKON",5)) {
      //fseek (ifp, get4()+base, SEEK_SET);
	  DATALOC = myget4(buf)+base;
      parse_tiff_ifd (base,buf);
    }
    if (tag == 0x14 && type == 7) {
      if (len == 2560) {
	//fseek (ifp, 1248, SEEK_CUR);
	DATALOC += 1248;
	goto get2_256;
      }
      //fread (buf, 1, 10, ifp);
	  for (iii=0; iii<10; ++iii)
		{
			*(locbuf+iii) = *(buf+DATALOC); ++DATALOC;
			if (DATALOC > DATALEN)
			{
				printf("buffer length exceeded...exiting...."); exit(0);
			}
		}	
	  
      if (!mystrncmp(locbuf,"NRW ",4)) {
	//fseek (ifp, strcmp(buf+4,"0100") ? 46:1546, SEEK_CUR);
	if (mystrcmp(locbuf+4,"0100"))
		DATALOC +=4;
	else DATALOC += 1546;
	cam_mul[0] = myget4(buf) << 2;
	cam_mul[1] = myget4(buf) + myget4(buf);
	cam_mul[2] = myget4(buf) << 2;
      }
    }
    if (tag == 0x15 && type == 2 && is_raw)
      //fread (model, 64, 1, ifp);
		for (iii=0; iii<64; ++iii)
		{
			*(model+iii) = *(buf+DATALOC); ++DATALOC;
			if (DATALOC > DATALEN)
			{
				printf("buffer length exceeded...exiting...."); exit(0);
			}
		}	
    if (strstr(make,"PENTAX")) {
      if (tag == 0x1b) tag = 0x1018;
      if (tag == 0x1c) tag = 0x1017;
    }
    if (tag == 0x1d)
      while ((c = myfgetc(buf)) && c != EOF)
	serial = serial*10 + (myisdigit(c) ? c - '0' : c % 10);
    if (tag == 0x29 && type == 1) {
      c = wbi < 18 ? "012347800000005896"[wbi]-'0' : 0;
      //fseek (ifp, 8 + c*32, SEEK_CUR);
	  DATALOC += 8 + c*32;
      FORC4 cam_mul[c ^ (c >> 1) ^ 1] = myget4(buf);
    }
    if (tag == 0x3d && type == 3 && len == 4)
      FORC4 cblack[c ^ c >> 1] = myget2(buf) >> (14-tiff_ifd[2].bps);
    if (tag == 0x81 && type == 4) {
      data_offset = myget4(buf);
      //fseek (ifp, data_offset + 41, SEEK_SET);
	  DATALOC = data_offset + 41;
      raw_height = myget2(buf) * 2;
      raw_width  = myget2(buf);
      filters = 0x61616161;
    }
    if ((tag == 0x81  && type == 7) ||
	(tag == 0x100 && type == 7) ||
	(tag == 0x280 && type == 1)) {
      thumb_offset = DATALOC; //ftell(ifp);
      thumb_length = len;
    }
    if (tag == 0x88 && type == 4 && (thumb_offset = myget4(buf)))
      thumb_offset += base;
    if (tag == 0x89 && type == 4)
      thumb_length = myget4(buf);
    if (tag == 0x8c || tag == 0x96)
      meta_offset = DATALOC; //ftell(ifp);
    if (tag == 0x97) {
      for (i=0; i < 4; i++)
	ver97 = ver97 * 10 + myfgetc(buf)-'0';
      switch (ver97) {
	case 100:
	  //fseek (ifp, 68, SEEK_CUR);
	  DATALOC += 68;
	  FORC4 cam_mul[(c >> 1) | ((c & 1) << 1)] = myget2(buf);
	  break;
	case 102:
	  //fseek (ifp, 6, SEEK_CUR);
	  DATALOC += 6;
	  FORC4 cam_mul[c ^ (c >> 1)] = myget2(buf);
	  break;
	case 103:
	  //fseek (ifp, 16, SEEK_CUR);
	  DATALOC += 16;
	  FORC4 cam_mul[c] = myget2(buf);
      }
      if (ver97 >= 200) {
	if (ver97 != 205) //fseek (ifp, 280, SEEK_CUR);
		DATALOC += 280;
	//fread (buf97, 324, 1, ifp);
		for (iii=0; iii<324; ++iii)
		{
			*(buf97+iii) = *(buf+DATALOC); ++DATALOC;
			if (DATALOC > DATALEN)
			{
				printf("buffer length exceeded...exiting...."); exit(0);
			}
		}	
      }
    }
    if (tag == 0xa1 && type == 7) {
      order = 0x4949;
      //fseek (ifp, 140, SEEK_CUR);
	  DATALOC += 140;
      FORC3 cam_mul[c] = myget4(buf);
    }
    if (tag == 0xa4 && type == 3) {
      //fseek (ifp, wbi*48, SEEK_CUR);
	  DATALOC += wbi*48;
      FORC3 cam_mul[c] = myget2(buf);
    }
    if (tag == 0xa7 && (unsigned) (ver97-200) < 17) {
      ci = xlat[0][serial & 0xff];
      cj = xlat[1][myfgetc(buf)^myfgetc(buf)^myfgetc(buf)^myfgetc(buf)];
      ck = 0x60;
      for (i=0; i < 324; i++)
	buf97[i] ^= (cj += ci * ck++);
      i = "66666>666;6A;:;55"[ver97-200] - '0';
      FORC4 cam_mul[c ^ (c >> 1) ^ (i & 1)] =
	sget2 (buf97 + (i & -2) + c*2);
    }
    if (tag == 0x200 && len == 3)
      shot_order = (myget4(buf),myget4(buf));
    if (tag == 0x200 && len == 4)
      FORC4 cblack[c ^ c >> 1] = myget2(buf);
    if (tag == 0x201 && len == 4)
      FORC4 cam_mul[c ^ (c >> 1)] = myget2(buf);
    if (tag == 0x220 && type == 7)
      meta_offset = DATALOC; //ftell(ifp);
    if (tag == 0x401 && type == 4 && len == 4)
      FORC4 cblack[c ^ c >> 1] = myget4(buf);
    if (tag == 0xe01) {		/* Nikon Capture Note */
      order = 0x4949;
      //fseek (ifp, 22, SEEK_CUR);
	  DATALOC += 22;
      for (offset=22; offset+22 < len; offset += 22+i) {
	tag = myget4(buf);
	//fseek (ifp, 14, SEEK_CUR);
	DATALOC += 14;
	i = myget4(buf)-4;
	if (tag == 0x76a43207) flip = myget2(buf);
	else DATALOC+= i; //fseek (ifp, i, SEEK_CUR);
      }
    }
    if (tag == 0xe80 && len == 256 && type == 7) {
      //fseek (ifp, 48, SEEK_CUR);
	  DATALOC += 48;
      cam_mul[0] = myget2(buf) * 508 * 1.078 / 0x10000;
      cam_mul[2] = myget2(buf) * 382 * 1.173 / 0x10000;
    }
    if (tag == 0xf00 && type == 7) {
      if (len == 614)
	//fseek (ifp, 176, SEEK_CUR);
	DATALOC += 176;
      else if (len == 734 || len == 1502)
	//fseek (ifp, 148, SEEK_CUR);
	DATALOC += 148;
      else goto next;
      goto get2_256;
    }
    if ((tag == 0x1011 && len == 9) || tag == 0x20400200)
      for (i=0; i < 3; i++)
		FORC3 cmatrix[i][c] = ((short) myget2(buf)) / 256.0;
    if ((tag == 0x1012 || tag == 0x20400600) && len == 4)
      FORC4 cblack[c ^ c >> 1] = myget2(buf);
    if (tag == 0x1017 || tag == 0x20400100)
      cam_mul[0] = myget2(buf) / 256.0;
    if (tag == 0x1018 || tag == 0x20400100)
      cam_mul[2] = myget2(buf) / 256.0;
    if (tag == 0x2011 && len == 2) {
get2_256:
      order = 0x4d4d;
      cam_mul[0] = myget2(buf) / 256.0;
      cam_mul[2] = myget2(buf) / 256.0;
    }
    if ((tag | 0x70) == 0x2070 && (type == 4 || type == 13))
      //fseek (ifp, get4()+base, SEEK_SET);
		DATALOC = myget4(buf)+base;
    if (tag == 0x2020)
      parse_thumb_note (base, 257, 258, buf);
    if (tag == 0x2040)
      parse_makernote (base, 0x2040, buf);
    if (tag == 0xb028) {
      //fseek (ifp, get4()+base, SEEK_SET);
	  DATALOC = myget4(buf)+base;
      parse_thumb_note (base, 136, 137, buf);
    }
    if (tag == 0x4001 && len > 500) {
      i = len == 582 ? 50 : len == 653 ? 68 : len == 5120 ? 142 : 126;
      //fseek (ifp, i, SEEK_CUR);
	  DATALOC += i;
      FORC4 cam_mul[c ^ (c >> 1)] = myget2(buf);
      for (i+=18; i <= len; i+=10) {
		myget2(buf);
		FORC4 sraw_mul[c ^ (c >> 1)] = myget2(buf);
		if (sraw_mul[1] == 1170) break;
      }
    }
    if (tag == 0x4021 && myget4(buf) && myget4(buf))
      FORC4 cam_mul[c] = 1024;
    if (tag == 0xa021)
      FORC4 cam_mul[c ^ (c >> 1)] = myget4(buf);
    if (tag == 0xa028)
      FORC4 cam_mul[c ^ (c >> 1)] -= myget4(buf);
    if (tag == 0xb001)
      unique_id = myget2(buf);
next:
    //fseek (ifp, save, SEEK_SET);
	DATALOC = save;
  }
quit:
  order = sorder;
}



void   parse_exif (int base, char *buf)
{
  unsigned kodak, entries, tag, type, len, save, c;
  double expo;


  kodak = !mystrncmp(make,"EASTMAN",7) && tiff_nifds < 3;
  entries = myget2(buf);
  while (entries--) {
    tiff_get (base, &tag, &type, &len, &save, buf);
    switch (tag) {
      case 33434:  tiff_ifd[tiff_nifds-1].shutter =
	shutter = getreal(type,buf);		break;
    case 33437:  aperture = getreal(type,buf);		break;
      case 34855:  iso_speed = myget2(buf);			break;
      case 36867:
      case 36868:  /* get_timestamp(0); */			break;
    case 37377:  if ((expo = -getreal(type,buf)) < 128)
		     tiff_ifd[tiff_nifds-1].shutter =
		     shutter = pow (2, expo);		break;
    case 37378:  aperture = pow (2, getreal(type,buf)/2);	break;
    case 37386:  focal_len = getreal(type,buf);		break;
    case 37500:  parse_makernote (base, 0,buf);		break;
      case 40962:  if (kodak) raw_width  = myget4(buf);	break;
      case 40963:  if (kodak) raw_height = myget4(buf);	break;
      case 41730:
	if (myget4(buf) == 0x20002)
	  for (exif_cfa=c=0; c < 8; c+=2)
	    /* exif_cfa |= fgetc(ifp) * 0x01010101 << c; */
	    exif_cfa |= myfgetc(buf) * 0x01010101 << c;
    }
    /* fseek (ifp, save, SEEK_SET); */
    DATALOC = save;
  }
}



int   parse_tiff (int base, char *buf);

int   parse_tiff_ifd (int base, char *buf)
{
  unsigned entries, tag, type, len, plen=16, save;
  int ifd, use_cm=0, cfa, i, j, c; /* , ima_len=0; */
  char software[64], /* *cbuf, */ *cp;
  uchar cfa_pat[16], cfa_pc[] = { 0,1,2,3 }, tab[256];
  double cc[4][4], cm[4][3], cam_xyz[4][3], num;
  double ab[]={ 1,1,1,1 }, asn[] = { 0,0,0,0 }, xyz[] = { 1,1,1 };
  /* unsigned sony_curve[] = { 0,0,0,0,0,4095 }; */
  /* unsigned int *tmpbuf, sony_offset=0, sony_length=0, sony_key=0; */
  /* struct jhead jh; */
  /* FILE *sfp; */
  int NSFType =0;
  int hhh;

  printf("in parse tiff ifd...\n"); 

  if (tiff_nifds >= sizeof tiff_ifd / sizeof tiff_ifd[0])
    return 1;
  ifd = tiff_nifds++;
  printf("\t\t\tcurrent ifd = %d\n",ifd);
  tiff_ifd[ifd].endoffset = 0;
  for (j=0; j < 4; j++)
    for (i=0; i < 4; i++)
      cc[j][i] = i == j;
  entries = myget2(buf);

  /* Bevara for checking IFDs */
  int totalentries = entries;
  printf("Number of entries in this IFD = %d\n",totalentries);


  if (entries > 512) return 1;

  while (entries--) {
    /* printf("going to tiff_get with entries = %d\n",entries); */
    tiff_get (base, &tag, &type, &len, &save, buf);
    /* printf("tag after tiff_get= %d\n",tag); */
 printf("tag in entry#%d of %d after tiff_get= %d\n",entries,totalentries,tag); 


    switch (tag) {
		case 5:   width  = myget2(buf);  break;
		case 6:   height = myget2(buf);  break;
		case 7:   width += myget2(buf);  break;
		case 9:   if ((i = myget2(buf))) filters = i;   break;
		case 17: case 18:
			if (type == 3 && len == 1)
				cam_mul[(tag-17)*2] = myget2(buf) / 256.0;
			break;
		case 23:
			if (type == 3) iso_speed = myget2(buf);
			break;
		case 28: case 29: case 30:
			cblack[tag-28] = myget2(buf);
			cblack[3] = cblack[1];
			break;
		case 36: case 37: case 38:
			cam_mul[tag-36] = myget2(buf);
			break;
		case 39:
			if (len < 50 || cam_mul[0]) break;
			/* fseek (ifp, 12, SEEK_CUR); */
			DATALOC = 12;
			FORC3 cam_mul[c] = myget2(buf);
			break;
		case 46:
			/* if (type != 7 || fgetc(ifp) != 0xff || fgetc(ifp) != 0xd8) break; */
			/* thumb_offset = ftell(ifp) - 2; */
			if (type != 7 || myfgetc(buf) != 0xff || myfgetc(buf) != 0xd8) break; 
			thumb_offset = DATALOC - 2;
			thumb_length = len;
			printf("set thumb offset\n");
			break;
		case 61440:			/* Fuji HS10 table */
			/* fseek (ifp, myget4(buf)+base, SEEK_SET); */
			DATALOC =  myget4(buf)+base;
			parse_tiff_ifd (base,buf);
			break;
      
		case 254: /* NewSubFileType */
			NSFType = myget4(buf);
			if ( (NSFType & 0x000F) == 1)
				{	
					tiff_ifd[ifd].type = TYPE_THUMBNAIL;
					//TODO: currently adding in all thumbnails, whether useable or not					
					component[numComponents].type = TYPE_THUMBNAIL;
					component[numComponents].ifd_loc = ifd;
					++numComponents;
					
				}
			else if ( (NSFType & 0x000F) == 0)
				{	
					tiff_ifd[ifd].type = TYPE_MAIN_IMAGE;
					component[numComponents].type = TYPE_MAIN_IMAGE;
					component[numComponents].ifd_loc = ifd;
					++numComponents;
				}
			else
				tiff_ifd[ifd].type = TYPE_UNKNOWN;
			
			printf("\t\t\tNSF type -- type is %d\n",tiff_ifd[ifd].type);

			/* printf("need to handle new subfile type\n"); */
			/* mystrcpy(meta_table[TAGTBLNUM].tagname,"NewSubFileType"); */
			/* meta_table[TAGTBLNUM].tagdata = malloc(20); */
			/* int NSFType; */
			/* NSFType = myget4(buf); */
			/* myget4(buf); */
			/* if ( (NSFType & 0x000F) == 1) */
			/*   mystrcpy(meta_table[TAGTBLNUM].tagdata,"Thumbnail Image"); */
			/* else if ( (NSFType & 0x000F) == 0) */
			/*   mystrcpy(meta_table[TAGTBLNUM].tagdata,"Main Image"); */
			/* else if ( ((NSFType & 0x000F) == 4) || ((NSFType & 0x000F) == 5)) */
				/*   mystrcpy(meta_table[TAGTBLNUM].tagdata,"Transparency Image"); */
			/* else if ( (NSFType & 0x001F) == 65537) */
			/*   mystrcpy(meta_table[TAGTBLNUM].tagdata,"Alternate Image"); */
			/* else */
				/*   mystrcpy(meta_table[TAGTBLNUM].tagdata,"Unknown"); */
			/* ++TAGTBLNUM; */
			break;


		case 2: case 256: case 61441:	/* ImageWidth */
			tiff_ifd[ifd].width = getint(type,buf);
			printf("image width = %d\n",tiff_ifd[ifd].width);
			/* mystrcpy(meta_table[TAGTBLNUM].tagname,"ImageWidth"); */
			/* meta_table[TAGTBLNUM].tagdata = malloc(21); /\* from double max int size *\/ */
			/* myitoa(tiff_ifd[ifd].width,meta_table[TAGTBLNUM].tagdata); */
			/* ++TAGTBLNUM; */
			break;
		case 3: case 257: case 61442:	/* ImageHeight */
			tiff_ifd[ifd].height = getint(type,buf);
			printf("image height = %d\n",tiff_ifd[ifd].height);
			/* mystrcpy(meta_table[TAGTBLNUM].tagname,"ImageHeight"); */
			/* meta_table[TAGTBLNUM].tagdata = malloc(21); /\* from double max int size *\/ */
			/* myitoa(tiff_ifd[ifd].height,meta_table[TAGTBLNUM].tagdata); */
			/* ++TAGTBLNUM; */
			break;
		case 258:				/* BitsPerSample */
		case 61443:
			tiff_ifd[ifd].samples = len & 7;
			tiff_ifd[ifd].bps = getint(type,buf);
			/* printf("image samples=%d, bps = %d\n",tiff_ifd[ifd].samples,tiff_ifd[ifd].bps); */
			/* mystrcpy(meta_table[TAGTBLNUM].tagname,"BitsPerSample"); */
			/* meta_table[TAGTBLNUM].tagdata = malloc(21); /\* from double max int size *\/ */
			/* myitoa(tiff_ifd[ifd].bps,meta_table[TAGTBLNUM].tagdata); */
			/* ++TAGTBLNUM; */
			break;
		case 61446:
			printf("private tag 61446 not yet supported...contact Bevara\n"); exit(0);
			/* raw_height = 0; */
			/* if (tiff_ifd[ifd].bps > 12) break; */
			/* load_raw = &  packed_load_raw; */
			/* load_flags = myget4(buf) ? 24:80; */
			break;
		case 259:				/* Compression */
			tiff_ifd[ifd].comp = getint(type,buf);
			/* printf("image comp type = %d, read at dataloc = %ld\n",tiff_ifd[ifd].comp,DATALOC); */
			/* mystrcpy(meta_table[TAGTBLNUM].tagname,"Compression"); */
			/* meta_table[TAGTBLNUM].tagdata = malloc(32); */
			/* if ( tiff_ifd[ifd].comp == 1) */
			/*   mystrcpy(meta_table[TAGTBLNUM].tagdata,"Uncompressed"); */
			/* else if ( tiff_ifd[ifd].comp == 7) */
			/*   mystrcpy(meta_table[TAGTBLNUM].tagdata,"TIFF/JPEG Baseline or Lossless"); */
			/* else if ( tiff_ifd[ifd].comp == 8) */
			/*   mystrcpy(meta_table[TAGTBLNUM].tagdata,"ZIP"); */
			/* else if ( tiff_ifd[ifd].comp == 34892) */
			/*   mystrcpy(meta_table[TAGTBLNUM].tagdata,"Lossy JPEG"); */
			/* else */
			/*   mystrcpy(meta_table[TAGTBLNUM].tagdata,"Vendor Unique"); */
			/* ++TAGTBLNUM; */
			break;
		case 262:				/* PhotometricInterpretation */
			tiff_ifd[ifd].phint = myget2(buf);
			/* printf("image photometric = %d\n",tiff_ifd[ifd].phint); */
			/* mystrcpy(meta_table[TAGTBLNUM].tagname,"PhotometricInterpretation"); */
			/* meta_table[TAGTBLNUM].tagdata = malloc(14); */
			/* if ( tiff_ifd[ifd].phint == 1) */
			/*   mystrcpy(meta_table[TAGTBLNUM].tagdata,"BlackIsZero"); */
			/* else if ( tiff_ifd[ifd].phint == 2) */
			/*   mystrcpy(meta_table[TAGTBLNUM].tagdata,"RGB"); */
			/* else if ( tiff_ifd[ifd].phint == 6) */
			/*   mystrcpy(meta_table[TAGTBLNUM].tagdata,"YCbCr"); */
			/* else if ( tiff_ifd[ifd].phint == 32803) */
			/*   mystrcpy(meta_table[TAGTBLNUM].tagdata,"CFA"); */
			/* else */
			/*   mystrcpy(meta_table[TAGTBLNUM].tagdata,"Vendor Unique"); */
			/* ++TAGTBLNUM; */
			break;
      case 270:				/* ImageDescription */

			for (hhh=0; hhh<512; ++hhh)
			  {
				*(desc+hhh) = *(buf+DATALOC); ++DATALOC;
				if (DATALOC>DATALEN) 
				  {
				printf("buffer length exceeded...exiting...\n"); exit(0);
				  }
			  }

			/* mystrcpy(meta_table[TAGTBLNUM].tagname,"ImageDescription"); */
			/* meta_table[TAGTBLNUM].tagdata = malloc(512); */
			/* mystrcpy(meta_table[TAGTBLNUM].tagdata,desc); */
			/* ++TAGTBLNUM; */
			break;
      case 271:				/* Make */
			myfgets (make, 64, buf);
			/* printf("MAKE = %s\n",make); */
			/* mystrcpy(meta_table[TAGTBLNUM].tagname,"Make"); */
			/* meta_table[TAGTBLNUM].tagdata = malloc(64); */
			/* mystrcpy(meta_table[TAGTBLNUM].tagdata,make); */
			/* ++TAGTBLNUM; */
			break;
      case 272:				/* Model */
			myfgets (model, 64, buf);
			/* printf("MODEL = %s\n",model); */
			/* mystrcpy(meta_table[TAGTBLNUM].tagname,"Model"); */
			/* meta_table[TAGTBLNUM].tagdata = malloc(64); */
			/* mystrcpy(meta_table[TAGTBLNUM].tagdata,model); */
			/* ++TAGTBLNUM; */
			break;
      case 280:				/* Panasonic RW2 offset */
			/* if (type != 4) break; */
			/* load_raw = &  panasonic_load_raw; */
			/* load_flags = 0x2008; */
			printf("Panasonic not yet supported...contact Bevara\n"); exit(0);
      case 273:				/* StripOffset */
			tiff_ifd[ifd].offset = myget4(buf)+base;
			/* mystrcpy(meta_table[TAGTBLNUM].tagname,"StripOffset"); */
			/* meta_table[TAGTBLNUM].tagdata = malloc(64); */
			/* myitoa(tiff_ifd[ifd].offset,meta_table[TAGTBLNUM].tagdata);  */
			/* ++TAGTBLNUM; */
			break;
      case 513:				/* JpegIFOffset */
      case 61447:
			printf("JPEGIFOffset not yet supported..contact Bevara\n"); exit(0);
			/* tiff_ifd[ifd].offset = myget4(buf)+base; */
			/* if (!tiff_ifd[ifd].bps && tiff_ifd[ifd].offset > 0) { */
			/*   /\* fseek (ifp, tiff_ifd[ifd].offset, SEEK_SET); *\/ */
			/*   DATALOC = tiff_ifd[ifd].offset; */
			/*   if (ljpeg_start (&jh, 1, buf)) { */
			/*     tiff_ifd[ifd].comp    = 6; */
			/*     tiff_ifd[ifd].width   = jh.wide; */
			/*     tiff_ifd[ifd].height  = jh.high; */
			/*     tiff_ifd[ifd].bps     = jh.bits; */
			/*     tiff_ifd[ifd].samples = jh.clrs; */
			/*     if (!(jh.sraw || (jh.clrs & 1))) */
			/*       tiff_ifd[ifd].width *= jh.clrs; */
			/*     if ((tiff_ifd[ifd].width > 4*tiff_ifd[ifd].height) & ~jh.clrs) { */
			/*       tiff_ifd[ifd].width  /= 2; */
			/*       tiff_ifd[ifd].height *= 2; */
			/*     } */
			/*     i = order; */
			/*     parse_tiff (tiff_ifd[ifd].offset + 12,buf); */
			/*     order = i; */
			/*   } */
			/* } */
			break;
      case 274:				/* Orientation */
			tiff_ifd[ifd].flip = "50132467"[myget2(buf) & 7]-'0';
			printf("\t\t\tOrientation = %d\n",tiff_ifd[ifd].flip);
			/* mystrcpy(meta_table[TAGTBLNUM].tagname,"Orientation"); */
			/* meta_table[TAGTBLNUM].tagdata = malloc(14); */
			/* if ( tiff_ifd[ifd].flip == 1) */
			/*   mystrcpy(meta_table[TAGTBLNUM].tagdata,"Visual Top"); */
			/* else if ( tiff_ifd[ifd].flip == 3) */
			/*   mystrcpy(meta_table[TAGTBLNUM].tagdata,"Visual Bottom"); */
			/* else if ( tiff_ifd[ifd].flip == 6) */
			/*   mystrcpy(meta_table[TAGTBLNUM].tagdata,"Visual RHS"); */
			/* else if ( tiff_ifd[ifd].flip == 8) */
			/*   mystrcpy(meta_table[TAGTBLNUM].tagdata,"Visual LHS"); */
			/* else */
			/*   mystrcpy(meta_table[TAGTBLNUM].tagdata,"Unknown"); */
			/* ++TAGTBLNUM; */
			break;
      case 277:				/* SamplesPerPixel */
			tiff_ifd[ifd].samples = getint(type,buf) & 7;
			/* mystrcpy(meta_table[TAGTBLNUM].tagname,"SamplesPerPixel"); */
				/* meta_table[TAGTBLNUM].tagdata = malloc(14); */
			/* if ( tiff_ifd[ifd].samples == 1) */
			/*   { */
			/*     if (tiff_ifd[ifd].phint == 1) */
			/*       mystrcpy(meta_table[TAGTBLNUM].tagdata,"grayscale"); */
			/*     else if (tiff_ifd[ifd].phint == 32803) */
			/*       mystrcpy(meta_table[TAGTBLNUM].tagdata,"CFA"); */
			/*     else */
			/*       mystrcpy(meta_table[TAGTBLNUM].tagdata,"Unknown"); */
			/*   } */
			/* else if ( tiff_ifd[ifd].samples == 3) */
			/*   { */
			/*     if (tiff_ifd[ifd].phint == 2) */
			/*       mystrcpy(meta_table[TAGTBLNUM].tagdata,"RGB"); */
			/*     else if (tiff_ifd[ifd].phint == 6) */
			/*       mystrcpy(meta_table[TAGTBLNUM].tagdata,"YCbCr"); */
			/*     else */
			/*       mystrcpy(meta_table[TAGTBLNUM].tagdata,"Unknown"); */
			/*   } */
			/* else */
			/*   mystrcpy(meta_table[TAGTBLNUM].tagdata,"Vendor Unique"); */
			/* ++TAGTBLNUM; */
			break;
      case 279:				/* StripByteCounts */
			tiff_ifd[ifd].bytes = myget4(buf);
			/* mystrcpy(meta_table[TAGTBLNUM].tagname,"StripByteCounts"); */
				/* meta_table[TAGTBLNUM].tagdata = malloc(22); */
			/* myitoa(tiff_ifd[ifd].bytes,meta_table[TAGTBLNUM].tagdata); */
			/* ++TAGTBLNUM; */
			break;
      case 514:  /*JPEG interchange format length */
      case 61448:
			printf("JPEG interchange format not yet supported...contact Bevara\n"); exit(0);
			/* tiff_ifd[ifd].bytes = myget4(buf); */
			break;
      case 61454:
			printf("private tags not yet supported...contact Bevara\n"); exit(0);
			FORC3 cam_mul[(4-c) % 3] = getint(type,buf);
			break;
      case 305:  case 11:		/* Software */
			myfgets (software, 64, buf);
			if (!mystrncmp(software,"Adobe",5) ||
				!mystrncmp(software,"dcraw",5) ||
				!mystrncmp(software,"UFRaw",5) ||
				!mystrncmp(software,"Bibble",6) ||
				!mystrncmp(software,"Nikon Scan",10) ||
				!mystrcmp (software,"Digital Photo Professional"))
				{ printf("found Adobe ---setting is_raw to 0\n");
				is_raw = 0;}
			/* mystrcpy(meta_table[TAGTBLNUM].tagname,"Software"); */
				/* meta_table[TAGTBLNUM].tagdata = malloc(64); */
				/* mystrcpy(meta_table[TAGTBLNUM].tagdata,software); */
			/* ++TAGTBLNUM; */
			break;
      case 306:				/* DateTime */
			/* mystrcpy(meta_table[TAGTBLNUM].tagname,"DateTime"); */
				/* meta_table[TAGTBLNUM].tagdata = malloc(20);  */
			/* int tt; */
			/* for (tt=0;tt<20;++tt) */
			/*   *(meta_table[TAGTBLNUM].tagdata + tt) = myfgetc(buf); */
			/* ++TAGTBLNUM; */

			/* get_timestamp(0); */

			break;
      case 315:				/* Artist */

	for (hhh=0; hhh<64; ++hhh)
	  {
	    *(artist+hhh) = *(buf+DATALOC); ++DATALOC;
	    if (DATALOC>DATALEN) 
	      {
		printf("buffer length exceeded...exiting...\n"); exit(0);
	      }
	  }

	/* mystrcpy(meta_table[TAGTBLNUM].tagname,"Artist"); */
      	/* meta_table[TAGTBLNUM].tagdata = malloc(64); */
        /* mystrcpy(meta_table[TAGTBLNUM].tagdata,artist); */
	/* ++TAGTBLNUM; */
	break;
      case 322:				/* TileWidth */
	tiff_ifd[ifd].tile_width = getint(type,buf);
	/* mystrcpy(meta_table[TAGTBLNUM].tagname,"TileWidth"); */
      	/* meta_table[TAGTBLNUM].tagdata = malloc(22); */
	/* myitoa(tiff_ifd[ifd].tile_width,meta_table[TAGTBLNUM].tagdata); */
	/* ++TAGTBLNUM; */
	break;
      case 323:				/* TileLength */
	tiff_ifd[ifd].tile_length = getint(type,buf);
	/* mystrcpy(meta_table[TAGTBLNUM].tagname,"TileLength"); */
      	/* meta_table[TAGTBLNUM].tagdata = malloc(22); */
	/* myitoa(tiff_ifd[ifd].tile_length,meta_table[TAGTBLNUM].tagdata); */
	/* ++TAGTBLNUM; */
	break;
      case 324:				/* TileOffsets */
	/* tiff_ifd[ifd].offset = len > 1 ? ftell(ifp) : myget4(buf); */
	tiff_ifd[ifd].offset = len > 1 ? DATALOC : myget4(buf);
	if (len == 1)
	  tiff_ifd[ifd].tile_width = tiff_ifd[ifd].tile_length = 0;
	if (len == 4) {
	  printf("Sinar not yet supported...exiting...\n"); exit(0);
	  /* load_raw = &  sinar_4shot_load_raw; */
	  /* is_raw = 5; */
	}
	/* mystrcpy(meta_table[TAGTBLNUM].tagname,"TileOffsets"); */
      	/* meta_table[TAGTBLNUM].tagdata = malloc(22); */
	/* myitoa(tiff_ifd[ifd].offset,meta_table[TAGTBLNUM].tagdata); */
	/* ++TAGTBLNUM; */
	break;
      case 325:  /*TileByteCounts*/
        printf("Tile Bytes Counts\n");
        break;
      case 330:				/* SubIFDs */
	if (!mystrcmp(model,"DSLR-A100") && tiff_ifd[ifd].width == 3872) {
	  /* load_raw = &  sony_arw_load_raw; */

	  /* data_offset = myget4(buf)+base; */

	  printf("Sony not yet supported...exiting...\n");
	  exit(0);
	  /* ifd++; */  break;
	}
	while (len--) {
	  /* i = ftell(ifp); */
	  /* fseek (ifp, myget4(buf)+base, SEEK_SET); */
	  i = DATALOC;
	  DATALOC =  myget4(buf)+base;
	  if (parse_tiff_ifd (base,buf)) break;
	  /* fseek (ifp, i+4, SEEK_SET); */
	  DATALOC=i+4;
	}
	break;
      case 400:
	mystrcpy (make, "Sarnoff");
	maximum = 0xfff;
	break;
      case 700:  /* XMP Metadata*/

	parse_xmp(buf,ifd);

        printf("found XMP metadata..\n");  
	/* XMPLEN = 0; */
	/* XMPLOC = DATALOC; */
	/* if (parse_xmp(buf)==0) */
	/*   { */
	/*     printf("incorrect parse of XMP Metadata\n"); */
	/*   } */
	/* printf("new DATALOC after XMP = %ld\n",DATALOC); */
        break;
      case 33723:   /*IPTC Metadata */
        printf("IPTC metadata not yet supported...exiting\n"); exit(0);
        break;
      case 28688:
	/* FORC4 sony_curve[c+1] = myget2(buf) >> 2 & 0xfff; */
	/* for (i=0; i < 5; i++) */
	/*   for (j = sony_curve[i]+1; j <= sony_curve[i+1]; j++) */
	/*     curve[j] = curve[j-1] + (1 << i); */
	/* break; */
      case 29184: 
	/* sony_offset = myget4(buf);   */
	/* break; */
      case 29185: /* sony_length = myget4(buf);  break; */
      case 29217: /* sony_key    = myget4(buf);  */
	printf("Sony not yet supported...exiting...\n"); exit(0);
	break;
      case 29264:
	printf("Minolta not yet supported...exiting...\n"); exit(0);
	/* parse_minolta (ftell(ifp),buf); */
	/* parse_minolta (DATALOC,buf); */
	/* raw_width = 0; */
	break;
      case 29443:
	FORC4 cam_mul[c ^ (c < 2)] = myget2(buf);
	break;
      case 29459:
	FORC4 cam_mul[c] = myget2(buf);
	i = (cam_mul[1] == 1024 && cam_mul[2] == 1024) << 1;
	SWAP (cam_mul[i],cam_mul[i+1])
	break;
      case 33405:			/* Model2 */
	/* fgets (model2, 64, ifp); */
	myfgets (model2, 64, buf);
	printf("MODEL2 = %s\n",model2);
	break;
      case 33421:			/* CFARepeatPatternDim */
	if (myget2(buf) == 6 && myget2(buf) == 6)
	  {
	  filters = 9;
	 
	  }
	break;
      case 33422:			/* CFAPattern */
	if (filters == 9) {
	  /* FORC(36) ((char *)xtrans)[c] = fgetc(ifp) & 3; */
	   FORC(36) ((char *)xtrans)[c] = myfgetc(buf) & 3;
	  break;
	}
      case 64777:			/* Kodak P-series */
	if ((plen=len) > 16) plen = 16;
	/* fread (cfa_pat, 1, plen, ifp); */
	int mm;
	for (mm=0;mm<plen;++mm)
	  {
	    *(cfa_pat+mm)=*(buf+DATALOC); ++DATALOC;
	    if (DATALOC > DATALEN)
	      {printf("buffer read error..exiting...\n"); exit(0);
	      }
	  }
	
	for (colors=cfa=i=0; i < plen && colors < 4; i++) {
	  colors += !(cfa & (1 << cfa_pat[i]));
	  cfa |= 1 << cfa_pat[i];
	}
	if (cfa == 070) memcpy (cfa_pc,"\003\004\005",3);	/* CMY */
	if (cfa == 072) memcpy (cfa_pc,"\005\003\004\001",4);	/* GMCY */
printf("have kodak P-series: going to guess cfa\n");
	goto guess_cfa_pc;
      case 33424:
      case 65024:
	printf("not handling KODAK...contact Bevara\n"); exit(0);
	/* DATALOC =  myget4(buf)+base; */
	/* parse_kodak_ifd (base, buf); */
	break;
      case 33434:			/* ExposureTime */
	tiff_ifd[ifd].shutter = shutter = getreal(type,buf);
	break;
      case 33437:			/* FNumber */
	aperture = getreal(type,buf);
	break;
      case 34306:			/* Leaf white balance */
	FORC4 cam_mul[c ^ 1] = 4096.0 / myget2(buf);
	break;
      case 34307:			/* Leaf CatchLight color matrix */
	/* for (hhh=0; hhh<7; ++hhh) */
	/*   { */
	/*     *(software+hhh) = *(buf+DATALOC); ++DATALOC; */
	/*     if (DATALOC>DATALEN)  */
	/*       { */
	/* 	printf("buffer length exceeded...exiting...\n"); exit(0); */
	/*       } */
	/*   } */

	/* if (mystrncmp(software,"MATRIX",6)) break; */
	/* colors = 4; */
	/* for (raw_color = i=0; i < 3; i++) { */
	/*   FORC4 fscanf (ifp, "%f", &rgb_cam[i][c^1]); */
	/*   if (!use_camera_wb) continue; */
	/*   num = 0; */
	/*   FORC4 num += rgb_cam[i][c]; */
	/*   FORC4 rgb_cam[i][c] /= num; */
	/* } */
	/* break; */
      case 34310:			/* Leaf metadata */
	/* parse_mos (ftell(ifp),buf); */

	/* parse_mos (DATALOC,buf); */
      case 34303:
	printf("Leaf not yet supported....exiting...\n"); exit(0);
	/* mystrcpy (make, "Leaf"); */
	break;
      case 34665:			/* EXIF tag */
	/* fseek (ifp, myget4(buf)+base, SEEK_SET); */
	printf("found EXIF data\n");
	DATALOC =  myget4(buf)+base; 
	parse_exif (base,buf);
	break;
      /* case 34853:			/\* GPSInfo tag *\/ */
      /* 	/\* fseek (ifp, myget4(buf)+base, SEEK_SET); *\/ */
      /* 	printf("found GPS data\n"); */
      /* 	DATALOC =  myget4(buf)+base;  */
      /* 	parse_gps (base,buf); */
      /* 	break; */
      case 34675:			/* InterColorProfile */
      case 50831:			/* AsShotICCProfile */
	/* profile_offset = ftell(ifp); */
	profile_offset = DATALOC;
	profile_length = len;
	break;
      case 37122:			/* CompressedBitsPerPixel */
	/* kodak_cbpp = */ myget4(buf);
	break;
      case 37386:			/* FocalLength */
	focal_len = getreal(type,buf);
	break;
      case 37393:			/* ImageNumber */
	shot_order = getint(type,buf);
	printf("shot order = %d\n",shot_order);
	break;
      case 37400:			/* old Kodak KDC tag */
	for (raw_color = i=0; i < 3; i++) {
	  getreal(type,buf);
	  FORC3 rgb_cam[i][c] = getreal(type,buf);
	}
	break;
      case 40976:
	printf("Samsung not yet supported...exiting...\n"); exit(0);
	/* strip_offset = myget4(buf); */
	/* switch (tiff_ifd[ifd].comp) { */
	/*   case 32770: load_raw = &  samsung_load_raw;   break; */
	/*   case 32772: load_raw = &  samsung2_load_raw;  break; */
	/*   case 32773: load_raw = &  samsung3_load_raw;  break; */
	/* } */
	break;
      case 46275:			/* Imacon tags */
	/* mystrcpy (make, "Imacon"); */
	/* /\* data_offset = ftell(ifp); *\/ */
	/* data_offset = DATALOC; */
	/* printf("in IMACON data_offset = %d\n",(int)data_offset); */
	/* ima_len = len; */
	/* break; */
      case 46279:
	/* if (!ima_len) break; */
	/* /\* fseek (ifp, 38, SEEK_CUR); *\/ */
	/* DATALOC = 38; */
      case 46274:
	/* fseek (ifp, 40, SEEK_CUR); */
	/* DATALOC = 40; */
	/* raw_width  = myget4(buf); */
	/* raw_height = myget4(buf); */
	/* left_margin = myget4(buf) & 7; */
	/* width = raw_width - left_margin - (myget4(buf) & 7); */
	/* top_margin = myget4(buf) & 7; */
	/* height = raw_height - top_margin - (myget4(buf) & 7); */
	/* if (raw_width == 7262) { */
	/*   height = 5444; */
	/*   width  = 7244; */
	/*   left_margin = 7; */
	/* } */
	/* /\* fseek (ifp, 52, SEEK_CUR); *\/ */
	/* DATALOC = 52; */
	/* FORC3 cam_mul[c] = getreal(11,buf); */
	/* /\* fseek (ifp, 114, SEEK_CUR); *\/ */
	/* DATALOC = 114; */
	/* flip = (myget2(buf) >> 7) * 90; */
	/* if (width * height * 6 == ima_len) { */
	/*   if (flip % 180 == 90) SWAP(width,height); */
	/*   raw_width = width; */
	/*   raw_height = height; */
	/*   left_margin = top_margin = filters = flip = 0; */
	/* } */
	/* mystrcpy(model,"Ixpress"); */
	/* /\* sprintf (model, "Ixpress %d-Mp", height*width/1000000); *\/ */
	/* load_raw = &  imacon_full_load_raw; */
	/* if (filters) { */
	/*   if (left_margin & 1) filters = 0x61616161; */
	/*   load_raw = &  unpacked_load_raw; */
	/* } */
	/* maximum = 0xffff; */
	printf("Imacon not yet supported...exiting...\n"); exit(0);
	break;
      case 50454:			/* Sinar tag */
      case 50455:
	printf("Sinar not yet supported...exiting...\n"); exit(0);
	/* if (!(cbuf = (char *) malloc(len))) break; */

	/* for (hhh=0; hhh<len; ++hhh) */
	/*   { */
	/*     *(cbuf+hhh) = *(buf+DATALOC); ++DATALOC; */
	/*     if (DATALOC>DATALEN)  */
	/*       { */
	/* 	printf("buffer length exceeded...exiting...\n"); exit(0); */
	/*       } */
	/*   } */

	/* for (cp = cbuf-1; cp && cp < cbuf+len; cp = strchr(cp,'\n')) */
	/*   if (!mystrncmp (++cp,"Neutral ",8)) */
	/*     sscanf (cp+8, "%f %f %f", cam_mul, cam_mul+1, cam_mul+2); */
	/* free (cbuf); */
	break;
      case 50458:
	/* if (!make[0]) mystrcpy (make, "Hasselblad"); */
	/* break; */
      case 50459:			/* Hasselblad tag */
	/* i = order; */
	/* /\* j = ftell(ifp); *\/ */
	/* j=DATALOC; */
	/* c = tiff_nifds; */
	/* order = myget2(buf); */
	/* /\* fseek (ifp, j+(myget2(buf),myget4(buf)), SEEK_SET); *\/ */
	/* printf("DOUBLECHECK the mygets...exiting...\n"); exit(0); */
	/* DATALOC = j+(myget2(buf),myget4(buf)); */
	/* parse_tiff_ifd (j,buf); */
	/* maximum = 0xffff; */
	/* tiff_nifds = c; */
	/* order = i; */
	printf("Hasselblad not yet supported...exiting...\n"); exit(0);
	break;
      case 50706:			/* DNGVersion */
	/* FORC4 dng_version = (dng_version << 8) + fgetc(ifp); */
	FORC4 dng_version = (dng_version << 8) + myfgetc(buf);
	if (!make[0]) mystrcpy (make, "DNG");
	is_raw = 1;
	break;
       case 50707: /*DNGBackwardVersion */
	 printf("UNUSED: DNG backward version\n");
	 /* From DNG spec this is the oldest version of DNG for which */
	 /* this file is compatible. Used in readers. */
	 break;

      case 50708:			/* UniqueCameraModel */
	if (model[0]) break;
	/* fgets (make, 64, ifp); */
	myfgets (make, 64, buf);
        if ((cp = strchr(make,' '))) {
	  mystrcpy(model,cp+1);
	  *cp = 0;
	}
	break;

       case 50709: /*LocalizedCameraModel */
	 printf("UNUSED: LocalizedCameraModel\n");
	 break;

      case 50710:			/* CFAPlaneColor */
	printf("CFA plane color\n");
	if (filters == 9) break;
	if (len > 4) len = 4;
	colors = len;
	/* fread (cfa_pc, 1, colors, ifp); */
	/* myfread(cfa_pc, 1, colors, buf); */
	int ggg;
	for (ggg=0;ggg<colors;++ggg)
	  {
	    *(cfa_pc+ggg) = *(buf+DATALOC); ++DATALOC;
	  }
	printf("GOT CFA PLANE COLOR:\n");
	for (ggg=0;ggg<colors;++ggg)
	  {
	    printf("\t 0x%02x\n",cfa_pc[ggg]);
	  }


guess_cfa_pc:

	FORCC tab[cfa_pc[c]] = c;
	cdesc[c] = 0;


	for (i=16; i--; )
	  {
	  filters = filters << 2 | tab[cfa_pat[i % plen]];
	  }

	filters -= !filters;
	
	break;
      case 50711:			/* CFALayout */
	if (myget2(buf)  !=1)
	  {
	    printf("CFA layout is not rectangular...exiting...\n"); exit(0);
	  }
	/* if (CFALayout == 2) fuji_width = 1; */

	break;
      case 291:
      case 50712:			/* LinearizationTable */
	printf("LinearizationTable not supported....Contact Bevara...\n");
	/* linear_table (len,buf); */
	break;
      case 50713:			/* BlackLevelRepeatDim */
	cblack[4] = myget2(buf);
	cblack[5] = myget2(buf);
	if (cblack[4] * cblack[5] > sizeof cblack / sizeof *cblack - 6)
	    cblack[4] = cblack[5] = 1;
	break;
      case 61450:
	cblack[4] = cblack[5] = MIN(sqrt(len),64);
      case 50714:			/* BlackLevel */
	if (!(cblack[4] * cblack[5]))
	  cblack[4] = cblack[5] = 1;
	FORC (cblack[4] * cblack[5])
	  cblack[6+c] = getreal(type,buf);
	black = 0;
	break;
      case 50715:			/* BlackLevelDeltaH */
      case 50716:			/* BlackLevelDeltaV */
	for (num=i=0; i < len; i++)
	  num += getreal(type,buf);
	black += num/len + 0.5;
	break;
      case 50717:			/* WhiteLevel */
	maximum = getint(type,buf);
	break;
      case 50718:			/* DefaultScale */
	pixel_aspect  = getreal(type,buf);
	pixel_aspect /= getreal(type,buf);
	break;
	
    case 50719: /*DefaultCropOrigin*/
	 printf("UNUSED: DefaultCropOrigin\n");
	 break;
    case 50720: /*DefaultCropSize*/
	 printf("UNUSED: DefaultCropSize\n");
	 break;

      case 50721:			/* ColorMatrix1 */
      case 50722:			/* ColorMatrix2 */
	FORCC for (j=0; j < 3; j++)
	  cm[c][j] = getreal(type,buf);
	use_cm = 1;
	break;
      case 50723:			/* CameraCalibration1 */
      case 50724:			/* CameraCalibration2 */
	for (i=0; i < colors; i++)
	  FORCC cc[i][c] = getreal(type,buf);
	break;
    case 50725: /*ReductionMatrix1*/
	 printf("UNUSED: ReductionMatrix1\n");
	 break;

    case 50726: /*ReductionMatrix2*/
	 printf("UNUSED: ReductionMatrix2\n");
	 break;
      case 50727:			/* AnalogBalance */
	FORCC ab[c] = getreal(type,buf);
	break;
      case 50728:			/* AsShotNeutral */
	FORCC asn[c] = getreal(type,buf);
	break;
      case 50729:			/* AsShotWhiteXY */
	xyz[0] = getreal(type,buf);
	xyz[1] = getreal(type,buf);
	xyz[2] = 1 - xyz[0] - xyz[1];
	FORC3 xyz[c] /= d65_white[c];
	break;

    case 50730: /* BaselineExposure*/
	 printf("UNUSED: BaselineExposure\n");
	 break;
    case 50731: /* BaselineNoise*/
	 printf("UNUSED: BaselineNoise\n");
	 break;
    case 50732: /* BaselineSharpness*/
	 printf("UNUSED: BaselineSharpness\n");
	 break;
    case 50733: /* BayerGreenSplit*/
	 printf("UNUSED: BayerGreenSplit\n");
	 break;
    case 50734: /* LinearResponseLimit*/
	 printf("UNUSED: LinearResponseLimit\n");
	 break;

    case 50735: /* CameraSerialNumber*/
	 printf("UNUSED: CameraSerialNumber\n");
	 break;
   case 50736: /* LensInfo*/
	 printf("UNUSED: LensInfo\n");
	 break;
   case 50737: /* ChromaBlurRadius*/
	 printf("UNUSED: ChromaBlurRadius\n");
	 break;
   case 50738: /* AntiAliasStrength*/
	 printf("UNUSED: AntiAliasStrength\n");
	 break;
   case 50739: /* ShadowScale*/
	 printf("UNUSED: ShadowScale\n");
	 break;

      case 50740:			/* DNGPrivateData */
	printf("Check if UNUSED: DNGPrivateData\n");
	if (dng_version) break;
	printf("Minolta not yet supported...contact Bevara\n"); exit(0);
	/* parse_minolta (j = myget4(buf)+base,buf); */
	/* fseek (ifp, j, SEEK_SET); */
	DATALOC = j;
	parse_tiff_ifd (base,buf);
	break;

   case 50741: /* MakerNoteSafety*/
	 printf("UNUSED: MakerNoteSafety\n");
	 break;



      case 50752:
	read_shorts (cr2_slice, 3, buf);
	break;
    case 50778: /*CalibrationIlluminant1*/
	 printf("UNUSED: CalibrartionIlluminant1\n");
	 break;
    case 50779: /*CalibrationIlluminant2*/
	 printf("UNUSED: CalibrartionIlluminant2\n");
	 break;
    case 50780: /*BestQualityScale*/
	 printf("UNUSED: Best Quality Scale\n");
	 break;
    case 50781: /*RawDataUniqueID*/
	 printf("UNUSED: RawDataUniqueID\n");
	 break;

   case 50827: /* OriginalRawFileName*/
	 printf("UNUSED: OriginalRawFileName\n");
	 break;
   case 50828: /* OriginalRawFileData*/
	 printf("UNUSED: OriginalRawFileData\n");
	 break;


      case 50829:			/* ActiveArea */
	top_margin = getint(type,buf);
	left_margin = getint(type,buf);
	height = getint(type,buf) - top_margin;
	width = getint(type,buf) - left_margin;
	break;
      case 50830:			/* MaskedAreas */
        for (i=0; i < len && i < 32; i++)
	  ((int *)mask)[i] = getint(type,buf);
	black = 0;
	break;


   case 50832: /* AsShotPreProfileMatrix*/
	 printf("UNUSED: AsShotPreProfileMatrix\n");
	 break;

   case 50833: /* CurrentICCProfile*/
	 printf("UNUSED: CurrentICCProfile\n");
	 break;
   case 50834: /* CurrentPreProfileMatrix*/
	 printf("UNUSED: CurrentPreProfileMatrix\n");
	 break;

   case 50879: /* ColorimetricReference*/
	 printf("UNUSED: ColorimetricReference\n");
	 break;
   case 50931: /* CameraCalibrationSignature*/
	 printf("UNUSED: CameraCalibrationSignature\n");
	 break;
   case 50932: /* ProfileCalibrationSignature*/
	 printf("UNUSED: ProfileCalibrationSignature\n");
	 break;
   case 50933: /* ExtraCameraProfiles*/
	 printf("UNUSED: ExtraCameraProfiles\n");
	 break;
   case 50934: /* AsShotProfileName*/
	 printf("UNUSED: AsShotProfileName\n");
	 break;
   case 50935: /* NoiseReductionApplied*/
	 printf("UNUSED: NoiseReductionApplied\n");
	 break;
   case 50936: /* ProfileName*/
	 printf("UNUSED: ProfileName\n");
	 break;
   case 50937: /* ProfileHueSatMapDims*/
	 printf("UNUSED: ProfileHueSatMapDims\n");
	 break;

   case 50938: /* ProfileHueSatMapData1*/
	 printf("UNUSED: ProfileHueSatMapData1\n");
	 break;
   case 50939: /* ProfileHueSatMapData2*/
	 printf("UNUSED: ProfileHueSatMapData2\n");
	 break;
   case 50940: /* ProfileToneCurve*/
	 printf("UNUSED: ProfileToneCurve\n");
	 break;
   case 50941: /* ProfileEmbedPolicy*/
	 printf("UNUSED: ProfileEmbedPolicy\n");
	 break;
   case 50942: /* ProfileCopyright*/
	 printf("UNUSED: ProfileCopyright\n");
	 break;
   case 50964: /* ForwardMatrix1*/
	 printf("UNUSED: ForwardMatrix1\n");
	 break;
   case 50965: /* ForwardMatrix2*/
	 printf("UNUSED: ForwardMatrix2\n");
	 break;

   case 50966: /* PreviewApplicationName*/
	 printf("UNUSED: PreviewApplicationName\n");
	 break;

   case 50967: /* PreviewApplicationVersion*/
	 printf("UNUSED: PreviewApplicationVersion\n");
	 break;

   case 50968: /* PreviewSettingsName*/
	 printf("UNUSED: PreviewSettingsName\n");
	 break;

   case 50969: /* PreviewSettingsDigest*/
	 printf("UNUSED: PreviewSettingsDigest\n");
	 break;

   case 50970: /* PreviewColorSpace*/
	 printf("UNUSED: PreviewColorSpace\n");
	 break;

   case 50971: /* PreviewDateTime*/
	 printf("UNUSED: PreviewDateTime\n");
	 break;

   case 50972: /* RawImageDigest*/
	 printf("UNUSED: RawImageDigest\n");
	 break;

   case 50973: /* OriginalRawFileDigest*/
	 printf("UNUSED: OriginalRawFileDigest\n");
	 break;

   case 50974: /* SubTileBlockSize*/
	 printf("UNUSED: SubTileBlockSize\n");
	 break;


   case 50975: /* RowInterleaveFactor*/
	 printf("UNUSED: RowInterleaveFactor\n");
	 break;

   case 50981: /* ProfileLookTableDims*/
	 printf("UNUSED: ProfileLookTableDims\n");
	 break;

  case 50982: /* ProfileLookTableData*/
	 printf("UNUSED: ProfileLookTableData\n");
	 break;

      case 51008:			/* OpcodeList1 */
	/* meta_offset = ftell(ifp); */
	meta_offset = DATALOC;
		 printf("UNUSED: OpcodeList1\n");
	break;

      case 51009:			/* OpcodeList2 */
	/* meta_offset = ftell(ifp); */
	meta_offset = DATALOC;
	 printf("UNUSED: OpcodeList2\n");
	break;

      case 51022:			/* OpcodeList3 */
	/* meta_offset = ftell(ifp); */
	meta_offset = DATALOC;
	 printf("UNUSED: OpcodeList3\n");
	break;

   case 51041: /* NoiseProfile*/
	 printf("UNUSED: NoiseProfile\n");
	 break;

	 /* these are numbered in reverse order in the spec */
   case 51125: /* DefaultUserCrop*/
	 printf("UNUSED: DefaultUserCrop\n");
	 break;

   case 51110: /* DefaultBlackRender*/
	 printf("UNUSED: DefaultBlackRender\n");
	 break;


   case 51109: /* BaselineExposureOffset*/
	 printf("UNUSED: BaselineExposureOffset\n");
	 break;

   case 51108: /* ProfileLookTableEncoding*/
	 printf("UNUSED: ProfileLookTableEncoding\n");
	 break;

   case 51107: /* ProfileHueSatMapEncoding*/
	 printf("UNUSED: ProfileHueDatMapEncoding\n");
	 break;

   case 51089: /* OriginalDefaultFinalSize*/
	 printf("UNUSED: OriginalDefaultFinalSize\n");
	 break;

   case 51090: /* OriginalBestQualityFinalSize*/
	 printf("UNUSED: OriginalBestQualityFinalSize\n");
	 break;

   case 51091: /* OriginalDefaultCropSize*/
	 printf("UNUSED: OriginalDefaultCropSize\n");
	 break;

	 

   case 51111: /* NewRawImageDigest*/
	 printf("UNUSED: NewRawImageDigest\n");
	 break;


   case 51112: /* RawToPreviewGain*/
	 printf("UNUSED: RawToPreviewGain\n");
	 break;




      case 64772:			/* Kodak P-series */

	printf("Kodak not yet supported....enxiting...\n"); exit(0);
	/* if (len < 13) break; */
	/* /\* fseek (ifp, 16, SEEK_CUR); *\/ */
	/* DATALOC = 16; */
	/* data_offset = myget4(buf); */
	/* /\* fseek (ifp, 28, SEEK_CUR); *\/ */
	/* DATALOC = 28; */
	/* data_offset += myget4(buf); */

	/* printf("data offset in Kodak = %d\n",(int)data_offset); */
	/* load_raw = &  packed_load_raw; */
	break;
      case 65026:
	/* if (type == 2) fgets (model2, 64, ifp); */
	if (type == 2) myfgets (model2, 64, buf);
    }



 

    DATALOC=save;
   
  }



  /* if (sony_length && (tmpbuf = (unsigned *) malloc(sony_length))) { */
  /*   /\* fseek (ifp, sony_offset, SEEK_SET); *\/ */
  /*   DATALOC = sony_offset; */
  /*   /\* fread (tmpbuf, sony_length, 1, ifp); *\/ */
  /*   /\* myfread (tmpbuf, sony_length, 1, buf); *\/ */


  /* 	for (hhh=0; hhh<sony_length; ++hhh) */
  /* 	  { */
  /* 	    *(tmpbuf+hhh) = *(buf+DATALOC); ++DATALOC; */
  /* 	    if (DATALOC>DATALEN)  */
  /* 	      { */
  /* 		printf("buffer length exceeded...exiting...\n"); exit(0); */
  /* 	      } */
  /* 	  } */
  /*   sony_decrypt (tmpbuf, sony_length/4, 1, sony_key); */
  /*   sfp = ifp; */
  /*   printf("need to implement SONY... exiting\n"); exit(0); */
    /* if ((ifp = tmpfile())) { */
    /*   fwrite (tmpbuf, sony_length, 1, ifp); */
    /*   fseek (ifp, 0, SEEK_SET); */
    /*   parse_tiff_ifd (-sony_offset,buf); */
    /*   fclose (ifp); */
    /* } */
    /* ifp = sfp; */
  /*   free (tmpbuf); */
  /* } */
  for (i=0; i < colors; i++)
    FORCC cc[i][c] *= ab[i];
  if (use_cm) {
    FORCC for (i=0; i < 3; i++)
      for (cam_xyz[c][i]=j=0; j < colors; j++)
	cam_xyz[c][i] += cc[c][j] * cm[j][i] * xyz[i];
    cam_xyz_coeff (cmatrix, cam_xyz);
  }
  if (asn[0]) {
    cam_mul[3] = 0;
    FORCC cam_mul[c] = 1 / asn[c];
  }
  if (!use_cm)
    FORCC pre_mul[c] /= cc[c][c];

  return 0;
}

int   parse_tiff (int base, char *buf)
{
  int doff;
  int tmp;


  DATALOC = base; 
  printf("in parse-tiff, start parsing at offset %d\n",(int) DATALOC);



  order = myget2(buf);

  if (order != 0x4949 && order != 0x4d4d) return 0;
  tmp = myget2(buf);


  while ((doff = myget4(buf))) {

    DATALOC = doff+base;
    if (parse_tiff_ifd (base,buf)) break;
  }
  return 1;
}

void   apply_tiff(char *buf)
{
  int max_samp=0, ties=0, os, ns, raw=-1,  i;
  struct jhead jh;

  thumb_misc = 16;
  if (thumb_offset) {
    DATALOC = thumb_offset;
    if (ljpeg_start (&jh, 1, buf)) {
      thumb_misc   = jh.bits;
      thumb_width  = jh.wide;
      thumb_height = jh.high;
    }
  }
  for (i=tiff_nifds; i--; ) {
    if (tiff_ifd[i].shutter)
      shutter = tiff_ifd[i].shutter;
    tiff_ifd[i].shutter = shutter;
  }
  for (i=0; i < tiff_nifds; i++) {
    if (max_samp < tiff_ifd[i].samples)
	max_samp = tiff_ifd[i].samples;
    if (max_samp > 3) max_samp = 3;
    os = raw_width*raw_height;
    ns = tiff_ifd[i].width*tiff_ifd[i].height;
    if ((tiff_ifd[i].comp != 6 || tiff_ifd[i].samples != 3) &&
	(tiff_ifd[i].width | tiff_ifd[i].height) < 0x10000 &&
	 ns && ((ns > os && (ties = 1)) ||
		(ns == os && shot_select == ties++))) {
      raw_width     = tiff_ifd[i].width;
      raw_height    = tiff_ifd[i].height;
      tiff_bps      = tiff_ifd[i].bps;
      tiff_compress = tiff_ifd[i].comp;
      data_offset   = tiff_ifd[i].offset;
      tiff_flip     = tiff_ifd[i].flip;
      tiff_samples  = tiff_ifd[i].samples;
      tile_width    = tiff_ifd[i].tile_width;
      tile_length   = tiff_ifd[i].tile_length;
      shutter       = tiff_ifd[i].shutter;
      raw = i;


      tiff_ifd[i].dng_comp = COMP_UNKNOWN;
      switch (tiff_compress) {
      case 0:
      case 1:
      	tiff_ifd[i].dng_comp = PACKED_DNG;
      	load_raw = &packed_dng_load_raw;
      break;  /* this case expected only for thumbnails, handled separately*/
      case 7:
      	tiff_ifd[i].dng_comp = LOSSLESS_JPEG;
      	load_raw = &lossless_dng_load_raw;  /*main image load */
      break;
      case 8:
      	tiff_ifd[i].dng_comp = ZIP;
      	break; /* this case not yet handled */
      case 34892:
      	tiff_ifd[i].dng_comp = LOSSY_JPEG;
      	break;  /* lossy JPEG not yet handled */
      default: is_raw = 0;
      }


      
    }
  }
  if (is_raw == 1 && ties) is_raw = ties;
  if (!tile_width ) tile_width  = INT_MAX;
  if (!tile_length) tile_length = INT_MAX;
  for (i=tiff_nifds; i--; )
    if (tiff_ifd[i].flip) tiff_flip = tiff_ifd[i].flip;
  if (raw >= 0 && !load_raw)
    //printf("setting load raw\n");


  if (!dng_version)
    /* if ( (tiff_samples == 3 && tiff_ifd[raw].bytes && tiff_bps != 14 && */
    /* 	  (tiff_compress & -16) != 32768) */
    /*   || (tiff_bps == 8 && !strcasestr(make,"Kodak") && */
    /* 	  !strstr(model2,"DEBUG RAW"))) */
    /*   is_raw = 0; */
    {
      //printf("DNG version not found....exiting...\n");
      exit(0);
    }



  for (i=0; i < tiff_nifds; i++)
    {
    if (i != raw && tiff_ifd[i].samples == max_samp &&
	tiff_ifd[i].width * tiff_ifd[i].height / (SQR(tiff_ifd[i].bps)+1) >
	      thumb_width *       thumb_height / (SQR(thumb_misc)+1)
	/* && tiff_ifd[i].comp != 34892 */) 
      {
      thumb_width  = tiff_ifd[i].width;
      thumb_height = tiff_ifd[i].height;
      thumb_offset = tiff_ifd[i].offset;
      thumb_length = tiff_ifd[i].bytes;
      thumb_misc   = tiff_ifd[i].bps;
      thm = i;
     

      switch (tiff_ifd[i].comp) {
      case 0:
      case 1:
	tiff_ifd[i].dng_comp = UNCOMPRESSED;
      break;  /* this case expected only for thumbnails, handled separately*/
      case 7:
	tiff_ifd[i].dng_comp = LOSSLESS_JPEG;
      break;
      case 8:
	tiff_ifd[i].dng_comp = ZIP;
	break; /* this case not yet handled */
      case 34892: 
	tiff_ifd[i].dng_comp = LOSSY_JPEG;
	break;  /* lossy JPEG not yet handled */
      default: 
	tiff_ifd[i].dng_comp = COMP_UNKNOWN; 
	break;
      }

      /* BEVARA: choose which thumbnail to display, unncompressed or baseline
         JPEG. These are the only two options supported at the moment */
      /* switch (tiff_ifd[i].comp) { */
      /* case 1: */
      /* 	  /\* tiff_ifd[i].dng_comp = NO_COMPRESSION; *\/ */
      /* 	  break; */
      /* case 7: */
      /* 	  load_thumb=  &lossless_dng_load_raw; /\* placeholder *\/ */
      /* 	  /\* tiff_ifd[i].dng_comp = BASELINE_JPEG; *\/ */
      /* 	  break; */
      /* case 34892: */
      /* 	  load_thumb=  &lossless_dng_load_raw; /\* placeholder *\/ */
      /* 	  /\* tiff_ifd[i].dng_comp = LOSSY_JPEG; *\/ */
      /* 	  break; */
      /* default: */
      /* 	  /\* tiff_ifd[i].dng_comp = COMP_UNKNOWN; *\/ */
      /* 	  break; */
      /* } */

  /* if (tiff_ifd[i].comp == 1) /\* we found a simple thumbnail, so stop *\/ */
  /*     	{ */
  /*     	  break; */
  /*     	} */
  /*     if (tiff_ifd[i].comp == 7) /\* we found a TIFF/JPEG, so stop *\/ */
  /*     	{ */
  /*     	  load_thumb=  &  lossless_dng_load_raw; /\* placeholder *\/ */
  /*     	break; */
  /*     	} */
  /*     if (tiff_ifd[i].comp == 34892) /\* we found a lossy JPEG, so stop *\/ */
  /*     	{ */
  /*     	  printf("found non-baseline JPEG thumbnail\n"); */
  /*     	  /\* load_thumb = &  lossy_dng_load_raw; *\/ */
  /*     	  break; */
  /*     	} */



    }
    }
  if (thm >= 0) {
    thumb_misc |= tiff_ifd[thm].samples << 5;
    /* thumbs handled separately */
    /* switch (tiff_ifd[thm].comp) { */
    /*   case 0: */
    /* 	write_thumb = NULL; /\* &  layer_thumb; *\/ */
    /* 	break; */
    /*   case 1: */
    /* 	if (tiff_ifd[thm].bps <= 8) */
    /* 	  write_thumb = NULL; /\* &  ppm_thumb; *\/ */
    /* 	else if (!mystrcmp(make,"Imacon")) */
    /* 	  write_thumb = NULL;  /\* &  ppm16_thumb; *\/ */
    /* 	else */
    /* 	  thumb_load_raw = NULL;  /\* &  kodak_thumb_load_raw; *\/ */
    /* 	break; */
    /*   case 65000: */
    /* 	thumb_load_raw = tiff_ifd[thm].phint == 6 ? */
    /* 		&  kodak_ycbcr_load_raw : &  kodak_rgb_load_raw; */
    /* } */
  }
}


 


/*
   Identify which camera created this file, and set global variables
   accordingly.
 */
void   identify(char *buf, int insize)
{
	//BEVARA: removed all of the model tables

  
  
  
  static const char *corp[] =
    { "AgfaPhoto", "Canon", "Casio", "Epson", "Fujifilm",
      "Mamiya", "Minolta", "Motorola", "Kodak", "Konica", "Leica",
      "Nikon", "Nokia", "Olympus", "Pentax", "Phase One", "Ricoh",
      "Samsung", "Sigma", "Sinar", "Sony" };
  char head[32], *cp;
  int /* hlen, */ /*flen, fsize, *//* zero_fsize=1, */ i, c;
  /* struct jhead jh; */

  tiff_flip = flip = filters = UINT_MAX;	/* unknown */
  /* printf("UINT_MAX = %u, filters initialized to  %u\n",UINT_MAX,filters); */

  raw_height = raw_width = /* fuji_width = */ fuji_layout = cr2_slice[0] = 0;
  maximum = height = width = top_margin = left_margin = 0;
  cdesc[0] = desc[0] = artist[0] = make[0] = model[0] = model2[0] = 0;
  iso_speed = shutter = aperture = focal_len = unique_id = 0;
  tiff_nifds = 0;


  mymemset (tiff_ifd, 0, sizeof tiff_ifd);
  mymemset (gpsdata, 0, sizeof gpsdata);
  mymemset (cblack, 0, sizeof cblack);
  mymemset (white, 0, sizeof white);
  mymemset (mask, 0, sizeof mask);


  thumb_offset = thumb_length = thumb_width = thumb_height = 0;
  load_raw = thumb_load_raw = NULL;
  
  data_offset = meta_offset = meta_length = tiff_bps = tiff_compress = 0;
  printf("init data offset\n");
  /* kodak_cbpp = */ zero_after_ff = dng_version = load_flags = 0;
  /* timestamp = */ shot_order = tiff_samples = black = is_foveon = 0;
  mix_green = profile_length = data_error = /* zero_is_bad = */ 0;
  pixel_aspect = is_raw = raw_color = 1;
  tile_width = tile_length = 0;

 

  for (i=0; i < 4; i++) {
    cam_mul[i] = i == 1;
    pre_mul[i] = i < 3;
    FORC3 cmatrix[c][i] = 0;
    FORC3 rgb_cam[c][i] = c == i;
  }
  colors = 3;
  for (i=0; i < 0x10000; i++) curve[i] = i;

  /* header bytes 0-1 for endianness */
  order = myget2(buf);
  DATALOC = 0;  /*reset data ptr to read entire header */

  

  /* This is a general header read, useful for raw as well as TIFF/DNG */
  int hloop;
  for (hloop=0;hloop<32;++hloop)
    {
      head[hloop] = *(buf+DATALOC); ++DATALOC;
    }
    
	/*flen=fsize=insize;*/




  if (order == 0x4949 || order == 0x4d4d) 
    {

      printf("we found endian flag \n");

      if (!memcmp (head+6,"HEAPCCDR",8)) 
		{
	  printf("not yet handling HEAPCCDR (Canon CRW)...contact Bevara for suppport...\n"); exit(0);
	  /* data_offset = hlen; */
	  /* parse_ciff (hlen, flen-hlen, 0); */
	  /* load_raw = &  canon_load_raw; */
		} 
      else if (parse_tiff(0,buf))
		{
			apply_tiff(buf);
		}
    } 
  else
    {
      printf("Expected DNG header not found...exiting...\n");
      exit(0);

    }



  if (make[0] == 0)

    {
      printf("make[0] == 0\n"); exit(0);

    }


  for (i=0; i < sizeof corp / sizeof *corp; i++)
    if (strcasestr (make, corp[i]))	/* Simplify company names */
      {
	    mystrcpy (make, corp[i]);

      }
  if ((!mystrcmp(make,"Kodak") || !mystrcmp(make,"Leica")) &&
	((cp = strcasestr(model," DIGITAL CAMERA")) ||
	 (cp = strstr(model,"FILE VERSION"))))
     *cp = 0;
  if (!strncasecmp(model,"PENTAX",6))
    mystrcpy (make, "Pentax");

  cp = make + mystrlen(make);		/* Remove trailing spaces */
  while (*--cp == ' ') *cp = 0;
  cp = model + mystrlen(model);
  while (*--cp == ' ') *cp = 0;
  i = mystrlen(make);			/* Remove make from model */
  if (!strncasecmp (model, make, i) && model[i++] == ' ')
    memmove (model, model+i, 64-i);
  if (!mystrncmp (model,"FinePix ",8))
    mystrcpy (model, model+8);
  if (!mystrncmp (model,"Digital Camera ",15))
    mystrcpy (model, model+15);
  desc[511] = artist[63] = make[63] = model[63] = model2[63] = 0;



  if (!is_raw) goto notraw;

    

  if (!height) height = raw_height;
  if (!width)  width  = raw_width;
  if (height == 2624 && width == 3936)	/* Pentax K10D and Samsung GX10 */
    { height  = 2616;   width  = 3896; }
  if (height == 3136 && width == 4864)  /* Pentax K20D and Samsung GX20 */
    { height  = 3124;   width  = 4688; filters = 0x16161616; }
  if (width == 4352 && (!mystrcmp(model,"K-r") || !mystrcmp(model,"K-x")))
    {			width  = 4309; filters = 0x16161616; }
  if (width >= 4960 && !mystrncmp(model,"K-5",3))
    { left_margin = 10; width  = 4950; filters = 0x16161616; }
  if (width == 4736 && !mystrcmp(model,"K-7"))
    { height  = 3122;   width  = 4684; filters = 0x16161616; top_margin = 2; }
  if (width == 6080 && !mystrcmp(model,"K-3"))
    { left_margin = 4;  width  = 6040; }
  if (width == 7424 && !mystrcmp(model,"645D"))
    { height  = 5502;   width  = 7328; filters = 0x61616161; top_margin = 29;
      left_margin = 48; }
  if (height == 3014 && width == 4096)	/* Ricoh GX200 */
			width  = 4014;
  if (dng_version) {
    if (filters == UINT_MAX) filters = 0;
   
    if (filters) is_raw *= tiff_samples;
    else	 colors  = tiff_samples;
    switch (tiff_compress) {
      case 0:
      case 1:    
	//printf("\t setting decompression to packed dng load raw\n");  
	load_raw = &    packed_dng_load_raw;  break;
      case 7:    
	//printf("\t setting decompression to lossless dng load raw\n"); 
	load_raw = &  lossless_dng_load_raw; 

	break;
      case 34892: 
	{ 
	  /* load_raw = &  lossy_dng_load_raw;  */
	  printf("not handling lossy DNGs. Contact Bevara for suppport.\n");
	  exit(0);
	  break;

	}
      default:    load_raw = 0;
    }
    goto dng_skip;
  }


  /* removed all non-DNG correction code here */

 


dng_skip:
  
 
  if ((use_camera_matrix & (/* use_camera_wb || */ dng_version))
	&& cmatrix[0][0] > 0.125) {
    memcpy (rgb_cam, cmatrix, sizeof cmatrix);
    raw_color = 0;
  }
  /* if (raw_color) adobe_coeff (make, model); */
  /* if (load_raw == &  kodak_radc_load_raw) */
  /*   { */
  /*     printf("not currently supporting kodak radc...exiting...\n"); */
  /*   /\* if (raw_color) adobe_coeff ("Apple","Quicktake"); *\/ */
  /*   } */
  /* if (fuji_width) { */
  /*   fuji_width = width >> !fuji_layout; */
  /*   filters = fuji_width & 1 ? 0x94949494 : 0x49494949; */
  /*   width = (height >> fuji_layout) + fuji_width; */
  /*   height = width - 1; */
  /*   pixel_aspect = 1; */
  /* } else { */
    if (raw_height < height) raw_height = height;
    if (raw_width  < width ) raw_width  = width;
  /* } */
  if (!tiff_bps) tiff_bps = 12;
  if (!maximum) maximum = (1 << tiff_bps) - 1;
  if (!load_raw || height < 22 || width < 22 ||
	tiff_bps > 16 || tiff_samples > 6 || colors > 4)
    is_raw = 0;


  /* if (/\* load_raw == &  kodak_jpeg_load_raw || *\/ */
  /*     load_raw == &  lossy_dng_load_raw)  */
  /*   { */
  /*   is_raw = 0; */
  /*   } */


  if (!cdesc[0])
    mystrcpy (cdesc, colors == 3 ? "RGBG":"GMCY");
  if (!raw_height) raw_height = height;
  if (!raw_width ) raw_width  = width;
  if (filters > 999 && colors == 3)
    filters |= ((filters >> 2 & 0x22222222) |
		(filters << 2 & 0x88888888)) & filters << 1;
notraw:
  if (flip == UINT_MAX) flip = tiff_flip;
  if (flip == UINT_MAX) flip = 0;
}




void   convert_to_rgb()
{
  int row, col, c, i, j, k;
  ushort *img;
  float out[3], out_cam[3][4];
  double num, inverse[3][3];
  static const double xyzd50_srgb[3][3] =
  { { 0.436083, 0.385083, 0.143055 },
    { 0.222507, 0.716888, 0.060608 },
    { 0.013930, 0.097097, 0.714022 } };
  static const double rgb_rgb[3][3] =
  { { 1,0,0 }, { 0,1,0 }, { 0,0,1 } };
  static const double adobe_rgb[3][3] =
  { { 0.715146, 0.284856, 0.000000 },
    { 0.000000, 1.000000, 0.000000 },
    { 0.000000, 0.041166, 0.958839 } };
  static const double wide_rgb[3][3] =
  { { 0.593087, 0.404710, 0.002206 },
    { 0.095413, 0.843149, 0.061439 },
    { 0.011621, 0.069091, 0.919288 } };
  static const double prophoto_rgb[3][3] =
  { { 0.529317, 0.330092, 0.140588 },
    { 0.098368, 0.873465, 0.028169 },
    { 0.016879, 0.117663, 0.865457 } };
  static const double (*out_rgb[])[3] =
  { rgb_rgb, adobe_rgb, wide_rgb, prophoto_rgb, xyz_rgb };
  static const char *name[] =
  { "sRGB", "Adobe RGB (1998)", "WideGamut D65", "ProPhoto D65", "XYZ" };
  static const unsigned phead[] =
  { 1024, 0, 0x2100000, 0x6d6e7472, 0x52474220, 0x58595a20, 0, 0, 0,
    0x61637370, 0, 0, 0x6e6f6e65, 0, 0, 0, 0, 0xf6d6, 0x10000, 0xd32d };
  unsigned pbody[] =
  { 10, 0x63707274, 0, 36,	/* cprt */
	0x64657363, 0, 40,	/* desc */
	0x77747074, 0, 20,	/* wtpt */
	0x626b7074, 0, 20,	/* bkpt */
	0x72545243, 0, 14,	/* rTRC */
	0x67545243, 0, 14,	/* gTRC */
	0x62545243, 0, 14,	/* bTRC */
	0x7258595a, 0, 20,	/* rXYZ */
	0x6758595a, 0, 20,	/* gXYZ */
	0x6258595a, 0, 20 };	/* bXYZ */
  static const unsigned pwhite[] = { 0xf351, 0x10000, 0x116cc };
  unsigned pcurve[] = { 0x63757276, 0, 1, 0x1000000 };

  gamma_curve (gamm[0], gamm[1], 0, 0);
  memcpy (out_cam, rgb_cam, sizeof out_cam);
  raw_color |= colors == 1 || /* document_mode || */
		output_color < 1 || output_color > 5;
  if (!raw_color) {
    oprof = (unsigned *) calloc (phead[0], 1);
    merror (oprof, "convert_to_rgb()");
    memcpy (oprof, phead, sizeof phead);
    if (output_color == 5) oprof[4] = oprof[5];
    oprof[0] = 132 + 12*pbody[0];
    for (i=0; i < pbody[0]; i++) {
      oprof[oprof[0]/4] = i ? (i > 1 ? 0x58595a20 : 0x64657363) : 0x74657874;
      pbody[i*3+2] = oprof[0];
      oprof[0] += (pbody[i*3+3] + 3) & -4;
    }

    memcpy (oprof+32, pbody, sizeof pbody);
    oprof[pbody[5]/4+2] = mystrlen(name[output_color-1]) + 1;
    memcpy ((char *)oprof+pbody[8]+8, pwhite, sizeof pwhite);
    pcurve[3] = (short)(256/gamm[5]+0.5) << 16;
    for (i=4; i < 7; i++)
      memcpy ((char *)oprof+pbody[i*3+2], pcurve, sizeof pcurve);
    pseudoinverse ((double (*)[3]) out_rgb[output_color-1], inverse, 3);
    for (i=0; i < 3; i++)
      for (j=0; j < 3; j++) {
	for (num = k=0; k < 3; k++)
	  num += xyzd50_srgb[i][k] * inverse[j][k];
	oprof[pbody[j*3+23]/4+i+2] = num * 0x10000 + 0.5;
      }
    /* BEVARA: implement local htonl() */
    if (!is_bigendian())
      {
	for (i=0; i < phead[0]/4; i++)
	  {
	    oprof[i] = myswabint(oprof[i]);
	  }
      }
    mystrcpy ((char *)oprof+pbody[2]+8, "auto-generated by dcraw");
    mystrcpy ((char *)oprof+pbody[5]+12, name[output_color-1]);
    for (i=0; i < 3; i++)
      for (j=0; j < colors; j++)
	for (out_cam[i][j] = k=0; k < 3; k++)
	  out_cam[i][j] += out_rgb[output_color-1][i][k] * rgb_cam[k][j];
  }
 



  mymemset (histogram, 0, sizeof histogram);
  for (img=image[0], row=0; row < height; row++)
    for (col=0; col < width; col++, img+=4) {
      if (!raw_color) {
	out[0] = out[1] = out[2] = 0;
	FORCC {
	  out[0] += out_cam[0][c] * img[c];
	  out[1] += out_cam[1][c] * img[c];
	  out[2] += out_cam[2][c] * img[c];
	}
	FORC3 img[c] = CLIP((int) out[c]);
      }
      FORCC histogram[c][img[c] >> 3]++;
    }
  if (colors == 4 && output_color) colors = 3;
}



int   flip_index (int row, int col)
{
  if (flip & 4) SWAP(row,col);
  if (flip & 2) row = iheight - 1 - row;
  if (flip & 1) col = iwidth  - 1 - col;
  return row * iwidth + col;
}



/**    BEVARA: this is the embedded baseline JPEG decoder 
***   from nanojpeg
**/
// NanoJPEG -- KeyJ's Tiny Baseline JPEG Decoder
// version 1.3 (2012-03-05)
// by Martin J. Fiedler <martin.fiedler@gmx.net>
//
// This software is published under the terms of KeyJ's Research License,
// version 0.2. Usage of this software is subject to the following conditions:
// 0. There's no warranty whatsoever. The author(s) of this software can not
//    be held liable for any damages that occur when using this software.
// 1. This software may be used freely for both non-commercial and commercial
//    purposes.
// 2. This software may be redistributed freely as long as no fees are charged
//    for the distribution and this license information is included.
// 3. This software may be modified freely except for this license information,
//    which must not be changed in any way.
// 4. If anything other than configuration, indentation or comments have been
//    altered in the code, the original author(s) must receive a copy of the
//    modified code.


///////////////////////////////////////////////////////////////////////////////
// DOCUMENTATION SECTION                                                     //
// read this if you want to know what this is all about                      //
///////////////////////////////////////////////////////////////////////////////

// INTRODUCTION
// ============
//
// This is a minimal decoder for baseline JPEG images. It accepts memory dumps
// of JPEG files as input and generates either 8-bit grayscale or packed 24-bit
// RGB images as output. It does not parse JFIF or Exif headers; all JPEG files
// are assumed to be either grayscale or YCbCr. CMYK or other color spaces are
// not supported. All YCbCr subsampling schemes with power-of-two ratios are
// supported, as are restart intervals. Progressive or lossless JPEG is not
// supported.
// Summed up, NanoJPEG should be able to decode all images from digital cameras
// and most common forms of other non-progressive JPEG images.
// The decoder is not optimized for speed, it's optimized for simplicity and
// small code. Image quality should be at a reasonable level. A bicubic chroma
// upsampling filter ensures that subsampled YCbCr images are rendered in
// decent quality. The decoder is not meant to deal with broken JPEG files in
// a graceful manner; if anything is wrong with the bitstream, decoding will
// simply fail.
// The code should work with every modern C compiler without problems and
// should not emit any warnings. It uses only (at least) 32-bit integer
// arithmetic and is supposed to be endianness independent and 64-bit clean.
// However, it is not thread-safe.


// COMPILE-TIME CONFIGURATION
// ==========================
//
// The following aspects of NanoJPEG can be controlled with preprocessor
// defines:
//
// _NJ_EXAMPLE_PROGRAM     = Compile a main() function with an example
//                           program.
// _NJ_INCLUDE_HEADER_ONLY = Don't compile anything, just act as a header
//                           file for NanoJPEG. Example:
//                               #define _NJ_INCLUDE_HEADER_ONLY
//                               #include "nanojpeg.c"
//                               int main(void) {
//                                   njInit();
//                                   // your code here
//                                   njDone();
//                               }
// NJ_USE_LIBC=1           = Use the malloc(), free(), memset() and memcpy()
//                           functions from the standard C library (default).
// NJ_USE_LIBC=0           = Don't use the standard C library. In this mode,
//                           external functions njAlloc(), njFreeMem(),
//                           njFillMem() and njCopyMem() need to be defined
//                           and implemented somewhere.
// NJ_USE_WIN32=0          = Normal mode (default).
// NJ_USE_WIN32=1          = If compiling with MSVC for Win32 and
//                           NJ_USE_LIBC=0, NanoJPEG will use its own
//                           implementations of the required C library
//                           functions (default if compiling with MSVC and
//                           NJ_USE_LIBC=0).
// NJ_CHROMA_FILTER=1      = Use the bicubic chroma upsampling filter
//                           (default).
// NJ_CHROMA_FILTER=0      = Use simple pixel repetition for chroma upsampling
//                           (bad quality, but faster and less code).


// API
// ===
//
// For API documentation, read the "header section" below.


// EXAMPLE
// =======
//
// A few pages below, you can find an example program that uses NanoJPEG to
// convert JPEG files into PGM or PPM. To compile it, use something like
//     gcc -O3 -D_NJ_EXAMPLE_PROGRAM -o nanojpeg nanojpeg.c
// You may also add -std=c99 -Wall -Wextra -pedantic -Werror, if you want :)


///////////////////////////////////////////////////////////////////////////////
// HEADER SECTION                                                            //
// copy and pase this into nanojpeg.h if you want                            //
///////////////////////////////////////////////////////////////////////////////

#ifndef _NANOJPEG_H
#define _NANOJPEG_H

// nj_result_t: Result codes for njDecode().
typedef enum _nj_result {
    NJ_OK = 0,        // no error, decoding successful
    NJ_NO_JPEG,       // not a JPEG file
    NJ_UNSUPPORTED,   // unsupported format
    NJ_OUT_OF_MEM,    // out of memory
    NJ_INTERNAL_ERR,  // internal error
    NJ_SYNTAX_ERROR,  // syntax error
    __NJ_FINISHED,    // used internally, will never be reported
} nj_result_t;

// njInit: Initialize NanoJPEG.
// For safety reasons, this should be called at least one time before using
// using any of the other NanoJPEG functions.
void njInit(void);

// njDecode: Decode a JPEG image.
// Decodes a memory dump of a JPEG file into internal buffers.
// Parameters:
//   jpeg = The pointer to the memory dump.
//   size = The size of the JPEG file.
// Return value: The error code in case of failure, or NJ_OK (zero) on success.
nj_result_t njDecode(const void* jpeg, const int size);

// njGetWidth: Return the width (in pixels) of the most recently decoded
// image. If njDecode() failed, the result of njGetWidth() is undefined.
int njGetWidth(void);

// njGetHeight: Return the height (in pixels) of the most recently decoded
// image. If njDecode() failed, the result of njGetHeight() is undefined.
int njGetHeight(void);

// njIsColor: Return 1 if the most recently decoded image is a color image
// (RGB) or 0 if it is a grayscale image. If njDecode() failed, the result
// of njGetWidth() is undefined.
int njIsColor(void);

// njGetImage: Returns the decoded image data.
// Returns a pointer to the most recently image. The memory layout it byte-
// oriented, top-down, without any padding between lines. Pixels of color
// images will be stored as three consecutive bytes for the red, green and
// blue channels. This data format is thus compatible with the PGM or PPM
// file formats and the OpenGL texture formats GL_LUMINANCE8 or GL_RGB8.
// If njDecode() failed, the result of njGetImage() is undefined.
unsigned char* njGetImage(void);

// njGetImageSize: Returns the size (in bytes) of the image data returned
// by njGetImage(). If njDecode() failed, the result of njGetImageSize() is
// undefined.
int njGetImageSize(void);

// njDone: Uninitialize NanoJPEG.
// Resets NanoJPEG's internal state and frees all memory that has been
// allocated at run-time by NanoJPEG. It is still possible to decode another
// image after a njDone() call.
void njDone(void);

#endif//_NANOJPEG_H


///////////////////////////////////////////////////////////////////////////////
// CONFIGURATION SECTION                                                     //
// adjust the default settings for the NJ_ defines here                      //
///////////////////////////////////////////////////////////////////////////////


#ifndef _NJ_EXAMPLE_PROGRAM
    #define _NJ_EXAMPLE_PROGRAM
#endif



#ifndef NJ_USE_LIBC
    #define NJ_USE_LIBC 1
#endif

#ifndef NJ_USE_WIN32
  #ifdef _MSC_VER
    #define NJ_USE_WIN32 (!NJ_USE_LIBC)
  #else
    #define NJ_USE_WIN32 0
  #endif
#endif

#ifndef NJ_CHROMA_FILTER
    #define NJ_CHROMA_FILTER 1
#endif





///////////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION SECTION                                                    //
// you may stop reading here                                                 //
///////////////////////////////////////////////////////////////////////////////

#ifndef _NJ_INCLUDE_HEADER_ONLY

#ifdef _MSC_VER
    #define NJ_INLINE static __inline
    #define NJ_FORCE_INLINE static __forceinline
#else
    #define NJ_INLINE static inline
    #define NJ_FORCE_INLINE static inline
#endif

typedef struct _nj_code {
    unsigned char bits, code;
} nj_vlc_code_t;

typedef struct _nj_cmp {
    int cid;
    int ssx, ssy;
    int width, height;
    int stride;
    int qtsel;
    int actabsel, dctabsel;
    int dcpred;
    unsigned char *pixels;
} nj_component_t;

typedef struct _nj_ctx {
    nj_result_t error;
    const unsigned char *pos;
    int size;
    int length;
    int width, height;
    int mbwidth, mbheight;
    int mbsizex, mbsizey;
    int ncomp;
    nj_component_t comp[3];
    int qtused, qtavail;
    unsigned char qtab[4][64];
    nj_vlc_code_t vlctab[4][65536];
    int buf, bufbits;
    int block[64];
    int rstinterval;
    unsigned char *rgb;
} nj_context_t;

static nj_context_t nj;

static const char njZZ[64] = { 0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18,
11, 4, 5, 12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28, 35,
42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45,
38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63 };

NJ_FORCE_INLINE unsigned char njClip(const int x) {
    return (x < 0) ? 0 : ((x > 0xFF) ? 0xFF : (unsigned char) x);
}

#define W1 2841
#define W2 2676
#define W3 2408
#define W5 1609
#define W6 1108
#define W7 565

NJ_INLINE void njRowIDCT(int* blk) {
    int x0, x1, x2, x3, x4, x5, x6, x7, x8;
    if (!((x1 = blk[4] << 11)
        | (x2 = blk[6])
        | (x3 = blk[2])
        | (x4 = blk[1])
        | (x5 = blk[7])
        | (x6 = blk[5])
        | (x7 = blk[3])))
    {
        blk[0] = blk[1] = blk[2] = blk[3] = blk[4] = blk[5] = blk[6] = blk[7] = blk[0] << 3;
        return;
    }
    x0 = (blk[0] << 11) + 128;
    x8 = W7 * (x4 + x5);
    x4 = x8 + (W1 - W7) * x4;
    x5 = x8 - (W1 + W7) * x5;
    x8 = W3 * (x6 + x7);
    x6 = x8 - (W3 - W5) * x6;
    x7 = x8 - (W3 + W5) * x7;
    x8 = x0 + x1;
    x0 -= x1;
    x1 = W6 * (x3 + x2);
    x2 = x1 - (W2 + W6) * x2;
    x3 = x1 + (W2 - W6) * x3;
    x1 = x4 + x6;
    x4 -= x6;
    x6 = x5 + x7;
    x5 -= x7;
    x7 = x8 + x3;
    x8 -= x3;
    x3 = x0 + x2;
    x0 -= x2;
    x2 = (181 * (x4 + x5) + 128) >> 8;
    x4 = (181 * (x4 - x5) + 128) >> 8;
    blk[0] = (x7 + x1) >> 8;
    blk[1] = (x3 + x2) >> 8;
    blk[2] = (x0 + x4) >> 8;
    blk[3] = (x8 + x6) >> 8;
    blk[4] = (x8 - x6) >> 8;
    blk[5] = (x0 - x4) >> 8;
    blk[6] = (x3 - x2) >> 8;
    blk[7] = (x7 - x1) >> 8;
}

NJ_INLINE void njColIDCT(const int* blk, unsigned char *out, int stride) {
    int x0, x1, x2, x3, x4, x5, x6, x7, x8;
    if (!((x1 = blk[8*4] << 8)
        | (x2 = blk[8*6])
        | (x3 = blk[8*2])
        | (x4 = blk[8*1])
        | (x5 = blk[8*7])
        | (x6 = blk[8*5])
        | (x7 = blk[8*3])))
    {
        x1 = njClip(((blk[0] + 32) >> 6) + 128);
        for (x0 = 8;  x0;  --x0) {
            *out = (unsigned char) x1;
            out += stride;
        }
        return;
    }
    x0 = (blk[0] << 8) + 8192;
    x8 = W7 * (x4 + x5) + 4;
    x4 = (x8 + (W1 - W7) * x4) >> 3;
    x5 = (x8 - (W1 + W7) * x5) >> 3;
    x8 = W3 * (x6 + x7) + 4;
    x6 = (x8 - (W3 - W5) * x6) >> 3;
    x7 = (x8 - (W3 + W5) * x7) >> 3;
    x8 = x0 + x1;
    x0 -= x1;
    x1 = W6 * (x3 + x2) + 4;
    x2 = (x1 - (W2 + W6) * x2) >> 3;
    x3 = (x1 + (W2 - W6) * x3) >> 3;
    x1 = x4 + x6;
    x4 -= x6;
    x6 = x5 + x7;
    x5 -= x7;
    x7 = x8 + x3;
    x8 -= x3;
    x3 = x0 + x2;
    x0 -= x2;
    x2 = (181 * (x4 + x5) + 128) >> 8;
    x4 = (181 * (x4 - x5) + 128) >> 8;
    *out = njClip(((x7 + x1) >> 14) + 128);  out += stride;
    *out = njClip(((x3 + x2) >> 14) + 128);  out += stride;
    *out = njClip(((x0 + x4) >> 14) + 128);  out += stride;
    *out = njClip(((x8 + x6) >> 14) + 128);  out += stride;
    *out = njClip(((x8 - x6) >> 14) + 128);  out += stride;
    *out = njClip(((x0 - x4) >> 14) + 128);  out += stride;
    *out = njClip(((x3 - x2) >> 14) + 128);  out += stride;
    *out = njClip(((x7 - x1) >> 14) + 128);
}

#define njThrow(e) do { nj.error = e; return; } while (0)
#define njCheckError() do { if (nj.error) return; } while (0)

static int njShowBits(int bits) {
    unsigned char newbyte;
    if (!bits) return 0;
    while (nj.bufbits < bits) {
        if (nj.size <= 0) {
            nj.buf = (nj.buf << 8) | 0xFF;
            nj.bufbits += 8;
            continue;
        }
        newbyte = *nj.pos++;
        nj.size--;
        nj.bufbits += 8;
        nj.buf = (nj.buf << 8) | newbyte;
        if (newbyte == 0xFF) {
            if (nj.size) {
                unsigned char marker = *nj.pos++;
                nj.size--;
                switch (marker) {
                    case 0x00:
                    case 0xFF:
                        break;
                    case 0xD9: nj.size = 0; break;
                    default:
                        if ((marker & 0xF8) != 0xD0)
                            nj.error = NJ_SYNTAX_ERROR;
                        else {
                            nj.buf = (nj.buf << 8) | marker;
                            nj.bufbits += 8;
                        }
                }
            } else
                nj.error = NJ_SYNTAX_ERROR;
        }
    }
    return (nj.buf >> (nj.bufbits - bits)) & ((1 << bits) - 1);
}

NJ_INLINE void njSkipBits(int bits) {
    if (nj.bufbits < bits)
        (void) njShowBits(bits);
    nj.bufbits -= bits;
}

NJ_INLINE int njGetBits(int bits) {
    int res = njShowBits(bits);
    njSkipBits(bits);
    return res;
}

NJ_INLINE void njByteAlign(void) {
    nj.bufbits &= 0xF8;
}

static void njSkip(int count) {
    nj.pos += count;
    nj.size -= count;
    nj.length -= count;
    if (nj.size < 0) nj.error = NJ_SYNTAX_ERROR;
}

NJ_INLINE unsigned short njDecode16(const unsigned char *pos) {
    return (pos[0] << 8) | pos[1];
}

static void njDecodeLength(void) {
    if (nj.size < 2) njThrow(NJ_SYNTAX_ERROR);
    nj.length = njDecode16(nj.pos);
    if (nj.length > nj.size) njThrow(NJ_SYNTAX_ERROR);
    njSkip(2);
}

NJ_INLINE void njSkipMarker(void) {
    njDecodeLength();
    njSkip(nj.length);
}

NJ_INLINE void njDecodeSOF(void) {
    int i, ssxmax = 0, ssymax = 0;
    nj_component_t* c;
    njDecodeLength();
    if (nj.length < 9) njThrow(NJ_SYNTAX_ERROR);
    if (nj.pos[0] != 8) njThrow(NJ_UNSUPPORTED);
    nj.height = njDecode16(nj.pos+1);
    nj.width = njDecode16(nj.pos+3);
    nj.ncomp = nj.pos[5];
    njSkip(6);
    switch (nj.ncomp) {
        case 1:
        case 3:
            break;
        default:
            njThrow(NJ_UNSUPPORTED);
    }
    if (nj.length < (nj.ncomp * 3)) njThrow(NJ_SYNTAX_ERROR);
    for (i = 0, c = nj.comp;  i < nj.ncomp;  ++i, ++c) {
        c->cid = nj.pos[0];
        if (!(c->ssx = nj.pos[1] >> 4)) njThrow(NJ_SYNTAX_ERROR);
        if (c->ssx & (c->ssx - 1)) njThrow(NJ_UNSUPPORTED);  // non-power of two
        if (!(c->ssy = nj.pos[1] & 15)) njThrow(NJ_SYNTAX_ERROR);
        if (c->ssy & (c->ssy - 1)) njThrow(NJ_UNSUPPORTED);  // non-power of two
        if ((c->qtsel = nj.pos[2]) & 0xFC) njThrow(NJ_SYNTAX_ERROR);
        njSkip(3);
        nj.qtused |= 1 << c->qtsel;
        if (c->ssx > ssxmax) ssxmax = c->ssx;
        if (c->ssy > ssymax) ssymax = c->ssy;
    }
    if (nj.ncomp == 1) {
        c = nj.comp;
        c->ssx = c->ssy = ssxmax = ssymax = 1;
    }
    nj.mbsizex = ssxmax << 3;
    nj.mbsizey = ssymax << 3;
    nj.mbwidth = (nj.width + nj.mbsizex - 1) / nj.mbsizex;
    nj.mbheight = (nj.height + nj.mbsizey - 1) / nj.mbsizey;
    for (i = 0, c = nj.comp;  i < nj.ncomp;  ++i, ++c) {
        c->width = (nj.width * c->ssx + ssxmax - 1) / ssxmax;
        c->stride = (c->width + 7) & 0x7FFFFFF8;
        c->height = (nj.height * c->ssy + ssymax - 1) / ssymax;
        c->stride = nj.mbwidth * nj.mbsizex * c->ssx / ssxmax;
        if (((c->width < 3) && (c->ssx != ssxmax)) || ((c->height < 3) && (c->ssy != ssymax))) njThrow(NJ_UNSUPPORTED);
        if (!(c->pixels = (unsigned char *) malloc(c->stride * (nj.mbheight * nj.mbsizey * c->ssy / ssymax)))) njThrow(NJ_OUT_OF_MEM);
    }
    //if (nj.ncomp == 3) {
		// output only RGBA now
        nj.rgb = (unsigned char *) malloc(nj.width * nj.height *4);
        if (!nj.rgb) njThrow(NJ_OUT_OF_MEM);
    //}
    njSkip(nj.length);
}

NJ_INLINE void njDecodeDHT(void) {
    int codelen, currcnt, remain, spread, i, j;
    nj_vlc_code_t *vlc;
    static unsigned char counts[16];
    njDecodeLength();
    while (nj.length >= 17) {
        i = nj.pos[0];
        if (i & 0xEC) njThrow(NJ_SYNTAX_ERROR);
        if (i & 0x02) njThrow(NJ_UNSUPPORTED);
        i = (i | (i >> 3)) & 3;  // combined DC/AC + tableid value
        for (codelen = 1;  codelen <= 16;  ++codelen)
            counts[codelen - 1] = nj.pos[codelen];
        njSkip(17);
        vlc = &nj.vlctab[i][0];
        remain = spread = 65536;
        for (codelen = 1;  codelen <= 16;  ++codelen) {
            spread >>= 1;
            currcnt = counts[codelen - 1];
            if (!currcnt) continue;
            if (nj.length < currcnt) njThrow(NJ_SYNTAX_ERROR);
            remain -= currcnt << (16 - codelen);
            if (remain < 0) njThrow(NJ_SYNTAX_ERROR);
            for (i = 0;  i < currcnt;  ++i) {
                register unsigned char code = nj.pos[i];
                for (j = spread;  j;  --j) {
                    vlc->bits = (unsigned char) codelen;
                    vlc->code = code;
                    ++vlc;
                }
            }
            njSkip(currcnt);
        }
        while (remain--) {
            vlc->bits = 0;
            ++vlc;
        }
    }
    if (nj.length) njThrow(NJ_SYNTAX_ERROR);
}

NJ_INLINE void njDecodeDQT(void) {
    int i;
    unsigned char *t;
    njDecodeLength();
    while (nj.length >= 65) {
        i = nj.pos[0];
        if (i & 0xFC) njThrow(NJ_SYNTAX_ERROR);
        nj.qtavail |= 1 << i;
        t = &nj.qtab[i][0];
        for (i = 0;  i < 64;  ++i)
            t[i] = nj.pos[i + 1];
        njSkip(65);
    }
    if (nj.length) njThrow(NJ_SYNTAX_ERROR);
}

NJ_INLINE void njDecodeDRI(void) {
    njDecodeLength();
    if (nj.length < 2) njThrow(NJ_SYNTAX_ERROR);
    nj.rstinterval = njDecode16(nj.pos);
    njSkip(nj.length);
}

static int njGetVLC(nj_vlc_code_t* vlc, unsigned char* code) {
    int value = njShowBits(16);
    int bits = vlc[value].bits;
    if (!bits) { nj.error = NJ_SYNTAX_ERROR; return 0; }
    njSkipBits(bits);
    value = vlc[value].code;
    if (code) *code = (unsigned char) value;
    bits = value & 15;
    if (!bits) return 0;
    value = njGetBits(bits);
    if (value < (1 << (bits - 1)))
        value += ((-1) << bits) + 1;
    return value;
}

NJ_INLINE void njDecodeBlock(nj_component_t* c, unsigned char* out) {
    unsigned char code = 0;
    int value, coef = 0;
    memset(nj.block, 0, sizeof(nj.block));
    c->dcpred += njGetVLC(&nj.vlctab[c->dctabsel][0], NULL);
    nj.block[0] = (c->dcpred) * nj.qtab[c->qtsel][0];
    do {
        value = njGetVLC(&nj.vlctab[c->actabsel][0], &code);
        if (!code) break;  // EOB
        if (!(code & 0x0F) && (code != 0xF0)) njThrow(NJ_SYNTAX_ERROR);
        coef += (code >> 4) + 1;
        if (coef > 63) njThrow(NJ_SYNTAX_ERROR);
        nj.block[(int) njZZ[coef]] = value * nj.qtab[c->qtsel][coef];
    } while (coef < 63);
    for (coef = 0;  coef < 64;  coef += 8)
        njRowIDCT(&nj.block[coef]);
    for (coef = 0;  coef < 8;  ++coef)
        njColIDCT(&nj.block[coef], &out[coef], c->stride);
}

NJ_INLINE void njDecodeScan(void) {
    int i, mbx, mby, sbx, sby;
    int rstcount = nj.rstinterval, nextrst = 0;
    nj_component_t* c;
    njDecodeLength();
    if (nj.length < (4 + 2 * nj.ncomp)) njThrow(NJ_SYNTAX_ERROR);
    if (nj.pos[0] != nj.ncomp) njThrow(NJ_UNSUPPORTED);
    njSkip(1);
    for (i = 0, c = nj.comp;  i < nj.ncomp;  ++i, ++c) {
        if (nj.pos[0] != c->cid) njThrow(NJ_SYNTAX_ERROR);
        if (nj.pos[1] & 0xEE) njThrow(NJ_SYNTAX_ERROR);
        c->dctabsel = nj.pos[1] >> 4;
        c->actabsel = (nj.pos[1] & 1) | 2;
        njSkip(2);
    }
    if (nj.pos[0] || (nj.pos[1] != 63) || nj.pos[2]) njThrow(NJ_UNSUPPORTED);
    njSkip(nj.length);
    for (mbx = mby = 0;;) {
        for (i = 0, c = nj.comp;  i < nj.ncomp;  ++i, ++c)
            for (sby = 0;  sby < c->ssy;  ++sby)
                for (sbx = 0;  sbx < c->ssx;  ++sbx) {
                    njDecodeBlock(c, &c->pixels[((mby * c->ssy + sby) * c->stride + mbx * c->ssx + sbx) << 3]);
                    njCheckError();
                }
        if (++mbx >= nj.mbwidth) {
            mbx = 0;
            if (++mby >= nj.mbheight) break;
        }
        if (nj.rstinterval && !(--rstcount)) {
            njByteAlign();
            i = njGetBits(16);
            if (((i & 0xFFF8) != 0xFFD0) || ((i & 7) != nextrst)) njThrow(NJ_SYNTAX_ERROR);
            nextrst = (nextrst + 1) & 7;
            rstcount = nj.rstinterval;
            for (i = 0;  i < 3;  ++i)
                nj.comp[i].dcpred = 0;
        }
    }
    nj.error = __NJ_FINISHED;
}

#if NJ_CHROMA_FILTER

#define CF4A (-9)
#define CF4B (111)
#define CF4C (29)
#define CF4D (-3)
#define CF3A (28)
#define CF3B (109)
#define CF3C (-9)
#define CF3X (104)
#define CF3Y (27)
#define CF3Z (-3)
#define CF2A (139)
#define CF2B (-11)
#define CF(x) njClip(((x) + 64) >> 7)

NJ_INLINE void njUpsampleH(nj_component_t* c) {
    const int xmax = c->width - 3;
    unsigned char *out, *lin, *lout;
    int x, y;
    out = (unsigned char *) malloc((c->width * c->height) << 1);
    if (!out) njThrow(NJ_OUT_OF_MEM);
    lin = c->pixels;
    lout = out;
    for (y = c->height;  y;  --y) {
        lout[0] = CF(CF2A * lin[0] + CF2B * lin[1]);
        lout[1] = CF(CF3X * lin[0] + CF3Y * lin[1] + CF3Z * lin[2]);
        lout[2] = CF(CF3A * lin[0] + CF3B * lin[1] + CF3C * lin[2]);
        for (x = 0;  x < xmax;  ++x) {
            lout[(x << 1) + 3] = CF(CF4A * lin[x] + CF4B * lin[x + 1] + CF4C * lin[x + 2] + CF4D * lin[x + 3]);
            lout[(x << 1) + 4] = CF(CF4D * lin[x] + CF4C * lin[x + 1] + CF4B * lin[x + 2] + CF4A * lin[x + 3]);
        }
        lin += c->stride;
        lout += c->width << 1;
        lout[-3] = CF(CF3A * lin[-1] + CF3B * lin[-2] + CF3C * lin[-3]);
        lout[-2] = CF(CF3X * lin[-1] + CF3Y * lin[-2] + CF3Z * lin[-3]);
        lout[-1] = CF(CF2A * lin[-1] + CF2B * lin[-2]);
    }
    c->width <<= 1;
    c->stride = c->width;
    free(c->pixels);
    c->pixels = out;
}

NJ_INLINE void njUpsampleV(nj_component_t* c) {
    const int w = c->width, s1 = c->stride, s2 = s1 + s1;
    unsigned char *out, *cin, *cout;
    int x, y;
    out = (unsigned char *) malloc((c->width * c->height) << 1);
    if (!out) njThrow(NJ_OUT_OF_MEM);
    for (x = 0;  x < w;  ++x) {
        cin = &c->pixels[x];
        cout = &out[x];
        *cout = CF(CF2A * cin[0] + CF2B * cin[s1]);  cout += w;
        *cout = CF(CF3X * cin[0] + CF3Y * cin[s1] + CF3Z * cin[s2]);  cout += w;
        *cout = CF(CF3A * cin[0] + CF3B * cin[s1] + CF3C * cin[s2]);  cout += w;
        cin += s1;
        for (y = c->height - 3;  y;  --y) {
            *cout = CF(CF4A * cin[-s1] + CF4B * cin[0] + CF4C * cin[s1] + CF4D * cin[s2]);  cout += w;
            *cout = CF(CF4D * cin[-s1] + CF4C * cin[0] + CF4B * cin[s1] + CF4A * cin[s2]);  cout += w;
            cin += s1;
        }
        cin += s1;
        *cout = CF(CF3A * cin[0] + CF3B * cin[-s1] + CF3C * cin[-s2]);  cout += w;
        *cout = CF(CF3X * cin[0] + CF3Y * cin[-s1] + CF3Z * cin[-s2]);  cout += w;
        *cout = CF(CF2A * cin[0] + CF2B * cin[-s1]);
    }
    c->height <<= 1;
    c->stride = c->width;
    free(c->pixels);
    c->pixels = out;
}

#else

NJ_INLINE void njUpsample(nj_component_t* c) {
    int x, y, xshift = 0, yshift = 0;
    unsigned char *out, *lin, *lout;
    while (c->width < nj.width) { c->width <<= 1; ++xshift; }
    while (c->height < nj.height) { c->height <<= 1; ++yshift; }
    out = malloc(c->width * c->height);
    if (!out) njThrow(NJ_OUT_OF_MEM);
    lin = c->pixels;
    lout = out;
    for (y = 0;  y < c->height;  ++y) {
        lin = &c->pixels[(y >> yshift) * c->stride];
        for (x = 0;  x < c->width;  ++x)
            lout[x] = lin[x >> xshift];
        lout += c->width;
    }
    c->stride = c->width;
    free(c->pixels);
    c->pixels = out;
}

#endif

NJ_INLINE void njConvert() {
    int i;
    nj_component_t* c;
    for (i = 0, c = nj.comp;  i < nj.ncomp;  ++i, ++c) {
        #if NJ_CHROMA_FILTER
            while ((c->width < nj.width) || (c->height < nj.height)) {
                if (c->width < nj.width) njUpsampleH(c);
                njCheckError();
                if (c->height < nj.height) njUpsampleV(c);
                njCheckError();
            }
        #else
            if ((c->width < nj.width) || (c->height < nj.height))
                njUpsample(c);
        #endif
        if ((c->width < nj.width) || (c->height < nj.height)) njThrow(NJ_INTERNAL_ERR);
    }
    if (nj.ncomp == 3) {
        // convert to RGB
        int x, yy;
        unsigned char *prgb = nj.rgb;
        const unsigned char *py  = nj.comp[0].pixels;
        const unsigned char *pcb = nj.comp[1].pixels;
        const unsigned char *pcr = nj.comp[2].pixels;
        for (yy = nj.height;  yy;  --yy) {
            for (x = 0;  x < nj.width;  ++x) {
                register int y = py[x] << 8;
                register int cb = pcb[x] - 128;
                register int cr = pcr[x] - 128;
                *prgb++ = njClip((y            + 359 * cr + 128) >> 8);
                *prgb++ = njClip((y -  88 * cb - 183 * cr + 128) >> 8);
                *prgb++ = njClip((y + 454 * cb            + 128) >> 8);
				*prgb++ = 255; // adding in RGBA alpha value
            }
            py += nj.comp[0].stride;
            pcb += nj.comp[1].stride;
            pcr += nj.comp[2].stride;
        }
    } else if (nj.comp[0].width != nj.comp[0].stride) {
        // convert to RGB
        int x, yy;
        unsigned char *prgb = nj.rgb;
        const unsigned char *py  = &nj.comp[0].pixels[nj.comp[0].stride];
		unsigned char tmp;
        for (yy = nj.height;  yy;  --yy) {
            for (x = 0;  x < nj.width;  ++x) {
	      //register int y = py[x] << 8;
	      tmp = py[x];
	      *prgb++ = tmp;
	      *prgb++ = tmp;
	      *prgb++ = tmp;
		  *prgb++ = 255; // adding in RGBA alpha value
            }
            py += nj.comp[0].stride;
		}
    }
}

void njInit(void) {
    memset(&nj, 0, sizeof(nj_context_t));
}

void njDone(void) {
    int i;
    for (i = 0;  i < 3;  ++i)
        if (nj.comp[i].pixels) free((void*) nj.comp[i].pixels);
    if (nj.rgb) free((void*) nj.rgb);
    njInit();
}

nj_result_t njDecode(const void* jpeg, const int size) {
    njDone();
    nj.pos = (const unsigned char*) jpeg;
    nj.size = size & 0x7FFFFFFF;
    if (nj.size < 2) return NJ_NO_JPEG;
    if ((nj.pos[0] ^ 0xFF) | (nj.pos[1] ^ 0xD8)) return NJ_NO_JPEG;
    njSkip(2);
    while (!nj.error) {
        if ((nj.size < 2) || (nj.pos[0] != 0xFF)) return NJ_SYNTAX_ERROR;
        njSkip(2);
        switch (nj.pos[-1]) {
            case 0xC0: njDecodeSOF();  break;
            case 0xC4: njDecodeDHT();  break;
            case 0xDB: njDecodeDQT();  break;
            case 0xDD: njDecodeDRI();  break;
            case 0xDA: njDecodeScan(); break;
            case 0xFE: njSkipMarker(); break;
            default:
                if ((nj.pos[-1] & 0xF0) == 0xE0)
                    njSkipMarker();
                else
                    return NJ_UNSUPPORTED;
        }
    }
    if (nj.error != __NJ_FINISHED) return nj.error;
    nj.error = NJ_OK;
    njConvert();
    return nj.error;
}

int njGetWidth(void)            { return nj.width; }
int njGetHeight(void)           { return nj.height; }
int njIsColor(void)             { return (nj.ncomp != 1); }
unsigned char* njGetImage(void) { return  nj.rgb; } // outputting only RGBA
int njGetImageSize(void)        { return nj.width * nj.height * 4; } // changed to RGBA

#endif // _NJ_INCLUDE_HEADER_ONLY



int EMSCRIPTEN_KEEPALIVE imgShutdown()
{

   
    if (meta_data) free (meta_data);
    if (ofname) free (ofname);
    if (oprof) free (oprof);
    if (image) free (image);
	if (outbuf != NULL)
     free(outbuf);

    return GF_OK;

}



int dngParse(char *inbuf, int insize )
{
    status = 1;
    raw_image = 0;
    image = 0;
    oprof = 0;
    meta_data = 0;
    //ofname = 0;
 
    DATALEN = insize;
    
    status = (identify(inbuf,insize),!is_raw);

	printf("is raw after identify for DNG is %d\n",is_raw);
	
	// reserve a component -- if available -- for metadata 
	if (numComponents < MAX_NUM_COMPONENTS) {
		component[numComponents].type = TYPE_METADATA;
		component[numComponents].ifd_loc = 0;  // assume valid IFD in 0 to MAX_NUM_IFD range
		component[numComponents].width = 0;
		component[numComponents].height = 0;
		component[numComponents].size = 0;
		++numComponents;
	}
	
	
    return status;
}
  



int dngDecodeMainImage(char *inbuf, int insize, int ifd_idx)
{
  
  // TODO: need to copy the height, width and other params
  // from the tiff_ifd into the vars, also consider getting
  // rid of all of the globals, as detailed up top



    if (!is_raw) 
    {
		printf("is not a raw file....exiting\n");
		return GF_NOT_SUPPORTED;
    }
   

    shrink = filters && (half_size || (!identify_only &&
	(threshold || aber[0] != 1 || aber[2] != 1)));
    iheight = (height + shrink) >> shrink;
    iwidth  = (width  + shrink) >> shrink;


    if (meta_length) {
      meta_data = (char *) malloc (meta_length);
      merror (meta_data, "main()");
    }
    if (filters || colors == 1) {
      raw_image = (ushort *) calloc ((raw_height+7), raw_width*2);
      merror (raw_image, "main()");
    } else {
      image = (ushort (*)[4]) calloc (iheight, iwidth*sizeof *image);
      merror (image, "main()");
    }
   
    /* if (shot_select >= is_raw) */
    /*   printf (" \"-s %d\" requests a nonexistent image!\n", shot_select); */

    // Bevara: changed
    //DATALOC = data_offset;

    DATALOC = tiff_ifd[ifd_idx].offset;

   
    //lossless_dng_load_raw(inbuf);

    (*load_raw)(inbuf);


    iheight = (height + shrink) >> shrink;
    iwidth  = (width  + shrink) >> shrink;


    if (raw_image)
      {

 	
      image = (ushort (*)[4]) calloc (iheight, iwidth*sizeof *image);
      merror (image, "main()");
      crop_masked_pixels(inbuf);
      free (raw_image);
      }

  

    /* if (user_qual >= 0) quality = user_qual; */
    i = cblack[3];
    FORC3 if (i > cblack[c]) i = cblack[c];
    FORC4 cblack[c] -= i;
    black += i;
    i = cblack[6];
    FORC (cblack[4] * cblack[5])
      if (i > cblack[6+c]) i = cblack[6+c];
    FORC (cblack[4] * cblack[5])
      cblack[6+c] -= i;
    black += i;
    /* if (user_black >= 0) black = user_black; */
    FORC4 cblack[c] += black;
    /* if (user_sat > 0) maximum = user_sat; */


  
    /* if (document_mode < 2) */
    scale_colors();

    pre_interpolate();



    /* Bevara: currently don't give users the option to select
       interpolation type. If a particular image calls for it - add the 
	   interpolator back in */

    /* if (filters && !document_mode) */
    /*   { */
       
      /* if (quality == 0) */
      /* 	lin_interpolate(); */
      /* else if (quality == 1 || colors > 3) */
      /* 	vng_interpolate(); */
      /* else if (quality == 2 && filters > 1000) */
      /* 	ppg_interpolate(); */
      /* else if (filters == 9) */
      /* 	xtrans_interpolate (quality*2-3); */
      /* else */
 	ahd_interpolate();
      /* } */


    if (mix_green)
      for (colors=3, i=0; i < height*width; i++)
 	image[i][1] = (image[i][1] + image[i][3]) >> 1;

   
   
    convert_to_rgb();
	// at this point colors should be RGB
	
    outbuf = (unsigned char *) malloc(4*height*width); // going to RGBA
   
 	uchar *ppm;
 	uchar *outbuf_ptr;
 	int c, row, col, soff, rstep, cstep;
 	int perc, val, total, white=0x2000;
 	int wr;

 	perc = width * height * 0.01;		/* 99th percentile white level */
 	/* if (fuji_width) perc /= 2; */
 	if (!((highlight & ~2) /* || no_auto_bright */))
 	  for (white=c=0; c < colors; c++) {
 	    for (val=0x2000, total=0; --val > 32; )
 	      if ((total += histogram[c][val]) > perc) break;
 	    if (white < val) white = val;
 	  }
 	gamma_curve (gamm[0], gamm[1], 2, (white << 3)/bright);
 	iheight = height;
 	iwidth  = width;
 	if (flip & 4) SWAP(height,width);
 	ppm = (uchar *) calloc (width, colors*output_bps/8);

 	outbuf_ptr=outbuf;
  
 	soff  = flip_index (0, 0);
 	cstep = flip_index (0, 1) - soff;
 	rstep = flip_index (1, 0) - flip_index (0, width);
 	for (row=0; row < height; row++, soff += rstep) {
 	  for (col=0; col < width; col++, soff += cstep)
	    
 	    /*BEVARA - only allow 8 bits per sample */
 	    FORCC ppm [col*colors+c] = curve[image[soff][c]] >> 8;
	 
 	  /* fwrite (ppm, colors*output_bps/8, width, ofp); */
	  // add in the alpha channel 
 	  for (wr=0;wr< (colors*output_bps/8)*width; wr=wr+3)
 	    {
 	      *outbuf_ptr = ppm[wr]; ++outbuf_ptr;
		  *outbuf_ptr = ppm[wr+1]; ++outbuf_ptr;
		  *outbuf_ptr = ppm[wr+2]; ++outbuf_ptr;
		  *outbuf_ptr = 255; ++outbuf_ptr;
 	    }
 	}
 	free (ppm);

   

 	outwidth=width;
 	outheight = height;

	return status;
}

int  dngDecodeThumbnail(char *inbuf, int ifd_num)
 {

   thumb_width  = tiff_ifd[ifd_num].width;
   thumb_height = tiff_ifd[ifd_num].height;
   thumb_offset = tiff_ifd[ifd_num].offset;	    


   unsigned char *tptr;
   int tloop, tmpoffset, thumb_len;
   int row, col, soff, rstep, cstep;

	// Bevara: now outputting RGBA
   thumb_len = thumb_width*thumb_height*4;

   if (outbuf != NULL)
     free(outbuf);
   outbuf = (unsigned char *) malloc(thumb_len);

   if (tiff_ifd[ifd_num].comp == 1)
     {
       tptr = outbuf;
       tmpoffset = thumb_offset;

       /* only rotate if orientation is 5 */
       if (flip&4)
	 {
	   /* reset iheight and iwidth to use flip_index*/
	   iheight = thumb_height;
	   iwidth  = thumb_width;
	   SWAP(thumb_height,thumb_width);

	   soff  = flip_index (0, 0)*3;
	   cstep = flip_index (0, 1)*3 - soff;
	   rstep = flip_index (1, 0)*3 - (flip_index (0, thumb_width)*3);

	   for (row=0; row < thumb_height; row++, soff += rstep) {
	     for (col=0; col < thumb_width; col++, soff += cstep) 
	       {
		 /* copy each of the colors */
		 *tptr = *(inbuf+tmpoffset+soff); ++tptr;
		 *tptr = *(inbuf+tmpoffset+soff+1); ++tptr;
		 *tptr = *(inbuf+tmpoffset+soff+2); ++tptr;
		 *tptr = 255; ++tptr;  // insert alpha channel
	       }
	   }
	 }
       else 
	 { /* don't rotate */
	   for (tloop=0;tloop<thumb_len;tloop=tloop+3)
	     {
	       *tptr = *(inbuf+tmpoffset); ++tmpoffset; ++tptr;
		   *tptr = *(inbuf+tmpoffset); ++tmpoffset; ++tptr;
		   *tptr = *(inbuf+tmpoffset); ++tmpoffset; ++tptr;
		   *tptr = 255;  ++tptr; // insert alpha channel
	     }
	 }
	outwidth=thumb_width;
 	outheight = thumb_height;
	
	return GF_OK;
     }


   if (tiff_ifd[ifd_num].comp == 7)
     {
       /* TODO: can just output the JPEG by calling jpeg_thumb */
       /* ALT:  decode the jpeg */
       /* need to reset to thumbnail info */


      

       DATALOC =  thumb_offset;
	
       tptr = outbuf;
       
       njInit();

       if (njDecode(inbuf+DATALOC, DATALEN-DATALOC)) {
			return -5;
       }

       /* only rotate if orientation is 5 */
       if (flip&4)
	 {
	   unsigned char *decbuf = njGetImage();
	   /* reset iheight and iwidth to use flip_index*/
	   iheight = thumb_height;
	   iwidth  = thumb_width;
	   SWAP(thumb_height,thumb_width);

	   // switched from 3 to convert to RGBA
	   soff  = flip_index (0, 0)*4;
	   cstep = flip_index (0, 1)*4 - soff;
	   rstep = flip_index (1, 0)*4 - (flip_index (0, thumb_width)*4);

	   for (row=0; row < thumb_height; row++, soff += rstep) {
	     for (col=0; col < thumb_width; col++, soff += cstep) 
	       {
		 /* copy each of the colors */
		 *tptr = *(decbuf+soff); ++tptr;
		 *tptr = *(decbuf+soff+1); ++tptr;
		 *tptr = *(decbuf+soff+2); ++tptr;
		 *tptr = *(decbuf+soff+3); ++tptr; // insert alpha channel 
	       }
	   }
	 }
       else 
	 { /* don't rotate */
	   memcpy(outbuf, njGetImage(), njGetImageSize()); // should return RGBA	
	 }

       njDone();

       outwidth=thumb_width;
       outheight = thumb_height;
	
	return GF_OK;

     }
	 


    return GF_NOT_SUPPORTED;

}


int  dngExtractXMP(char *inbuf, int ifd_num)
 {


   if (outbuf != NULL)
     free(outbuf);
 
 
    //printf("start is %d stop is %d\n",tiff_ifd[ifd_num].XMP_start, tiff_ifd[ifd_num].XMP_stop);
    outbuf = (unsigned char *) malloc(tiff_ifd[ifd_num].XMP_stop -tiff_ifd[ifd_num].XMP_start );
	memcpy(outbuf, inbuf+tiff_ifd[ifd_num].XMP_start,tiff_ifd[ifd_num].XMP_stop -tiff_ifd[ifd_num].XMP_start);  
   
   return GF_OK;
 }


 int  dngExtractMetadata()
 {

	int outbufPtr = 0;
	char buffer[100];
	int c;

	if (outbuf != NULL)
     free(outbuf);
 
    outbuf = (unsigned char *) malloc(MAX_METADATA_LENGTH);
	
	// This is the metadata production code; add as a final IFD
	if (make[0]) {	 
      //printf ("\nFilename: %s\n", ifname);
	  
      sprintf (buffer,"Camera: %s %s\n", make, model);
	  memcpy(outbuf+outbufPtr,buffer,strlen(buffer)); outbufPtr += strlen(buffer);
	  
	  
      if (artist[0])
	  {
			sprintf (buffer,"Owner: %s\n", artist);
			memcpy(outbuf+outbufPtr,buffer,strlen(buffer)); outbufPtr += strlen(buffer);
			
	  }
      if (dng_version) {
			sprintf (buffer,"DNG Version: ");
			memcpy(outbuf+outbufPtr,buffer,strlen(buffer)); outbufPtr += strlen(buffer);
			
			for (i=24; i >= 0; i -= 8)
			{
			  sprintf (buffer,"%d%c", dng_version >> i & 255, i ? '.':'\n');
			  memcpy(outbuf+outbufPtr,buffer,strlen(buffer)); outbufPtr += strlen(buffer);
			 
			}
      }
	  //printf ("Timestamp: %s", ctime(&timestamp));
      sprintf (buffer,"ISO speed: %d\n", (int) iso_speed);
	  memcpy(outbuf+outbufPtr,buffer,strlen(buffer)); outbufPtr += strlen(buffer);
	  
      sprintf (buffer,"Shutter: ");
	  memcpy(outbuf+outbufPtr,buffer,strlen(buffer)); outbufPtr += strlen(buffer);
	  
      if (shutter > 0 && shutter < 1)
	  {
			//shutter = (printf ("1/"), 1 / shutter);
			sprintf (buffer,"1/%0.1f sec\n", 1/shutter);
			memcpy(outbuf+outbufPtr,buffer,strlen(buffer)); outbufPtr += strlen(buffer);
			
			
	  }
	  else{
		  sprintf (buffer,"%0.1f sec\n", shutter);
		  memcpy(outbuf+outbufPtr,buffer,strlen(buffer)); outbufPtr += strlen(buffer);
		  
	  }
      sprintf (buffer,"Aperture: f/%0.1f\n", aperture);
	  memcpy(outbuf+outbufPtr,buffer,strlen(buffer)); outbufPtr += strlen(buffer);
	  
      sprintf (buffer,"Focal length: %0.1f mm\n", focal_len);
	  memcpy(outbuf+outbufPtr,buffer,strlen(buffer)); outbufPtr += strlen(buffer);
	  
      sprintf (buffer,"Embedded ICC profile: %s\n", profile_length ? "yes":"no");
	  memcpy(outbuf+outbufPtr,buffer,strlen(buffer)); outbufPtr += strlen(buffer);
	  
      sprintf (buffer,"Number of raw images: %d\n", is_raw);
	  memcpy(outbuf+outbufPtr,buffer,strlen(buffer)); outbufPtr += strlen(buffer);
	  
      if (pixel_aspect != 1)
	  {
			sprintf (buffer,"Pixel Aspect Ratio: %0.6f\n", pixel_aspect);
			memcpy(outbuf+outbufPtr,buffer,strlen(buffer)); outbufPtr += strlen(buffer);
			

	  }
      if (thumb_offset)
	  {
			sprintf (buffer,"Thumb size:  %4d x %d\n", thumb_width, thumb_height);
			memcpy(outbuf+outbufPtr,buffer,strlen(buffer)); outbufPtr += strlen(buffer);
			

	  }
      sprintf (buffer,"Full size:   %4d x %d\n", raw_width, raw_height);
	  memcpy(outbuf+outbufPtr,buffer,strlen(buffer)); outbufPtr += strlen(buffer);
	 

    }
	
    shrink = filters && (half_size || (!identify_only &&
	(threshold || aber[0] != 1 || aber[2] != 1)));
    iheight = (height + shrink) >> shrink;
    iwidth  = (width  + shrink) >> shrink;
    
      
	//if (document_mode == 3) {
	//  top_margin = left_margin = fuji_width = 0;
	//  height = raw_height;
	//  width  = raw_width;
	//}
	iheight = (height + shrink) >> shrink;
	iwidth  = (width  + shrink) >> shrink;
	
	if (flip & 4)
	  SWAP(iheight,iwidth);
	sprintf (buffer,"Image size:  %4d x %d\n", width, height);
	memcpy(outbuf+outbufPtr,buffer,strlen(buffer)); outbufPtr += strlen(buffer);
	  
	sprintf (buffer,"Output size: %4d x %d\n", iwidth, iheight);
	memcpy(outbuf+outbufPtr,buffer,strlen(buffer)); outbufPtr += strlen(buffer);
	  
	sprintf (buffer,"Raw colors: %d", colors);
	memcpy(outbuf+outbufPtr,buffer,strlen(buffer)); outbufPtr += strlen(buffer);
	  
	if (filters) {
	  int fhigh = 2, fwide = 2;
	  if ((filters ^ (filters >>  8)) & 0xff)   fhigh = 4;
	  if ((filters ^ (filters >> 16)) & 0xffff) fhigh = 8;
	  if (filters == 1) fhigh = fwide = 16;
	  if (filters == 9) fhigh = fwide = 6;
	  sprintf (buffer,"\nFilter pattern: ");
	  memcpy(outbuf+outbufPtr,buffer,strlen(buffer)); outbufPtr += strlen(buffer);
	  
   
	  for (i=0; i < fhigh; i++)
	  {  
		// for (c = i && putchar('/') && 0; c < fwide; c++)   UGH! Change to the following:
		if (i>0)
		{
			sprintf (buffer,"/");
			memcpy(outbuf+outbufPtr,buffer,strlen(buffer)); outbufPtr += strlen(buffer);
		}
	    
		for (c = i; c < fwide; c++)
		{
	      sprintf (buffer,"%c",cdesc[fcol(i,c)]);
		  memcpy(outbuf+outbufPtr,buffer,strlen(buffer)); outbufPtr += strlen(buffer);		
		}
			
	  }
	}
	sprintf (buffer,"\nDaylight multipliers:");
	memcpy(outbuf+outbufPtr,buffer,strlen(buffer)); outbufPtr += strlen(buffer);
	 
	for (c=0;  c<colors; ++c) {
		sprintf(buffer," %f", pre_mul[c]);
		memcpy(outbuf+outbufPtr,buffer,strlen(buffer)); outbufPtr += strlen(buffer);
	}
	if (cam_mul[0] > 0) {
	  sprintf (buffer,"\nCamera multipliers:");
	  memcpy(outbuf+outbufPtr,buffer,strlen(buffer)); outbufPtr += strlen(buffer);
	  
	  for (c=0;  c<4; ++c) {
		sprintf(buffer," %f", cam_mul[c]);
		memcpy(outbuf+outbufPtr,buffer,strlen(buffer)); outbufPtr += strlen(buffer);
	  }
	}
	sprintf (buffer,"\n"); 
	memcpy(outbuf+outbufPtr,buffer,strlen(buffer)+1); outbufPtr += strlen(buffer+1);

	printf("\nOutbuf is:\n %s\n",outbuf);
	
   
   return GF_OK;
 }
 

int EMSCRIPTEN_KEEPALIVE imgSetup()
{

  /* pointer to input data stream */
  /* for reading data */
  DATALOC = 0;
  DATALEN = 0;

  /* set these values once we receive the data to decode */
  globalInbuf=NULL;
  globalInsize=0;
  
  outbuf = NULL;

 /* explicitly zero out the output params */
 /* TODO: why do we need these? */
  out_size = 0;
  width=0;
  height=0;
  output_bps=8;

  //extra output params
  outwidth = 0;
  outheight = 0;
  numComponents=0;
  outsize=0;

  // initializae the IFD info, use a loop 
  // in case memset gives problems
  int i;
  for (i=0; i<MAX_NUM_IFDS; ++i)
    {
      tiff_ifd[i].type =0;
      tiff_ifd[i].dng_comp =0; 
      tiff_ifd[i].XMP_start =0;
      tiff_ifd[i].XMP_stop =0;
      tiff_ifd[i].XMP_coding =0;
      tiff_ifd[i].width =0; 
      tiff_ifd[i].height =0;
      tiff_ifd[i].bps =0;
      tiff_ifd[i].comp =0;
      tiff_ifd[i].phint =0;
      tiff_ifd[i].offset =0; 
      tiff_ifd[i].flip =0; 
      tiff_ifd[i].samples =0; 
      tiff_ifd[i].bytes =0; 
      tiff_ifd[i].endoffset =0;
      tiff_ifd[i].tile_width =0; 
      tiff_ifd[i].tile_length =0;
      tiff_ifd[i].shutter = 0.0;
    }


	for (i=0; i<MAX_NUM_COMPONENTS; ++i)
    {
		component[i].type = TYPE_UNKNOWN;
		component[i].ifd_loc = -1;  // assume valid IFD in 0 to MAX_NUM_IFD range
		component[i].width = 0;
		component[i].height = 0;
		component[i].size = 0;
	}
	
  return GF_OK;
}




// returns height of associated image; for XMP and similar returns 0
int EMSCRIPTEN_KEEPALIVE getHeight(int idx)
{
	if (idx < MAX_NUM_COMPONENTS)
		return component[idx].height;
	else
		return 0;
}


// returns width of associated image; for XMP and similar returns 0
int EMSCRIPTEN_KEEPALIVE getWidth(int idx)
{
	if (idx < MAX_NUM_COMPONENTS)
		return component[idx].width;
	else
		return 0;
}


// returns the size of the output data, for RGBA = width*height*4 for XMP size in chars
int EMSCRIPTEN_KEEPALIVE getSize(int idx)
{
	if (idx < MAX_NUM_COMPONENTS)
		return component[idx].size;
	else
		return 0;
}


// For a particular component, returns the type from list:TYPE_UNKNOWN, TYPE_MAIN_IMAGE, TYPE_THUMBNAIL, TYPE_XMP 
int EMSCRIPTEN_KEEPALIVE getComponentType(int idx)
{
	if (idx < MAX_NUM_COMPONENTS)
		return component[idx].type;
	else
		return TYPE_UNKNOWN;
}


// Returns a pointer to decoded or raw data as appropriate
unsigned char*  EMSCRIPTEN_KEEPALIVE  getData(int idx)
{
	//TODO
	// check IFD, decode with appropriate decoder, set output widht/height/size 
	//  in component
	
	if (outbuf != NULL)
		free(outbuf);
	
	
	printf("component= %d\n",component[idx].type);
	
	//thumbnail
	if (component[idx].type == TYPE_THUMBNAIL)
		{	
			dngDecodeThumbnail(globalInbuf, component[idx].ifd_loc);
			component[idx].width = outwidth;
			component[idx].height = outheight;	  
			component[idx].size = outwidth*outheight*4; // hardcoding the RGBA output
			return outbuf;
    	}
	// XMP 
	else if (component[idx].type == TYPE_XMP)
		{
			printf("the XMP IFD is %d\n",component[idx].ifd_loc);
			dngExtractXMP(globalInbuf, component[idx].ifd_loc);
			// component width and height should be 0
			component[idx].size = tiff_ifd[component[idx].ifd_loc].XMP_stop -tiff_ifd[component[idx].ifd_loc].XMP_start;
			return outbuf;
		}
	// metadata
	else if (component[idx].type == TYPE_METADATA)
		{
			printf("the metadata IFD is %d\n",component[idx].ifd_loc);
			dngExtractMetadata();
			return outbuf;
		}
	//Main
	else if (component[idx].type == TYPE_MAIN_IMAGE)
		{	
			dngDecodeMainImage(globalInbuf,globalInsize, component[idx].ifd_loc);
			component[idx].width = outwidth;
			component[idx].height = outheight;	  
			component[idx].size = outwidth*outheight*4; // hardcoding the RGBA output
			return outbuf;
    	}
	//
	else
		return NULL;
}


void discardUnsupportedComponents()
{
	int tmpnumComponents = numComponents;
	COMPONENT tmpcomponent[MAX_NUM_COMPONENTS];
	int i;
	
	
	numComponents =0;
	
	// copy old list to temp, check temp and re-add valid components to old list
	for (i=0; i<MAX_NUM_COMPONENTS; ++i)
    {
		tmpcomponent[i].type = component[i].type;
		tmpcomponent[i].ifd_loc = component[i].ifd_loc;
		tmpcomponent[i].width = component[i].width;
		tmpcomponent[i].height = component[i].height;
		tmpcomponent[i].size = component[i].size;
		component[i].type = TYPE_UNKNOWN;
		component[i].ifd_loc = -1;  // assume valid IFD in 0 to MAX_NUM_IFD range
		component[i].width = 0;
		component[i].height = 0;
		component[i].size = 0;
	}
	
	for (i=0; i<tmpnumComponents; ++i)
		{
			if (tmpcomponent[i].ifd_loc >= 0) // check we have a valid component
				{
				if ( (tiff_ifd[tmpcomponent[i].ifd_loc].dng_comp == UNCOMPRESSED) || (tiff_ifd[tmpcomponent[i].ifd_loc].dng_comp == LOSSLESS_JPEG) || (tiff_ifd[tmpcomponent[i].ifd_loc].dng_comp == PACKED_DNG)) // check that our valid component has an accessor
					{	
						component[numComponents].type = tmpcomponent[i].type;
						component[numComponents].ifd_loc = tmpcomponent[i].ifd_loc;
						component[numComponents].width = tmpcomponent[i].width;
						component[numComponents].height = tmpcomponent[i].height;
						component[numComponents].size = tmpcomponent[i].size;
						++numComponents;
					}
				}
		}		
}
				
				
// Currently returns only the useful components; that is the ones with
// decoders/accessors built in. 
int EMSCRIPTEN_KEEPALIVE getNumComponents(char *inbuf, int insize )
{
	//set the global pointers
	globalInbuf = inbuf;
	globalInsize = insize;
	
	// dngParse returns 0 on success
	if (!dngParse(inbuf,insize) && (numComponents >0))
		{ //let's ditch any reference to components we don't support
		  // currently supporting only UNCOMPRESSED and LOSSLESS_JPEG types
			discardUnsupportedComponents();
			return numComponents;
		}
	
	return 0;
}


// BEVARA: we keep this around for debugging
int main(int argc, char* argv[]) {
    int insize; 
    char *inbuf;   
    FILE *f;
   int idx = 0;

    if (argc <2)
    {   
      printf ("You should provide the filename as the first argument");
      return 0;
    }
   
    f = fopen(argv[1], "rb");
    if (!f) {
        printf("Error opening the input file.\n");
        return 1;
    }


    fseek(f, 0, SEEK_END);
    insize = (int) ftell(f);
    inbuf = malloc(insize);
    fseek(f, 0, SEEK_SET);
    insize = (int) fread(inbuf, 1, insize, f);
    fclose(f);

	
	
	// call setup and parsing
	imgSetup();
	printf("num usable components = %d \n",getNumComponents(inbuf,insize)); // calls imgParse(inbuf,insize);
	getComponentType(idx);
	
	// // print out components
	
	for (idx=0; idx<numComponents; ++idx)
		printf("Component type = %d in IFD#= %d, compression type is %d\n",getComponentType(idx), component[idx].ifd_loc, tiff_ifd[component[idx].ifd_loc].dng_comp);
	
	
	// //set this to test output of one component
	idx=1;
	
	unsigned char * locoutbuf = getData(idx); // do decode on the fly, call decodeMain or decodeThumb or 
		// // retrieveXMP; set the width/height/size in component
	int locwidth = getWidth(idx); // returns 0 for XMP and similar
	int locheight = getHeight(idx); // returns 0 for XMP and similar
	int locsize = getSize(idx);  // non-zero for all images and metadata
	
	// // sample code to check the output images and XMP
	FILE *f2;
	int i,j;
	unsigned char* tmpoutbuf;
	unsigned  char *tmp;
	
	
	// // for images:
	if ((component[idx].type == TYPE_MAIN_IMAGE) ||(component[idx].type == TYPE_THUMBNAIL))
		{
		printf("width = %d, height = %d\n",locwidth,locheight);
		f2=fopen("DNG_image_output.ppm","wb");
		fprintf(f2, "P6\n%d %d\n255\n", locwidth, locheight);
		// remove the alphas for ppm 
		tmpoutbuf = (unsigned char*)malloc(locwidth*locheight*3);
		tmp=locoutbuf;
		unsigned char *otmp;
		otmp = tmpoutbuf;
		for (i=0 ; i<locheight; ++i)
		{
			for (j=0; j<locwidth; ++j)
			{
				*(otmp) = *(tmp); ++otmp; ++tmp;
				*(otmp) = *(tmp); ++otmp; ++tmp;
				*(otmp) = *(tmp); ++otmp; ++tmp;
				++tmp; // skip A
			}
		}
		fwrite(tmpoutbuf, 1, locwidth*locheight*3, f2);
		fclose(f2); 
		free(tmpoutbuf);
		}
	if (component[idx].type == TYPE_XMP)
	{
		 for (i=0 ; i<locsize; ++i)
		{
			printf("%c",*(outbuf+i));
		} 
	}
	
	
	
	
	
	
	imgShutdown();
	
    
    free (inbuf);
    
	return 1;

}

