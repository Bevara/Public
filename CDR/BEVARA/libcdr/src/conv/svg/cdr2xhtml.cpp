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

#include <iostream>
#include <stdio.h>
#include <string.h>
#include <librevenge/librevenge.h>
#include <librevenge-generators/librevenge-generators.h>
#include <librevenge-stream/librevenge-stream.h>
#include <libcdr/libcdr.h>

#ifndef VERSION
#define VERSION "UNKNOWN VERSION"
#endif

namespace
{

int printUsage()
{
  printf("`cdr2xhtml' converts CorelDRAW documents to SVG.\n");
  printf("\n");
  printf("Usage: cdr2xhtml [OPTION] FILE\n");
  printf("\n");
  printf("Options:\n");
  printf("\t--help                show this help message\n");
  printf("\t--version             show version information and exit\n");
  printf("\n");
  printf("Report bugs to <https://bugs.documentfoundation.org/>.\n");
  return -1;
}

int printVersion()
{
  printf("cdr2xhtml " VERSION "\n");
  return 0;
}

} // anonymous namespace

int cdr_entry(const unsigned char *inBuf, const unsigned int inSize, unsigned char *outBuf,unsigned int* outSize)
{
  // if (argc < 2)
    // return printUsage();

  // char *file = nullptr;

  // for (int i = 1; i < argc; i++)
  // {
    // if (!strcmp(argv[i], "--version"))
      // return printVersion();
    // else if (!file && strncmp(argv[i], "--", 2))
      // file = argv[i];
    // else
      // return printUsage();
  // }

  // if (!file)
    // return printUsage();

 // librevenge::RVNGFileStream input(file);
 
  librevenge::RVNGStringStream input(inBuf,inSize);
  librevenge::RVNGStringVector output;
  librevenge::RVNGSVGDrawingGenerator painter(output, "svg");

  if (!libcdr::CDRDocument::isSupported(&input))
  {
    if (!libcdr::CMXDocument::isSupported(&input))
    {
      fprintf(stderr, "ERROR: Unsupported file format (unsupported version) or file is encrypted!\n");
      return 1;
    }
    else if (!libcdr::CMXDocument::parse(&input, &painter))
    {
      fprintf(stderr, "ERROR: Parsing of document failed!\n");
      return 1;
    }
  }
  else if (!libcdr::CDRDocument::parse(&input, &painter))
  {
    fprintf(stderr, "ERROR: Parsing of document failed!\n");
    return 1;
  }

  if (output.empty())
  {
    std::cerr << "ERROR: No SVG document generated!" << std::endl;
    return 1;
  }
  
// char tmpStr[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?> \n";
//  memcpy((void *) outBuf, tmpStr, (strlen(tmpStr)+1));

	
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
	
	for (unsigned k = 0; k<output.size(); ++k)
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
		strLength = strlen(output[k].cstr());
		memcpy((outBuf + *outSize),output[k].cstr(),strLength);
		*outSize = *outSize + strLength;
   
		}

	strLength = strlen("\n  </body> \n </html> \n");
	memcpy((outBuf + *outSize),"\n  </body> \n </html> \n",strLength);
	*outSize = *outSize + strLength;
	

		
  // std::cout << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
  // std::cout << "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">" << std::endl;
  // std::cout << "<html xmlns=\"http://www.w3.org/1999/xhtml\" xmlns:svg=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\">" << std::endl;
  // std::cout << "<body>" << std::endl;
  // std::cout << "<?import namespace=\"svg\" urn=\"http://www.w3.org/2000/svg\"?>" << std::endl;

  // for (unsigned k = 0; k<output.size(); ++k)
  // {
    // if (k>0)
      // std::cout << "<hr/>\n";

    // std::cout << "<!-- \n";
    // std::cout << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n";
    // std::cout << "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\"";
    // std::cout << " \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n";
    // std::cout << " -->\n";

    // std::cout << output[k].cstr() << std::endl;
  // }

  // std::cout << "</body>" << std::endl;
  // std::cout << "</html>" << std::endl;

  return 0;
}

// Bevara: main is for debugging
int main(int argc, char *argv[])
{
	
	unsigned int insize; 
    unsigned char *inbuf;   
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
    inbuf = (unsigned char *) malloc(insize);
    fseek(f, 0, SEEK_SET);
    insize = (int) fread(inbuf, 1, insize, f);
    fclose(f);
	
	unsigned char *outBuf;
	int initoutsize = 10000000;
	outBuf = (unsigned char *) malloc(initoutsize);
	unsigned int outSize;

	
	int res = cdr_entry(inbuf,insize,outBuf, &outSize);
	
	int i;
	
	//add some error handling
	 if (res ==1 )
			fprintf(stderr,"This CDR file version is not supported, please contact Bevara\n");
		
	else 
	{
		for (i=0; i<outSize; ++i)
			printf("%c",*(outBuf+i));
	}
	  
	return 0;
	
	
}
/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
