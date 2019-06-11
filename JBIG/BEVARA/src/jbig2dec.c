/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/*
    jbig2dec
*/

#include <emscripten/emscripten.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef PACKAGE
#define PACKAGE "jbig2dec"
#endif
#ifndef VERSION
#define VERSION "unknown-version"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>


/* #ifdef HAVE_GETOPT_H */
/* # include <getopt.h> */
/* #else */
/* # include "getopt.h" */
/* #endif */

#include "os_types.h"
/* #include "sha1.h" */

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_image.h"

// Bevara: shifted these to globals
Jbig2Ctx *ctx;
int current_page;
int total_pages;
int outwidth;
int outheight;
unsigned char* outBuf;


static int
error_callback(void *error_callback_data, const char *buf, Jbig2Severity severity,
	       int32_t seg_idx)
{
   
  printf("msg:   %s\n",buf);

    return 0;
}


int EMSCRIPTEN_KEEPALIVE imgSetup()
{
  
  ctx = jbig2_ctx_new(NULL, 0, NULL, error_callback, NULL);
  return 1;
}

int EMSCRIPTEN_KEEPALIVE imgShutdown()
{

 

  if (ctx != NULL)
  jbig2_ctx_free(ctx);



  return 1;
}

int EMSCRIPTEN_KEEPALIVE imgDecode(unsigned char* inbuf, int inbuf_size)
{

  int n_bytes =0;
  int bufPtr =0;
  int bytesToRead = 4096;
  uint8_t buf[4096];

  /* read 1024 bytes per time from inbuf */
  for (;;)
    {
      /* int n_bytes = fread(buf, 1, sizeof(buf), f); */
      if (bufPtr+bytesToRead <= inbuf_size) /* read 4096 bytes */
		{
		memcpy(buf,inbuf+bufPtr,bytesToRead);
		n_bytes=4096;
		bufPtr += 4096;
		}
      else /* read whatever is left in input stream */
		{
		memcpy(buf,inbuf+bufPtr,inbuf_size-bufPtr);
		n_bytes=inbuf_size-bufPtr;
		bufPtr = inbuf_size; /* just for debugging */
		}

      if (n_bytes <= 0)
		break;
      if (jbig2_data_in(ctx, buf, n_bytes))
		break;
      if (n_bytes < 4096) /*we're done with the input */
		break;
    }

   Jbig2Image *image;
   if ((image = jbig2_page_out(ctx)) != NULL)
     {
		 
		 printf("outputting one page\n");
		 
       /* allocate outbuf, and convert to RGB */
       /* we know it's a waste of space, but we want a consistent */
       /* image output format */
       outwidth = image->width;
       outheight = image->height;

       /* overallocing so we don't have to do length checks below */
       outBuf = (unsigned char*) malloc(image->width*image->height*10+8); // now outputting RGBA
       unsigned char* tempptr;
       tempptr = outBuf;
       unsigned char* dataptr;
       dataptr = (unsigned char *)image->data;
	   
	 

       int ii =0;
       int temp = 0;
       while( ii<image->width*image->height)
			{
		   /* since the BW values are packed in bytes we need to 
			  read them out */
		   temp = (((~(*dataptr)& 0x80) >> 7)) *255; 	  
		   *tempptr = temp; ++tempptr; /* RGB for curr pix */
		   *tempptr = temp; ++tempptr;
		   *tempptr = temp; ++tempptr; ++ii;
		   *tempptr = 255; ++tempptr; // add alpha 
		   temp = (((~(*dataptr) & 0x40) >> 6)) *255;
		   *tempptr = temp; ++tempptr; /* RGB for curr pix */
		   *tempptr = temp; ++tempptr;
		   *tempptr = temp; ++tempptr; ++ii;
		   *tempptr = 255; ++tempptr; // add alpha 
		   temp = (((~(*dataptr) & 0x20) >> 5)) *255;
		   *tempptr = temp; ++tempptr; /* RGB for curr pix */
		   *tempptr = temp; ++tempptr;
		   *tempptr = temp; ++tempptr; ++ii;
		   *tempptr = 255; ++tempptr; // add alpha 
		   temp = (((~(*dataptr)  & 0x10)>> 4)) *255;
		   *tempptr = temp; ++tempptr; /* RGB for curr pix */
		   *tempptr = temp; ++tempptr;
		   *tempptr = temp; ++tempptr; ++ii;
		   *tempptr = 255; ++tempptr; // add alpha 
		   temp = (((~(*dataptr)  & 0x08)>> 3)) *255;
		   *tempptr = temp; ++tempptr; /* RGB for curr pix */
		   *tempptr = temp; ++tempptr;
		   *tempptr = temp; ++tempptr; ++ii;
		   *tempptr = 255; ++tempptr; // add alpha 
		   temp = (((~(*dataptr) & 0x04) >> 2)) *255;
		   *tempptr = temp; ++tempptr; /* RGB for curr pix */
		   *tempptr = temp; ++tempptr;
		   *tempptr = temp; ++tempptr; ++ii;
		   *tempptr = 255; ++tempptr; // add alpha 
		   temp = (((~(*dataptr)  & 0x02) >> 1)) *255;
		   *tempptr = temp; ++tempptr; /* RGB for curr pix */
		   *tempptr = temp; ++tempptr;
		   *tempptr = temp; ++tempptr; ++ii;
		   *tempptr = 255; ++tempptr; // add alpha 
		   temp = ((~(*dataptr) & 0x01)) *255;
		   *tempptr = temp; ++tempptr; /* RGB for curr pix */
		   *tempptr = temp; ++tempptr;
		   *tempptr = temp; ++tempptr; ++ii;
		   *tempptr = 255; ++tempptr; // add alpha 
		 

		   ++dataptr; /* get next byte's worth of BW data */

		}

		printf("done with copying page to RGB\n");
       jbig2_release_page(ctx, image);

     }

   return 1;

}




int EMSCRIPTEN_KEEPALIVE imgGetWidth()
{
		return outwidth;
}


int EMSCRIPTEN_KEEPALIVE imgGetHeight()
{
	return outheight;
}

unsigned char* EMSCRIPTEN_KEEPALIVE imgGetImage()
{
	return outBuf;
}

int main(int argc, const char **argv)
{

    FILE *f;
    char *inbuf;
    int insize;
	unsigned char *locoutbuf;    
    int locoutwidth;
    int locoutheight; 
	
	
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

	
	imgSetup(); 
	

    fseek(f, 0, SEEK_END);
    insize = (int) ftell(f);
    inbuf = malloc(insize);
    fseek(f, 0, SEEK_SET);
    insize = (int) fread(inbuf, 1, insize, f);
    fclose(f);
 
 
 
	
	
	imgDecode(inbuf,insize);
	
	printf("going to output ppm\n");
	
	
	
	locoutwidth = imgGetWidth();
	locoutheight = imgGetHeight();
	locoutbuf = imgGetImage();


	// check the output
	/* printf("width = %d, height = %d\n",locoutwidth,locoutheight);
	FILE *f2;
	f2=fopen("TEST_output.ppm","wb");
	fprintf(f2, "P6\n%d %d\n255\n", locoutwidth, locoutheight);
	// remove the alphas for ppm 
	unsigned char* tmpoutbuf;
	tmpoutbuf = (unsigned char*)malloc(locoutwidth*locoutheight*3);
	int i,j;
	unsigned  char *tmp;
	tmp=locoutbuf;
	unsigned char *otmp;
	otmp = tmpoutbuf;
	for (i=0 ; i<outheight; ++i)
	{
		for (j=0; j<outwidth; ++j)
		{
			*(otmp) = *(tmp); ++otmp; ++tmp;
			*(otmp) = *(tmp); ++otmp; ++tmp;
			*(otmp) = *(tmp); ++otmp; ++tmp;
			++tmp; // skip A
		}
	}

	fwrite(tmpoutbuf, 1, locoutwidth*locoutheight*3, f2);
	free(tmpoutbuf);
	fclose(f2);  */
	//end of output check

	
	imgShutdown();
	
		// after everything we have to:
	   free(outBuf);
	   free(inbuf);

		return 1;
}


 
/* 	/\*** SAVE THIS CODE IN CASE WE WANT TO IMPLEMENT LATER ****\/ */
/*   /\* if there's a local page stream read that in its entirety *\/ */
/*   /\* if (f_page != NULL) *\/ */
/*   /\*   { *\/ */
/*   /\*     Jbig2GlobalCtx *global_ctx = jbig2_make_global_ctx(ctx); *\/ */
/*   /\*     ctx = jbig2_ctx_new(NULL, JBIG2_OPTIONS_EMBEDDED, global_ctx, *\/ */
/*   /\* 			 error_callback, &params); *\/ */
/*   /\*     for (;;) *\/ */
/*   /\* 	{ *\/ */
/*   /\* 	  int n_bytes = fread(buf, 1, sizeof(buf), f_page); *\/ */
/*   /\* 	  if (n_bytes <= 0) *\/ */
/*   /\* 	    break; *\/ */
/*   /\* 	  if (jbig2_data_in(ctx, buf, n_bytes)) *\/ */
/*   /\* 	    break; *\/ */
/*   /\* 	} *\/ */
/*   /\*     fclose(f_page); *\/ */
/*   /\*     jbig2_global_ctx_free(global_ctx); *\/ */
/*   /\*   } *\/ */

/*   /\* retrieve and output the returned pages *\/ */
/*   /\* { *\/ */
/*   /\*   Jbig2Image *image; *\/ */

/*     /\* work around broken CVision embedded streams *\/ */
/*     /\* if (f_page != NULL) *\/ */
/*     /\*   jbig2_complete_page(ctx); *\/ */

 
/*     /\* retrieve and write out all the completed pages *\/ */
/*     /\* while ((image = jbig2_page_out(ctx)) != NULL) { *\/ */
/*     /\* write out first page *\/ */

/* /\* printf("writing a page\n"); *\/ */
/* /\*     if ((image = jbig2_page_out(ctx)) != NULL) { *\/ */
/*       /\* write_page_image(&params, image); *\/ */

/* 	printf("wirintg ppm file\n"); */

/* 	FILE *f2; */
/* 	f2=fopen("TEST_output.ppm","w"); */
/* 	fprintf(f2, "P6\n%d %d\n255\n", outwidth, outheight); */
/* 	fwrite(outBuf, 1, outheight*outwidth*3, f2);  */

/* 	/\* f2=fopen("TEST_output.pbm","w"); *\/ */
/* 	/\* fprintf(f2, "P4\n%d %d\n", image->width,image->height); *\/ */
/* 	/\* fwrite(image->data, 1, image->height*image->stride, f2); *\/ */
/* 	fclose(f2); */

/*       /\* if (params.hash) hash_image(&params, image); *\/ */
/*       /\* jbig2_release_page(ctx, image); *\/ */
/*    /\*  } *\/ */
/*   /\*   /\\* if (params.hash) write_document_hash(&params); *\\/ *\/ */
/*   /\* } *\/ */


