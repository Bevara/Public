/*
 * mudraw -- command line tool for drawing pdf/xps/cbz documents
 */
 
#include "fitz.h"
#include "pdf.h" 

#define GF_BAD_PARAM -9
#define GF_BUFFER_TOO_SMALL -5
#define GF_NOT_SUPPORTED -1
#define GF_OK 1

#include <stdio.h>
#include <stdlib.h>

// Bevara change for debugging
#define EMSCRIPTEN_KEEPALIVE
/*
#include <emscripten/emscripten.h>
*/


// for debugging
int debugVal;
char debugValChar;
char debugString[50];

// the globals holding the output page
unsigned char* outBuf;
int outwidth;
int outheight;

#define IMG_CM_SIZE		1

enum { TEXT_PLAIN = 1, TEXT_HTML = 2, TEXT_XML = 3 };

enum { OUT_PNG, OUT_PPM, OUT_PNM, OUT_PAM, OUT_PGM, OUT_PBM, OUT_SVG, OUT_PWG, OUT_PCL, OUT_PDF, OUT_TGA };

enum { CS_INVALID, CS_UNSET, CS_MONO, CS_GRAY, CS_GRAY_ALPHA, CS_RGB, CS_RGB_ALPHA, CS_CMYK, CS_CMYK_ALPHA };



/* globals */
fz_context *ctx;
static float resolution = 72;
/* static int res_specified = 0; */
static float rotation = 0;
static int alphabits = 8;
static int width = 0;
static int height = 0;
static int fit = 0;
static int errored = 0;
static int ignore_errors = 0;
static int output_format;
static int out_cs = CS_UNSET;
static fz_colorspace *colorspace;
static char *filename = "TEST.pdf";
fz_output *out = NULL;
int height_output; /*height of curr page in pix*/
int width_output; /* width of curr page in pic */
int pagecount = 0; /* total number of pages in doc */
int current_page = 1; /* current page to be decoded */
fz_pixmap *pix = NULL;
int totalheight;
/* end globals */

int outbuf_size = 0;






int drawpage(fz_context *ctx, pdf_document *doc, int pagenum)
{
	pdf_page *page;
	/* fz_display_list *list = NULL; */
	fz_device *dev = NULL;
	fz_cookie cookie = { 0 };
	float zoom;
	fz_matrix ctm;
	fz_rect bounds, tbounds;
	fz_irect ibounds;
	int w, h;
	//fz_output *output_file = NULL;




	/* printf("decoding pagenum = %d\n",pagenum); */

	fz_var(dev);

	fz_try(ctx)
	{
		page = pdf_load_page(doc, pagenum - 1);
	}
	fz_catch(ctx)
	{
		fz_rethrow_message(ctx, "cannot load page %d in file '%s'", pagenum, filename);
	}

	fz_var(pix);
	

	/* Leave all of this in, just in case we eventually want to do */
	/* scaling using this instead of GPAC */


	pdf_bound_page(doc, page, &bounds);
	zoom = resolution / 72;


        fz_pre_scale(fz_rotate(&ctm, rotation), zoom, zoom);
	tbounds = bounds;


	fz_round_rect(&ibounds, fz_transform_rect(&tbounds, &ctm));

	/* Make local copies of our width/height */
	w = width;
	h = height;

	        /* Leave this in, just in case of later scaling */
		/* If a resolution is specified, check to see whether w/h are
		 * exceeded; if not, unset them. */
		/* if (res_specified) */
		/* { */
		/* 	int t; */
		/* 	t = ibounds.x1 - ibounds.x0; */
		/* 	if (w && t <= w) */
		/* 		w = 0; */
		/* 	t = ibounds.y1 - ibounds.y0; */
		/* 	if (h && t <= h) */
		/* 		h = 0; */
		/* } */

	 /* Now w or h will be 0 unless they need to be enforced. */
	 if (w || h)
	   {
	     float scalex = w / (tbounds.x1 - tbounds.x0);
	     float scaley = h / (tbounds.y1 - tbounds.y0);
	     fz_matrix scale_mat;

	     /* Leave this mess in, in case we want to adding scaling later */
	     if (fit)
	       {
		 if (w == 0)
		   scalex = 1.0f;
		 if (h == 0)
		   scaley = 1.0f;
	       }
	     else
	       {
		 if (w == 0)
		   scalex = scaley;
		 if (h == 0)
		   scaley = scalex;
	       }
	     if (!fit)
	       {
		 if (scalex > scaley)
		   scalex = scaley;
		 else
		   scaley = scalex;
	       }

	     fz_scale(&scale_mat, scalex, scaley);
	     fz_concat(&ctm, &ctm, &scale_mat);
	     tbounds = bounds;
	     fz_transform_rect(&tbounds, &ctm);
	   }


	 fz_round_rect(&ibounds, &tbounds);


	 fz_rect_from_irect(&tbounds, &ibounds);



        fz_try(ctx)
	{

	  fz_irect band_ibounds = ibounds;
	  //int band = 0;
	  //char filename_buf[512];
	  totalheight = ibounds.y1 - ibounds.y0;
	  int drawheight = totalheight;



	  pix = fz_new_pixmap_with_bbox(ctx, colorspace, &band_ibounds);


	  fz_pixmap_set_resolution(pix, resolution);


	  /* check outbuf_size */
	  /* NOTE - have this hardcoded to the three-byte RGB output */
	  /* printf("in drawpage need outbuf space %d have output space %d\n",pix->w * totalheight *3, outbuf_size); */
	  /* if ( 	outbuf_size < pix->w * totalheight *3) */
	  /*   { */
	  /*     printf("in drawpage outbuffer not big enough....try again \n"); */
	  /*     outbuf_size = pix->w * totalheight *3; */
	  /*     /\* shut everything down and go back for more memory *\/ */
	  /*     fz_free_device(dev); */
	  /*     fz_drop_pixmap(ctx, pix); */
	  /*     pdf_free_page(doc, page); */
	  /*     return -9; */
	  /*   } */


	  /* temp for debugging */
	  /* sprintf(filename_buf, output, pagenum); */
	  /* output_file = fz_new_output_to_filename(ctx, filename_buf); */
          /* fz_output_pnm_header(output_file, pix->w, totalheight, pix->n); */



	  /* start the decoding of curr page */

	  fz_clear_pixmap_with_value(ctx, pix, 255);

	  dev = fz_new_draw_device(ctx, pix);

	  pdf_run_page(doc, page, dev, &ctm, &cookie);
	  fz_free_device(dev);
	  dev = NULL;


	 

	  /* temp for debugging */
	  /* printf("writing to file %s\n",filename); */

	  width_output = pix->w;
	  height_output = totalheight;


          ctm.f -= drawheight;
	}
	fz_always(ctx)
	{


	  fz_free_device(dev);



	  dev = NULL;
	}
	fz_catch(ctx)
	{
	  pdf_free_page(doc, page);
	  fz_rethrow(ctx);
	}



	pdf_free_page(doc, page);


	if (cookie.errors)
		errored = 1;

	return GF_OK;
}


void EMSCRIPTEN_KEEPALIVE docShutdown()
{
	fz_free_context(ctx);
}

int EMSCRIPTEN_KEEPALIVE docSetup()
{

  printf("in docSetup\n");

	ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);

	if (!ctx)
	{
		printf("cannot initialise context\n");
		return GF_BAD_PARAM;
	}



	fz_set_aa_level(ctx, alphabits);

	printf("hardcoded input filename to TEST.pdf\n");
	printf("hardcoding output format to PPM\n");
	output_format = OUT_PPM; /*OUT_PNG;*/
	printf("hardcoding colorspace to RGB/RGBA\n");
	/* also hardcoded to RGB=3 bytes for outbuf_size */
	out_cs = CS_RGB;

	fz_try(ctx)
	{
	  colorspace = fz_device_rgb(ctx);
	  fz_register_document_handlers(ctx);
	  printf("done registering doc handlers\n");
	}
	fz_catch(ctx)
	{
	  printf("error in colorspace or document handler registering\n");
	  return GF_BAD_PARAM;
	}

	return GF_OK; /*success */

}

int out_size =0;



int EMSCRIPTEN_KEEPALIVE docGetPageCount()
{
  return pagecount;

}

int EMSCRIPTEN_KEEPALIVE docSetPage(int  currpage)
{
  current_page  = currpage;
  return GF_OK; /* set to success value */
}


int EMSCRIPTEN_KEEPALIVE
/* entry(char *inbuf, int insize,  unsigned char **outbuf, int *outbuf_sz, char PaddingBits, unsigned int mmlevel ) */
docDecode(char *inbuf, int insize)
{
  fz_stream *stream = NULL;
  pdf_document *doc = NULL;
  int drawpage_result = 0;




  fz_try(ctx)
  {
    fz_try(ctx)
    {

      /* replaced this with modified pdf_open_document fn that it calls
	 from pdf_document_handler */
      /* doc = fz_open_document(ctx, filename);   */

	  
	  printf("creatin gnew stream\n");
      stream = fz_open_inbuf(ctx, inbuf, insize); /* new fn to handle inbuf */


		printf("creatin gnew doc\n");
      doc = pdf_new_document(ctx, stream);


	  printf("going to init doc\n");
      pdf_init_document(doc);


      /* this is the remainder of pdf_open_document */
	  
	  printf("run page\n");
      doc->super.run_page_contents = (fz_document_run_page_contents_fn *)pdf_run_page_contents;
	   printf("run annot\n");
      doc->super.run_annot = (fz_document_run_annot_fn *)pdf_run_annot;
	  
	   printf("update appearance\n");
      doc->update_appearance = pdf_update_appearance;

    }
    fz_catch(ctx)
    {
      pdf_close_document(doc);
      fz_rethrow_message(ctx, "cannot open document: %s", filename);
    }

    /* decode and output */

    printf("count the pdf pages\n"); 

    pagecount = pdf_count_pages(doc);
    printf("currentpage = %d pagecount = %d\n",current_page,pagecount); 
    if ((current_page > pagecount) || (current_page <1))
       {
	printf("invalid page number\n");
	/* shutdown and give up */
	pdf_close_document(doc);
        return GF_BAD_PARAM; /* choose correct return value */
       }

  
   

    drawpage_result = drawpage(ctx, doc, current_page);
   
    outwidth = pix->w;
    outheight = totalheight;


 
    /* if (drawpage_result == -9) */
    /*   { */
    /* 	  *outbuf_sz = outbuf_size; */
    /* 	printf("outbuf not large enough....reallocate and try again...\n"); */
    /* 	/\* shutdown and head out for more memory *\/ */
    /* 	pdf_close_document(doc); */
    /*     return GF_BUFFER_TOO_SMALL; */
    /*   } */


    outBuf= (unsigned char *) malloc(pix->w*totalheight*4); //now do RGBA
    /*copy from global pimap to outbuf*/
    /* This is indeed crazy, but lets us keep mupdf's mallocs and reallocs */
    /* in place, rather than rewriting */
	  int i;
	  unsigned char *tempout_ptr;
	  unsigned char *tempin_ptr;
	  tempout_ptr = outBuf;
	  tempin_ptr = pix->samples;

	  

	   /* hardcode 3 byte output for RGB format  */


	  for (i=0; i<  pix->w * totalheight; ++i)
	    {
	      /* printf("i=%d RGB=%d %d %d \n",i,*tempin_ptr++,*tempin_ptr++,*tempin_ptr++); ++tempin_ptr; */
	      /* write RGB vals */
	      
	      *tempout_ptr = *tempin_ptr; /* R */
	      ++tempout_ptr; ++tempin_ptr;
	      *tempout_ptr = *tempin_ptr; /*G */
	      ++tempout_ptr; ++tempin_ptr;
	      *tempout_ptr = *tempin_ptr; /*B */
	      ++tempout_ptr; ++tempin_ptr;
		  *tempout_ptr = *tempin_ptr; /*A */
	      ++tempout_ptr; ++tempin_ptr;
	    }
	

    fz_drop_pixmap(ctx, pix); 

    pdf_close_document(doc);
    doc = NULL;
  }
  fz_catch(ctx)
  {
    printf("got errors after tried to drawpage\n");
    if (!ignore_errors)
      fz_rethrow(ctx);
    pdf_close_document(doc);
    doc = NULL;

  }

	return GF_OK; /*success */
}



int EMSCRIPTEN_KEEPALIVE docGetWidth()
{
		return outwidth;
}


int EMSCRIPTEN_KEEPALIVE docGetHeight()
{
	return outheight;
}

unsigned char* EMSCRIPTEN_KEEPALIVE docGetPage()
{
	return outBuf;
}


int main(int argc, char* argv[])
{



	if (docSetup() != GF_OK) 
	  {
	    printf("pdf setup step failed....exiting\n");
	    return 0;
	  }


	int insize;
	char *inbuf = NULL;
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
	inbuf = malloc(insize);
	fseek(f, 0, SEEK_SET);
	insize = (int) fread(inbuf, 1, insize, f);
	printf("filesize = %d\n",insize);
	fclose(f);
 
 
	int page=1; // this is the page you want to decode; start with 1
	int numpages =0; // will store the number of pages in the doc 

	// do this in order - parsing actually sets the total number of pages
	// so on first call decode page 1 and retrieve and display number of pages
	// at same time you get the page RGBA data
	docSetPage(page);
	
	printf("going to doc decode\n"); fflush(stdout);
	docDecode(inbuf,insize);
	numpages = docGetPageCount();
 
	unsigned char *locoutbuf;    
    int locoutwidth;
    int locoutheight; 
	locoutwidth = docGetWidth();
	locoutheight = docGetHeight();
	locoutbuf = docGetPage();
	
	// checking the output 
	printf("decoded page %d of %d; page size is %d x %d\n",page,numpages,locoutwidth,locoutheight);	
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

	docShutdown();


	// do this at end 
	free(inbuf);
	free(outBuf);


	return (errored != 0);
}


