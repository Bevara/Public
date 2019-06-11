/* Parts of this software are adapted from pgm fileio routines.
** Copyright (C) 1989 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#include <emscripten/emscripten.h>

/* #include <stdio.h> */
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>

/* globals going between this and the wrapper */
int imgHeight, imgWidth;
unsigned char* outbuf;

/* hardcode a 32-bit max */
#define INT_MAX 0x7FFFFFFF


/* stream is read in a number of routines,so make global */
int stream_loc = 0;
int sample_counter = 0;


char
pm_getc(unsigned char *buf, int streamlength) 
{
    int ich;
    char ch;



    ich = (int) *(buf+stream_loc);
    
    ++stream_loc;


    if (stream_loc > streamlength)
      {
	printf("Error reading the stream - length exceeded\n");
	exit(0);
      }
    ch = (char) ich;
    
    /* need to skip the comments */
    if (ch == '#') {
        do {
	  ich = (int) *(buf+stream_loc);

	  /*printf("ich = %d ch = %c\n",ich,(char)ich);*/
	  ++stream_loc;
	  if (stream_loc >= streamlength)
	    {
	      printf("Error reading the stream - length exceeded\n");
	      exit(0);
	    }
	    ch = (char) ich;
         } while (ch != '\n' && ch != '\r'); /* check for int == 10*/
	}
    return ch;
}




unsigned int
pm_getuint(unsigned char *buf, int streamlength) {
/*----------------------------------------------------------------------------
   Read an unsigned integer in ASCII decimal from the file stream
   represented by 'ifP' and return its value.

   If there is nothing at the current position in the file stream that
   can be interpreted as an unsigned integer, issue an error message
   to stderr and abort the program.

   If the number at the current position in the file stream is too
   great to be represented by an 'int' (Yes, I said 'int', not
   'unsigned int'), issue an error message to stderr and abort the
   program.
-----------------------------------------------------------------------------*/
    char ch;
    unsigned int i;

    do {
      ch = pm_getc(buf,streamlength);
	} while (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r');

    if (ch < '0' || ch > '9')
      {
        //printf("PPM file in unreadable format\n");
	exit(0);
      }

    i = 0;
    do {
        unsigned int const digitVal = ch - '0';

        if (i > INT_MAX/10 - digitVal)
	  {
	    //printf("ASCII decimal integer in file is too large to be processed.  ");
	   exit(0);
	  }
        i = i * 10 + digitVal;
	/*printf("int = %d\n",i);*/
        ch = pm_getc(buf,streamlength);
    } while (ch >= '0' && ch <= '9');


    /*printf(" returning int = %d\n",i);*/
    ++sample_counter;
    return i;
}

int filesize = 0;
char* in_data;
int in_ptr=0;

int EMSCRIPTEN_KEEPALIVE imgSetInSize(int inSize)
{
	filesize = inSize;
	in_data = malloc(inSize);
	return 1;
}


/* int entry( char *inBuffer, unsigned int inBufferLength, */
/* 		char *outBuffer, unsigned int *outBufferLength, */
/* 		unsigned char PaddingBits, unsigned int mmlevel) */
int EMSCRIPTEN_KEEPALIVE imgDecode(char *inBuffer, int inBufferLength)
{
  int i;
  int maxval;
  unsigned char *tempptr;
  //unsigned char ch;
  int intch;
  unsigned int uintch;
  int format = 2;

	if(filesize){
		memcpy(in_data+in_ptr, inBuffer, inBufferLength);
		in_ptr += inBufferLength;
		if (filesize > in_ptr){
			printf("Need more data before decoding...");
			return 0;
		}
		inBuffer = in_data;
	}
	

  /* check magic number = P2, P5 */
  if ((*inBuffer =='P') && (*(inBuffer+1)=='5'))
    {
      format = 5; /*here for debugging */
    }
  else if ((*inBuffer =='P') && (*(inBuffer+1)=='2'))
    {
      format = 2; 
    }
  else
    {
printf("incorrect magic number\n");
      return -4; 
    }
  stream_loc =2; /* start at byte 2 */

  /* get image size */
  imgWidth = (int) pm_getuint((unsigned char*)inBuffer,inBufferLength);
  imgHeight = (int) pm_getuint((unsigned char*)inBuffer,inBufferLength);

  /* if too large for int size */
  if ((imgWidth <0) || (imgHeight <0))
    {
 printf("invalid image size\n");
      return -5;
    }

  if (outbuf != NULL)
    free(outbuf);
  outbuf = (unsigned char*) malloc(imgWidth*imgHeight*4); // now outputting RGBA

  /* get maval */
  maxval = (int) pm_getuint((unsigned char*)inBuffer,inBufferLength);
  if ((maxval >= 65536) || (maxval <=0))
    {
//printf("maximum gray value is out of range\n");
      return -5;
    }
 
  tempptr = outbuf;

  /* for ease of display, and to make things generic, we're 
   going to convert to 24-bit RGB - no gamma corr.*/


  switch(format)
    {
       case 5:
	 if (maxval <256) /* one byte per pixel */
	   {
	     for (i=0;i< imgWidth*imgHeight;++i)
	       {
		 /* convert each bit to RGB 3-byte pixel */
		 /* by just copying, no scaling */
		 intch = (int)  *(inBuffer+stream_loc);
		 ++stream_loc;
		 /*printf("\ni= %d char = %d: ",i,intch & 0xFF);*/
		 *tempptr = intch; ++tempptr;

		 // added extra two bytes
		 *tempptr = intch; ++tempptr;
		 *tempptr = intch; ++tempptr;
		 // add alpha channel 
		 *tempptr = 255; ++tempptr;
	       }
	   }
	 else  /* two bytes per pixel */
	   {
	     printf("Two bytes case not yet tested\n");
	     return -4;
	     /* grab two bytes and scale to 255 max */
	     for (i=0;i< imgWidth*imgHeight;++i)
	       {
		 /* convert each bit to RGB 3-byte pixel */
		 /* by just copying -- scale by maxval */
		 intch = (int)  *(inBuffer+stream_loc) * 256;
		 ++stream_loc;
		 intch += (int)  *(inBuffer+stream_loc);
		 ++stream_loc;
		 intch = intch/maxval*255;
		 /*printf("\ni= %d char = %d: ",i,intch & 0xFF);*/
		 *tempptr = intch;  ++tempptr;
		 // added extra two bytes
		 *tempptr = intch; ++tempptr;
		 *tempptr = intch; ++tempptr;	
		// add alpha channel 
		 *tempptr = 255; ++tempptr;	     
	       }
	   }
	 break;

       case 2:
	  if (maxval <256) /* dont' normalize */
	   {
	     do {
		uintch = pm_getuint((unsigned char*)inBuffer,inBufferLength);
		*tempptr = uintch;  ++tempptr; 	
		 // added extra two bytes
		 *tempptr = uintch; ++tempptr;
		 *tempptr = uintch; ++tempptr;
		 // add alpha channel 
		 *tempptr = 255; ++tempptr;

	     } while ((stream_loc <= inBufferLength) && (sample_counter < (imgWidth*imgHeight)));
	   }
	  else /* normalize */
	   {
	     do {
		uintch = pm_getuint((unsigned char*)inBuffer,inBufferLength);
		*tempptr = (uintch/maxval*255);  ++tempptr;
		// added extra two
		*tempptr = (uintch/maxval*255);  ++tempptr;
		*tempptr = (uintch/maxval*255);  ++tempptr;
		// add alpha channel 
		 *tempptr = 255; ++tempptr;
	     } while ((stream_loc <= inBufferLength)&& (sample_counter < (imgWidth*imgHeight)));
	   }
	 
           break;
       default:
	 //printf("corrupt format\n");
          return -5;
    }

  return 0; /* success */
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

	

    fseek(f, 0, SEEK_END);
    insize = (int) ftell(f);
    inbuf = malloc(insize);
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
	tmpoutbuf = (unsigned char*)malloc(outwidth*outheight*3);
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
	free(tmpoutbuf);
	fclose(f2); 
	
	
	//end of output check

		// after everything we have to:
	   free(outbuf);
	   free(inbuf);

		return 1;
}





