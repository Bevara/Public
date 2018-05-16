/*
 * Copyright 2008 Sean Fox <dyntryx@gmail.com>
 * Copyright 2008 James Bursa <james@netsurf-browser.org>
 *
 * This file is part of NetSurf's libnsgif, http://www.netsurf-browser.org/
 * Licenced under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 */
 
 
 /* BEVARA: 
 ** modified to add access points and output format
 **
 */
 
#include <emscripten/emscripten.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "libnsgif.h"


//BEVARA: we need global params
int imgWidth;
int imgHeight;
int numFrames;
unsigned char* outbuf;



unsigned char *load_file(const char *path, size_t *data_size);
/* void warning(const char *context, int code); */
void *bitmap_create(int width, int height);
void bitmap_set_opaque(void *bitmap, bool opaque);
bool bitmap_test_opaque(void *bitmap);
unsigned char *bitmap_get_buffer(void *bitmap);
void bitmap_destroy(void *bitmap);
void bitmap_modified(void *bitmap);



	gif_bitmap_callback_vt bitmap_callbacks = {
		bitmap_create,
		bitmap_destroy,
		bitmap_get_buffer,
		bitmap_set_opaque,
		bitmap_test_opaque,
		bitmap_modified
	};
	gif_animation gif;


/* int gif_setup(void* data, unsigned int dataLength) */
int EMSCRIPTEN_KEEPALIVE gif_setup()
{
	/* create our gif animation */
	gif_create(&gif, &bitmap_callbacks);
	return 0;
}

int EMSCRIPTEN_KEEPALIVE gif_shutdown(void)
{
	/* clean up */
	gif_finalise(&gif);
	
	free(outbuf);

	return 0;
}

int EMSCRIPTEN_KEEPALIVE parseGIF(unsigned char *data, int size)
{

	gif_result code;
	unsigned int i;
	unsigned char *tempptr;


	/* begin decoding */
	do {
		code = gif_initialise(&gif, size, data);
     		if (code != GIF_OK && code != GIF_WORKING) {
                 	                /* warning("gif_initialise", code); */
			return(0);
			/*exit(1);*/
		}
	} while (code != GIF_OK);

	
	/* set outbuf length in bytes*/
	outbuf = (unsigned char *)malloc(gif.width * gif.height * gif.frame_count * 4); 	/* hardcoded RGBA 8bits per channel */
	tempptr = outbuf;
	
		
	/* decode the frames */
	for (i = 0; i != gif.frame_count; i++) {
		unsigned int row, col;
		unsigned char *image;

		code = gif_decode_frame(&gif, i);
		if (code != GIF_OK)
		  return 0;

		/*printf("# frame %u:\n", i);*/
		image = (unsigned char *) gif.frame_image;
		for (row = 0; row != gif.height; row++) {
			for (col = 0; col != gif.width; col++) {
				size_t z = (row * gif.width + col) * 4;
				/*printf("%u %u %u ",
					(unsigned char) image[z],
					(unsigned char) image[z + 1],
					(unsigned char) image[z + 2]);*/
				*tempptr = image[z];
				++tempptr;
				*tempptr = image[z+1];
				++tempptr;
				*tempptr = image[z+2];
				++tempptr;
				*tempptr = 255;  /* added alpha channel */
				++tempptr; 
			}
			/*printf("\n");*/
		}
	}



	return 0;
}






/* void warning(const char *context, gif_result code) */
/* { */
/* 	printf( "%s failed: ", context); */
/* 	switch (code) */
/* 	{ */
/* 	case GIF_INSUFFICIENT_FRAME_DATA: */
/* 		printf( "GIF_INSUFFICIENT_FRAME_DATA"); */
/* 		break; */
/* 	case GIF_FRAME_DATA_ERROR: */
/* 		printf( "GIF_FRAME_DATA_ERROR"); */
/* 		break; */
/* 	case GIF_INSUFFICIENT_DATA: */
/* 		printf("GIF_INSUFFICIENT_DATA"); */
/* 		break; */
/* 	case GIF_DATA_ERROR: */
/* 		printf( "GIF_DATA_ERROR"); */
/* 		break; */
/* 	case GIF_INSUFFICIENT_MEMORY: */
/* 		printf("GIF_INSUFFICIENT_MEMORY"); */
/* 		break; */
/* 	default: */
/* 		printf("unknown code %i", code); */
/* 		break; */
/* 	} */
/* 	printf( "\n"); */
/* } */


void *bitmap_create(int width, int height)
{
	return calloc(width * height, 4);
}


void bitmap_set_opaque(void *bitmap, bool opaque)
{
	(void) opaque;  /* unused */
	assert(bitmap);
}


bool bitmap_test_opaque(void *bitmap)
{
	assert(bitmap);
	return false;
}


unsigned char *bitmap_get_buffer(void *bitmap)
{
	assert(bitmap);
	return bitmap;
}


void bitmap_destroy(void *bitmap)
{
	assert(bitmap);
	free(bitmap);
}


void bitmap_modified(void *bitmap)
{
	assert(bitmap);
	return;
}

unsigned char* EMSCRIPTEN_KEEPALIVE GIFGetImage(void) { return  outbuf;	}
int EMSCRIPTEN_KEEPALIVE GIFGetWidth(void)            {  return gif.width; }
int EMSCRIPTEN_KEEPALIVE GIFGetHeight(void)           { return gif.height; }	
int EMSCRIPTEN_KEEPALIVE GIFNumFrames(void) { return gif.frame_count;}


// BEVARA: use for debugging 	
// int main(int argc, const char **argv)
// {



    // FILE *f;
    // unsigned char *inbuf;
    // int insize;
	
	
    // f = fopen("test.gif", "rb");
    // if (!f) {
        // printf("Error opening the input file.\n");
        // return 1;
    // }


    // fseek(f, 0, SEEK_END);
    // insize = (int) ftell(f);
    // inbuf = malloc(insize);
    // fseek(f, 0, SEEK_SET);
    // insize = (int) fread(inbuf, 1, insize, f);
    // fclose(f);
 
    // gif_setup();
	

    // parseGIF(inbuf,insize);
    
	
	
	//free(inbuf);
	//gif_shutdown(); //shuts down and frees the outbuf output buffer


	//return 0;

//}
