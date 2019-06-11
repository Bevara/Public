
/*		Bevara J2K Entry API 
*/


/* Openjpeg copyright below - we have simply developed a new interface to this
** code and removed un-needed code. Openjpeg copyright must be left in. 
*/

/*
 * Copyright (c) 2002-2007, Communications and Remote Sensing Laboratory, Universite catholique de Louvain (UCL), Belgium
 * Copyright (c) 2002-2007, Professor Benoit Macq
 * Copyright (c) 2001-2003, David Janssens
 * Copyright (c) 2002-2003, Yannick Verschueren
 * Copyright (c) 2003-2007, Francois-Olivier Devaux and Antonin Descampe
 * Copyright (c) 2005, Herve Drolon, FreeImage Team
 * Copyright (c) 2006-2007, Parvatha Elangovan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS `AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <emscripten/emscripten.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "j2k_dec.h"
#include "opj_includes.h"


#include "openjpeg.h"
#include "opj_getopt.h"
#include "convert.h"
#include "index.h"
#include "color.h"
#include "format_defs.h"



int imgHeight;
int  imgWidth;
unsigned char* outbuf;

/*no support for scalability with JPEG (progressive JPEG to test)*/
unsigned int bpp, nb_comp, width, height, out_size, pixel_format, dsi_size;
char *dsi;
opj_image_t *image;

#define u32 unsigned int




/* -------------------------------------------------------------------------- */
/**
 * J2K entry point
 */
/* -------------------------------------------------------------------------- */
int EMSCRIPTEN_KEEPALIVE imgDecode(char *inBuffer, int inBufferLength)
{





        unsigned char *tempptr;
	int i;
	int *redptr, *greenptr, *blueptr; //, *alphaptr;
	int adjustR, adjustG, adjustB; //, adjustA;
	int precision;

	opj_dparameters_t parameters;			
	//opj_event_mgr_t event_mgr;			
	opj_image_t* image = NULL;
	/* opj_stream_t *cio = NULL; */
	opj_cio_t *cio = NULL;
	/*opj_codec_t* dinfo = NULL;*/
	opj_dinfo_t *dinfo = NULL;
		

	/* set decoding parameters to default values */
	opj_set_default_decoder_parameters(&parameters);
	/* Set default event mgr */
	/*opj_initialize_default_event_handler(&event_mgr, 1);*/
	/* Set decoding format */
	dinfo = opj_create_decompress(CODEC_JP2);
	/* Setup decoder*/
	opj_setup_decoder(dinfo,&parameters);
	/* Setup stream */
	cio = opj_cio_open((opj_common_ptr)dinfo, (unsigned char *) inBuffer, inBufferLength);
	/* decode */	

	// hardcoded for JP2
	//image = opj_decode(dinfo,cio);
        image = opj_jp2_decode((opj_jp2_t*)dinfo->jp2_handle, cio, NULL);
	// just return so can pass back the subfn messages



	if (image == NULL)
	  {
	   
	    return 1;
	  }




	int spp=3; /* only dealing with RGB for now */

		switch (image->color_space)
		  {
		  case  CLRSPC_SRGB:
		    /*handle RGB ouput */
		    /* set fourcc, alloc and copy over */
		    /*strcpy(format,"RGB");*/
		    spp=3;
		    break;
		  case CLRSPC_SYCC:
		    	spp=0;
		    /* handle YUV format */
		    if((image->comps[0].dx == 1)
		       && (image->comps[1].dx == 2)
		       && (image->comps[2].dx == 2)
		       && (image->comps[0].dy == 1)
		       && (image->comps[1].dy == 2)
		       && (image->comps[2].dy == 2))
		      {
		      /* horizontal and vertical sub-sample (420)*/

			/* set fourcc, alloc and copy over */
		
		      }
		    if((image->comps[0].dx == 1)
		       && (image->comps[1].dx == 2)
		       && (image->comps[2].dx == 2)
		       && (image->comps[0].dy == 1)
		       && (image->comps[1].dy == 1)
		       && (image->comps[2].dy == 1))
		      {
			/* horizontal sub-sample only (422) */
			/* set fourcc, alloc and copy over */
		      }

		    if((image->comps[0].dx == 1)
		       && (image->comps[1].dx == 1)
		       && (image->comps[2].dx == 1)
		       && (image->comps[0].dy == 1)
		       && (image->comps[1].dy == 1)
		       && (image->comps[2].dy == 1))
		      {
			/* no sub-sample (444) */
			/* set fourcc, alloc and copy over */
		      }
		    
		      break;

		  case CLRSPC_GRAY:
		    /* handle grayscale image */
		    /* set fourcc, alloc and copy over */
		    spp=1;
		    break;

		  default:
		    /* not specified image format; check number of layers */

		    /* if (image->numcomps <=2) */
		    /*   strcpy(format,"PGM"); */
		    /* else */
		    /*   strcpy(format,"PNM"); */
		    spp=3;
		    break;
		  }



		/* setup output */


 

		imgWidth=image->comps[0].w;
		imgHeight=image->comps[0].h;

		if (spp!=3)
		  return 1; // only expecting RGB output now


		// malloc() and free() are poisoned
		if (outbuf != NULL)
		   opj_aligned_free(outbuf);
	   
		/* alloc space specifically for outstream so can 
		   destroy entire image structure */
		outbuf = (unsigned char*) opj_malloc(imgWidth*imgHeight*4); //output RGBA
		tempptr = outbuf; 

		precision = image->comps[0].prec;
		if (precision > 8)
		  return EXIT_FAILURE;


		/* set up red or gray data pointers*/
		redptr = image->comps[0].data;
		adjustR = (image->comps[0].sgnd ? 1 << (image->comps[0].prec - 1) : 0);
		/* setup green and blue data pointers */
		if (image->numcomps > 2)		  
		  {
		    greenptr = image->comps[1].data;
		    blueptr = image->comps[2].data;
		    adjustG = (image->comps[1].sgnd ? 1 << (image->comps[1].prec - 1) : 0);
		    adjustB = (image->comps[2].sgnd ? 1 << (image->comps[2].prec - 1) : 0);
		  }
		for (i=0; i< image->comps[0].w * image->comps[0].h; ++i)
		  {
		    /*printf("%d %d %d %d ",i,*redptr,*greenptr,*blueptr);*/
		    /* output red first */
		    if (precision <= 8)
		      {
			*tempptr= (*redptr);
		      }
		    /* else - not yet implemented 
		      {

		        *outbuf= (unsigned char) (*redptr + adjustR);
			} */

		    /*printf("      %d ",*tempptr);*/

		    ++tempptr;
		    ++redptr;
		    /* output green and blue, if necessary */
		    if (image->numcomps > 2)		  
		      {

				/* green and blue  data */
				if (precision <= 8)
				{	
					*tempptr= (*greenptr);
					++tempptr;
					++greenptr;
					*tempptr = (*blueptr );
					/*printf("%d \n",*tempptr);*/
					++tempptr;
					++blueptr;
					*tempptr = 255; ++tempptr; // make A channel
				}
		      }
			 else /* just copy over the R value to GB and set the alpha */
				{
					*tempptr= *(redptr-1); ++tempptr;
					*tempptr= *(redptr-1); ++tempptr;
					*tempptr= 255; ++tempptr;
					
				} 
		  }

  

      	if (dinfo) {
	  opj_destroy_decompress(dinfo);
	}


	if (image) {
		opj_image_destroy(image);
		image = NULL;
	}


	return 1;
}


int EMSCRIPTEN_KEEPALIVE imgGetWidth()
{
		return imgWidth;
}


int EMSCRIPTEN_KEEPALIVE imgGetHeight()
{
	return imgHeight;
}

unsigned char* EMSCRIPTEN_KEEPALIVE imgGetImage()
{
	return outbuf;
}

int main(int argc, const char **argv)
{

    
 


    FILE *f;
    char *inbuf;
    int insize;
	
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

	
	// BEVARA: note that JP2 has poisoned malloc() and free() so
	// we use their versions. Sigh. 

    fseek(f, 0, SEEK_END);
    insize = (int) ftell(f);
    inbuf = opj_malloc(insize);
    fseek(f, 0, SEEK_SET);
    insize = (int) fread(inbuf, 1, insize, f);
    fclose(f);
 
 
 
	unsigned char *locoutbuf;    
     int outwidth;
    int outheight; 
    imgDecode(inbuf,insize);
	outwidth = imgGetWidth();
	outheight = imgGetHeight();
	locoutbuf = imgGetImage();


	// check the output
	printf("width = %d, height = %d\n",outwidth,outheight);
	FILE *f2;
	f2=fopen("TEST_output.ppm","wb");
	fprintf(f2, "P6\n%d %d\n255\n", outwidth, outheight);
	// remove the alphas for ppm 
	
	unsigned char* tmpoutbuf;
	tmpoutbuf = (unsigned char*)opj_malloc(outwidth*outheight*3);
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

	fwrite(tmpoutbuf, 1, outwidth*outheight*3, f2);
	opj_free(tmpoutbuf);
	fclose(f2); 
	
	
	//end of output check

		// after everything we have to:
	   opj_free(outbuf);
	   opj_free(inbuf);

		return 1;
}









