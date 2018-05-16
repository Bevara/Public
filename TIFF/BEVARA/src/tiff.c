/*
 * mudraw -- command line tool for drawing pdf/xps/cbz documents
 */
/* #include "accessor/setup.h" */
#include <emscripten/emscripten.h>

#include "fitz.h"


// BEVARA: legacy params
#define GF_BAD_PARAM -9
#define GF_BUFFER_TOO_SMALL -5
#define GF_OK 1

#include <stdio.h>
#include <stdlib.h>



// BEVARA: the globals holding the output page
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
//static int alphabits = 8;
static int width = 0;
static int height = 0;
static int fit = 0;
static int errored = 0;
static int ignore_errors = 0;
static int output_format =  OUT_PPM;
static int out_cs = CS_UNSET;
static fz_colorspace *colorspace;
//static char *filename = "TEST.tiff";
fz_output *out = NULL;
int height_output =0 ; /*height of curr page in pix*/
int width_output =0; /* width of curr page in pic */
int pagecount = 0; /* total number of pages in doc */
int current_page = 0; /* current page to be decoded */
int final_out_size = 0;
fz_pixmap *pix = NULL;
int totalheight = 0;
/* end globals */


typedef struct tiff_document_s tiff_document;
typedef struct tiff_page_s tiff_page;

static void tiff_init_document(tiff_document *doc);

#define DPI 72.0f

struct tiff_page_s
{
	fz_image *image;
};

struct tiff_document_s
{
	fz_document super;

	fz_context *ctx;
	fz_stream *file;
	fz_buffer *buffer;
	int page_count;
};

void
tiff_free_page(tiff_document *doc, tiff_page *page)
{
	if (!page)
		return;
	fz_drop_image(doc->ctx, page->image);
	fz_free(doc->ctx, page);
}


void
tiff_close_document(tiff_document *doc)
{
	fz_context *ctx = doc->ctx;
	fz_drop_buffer(ctx, doc->buffer);
	fz_close(doc->file);
	fz_free(ctx, doc);
}

tiff_document *
tiff_open_document_with_stream(fz_context *ctx, fz_stream *file)
{
	tiff_document *doc;
	int len;
	unsigned char *buf;

	doc = fz_malloc_struct(ctx, tiff_document);
	tiff_init_document(doc);
	doc->ctx = ctx;
	doc->file = fz_keep_stream(file);
	doc->page_count = 0;

	fz_try(ctx)
	{
		doc->buffer = fz_read_all(doc->file, 1024);
		len = doc->buffer->len;
		buf = doc->buffer->data;

		doc->page_count = fz_load_tiff_subimage_count(ctx, buf, len);
	}
	fz_catch(ctx)
	{
		tiff_close_document(doc);
		fz_rethrow(ctx);
	}

	return doc;
}

tiff_document *
tiff_open_document(fz_context *ctx, const char *filename)
{
	/* fz_stream *file; */
	tiff_document *doc =  NULL;

	/* file = fz_open_file(ctx, filename); */
	/* if (!file) */
	/* 	fz_throw(ctx, FZ_ERROR_GENERIC, "cannot open file"); */

	/* fz_try(ctx) */
	/* { */
	/* 	doc = tiff_open_document_with_stream(ctx, file); */
	/* } */
	/* fz_always(ctx) */
	/* { */
	/* 	fz_close(file); */
	/* } */
	/* fz_catch(ctx) */
	/* { */
	/* 	fz_rethrow(ctx); */
	/* } */

	return doc;
}


int
tiff_count_pages(tiff_document *doc)
{
	return doc->page_count;
}

tiff_page *
tiff_load_page(tiff_document *doc, int number)
{
	fz_context *ctx = doc->ctx;
	fz_image *mask = NULL;
	fz_pixmap *pixmap = NULL;
	tiff_page *page = NULL;

	if (number < 0 || number >= doc->page_count)
		return NULL;

	fz_var(pixmap);
	fz_var(page);
	fz_try(ctx)
	{
		pixmap = fz_load_tiff_subimage(ctx, doc->buffer->data, doc->buffer->len, number);

		page = fz_malloc_struct(ctx, tiff_page);
		page->image = fz_new_image_from_pixmap(ctx, pixmap, mask);
	}
	fz_catch(ctx)
	{
		tiff_free_page(doc, page);
		fz_rethrow(ctx);
	}

	return page;
}


fz_rect *
tiff_bound_page(tiff_document *doc, tiff_page *page, fz_rect *bbox)
{
	fz_image *image = page->image;
	bbox->x0 = bbox->y0 = 0;
	bbox->x1 = image->w * DPI / image->xres;
	bbox->y1 = image->h * DPI / image->yres;
	return bbox;
}

void
tiff_run_page(tiff_document *doc, tiff_page *page, fz_device *dev, const fz_matrix *ctm, fz_cookie *cookie)
{
	fz_matrix local_ctm = *ctm;
	fz_image *image = page->image;
	float w = image->w * DPI / image->xres;
	float h = image->h * DPI / image->yres;
	fz_pre_scale(&local_ctm, w, h);
	fz_fill_image(dev, image, &local_ctm, 1);
}

static int
tiff_meta(tiff_document *doc, int key, void *ptr, int size)
{
	switch (key)
	{
	case FZ_META_FORMAT_INFO:
		sprintf((char *)ptr, "TIFF");
		return FZ_META_OK;
	default:
		return FZ_META_UNKNOWN_KEY;
	}
}

static void
tiff_rebind(tiff_document *doc, fz_context *ctx)
{
	doc->ctx = ctx;
	fz_rebind_stream(doc->file, ctx);
}

static void
tiff_init_document(tiff_document *doc)
{
	doc->super.close = (fz_document_close_fn *)tiff_close_document;
	doc->super.count_pages = (fz_document_count_pages_fn *)tiff_count_pages;
	doc->super.load_page = (fz_document_load_page_fn *)tiff_load_page;
	doc->super.bound_page = (fz_document_bound_page_fn *)tiff_bound_page;
	doc->super.run_page_contents = (fz_document_run_page_contents_fn *)tiff_run_page;
	doc->super.free_page = (fz_document_free_page_fn *)tiff_free_page;
	doc->super.meta = (fz_document_meta_fn *)tiff_meta;
	doc->super.rebind = (fz_document_rebind_fn *)tiff_rebind;
}

static int
tiff_recognize(fz_context *doc, const char *magic)
{
	char *ext = strrchr(magic, '.');

	if (ext)
	{
		if (!fz_strcasecmp(ext, ".tiff") || !fz_strcasecmp(ext, ".tif"))
			return 100;
	}
	if (!strcmp(magic, "tif") || !strcmp(magic, "image/tiff") ||
		!strcmp(magic, "tiff") || !strcmp(magic, "image/x-tiff"))
		return 100;

	return 0;
}

fz_document_handler tiff_document_handler =
{
	(fz_document_recognize_fn *)&tiff_recognize,
	(fz_document_open_fn *)&tiff_open_document,
	(fz_document_open_with_stream_fn *)&tiff_open_document_with_stream
};


int drawpage(fz_context *ctx, tiff_document *doc, int pagenum, int *outbuf_size)
{
	tiff_page *page;
	/* fz_display_list *list = NULL; */
	fz_device *dev = NULL;
	fz_cookie cookie = { 0 };
	float zoom;
	fz_matrix ctm;
	fz_rect bounds, tbounds;
	fz_irect ibounds;
	int w, h;
	//fz_output *output_file = NULL;




	printf("decoding pagenum = %d\n",pagenum);

	fz_var(dev);

	fz_try(ctx)
	{
		page = tiff_load_page(doc, pagenum - 1);
	}
	fz_catch(ctx)
	{
		fz_rethrow_message(ctx, "cannot load page %d", pagenum);
	}

	fz_var(pix);
	

	/* Bevara says: leave all of this in, just in case we eventually want to do */
	/* scaling using this instead of external tools */

	printf("going to bound pg\n");

	tiff_bound_page(doc, page, &bounds);
	zoom = resolution / 72;

	printf("going to pre-scale pg\n");
        fz_pre_scale(fz_rotate(&ctm, rotation), zoom, zoom);
	tbounds = bounds;

	printf("going to round rect\n");
	fz_round_rect(&ibounds, fz_transform_rect(&tbounds, &ctm));
	printf("back from round rect\n");
	/* Make local copies of our width/height */
	w = width;
	h = height;

	        /* Bevara: also leave this in, just in case of later scaling */
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

	     /* Bevara:  leave this in, in case we want to adding scaling later */
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
	     printf("head to scale /concat/transform\n");
	     fz_scale(&scale_mat, scalex, scaley);
	     fz_concat(&ctm, &ctm, &scale_mat);
	     tbounds = bounds;
	     fz_transform_rect(&tbounds, &ctm);
	   }

	 printf("at second round rect\n");
	 fz_round_rect(&ibounds, &tbounds);

	 printf("at reat from irect\n");
	 fz_rect_from_irect(&tbounds, &ibounds);

        fz_try(ctx)
	{

	  fz_irect band_ibounds = ibounds;
	  //int band = 0;
	  //char filename_buf[512];
	  totalheight = ibounds.y1 - ibounds.y0;
	  int drawheight = totalheight;

	  printf("try new pixmap\n");

	  pix = fz_new_pixmap_with_bbox(ctx, colorspace, &band_ibounds);

	  printf("try to set pixmap res\n");
	  fz_pixmap_set_resolution(pix, resolution);

	  *outbuf_size =  pix->w * totalheight *3;

	  /* check outbuf_size */
	  /* NOTE - have thsi hardcoded to the three-byte RGB output */
	  /* printf("in drawpage need outbuf space %d have output space %d\n",pix->w * totalheight *3, *outbuf_size); */
	  /* if ( 	*outbuf_size < pix->w * totalheight *3) */
	  /*   { */
	  /*     printf("outbuffer not big enough....try again \n"); */
	  /*     *outbuf_size = pix->w * totalheight *3; */
	  /*     /\* shut everything down and go back for more memory *\/ */
	  /*     fz_free_device(dev); */
	  /*     fz_drop_pixmap(ctx, pix); */
	  /*     tiff_free_page(doc, page); */
	  /*     return GF_BUFFER_TOO_SMALL; */
	  /*   } */


	  /* temp for debugging */
	  /* sprintf(filename_buf, output, pagenum); */
	  /* output_file = fz_new_output_to_filename(ctx, filename_buf); */
          /* fz_output_pnm_header(output_file, pix->w, totalheight, pix->n); */



	  /* start the decoding of curr page */
	  printf("heading to clear pixmap\n");
	  fz_clear_pixmap_with_value(ctx, pix, 255);
	  printf("heading to new draw device\n");
	  dev = fz_new_draw_device(ctx, pix);
	  printf("heading to run page\n");
	  tiff_run_page(doc, page, dev, &ctm, &cookie);
	  fz_free_device(dev);
	  dev = NULL;


	  /* temp for debugging */
	  /* printf("writing to file %s\n",filename); */
	  /* fz_output_pnm_band(output_file, pix->w, totalheight, pix->n, band, drawheight, pix->samples); */
	  width_output = pix->w;
	  height_output = totalheight;


	  /* This is indeed crazy, but lets us keep mupdf's mallocs and reallocs */
	  /* in place, rather than rewriting */
	  /* int i; */
	  /* char *tempout_ptr; */
	  /* char *tempin_ptr; */
	  /* tempout_ptr = *outbuf; */
	  /* tempin_ptr = pix->samples; */
	  /* /\* hardcode 3 byte output for RGB format  *\/ */
	  /* for (i=0; i<  pix->w * totalheight; ++i) */
	  /*   { */
	  /*     /\* write RGB vals *\/ */
	  /*     *tempout_ptr = *tempin_ptr; /\* R *\/ */
	  /*     ++tempout_ptr; ++tempin_ptr; */
	  /*     *tempout_ptr = *tempin_ptr; /\*G *\/ */
	  /*     ++tempout_ptr; ++tempin_ptr; */
	  /*     *tempout_ptr = *tempin_ptr; /\*B *\/ */
	  /*     ++tempout_ptr; ++tempin_ptr; */

	  /*     ++tempin_ptr;  /\* skip alpha *\/ */
	  /*   } */
				
          ctm.f -= drawheight;
	}
	fz_always(ctx)
	{

	  fz_free_device(dev);
	  dev = NULL;
	  /* fz_drop_pixmap(ctx, pix); */
	  /* if (output_file) */
	  /*   fz_close_output(output_file); */
	}
	fz_catch(ctx)
	{
	  tiff_free_page(doc, page);
	  fz_rethrow(ctx);
	}


	tiff_free_page(doc, page);

	fz_flush_warnings(ctx);

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
	ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);

	if (!ctx)
	{
		printf("cannot initialise context\n");
		return 0;
	}

	output_format = OUT_PPM; /*OUT_PNG;*/
	printf("hardcoding colorspace to RGB/RGBA\n");
	/* also hardcoded to RGB=3 bytes for outbuf_size */
	out_cs = CS_RGB;

	fz_try(ctx)
	{
	  colorspace = fz_device_rgb(ctx);
	  fz_register_document_handlers(ctx);
	}
	fz_catch(ctx)
	{
	  printf("error in colorspace or document handler regustering\n");
	  return GF_BAD_PARAM;
	}

	return GF_OK; /*success */

}


int EMSCRIPTEN_KEEPALIVE docGetPageCount()
{
  return pagecount;

 
}

int EMSCRIPTEN_KEEPALIVE docSetPage(int  currpage)
{
  current_page  = currpage;
  return GF_OK; /* set to success value */
}


int EMSCRIPTEN_KEEPALIVE docDecode(char *inbuf, int insize)
{
  fz_stream *stream = NULL;
  tiff_document *doc = NULL;
  int drawpage_result = 0;
  int outsize = 0;

  fz_try(ctx)
  {
    fz_try(ctx)
    {
      printf("make and open doc\n");
      /* replace this with modified pdf_open_document fn that it calls
	 from pdf_document_handler */
      /* doc = fz_open_document(ctx, filename);   */

      stream = fz_open_inbuf(ctx, inbuf, insize); /* new fn to handle inbuf */


      printf("calling new document\n");
      doc = tiff_open_document_with_stream(ctx, stream);
      printf("opened doc\n");
    }
    fz_catch(ctx)
    {
      /* pdf_close_document(doc); */
      fz_rethrow_message(ctx, "cannot open document");
      exit(0);

    }

    /* decode and output */

    printf("counting the pdf pages\n");

    
   
    pagecount = tiff_count_pages(doc);
    printf("pagecount = %d, current_page = %d\n",pagecount,current_page);
    if ((current_page > pagecount) || (current_page <1))
       {
		printf("invalid page number\n");
		/* shutdown and give up */
		tiff_close_document(doc);
		
        return GF_BAD_PARAM; /* choose correct return value */
       }

    printf("heading to draw page\n");

    drawpage_result = drawpage(ctx, doc, current_page, &outsize);

    outwidth = pix->w;
    outheight = totalheight;

  

    outBuf= (unsigned char *) malloc(pix->w*totalheight*4); // now ouputting RGBA
    /* This is indeed crazy, but lets us keep mupdf's mallocs and reallocs */
	  /* in place, rather than rewriting */
	  int i;
	  unsigned char *tempout_ptr;
	  unsigned char *tempin_ptr;
	  tempout_ptr = outBuf;  /* to test, pass in unsigned char **outbuf and put *outbuf here*/
	  tempin_ptr = pix->samples;
	  /* hardcode 3 byte output for RGB format  */
	  for (i=0; i<  pix->w * totalheight; ++i)
	    {
	      /* write RGB vals */
	      *tempout_ptr = *tempin_ptr; /* R */
	      ++tempout_ptr; ++tempin_ptr;
	      *tempout_ptr = *tempin_ptr; /*G */
	      ++tempout_ptr; ++tempin_ptr;
	      *tempout_ptr = *tempin_ptr; /*B */
	      ++tempout_ptr; ++tempin_ptr;
		  *tempout_ptr = *tempin_ptr; /*A */   // now added alpha channel 
	      ++tempout_ptr; ++tempin_ptr;

	    }



    /* closing document and dropping pixmap */
    tiff_close_document(doc);
    //fz_close_document(doc);
    fz_drop_pixmap(ctx, pix); 
    doc = NULL;
  }
  fz_catch(ctx)
  {
    if (!ignore_errors)
      fz_rethrow(ctx);
    tiff_close_document(doc);
    //fz_close_document(doc);
    doc = NULL;
    fz_drop_pixmap(ctx, pix); 
    fz_warn(ctx, "ignoring error in input");
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

