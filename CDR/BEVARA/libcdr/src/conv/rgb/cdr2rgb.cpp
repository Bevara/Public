/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libcdr project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <CDR_support_codes.h>
int supportCode;

//#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <librevenge/librevenge.h>
#include <librevenge-generators/librevenge-generators.h>
#include <librevenge-stream/librevenge-stream.h>
#include <libcdr/libcdr.h>


#ifndef VERSION
#define VERSION "UNKNOWN VERSION"
#endif


int cdr_entry(const unsigned char *inBuf, const unsigned int inSize, unsigned char *outBuf,unsigned int* outSize, unsigned char *rgbaOutBuf, int *rgbaOutWidth, int *rgbaOutHeight)
{

 
  librevenge::RVNGStringStream input(inBuf,inSize);
  
  //BEVARA: TODO make into RVNG class
  // these local vars were used for debugging, put into the painter
  // unsigned char *rgbaOutBufLoc;
  // int rgbaOutWidthLoc;
  // int rgbaOutHeightLoc; 
  // rgbaOutBufLoc = rgbaOutBuf;
  
  int nsFeatures =0; // concatenation of unsupported features code as determined on output


  
  librevenge::RVNGStringVector svgOutput;
  librevenge::RVNGSVGRGBDrawingGenerator painter(svgOutput, rgbaOutBuf, rgbaOutWidth, rgbaOutHeight, &nsFeatures, "svgrgb");
  
 
 
  
  if (!libcdr::CDRDocument::isSupported(&input))
  {
    if (!libcdr::CMXDocument::isSupported(&input))
    {
      fprintf(stderr, "ERROR: Unsupported file format (unsupported version) or file is encrypted!\n");
      supportCode = BEV_GNS;
	  return supportCode;
    }
    else if (!libcdr::CMXDocument::parse(&input, &painter))
    {
      fprintf(stderr, "ERROR: Parsing of document failed!\n");
      supportCode = BEV_NS;
	  return supportCode;
    }
  }
  else if (!libcdr::CDRDocument::parse(&input, &painter))
  {
    fprintf(stderr, "ERROR: Parsing of document failed!\n");
    supportCode = BEV_NS;
	return supportCode;
  }

  if (svgOutput.empty())
  {
    fprintf(stderr, "ERROR: No SVG document generated!\n");
    supportCode = BEV_FNS;
  }
  else
  {
	  
	int strLength;
	*outSize = 0;
	
	strLength = strlen("<?xml version=\"1.0\" encoding=\"UTF-8\"?> \n");
 	memcpy(outBuf+*outSize, "<?xml version=\"1.0\" encoding=\"UTF-8\"?> \n" , strLength);
	*outSize = *outSize + strLength;

	
	
	strLength = strlen("<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \" http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n");	
	memcpy((outBuf + *outSize), "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"  http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n",  strLength);
	*outSize = *outSize + strLength;


	strLength = strlen("<html xmlns=\"http://www.w3.org/1999/xhtml\" xmlns:svg=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\">\n");
	memcpy((outBuf + *outSize), "<html xmlns=\"http://www.w3.org/1999/xhtml\" xmlns:svg=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\">\n",  strLength);
	*outSize = *outSize + strLength;

	strLength = strlen("<body>\n");
	memcpy((outBuf + *outSize), "<body>\n", strLength);
	*outSize = *outSize + strLength;
	
	strLength = strlen("<?import namespace=\"svg\" urn=\"http://www.w3.org/2000/svg\"?>\n");
	memcpy((outBuf + *outSize), "<?import namespace=\"svg\" urn=\"http://www.w3.org/2000/svg\"?>\n", strLength);
	*outSize = *outSize + strLength;
	
	for (unsigned k = 0; k<svgOutput.size(); ++k)
		{
			if (k>0)
			{
				strLength = strlen("<hr/>\n");
				memcpy((outBuf + *outSize),"<hr/>\n",strLength);
				*outSize = *outSize + strLength;
			}
			
		strLength = strlen("<!-- \n");
		memcpy((outBuf + *outSize),"<!-- \n",strLength);
		*outSize = *outSize + strLength;
		strLength = strlen("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n");
		memcpy((outBuf + *outSize),"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n",strLength);
		*outSize = *outSize + strLength;
		strLength = strlen("<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\"");
		memcpy((outBuf + *outSize),"<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\"",strLength);
		*outSize = *outSize + strLength;
		strLength = strlen(" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n");
		memcpy((outBuf + *outSize)," \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n",strLength);
		*outSize = *outSize + strLength;
		strLength = strlen(" -->\n");
		memcpy((outBuf + *outSize)," -->\n",strLength);
		*outSize = *outSize + strLength;
		strLength = strlen(svgOutput[k].cstr());
		memcpy((outBuf + *outSize),svgOutput[k].cstr(),strLength);
		*outSize = *outSize + strLength;
   
		}

	strLength = strlen("\n  </body> \n </html> \n");
	memcpy((outBuf + *outSize),"\n  </body> \n </html> \n",strLength);
	*outSize = *outSize + strLength;  
	  
  }
  
  if (*rgbaOutWidth == 0 ||  *rgbaOutHeight ==0)
  {
	fprintf(stderr, "ERROR: No RGBA generated!\n");
    supportCode = BEV_FNS;
  }


	
	if ( nsFeatures != 0)
	{
		
		supportCode = BEV_FNS | (nsFeatures << 8);
		
	}
  

  

  return supportCode; // this is a conglomeration of Bevara's support of the given file; see CDR_support_codes.h
}

int cdr_setup()
{
	
	supportCode = BEV_S;
	
}

// Bevara: main is for debugging, use the entry point otherwise
int main(int argc, char *argv[])
{
	
	unsigned int insize; 
    unsigned char *inbuf;   
    FILE *f;

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
    inbuf = (unsigned char *) malloc(insize);
    fseek(f, 0, SEEK_SET);
    insize = (int) fread(inbuf, 1, insize, f);
    fclose(f);
	
	unsigned char *svgOutBuf;
	int initoutsize = 10000000;
	svgOutBuf = (unsigned char *) malloc(initoutsize);
	unsigned int svgOutSize;
	
	unsigned char *rgbaOutBuf;
	rgbaOutBuf = (unsigned char *) malloc(1000*1000*4);
	
	
	int rgbaOutWidthInit = 0;
	int rgbaOutHeightInit =0;
	int *rgbaOutWidth = &rgbaOutWidthInit;
	int *rgbaOutHeight = &rgbaOutHeightInit;

	// use the setup fn for any setup, right now just for error codes
	cdr_setup();

	//  this outputs svg in svgOutBuf with length outsize, and outputs an RGBA image 
	int res = cdr_entry(inbuf,insize,svgOutBuf, &svgOutSize, rgbaOutBuf, rgbaOutWidth, rgbaOutHeight );
	

	 
	//add some error handling
	 if (res < BEV_FNS )
			fprintf(stderr,"This CDR file version is not supported, please contact Bevara\n");
		
	// for SVG debugging -- this just outputs the SVG text in outBuf
	// else 
	// {
	  // int i;
		// for (i=0; i<svgOutSize; ++i)
			// printf("%c",*(svgOutBuf+i));
	// }
	  
	     
		 
	// this is for testing - take the RGBA values in the RGBA output buffer and write to a file.
	// printf("\n\nreturned  outwidth = %d, outheight = %d\n\n\n ",*rgbaOutWidth, *rgbaOutHeight);
	// FILE *f2; 
	// f2=fopen("TEST_cdr_output.ppm","wb");
	// fprintf(f2, "P6\n%d %d\n255\n", *rgbaOutWidth, *rgbaOutHeight);
	
	// int ii;
	// unsigned char *tmpptr = rgbaOutBuf;
	// for (ii=0; ii< (*rgbaOutWidth)*(*rgbaOutHeight); ++ii) // write the RGB vals to the ppm file
	// {
		// fwrite(tmpptr, 1, 1, f2); 
		// ++tmpptr;
		// fwrite(tmpptr, 1, 1, f2); 
		// ++tmpptr;
		// fwrite(tmpptr, 1, 1, f2); 
		// ++tmpptr;
		// ++tmpptr; //skip alpha
	// }
	// fclose(f2);
	  
	return 0;
	
	
}
/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
