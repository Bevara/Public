/* Parts of this software are adapted from pbm fileio routines.
** fileio.c - routines to read elements from Netpbm image files
**
** Copyright (C) 1988 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

// BEVARA
// modified just for RGB output and hooks into code
//

#include <emscripten/emscripten.h>

#include <stdio.h> 
#include <stdlib.h>
#include <string.h>

/* hardcode a 32-bit max */
#define INT_MAX 0x7FFFFFFF


/* stream is read in a number of routines,so make global */
int stream_loc = 0;

char
pm_getc(unsigned char *buf, int streamlength) 
{
    int ich;
    char ch;



    ich = (int) *(buf+stream_loc);
    printf("ich = %d ch = %c\n",ich,(char)ich);
    ++stream_loc;


    if (stream_loc >= streamlength)
      {
	printf("Error reading the stream - length exceeded\n");
	exit(0);
      }
    ch = (char) ich;
    
    /* need to skip the comments */
    if (ch == '#') {
        do {
	  ich = (int) *(buf+stream_loc);

    printf("ich = %d ch = %c\n",ich,(char)ich);
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
        printf("PPM file in unreadable format\n");
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
	//printf("int = %d\n",i);
        ch = pm_getc(buf,streamlength);
    } while (ch >= '0' && ch <= '9');


    //printf(" returning int = %d\n",i);
    return i;
}




 
int pbmformat = 4; /* default 4 */

// BEVARA: needs some globals
int imgHeight, imgWidth;
unsigned char* outbuf;


int imgDecode(char *inbuf, int insize)
{
 int i,j;
  unsigned char *tempptr;
  //unsigned char ch;
  int intch;


  /* check magic number = P1 or P4*/
  //if ((*inbuf =='P') && (*(inbuf+1)=='4'))
  // printf("found 4\n");
  if ((*inbuf =='P') && (*(inbuf+1)=='4'))
    {
      //printf("pbmformat = 4\n");
      pbmformat = 4; /*here for debugging */
    }
  else if ((*inbuf =='P') && (*(inbuf+1)=='1'))
    {
      //printf("pbmformat = 1\n");
      pbmformat = 1; /*here for debugging */
    }
  else
    {
      //printf("incorrect magic number\n");
      return(0); 
    }
  stream_loc =2; /* start at byte 2 */

  /* get image size */
  imgWidth = (int) pm_getuint((unsigned char*) inbuf,insize);
  imgHeight = (int) pm_getuint((unsigned char*)inbuf,insize);

  /* if too large for int size */
  if ((imgWidth <0) || (imgHeight <0))
    {
      return -5;
    }

  if (outbuf != NULL)
    free(outbuf);
  outbuf = (unsigned char*) malloc(imgWidth*imgHeight*4); // now outputting RGBA


  /* each pixel gets 3 bytes -- over allocate here*/
  tempptr = outbuf;

  /* for ease of display, and to make things generic, we're 
   going to convert to 24-bit RGB, so write each bit/byte as
  0x000 or 0xFFF*/


  switch(pbmformat)
    {
       case 4:
	 /*TODO: add a check that the width is divisable by 8,
	   then reduce comps */
	 for (i=0;i< imgWidth/8*imgHeight;++i)
	   {
	     /* convert each bit to RGB 3-byte pixel */
	     intch = (int)  *(inbuf+stream_loc);
	     ++stream_loc;
	     //printf("\ni= %d char = %d: ",i,intch & 0xFF);
	     for (j=0;j<8;++j)
	       {
		 //printf("%d  ",intch & 0xFF);
		 if ( intch & 0x80)
		   {
		     *tempptr = 0x00;  ++tempptr;
		     *tempptr = 0x00;  ++tempptr;
		     *tempptr = 0x00;  ++tempptr;
			 *tempptr = 255;  ++tempptr;
		   }
		 else
		   {
		     *tempptr = 0xFF;  ++tempptr;
		     *tempptr = 0xFF;  ++tempptr;
		     *tempptr = 0xFF;  ++tempptr;
			 *tempptr = 255;  ++tempptr;
		   }
		 intch = intch <<1;

	       }
	   }

	 break;

       case 1:
	 /*i=0;*/
	 do {
		/* convert each B/W byte to RGB 3-byte pixel */
		intch = (int)  *(inbuf+stream_loc);
		++stream_loc;
		/*++i;*/
		/*printf("val = %d, loc=%d, \n",intch,i);*/

		if ( intch == 49)
		   {
		     *tempptr = 0x00;  ++tempptr;
		     *tempptr = 0x00;  ++tempptr;
		     *tempptr = 0x00;  ++tempptr;
			 *tempptr = 255;  ++tempptr;
		   }
		else if (intch == 48)
		   {
		     *tempptr = 0xFF;  ++tempptr;
		     *tempptr = 0xFF;  ++tempptr;
		     *tempptr = 0xFF;  ++tempptr;
			 *tempptr = 255;  ++tempptr;
		   }
		/*else
		  printf("loc=%d  val = %d\n",i,intch);*/
	 } while (stream_loc <= insize);
    
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


// BEVARA: keeps this for testing
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






