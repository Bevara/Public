/*
 * djpeg.c
 *
 * Copyright (C) 1991-1997, Thomas G. Lane.
 * Modified 2009 by Guido Vollbeding.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains a command-line user interface for the JPEG decompressor.
 * It should work on any system with Unix- or MS-DOS-style command lines.
 *
 * Two different command line styles are permitted, depending on the
 * compile-time switch TWO_FILE_COMMANDLINE:
 *	djpeg [options]  inputfile outputfile
 *	djpeg [options]  [inputfile]
 * In the second style, output is always to standard output, which you'd
 * normally redirect to a file or pipe to some other program.  Input is
 * either from a named file or from standard input (typically redirected).
 * The second style is convenient on Unix but is unhelpful on systems that
 * don't support pipes.  Also, you MUST use the first style if your system
 * doesn't do binary I/O to stdin/stdout.
 * To simplify script writing, the "-outfile" switch is provided.  The syntax
 *	djpeg [options]  -outfile outputfile  inputfile
 * works regardless of which command line style is used.
 */

#include "cdjpeg.h"		/* Common decls for cjpeg/djpeg applications */
#include "jversion.h"		/* for version message */
#include "jpegint.h"

#include <emscripten/emscripten.h>

unsigned char *outbuf;
int imgWidth;
int imgHeight;





/* globals for processing and output */

long OUTPUT_PTR = 0;
struct jpeg_decompress_struct cinfo;
struct jpeg_error_mgr jerr;
djpeg_dest_ptr dest_mgr = NULL;

/* for debugging */
/* FILE * input_file; */
/* FILE * output_file; */


JDIMENSION num_scanlines;

/* end of globals */

/* Need this to read from buffer */
/* From write ppm code :Private version of data destination object */

typedef struct {
  struct djpeg_dest_struct pub;	/* public fields */

  /* Usually these two pointers point to the same place: */
  char *iobuffer;		/* fwrite's I/O buffer */
  JSAMPROW pixrow;		/* decompressor output buffer */
  size_t buffer_width;		/* width of I/O buffer */
  JDIMENSION samples_per_row;	/* JSAMPLEs per output row */
} ppm_dest_struct;

typedef ppm_dest_struct * ppm_dest_ptr;


/* GPAC stuff */
#define IMG_CM_SIZE		1


/* Create the add-on message string table. */

#define JMESSAGE(code,string)	string ,

static const char * const cdjpeg_message_table[] = {
#include "cderror.h"
  NULL
};


/*
 * This list defines the known output image formats
 * (not all of which need be supported by a given version).
 * You can change the default output format by defining DEFAULT_FMT;
 * indeed, you had better do so if you undefine PPM_SUPPORTED.
 */

typedef enum {
	FMT_BMP,		/* BMP format (Windows flavor) */
	FMT_GIF,		/* GIF format */
	FMT_OS2,		/* BMP format (OS/2 flavor) */
	FMT_PPM,		/* PPM/PGM (PBMPLUS formats) */
	FMT_RLE,		/* RLE format */
	FMT_TARGA,		/* Targa format */
	FMT_TIFF		/* TIFF format */
} IMAGE_FORMATS;


#define DEFAULT_FMT	FMT_PPM



/* BEVARA: leave in for future marker handling options */
/*
 * Marker processor for COM and interesting APPn markers.
 * This replaces the library's built-in processor, which just skips the marker.
 * We want to print out the marker as text, to the extent possible.
 * Note this code relies on a non-suspending data source.
 */

/* LOCAL(unsigned int) */
/* jpeg_getc (j_decompress_ptr cinfo) */
/* /\* Read next byte *\/ */
/* { */
/*   struct jpeg_source_mgr * datasrc = cinfo->src; */

/*   if (datasrc->bytes_in_buffer == 0) { */
/*     if (! (*datasrc->fill_input_buffer) (cinfo)) */
/*       ERREXIT(cinfo, JERR_CANT_SUSPEND); */
/*   } */
/*   datasrc->bytes_in_buffer--; */
/*   return GETJOCTET(*datasrc->next_input_byte++); */
/* } */


/* METHODDEF(boolean) */
/* print_text_marker (j_decompress_ptr cinfo) */
/* { */
/*   boolean traceit = (cinfo->err->trace_level >= 1); */
/*   INT32 length; */
/*   unsigned int ch; */
/*   unsigned int lastch = 0; */

/*   length = jpeg_getc(cinfo) << 8; */
/*   length += jpeg_getc(cinfo); */
/*   length -= 2;			/\* discount the length word itself *\/ */

/*   if (traceit) { */
/*     if (cinfo->unread_marker == JPEG_COM) */
/*       fprintf(stderr, "Comment, length %ld:\n", (long) length); */
/*     else			/\* assume it is an APPn otherwise *\/ */
/*       fprintf(stderr, "APP%d, length %ld:\n", */
/* 	      cinfo->unread_marker - JPEG_APP0, (long) length); */
/*   } */

/*   while (--length >= 0) { */
/*     ch = jpeg_getc(cinfo); */
/*     if (traceit) { */
/*       /\* Emit the character in a readable form. */
/*        * Nonprintables are converted to \nnn form, */
/*        * while \ is converted to \\. */
/*        * Newlines in CR, CR/LF, or LF form will be printed as one newline. */
/*        *\/ */
/*       if (ch == '\r') { */
/* 	fprintf(stderr, "\n"); */
/*       } else if (ch == '\n') { */
/* 	if (lastch != '\r') */
/* 	  fprintf(stderr, "\n"); */
/*       } else if (ch == '\\') { */
/* 	fprintf(stderr, "\\\\"); */
/*       } /\* else if (isprint(ch)) { *\/ */
/*       /\* 	putc(ch, stderr); *\/ */
/*       /\* } *\/ else { */
/* 	fprintf(stderr, "\\%03o", ch); */
/*       } */
/*       lastch = ch; */
/*     } */
/*   } */

/*   if (traceit) */
/*     fprintf(stderr, "\n"); */

/*   return TRUE; */
/* } */

int EMSCRIPTEN_KEEPALIVE jpg_complete_setup()
{
 /* Initialize the JPEG decompression object with default error handling. */
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);
  /* Add some application-specific error messages (from cderror.h) */
  jerr.addon_message_table = cdjpeg_message_table;
  jerr.first_addon_message = JMSG_FIRSTADDONCODE;
  jerr.last_addon_message = JMSG_LASTADDONCODE;

  


  /* BEVARA: leave in marker processing code in case we need to add */
  /* it back in */

  /* Insert custom marker processor for COM and APP12.
   * APP12 is used by some digital camera makers for textual info,
   * so we provide the ability to display it as text.
   * If you like, additional APPn marker types can be selected for display,
   * but don't try to override APP0 or APP14 this way (see libjpeg.doc).
   */
  /* jpeg_set_marker_processor(&cinfo, JPEG_COM, print_text_marker); */
  /* jpeg_set_marker_processor(&cinfo, JPEG_APP0+12, print_text_marker); */

  return 1;
}

int EMSCRIPTEN_KEEPALIVE jpg_complete_shutdown()
{
  /* clean up  and shutdown*/
  /* Finish decompression and release memory.
   * I must do it in this order because output module has allocated memory
   * of lifespan JPOOL_IMAGE; it needs to finish before releasing memory.
   */

  (void) jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);


  return 1;
}

/* int jpg_complete_entry(  */
/* 		unsigned char *inBuffer, unsigned int inBufferLength, */
/* 		unsigned char *outBuffer, unsigned int *outBufferLength, */
/* 		unsigned char PaddingBits, unsigned int mmlevel) */
int EMSCRIPTEN_KEEPALIVE decodeJPEG(char *inBuffer, unsigned int inBufferLength)
{

    /* Specify data source for decompression */
    /* jpeg_stdio_src(&cinfo, input_file); */
  jpeg_mem_src(&cinfo, (unsigned char*) inBuffer, (unsigned long) inBufferLength);


  /* Read file header, set default decompression parameters */ 
  /* printf("reading header \n");     */
  (void) jpeg_read_header(&cinfo, TRUE);
  
  /* printf("image dims are height = %d width = %d\n",cinfo.image_height,cinfo.Image_width); */
  /* printf("width =%d, height=%d, input format = %d, components = %d quantize flag = %d\n",out_width,out_height,cinfo.jpeg_color_space,cinfo.output_components,cinfo.quantize_colors); */

  imgWidth = cinfo.image_width; 
  imgHeight = cinfo.image_height; 


 

  if (outbuf != NULL)
    free(outbuf);
  // do full RGB alloc
   outbuf = (unsigned char *) malloc(imgHeight*imgWidth*4); // now outputting RGBA


  /* force to ppm */
  dest_mgr = jinit_write_ppm(&cinfo);
  /* dest_mgr->output_file = output_file; */

  /* Start decompressor */
  (void) jpeg_start_decompress(&cinfo);

  /* Write output file header */
  /* (*dest_mgr->start_output) (&cinfo, dest_mgr); */

  ppm_dest_ptr dest;

  
  unsigned char* tmpptr = outbuf;
  int i;
  
  
 switch(cinfo.jpeg_color_space)
    {
    case 1:
      // Greyscale gets converted to RGB for output
      /* Process data */
      while (cinfo.output_scanline < cinfo.output_height) {
			num_scanlines = jpeg_read_scanlines(&cinfo, dest_mgr->buffer,1);
			/* (*dest_mgr->put_pixel_rows) (&cinfo, dest_mgr, num_scanlines); */
			dest = (ppm_dest_ptr) dest_mgr;
			for (i=0; i<dest->buffer_width; ++i)
			  {
				*tmpptr = *(dest->iobuffer+i); ++tmpptr;
				*tmpptr = *(dest->iobuffer+i); ++tmpptr;
				*tmpptr = *(dest->iobuffer+i); ++tmpptr;
				*tmpptr = 255; ++tmpptr;
			  }
		}
      break;
    case 3: 
      /* Process data */
	  
      while (cinfo.output_scanline < cinfo.output_height) {
			num_scanlines = jpeg_read_scanlines(&cinfo, dest_mgr->buffer,1);
			/* (*dest_mgr->put_pixel_rows) (&cinfo, dest_mgr, num_scanlines); */
			dest = (ppm_dest_ptr) dest_mgr;
			// rewritten to insert alpha value
			//memcpy(outbuf+OUTPUT_PTR,dest->iobuffer,dest->buffer_width); 
			//OUTPUT_PTR += dest->buffer_width;
			
			for (i=0; i<dest->buffer_width; i=i+3)
				{
					*(outbuf+OUTPUT_PTR) = *(dest->iobuffer+i); ++OUTPUT_PTR;
					*(outbuf+OUTPUT_PTR) = *(dest->iobuffer+i+1); ++OUTPUT_PTR;
					*(outbuf+OUTPUT_PTR) = *(dest->iobuffer+i+2); ++OUTPUT_PTR;
					*(outbuf+OUTPUT_PTR) =255; ++OUTPUT_PTR;
				}
			
		}

      break;
    default:
      return 0;
    }

  return 1;
}  



int EMSCRIPTEN_KEEPALIVE jpegGetWidth()
{
		return imgWidth;
}


int EMSCRIPTEN_KEEPALIVE jpegGetHeight()
{
	return imgHeight;
}

unsigned char* EMSCRIPTEN_KEEPALIVE jpegGetImage()
{
	return outbuf;
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

	
	jpg_complete_setup();
	

    fseek(f, 0, SEEK_END);
    insize = (int) ftell(f);
    inbuf = malloc(insize);
    fseek(f, 0, SEEK_SET);
    insize = (int) fread(inbuf, 1, insize, f);
    fclose(f);
 
	
	
	decodeJPEG(inbuf,insize);
	
	printf("going to output ppm\n");
	
	
	
	locoutwidth = jpegGetWidth();
	locoutheight = jpegGetHeight();
	locoutbuf = jpegGetImage();


	// check the output
	printf("width = %d, height = %d\n",locoutwidth,locoutheight);
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
	for (i=0 ; i<locoutheight; ++i)
	{
		for (j=0; j<locoutwidth; ++j)
		{
			*(otmp) = *(tmp); ++otmp; ++tmp;
			*(otmp) = *(tmp); ++otmp; ++tmp;
			*(otmp) = *(tmp); ++otmp; ++tmp;
			++tmp; // skip A
		}
	}
	

	fwrite(tmpoutbuf, 1, locoutwidth*locoutheight*3, f2);
	free(tmpoutbuf);
	fclose(f2); 
	//end of output check

	
	 jpg_complete_shutdown();
	
		// after everything we have to:
	   free(outbuf);
	   free(inbuf);

		return 1;
}

