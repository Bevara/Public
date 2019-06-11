#include <cairo.h>
#include <stdio.h>
#include <math.h>


#include "cairoint.h"
#include "cairo-types-private.h"

#include "cairo-error-private.h"
#include "cairo-image-surface-private.h"
#include "cairo-output-stream-private.h"

int
cairo_write_surface_rgba(cairo_surface_t *surface)
{
	
	int i,j;
    cairo_status_t status;
    cairo_image_surface_t *image;
    cairo_image_surface_t * volatile clone = NULL;
    void *image_extra;
    // png_struct *png;
    // png_info *info;
    // png_byte **volatile rows = NULL;
    // png_color_16 white;
    // int png_color_type;
    // int bpc;

    status = _cairo_surface_acquire_source_image (surface,
						  &image,
						  &image_extra);
	
	

	
	/* Handle the various fallback formats (e.g. low bit-depth XServers)
     * by coercing them to a simpler format using pixman.
     */
    clone = _cairo_image_surface_coerce (image); // this should conver to ARGB32, RGB24 or just A8 channel
    status = clone->base.status;
    printf("status of clone operation = %d\n",status);



	if ((clone->format != CAIRO_FORMAT_ARGB32) && (clone->format != CAIRO_FORMAT_RGB24))
	{
		printf("we need ARGB or RGB cairo format -- exiting\n"); exit(1);
		
	}
	
	// because we're basing this on the cairo PNG output, we'll follow their format
	// and use their functions for internal conversion

	// now we have to ouput
		
	printf("printing the results to file\n");
	// do a check
	printf("imgW=%d imgH = %d, cloeW=%d, cloneH=%d\n",image->width,image->height,clone->width,clone->height);
	printf("output format RGBA?  = %d\n",clone->format == CAIRO_FORMAT_ARGB32 ? 1:0);
	printf("output format RGB?  = %d\n",clone->format == CAIRO_FORMAT_RGB24 ? 1:0);
	FILE *f2; 
	f2=fopen("TEST_cdr_output.ppm","wb");
		fprintf(f2, "P6\n%d %d\n255\n", image->width, clone->height);
	
	int RGBoutwidth = (int) ceil(image->width*0.75);
	
	char *tmpptr;
	uint32_t pixel;
    uint8_t  alpha;
	uint8_t  tmpout;

	
	if (clone->format == CAIRO_FORMAT_ARGB32) 
	{
		for (i = 0; i < clone->height; i++)
		{
			// we need to do the ARGB => RGBA conversion
			printf("row = %d:\n",i);
			
			tmpptr = (char *)  clone->data + i * clone->stride;
			for (j = 0; j < image->width; ++j)
			{
				memcpy (&pixel, tmpptr, sizeof (uint32_t));
				alpha = (pixel & 0xff000000) >> 24;
				if (alpha == 0) {
					fwrite(&alpha, 1, 1, f2);
					fwrite(&alpha, 1, 1, f2);
					fwrite(&alpha, 1, 1, f2);
					//fwrite(&alpha, 1, 1, f2);
				} else {
					tmpout = (((pixel & 0xff0000) >> 16) * 255 + alpha / 2) / alpha;
					printf("0x%02x  ",tmpout);
					fwrite(&tmpout, 1, 1, f2);
					tmpout = (((pixel & 0x00ff00) >>  8) * 255 + alpha / 2) / alpha;
					printf("0x%02x  ",tmpout);
					fwrite(&tmpout, 1, 1, f2);
					tmpout = (((pixel & 0x0000ff) >>  0) * 255 + alpha / 2) / alpha;
					printf("0x%02x  ",tmpout);
					fwrite(&tmpout, 1, 1, f2);
					tmpout = alpha;
					printf("0x%02x  \n",tmpout);
				}
				tmpptr += 4;
				// if (clone->format == CAIRO_FORMAT_ARGB32) 
				// {
					// printf("0x%02x ",*tmpptr);
						// ++tmpptr;
				// }
				// fwrite(tmpptr, 1, 1, f2); printf("0x%02x  ",*tmpptr); ++tmpptr;
				// fwrite(tmpptr, 1, 1, f2); printf("0x%02x  ",*tmpptr); ++tmpptr;
				// fwrite(tmpptr, 1, 1, f2); printf("0x%02x \n",*tmpptr); ++tmpptr;
				
				
			}
			
		}
	}
	else
	{
		// we shouldn't actually get here because we're committed to RGBA
		printf("RGB needs to handle the internal xRGB => RGBx conversion\n");
	}
			
	
	fclose(f2);
	
	  _cairo_surface_release_source_image (surface, image, image_extra);
	  
	  return 0;
}


int
main ()
{
		
			
		// this worked - draw some text 
		// cairo_surface_t *surface =
            // cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 240,80);
        // cairo_t *cr =
            // cairo_create (surface);
        // cairo_select_font_face (cr, "serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        // cairo_set_font_size (cr, 32.0);
        // cairo_set_source_rgb (cr, 0.0, 0.0, 1.0);
        // cairo_move_to (cr, 10.0, 50.0);
        // cairo_show_text (cr, "Hello, world");
		
		// this worked - draws a rectangle
		// -- we'll always go with RGBA and forestall conversion problems
		// cairo_surface_t *surface =
            // cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 120,120);
        // cairo_t *cr =
            // cairo_create (surface);
		// cairo_scale (cr, 120, 120);
		// cairo_set_line_width (cr, 0.1);
		// cairo_set_source_rgba (cr, 0.0, 0.0, 1.0, 1.0); 
		// cairo_rectangle (cr, 0.25, 0.25, 0.5, 0.5);
		// cairo_stroke (cr);
		
		
		// experiment with some path drawing
		
		// ALL OF THIS ALMOST WORKS, NEED TO EXPERIMENT WITH SUBPATH AND EVEN-ODD fill
		cairo_surface_t *surface =
            cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 648, 468);
        cairo_t *cr =
            cairo_create (surface);
		// set surface to white
		cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
		cairo_paint (cr);
		// let's get in the habit of clearing the current path
		cairo_new_path(cr);
		// this is just for testing
		//cairo_set_line_width (cr, 0.0);
		//cairo_set_source_rgba (cr, 0.0, 1.0, 1.0, 0.5);
		// end testing 
		
		// get in habit of doing new sub path, even though established by move to
		cairo_new_sub_path(cr);	
		
		cairo_move_to (cr, 486.0000, 232.8480);
		cairo_curve_to (cr, 487.2240,220.8240, 504.0000,219.0240, 509.4000,224.4240);
		cairo_line_to (cr, 512.4240,223.2720);
		cairo_line_to (cr, 516.0240,223.2720);
		cairo_line_to (cr, 516.6000,235.2240);
		cairo_line_to (cr, 513.0000,235.2240);
		cairo_curve_to (cr, 508.1760,223.8480, 494.4240,223.8480, 494.4240,231.0480);
		cairo_curve_to (cr, 494.4240,236.4480, 517.8240,232.8480, 517.8240,247.2480);
		cairo_curve_to (cr, 517.8240,262.7280, 496.2240,261.5760, 491.9760,257.4720);
		cairo_line_to (cr, 489.0240,259.2000);
		cairo_line_to (cr, 486.0000,259.2000);
		cairo_line_to (cr, 485.4240,246.0240);
		cairo_line_to (cr, 489.0240,246.0240);
		cairo_curve_to (cr, 495.0000,258.6240, 509.4000,257.4720, 509.4000,251.4240);
		cairo_curve_to (cr, 509.4000,244.2240, 484.7760,249.6240, 486.0000,232.8480);
		//cairo_close_path(cr); // this will draw to start point; dont need this since specified above
		//cairo_set_source_rgba (cr, 27/255.0, 19/255.0, 18/255.0, 1.0);
		//cairo_fill (cr); // this fills the path
		
		//cairo_stroke_preserve (cr); //actually draw the lines, but don't ditch the path in case we want to fill

		cairo_new_sub_path(cr);
		cairo_move_to (cr, 421.8480,231.6240);
cairo_curve_to (cr,420.6240,228.0240, 418.2480,227.4480, 416.4480,226.8720);
cairo_line_to (cr, 416.4480,222.6240);
cairo_line_to (cr, 437.4720,222.6240);
cairo_line_to (cr, 437.4720,226.8720);
cairo_curve_to (cr,433.2240,226.8720, 433.2240,228.6720, 433.8720,230.4720);
cairo_line_to (cr, 441.0720,247.2480);
cairo_line_to (cr, 446.4720,234.6480);
cairo_line_to (cr, 444.6720,229.8240);
cairo_curve_to (cr,444.0240,228.0240, 441.6480,226.8720, 439.8480,226.8720);
cairo_line_to (cr, 439.8480,222.6240);
cairo_line_to (cr, 461.4480,222.6240);
cairo_line_to (cr, 461.4480,226.8720);
cairo_curve_to (cr,457.2720,226.8720 ,455.4720,228.6720, 456.6240,231.0480);
cairo_line_to (cr, 463.8240,246.6720);
cairo_line_to (cr, 470.4480,232.2720);
cairo_curve_to (cr,471.6720,228.6720, 466.8480,226.8720, 464.4720,226.8720);
cairo_line_to (cr, 464.4720,222.6240);
cairo_line_to (cr, 481.8240,222.6240);
cairo_line_to (cr, 481.8240,226.8720);
cairo_curve_to (cr,477.6480,226.8720, 476.4240,231.6240, 476.4240,231.6240);
cairo_line_to (cr, 463.2480,259.7760);
cairo_line_to (cr, 457.2720,259.7760);
cairo_line_to (cr, 448.8480,240.6240);
cairo_line_to (cr, 440.4240,259.7760);
cairo_line_to (cr, 434.4480,259.7760);
cairo_line_to (cr, 421.8480,231.6240);
		//cairo_close_path(cr); // this will draw to start point; not need from prev line
		//cairo_set_source_rgba (cr, 27/255.0, 19/255.0, 18/255.0, 1.0);
		//cairo_fill (cr); // this fills the path
		
		// TBD == even-odd fill
		
		cairo_new_sub_path(cr);
cairo_move_to (cr, 400.3200,234.6480);
cairo_curve_to (cr,402.1200,224.4240, 387.7200,219.6720, 387.1440,237.6720);
cairo_line_to (cr, 397.3680,237.6720);
cairo_curve_to (cr,398.5200,237.6720, 400.3200,236.4480, 400.3200,234.6480);		
		//cairo_close_path(cr); // this will draw to start point
		//cairo_set_source_rgba (cr, 255/255.0, 255/255.0, 0/255.0, 1.0);
		//cairo_set_line_width (cr, 5.0);
		//cairo_stroke_preserve (cr); 
		// //cairo_fill (cr);
		
		cairo_new_sub_path(cr);
		cairo_move_to (cr, 373.3200,240.6240);
cairo_curve_to (cr,374.5440,213.6240 ,415.3680,216.6480 ,412.3440,241.8480);
cairo_line_to (cr, 387.1440,241.8480);
cairo_curve_to (cr,388.9440,261.5760, 404.5680,256.8240 ,408.7440,246.6720);
cairo_line_to (cr, 412.3440,248.4720);
cairo_curve_to (cr,406.9440,266.9760 ,374.5440,263.9520, 373.3200,240.6240);
		//cairo_close_path(cr); // this will draw to start point
		//cairo_set_source_rgba (cr, 27/255.0, 19/255.0, 18/255.0, 0.5);
		//cairo_fill (cr);
		
		
				cairo_new_sub_path(cr);
cairo_move_to (cr ,318.2400,212.4720);
cairo_curve_to (cr,315.8640,208.8720 ,312.2640,208.2240 ,309.8160,208.2240);
cairo_line_to (cr, 309.8160,204.0480);
cairo_line_to (cr, 329.6160,204.0480);
cairo_line_to (cr, 358.4160,238.8240);
cairo_line_to (cr, 358.4160,214.8480);
cairo_curve_to (cr,358.4160,209.4480 ,354.2400,208.8720, 350.0640,208.2240);
cairo_line_to (cr, 350.0640,204.0480);
cairo_line_to (cr, 372.2400,204.0480);
cairo_line_to (cr, 372.2400,208.2240);
cairo_curve_to (cr,369.2160,208.8720 ,363.8160,209.4480, 363.8160,214.8480);
cairo_line_to (cr, 363.8160,260.3520);
cairo_line_to (cr, 359.0640,260.3520);
cairo_line_to (cr, 323.6400,217.2240);
cairo_line_to (cr, 323.6400,247.2480);
cairo_curve_to (cr,323.6400,255.0240, 329.6160,255.0240 ,332.0640,255.0240);
cairo_line_to (cr, 332.0640,259.2000);
cairo_line_to (cr, 309.8160,259.2000);
cairo_line_to (cr, 309.8160,255.0240);
cairo_curve_to (cr,312.2640,255.0240 ,318.8160,254.4480, 318.2400,247.2480);
cairo_line_to (cr, 318.2400,212.4720	);			
				
				//cairo_close_path(cr); // this will draw to start point
				cairo_set_source_rgba (cr, 27/255.0, 19/255.0, 18/255.0, 1.0);
				cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);
		cairo_fill (cr);
		
		
// Now do first part
cairo_new_path(cr);
cairo_move_to (cr ,251.7120,232.8480);
cairo_curve_to (cr,252.2880,200.4480 ,283.5360,198.6480, 293.6880,207.0720);
cairo_line_to (cr, 297.9360,202.8240);
cairo_line_to (cr, 301.5360,202.8240);
cairo_line_to (cr, 302.6880,225.0720);
cairo_line_to (cr, 298.5120,225.6480);
cairo_curve_to (cr,294.3360,202.2480 ,267.3360,197.4240, 266.6880,228.6720);
cairo_curve_to (cr,265.5360,266.9760 ,297.2880,258.0480 ,299.0880,238.8240);
cairo_line_to (cr, 303.9120,240.0480);
cairo_curve_to (cr,299.7360,270.5760, 252.2880,266.3280 ,251.7120,232.8480);
//cairo_set_line_width (cr, 2.0160);
//cairo_set_source_rgba (cr, 27/255.0, 19/255.0, 18/255.0, 1.0);
//cairo_stroke_preserve (cr); 
//cairo_close_path(cr); // this will draw to start point
//cairo_set_source_rgba (cr, 255/255.0, 255/255.0, 255/255.0, 1.0);
//cairo_fill (cr);


cairo_new_sub_path(cr);
cairo_move_to (cr ,212.1840,233.4240);
cairo_line_to (cr, 212.1840,250.8480);
cairo_curve_to (cr,212.1840,255.0240 ,216.3600,254.4480, 219.9600,254.4480);
cairo_curve_to (cr,235.0080,254.4480, 233.2080,233.4240, 220.6080,233.4240);
cairo_line_to (cr, 212.1840,233.4240);
//cairo_set_line_width (cr, 2.0160);
//cairo_set_source_rgba (cr, 27/255.0, 19/255.0, 18/255.0, 1.0);
//cairo_stroke_preserve (cr); 
//cairo_close_path(cr); // this will draw to start point
//cairo_set_source_rgba (cr, 255/255.0, 255/255.0, 255/255.0, 1.0);
//cairo_fill (cr);

cairo_new_sub_path(cr);
cairo_move_to (cr ,212.1840,228.6720);
cairo_line_to (cr, 219.9600,228.6720);
cairo_curve_to (cr,230.7600,228.6720 ,231.9840,208.2240 ,219.9600,208.2240);
cairo_curve_to (cr,215.7840,208.2240 ,211.6080,207.0720, 212.1840,213.0480);
cairo_line_to (cr, 212.1840,228.6720);
//cairo_set_line_width (cr, 2.0160);
//cairo_set_source_rgba (cr, 27/255.0, 19/255.0, 18/255.0, 1.0);
//cairo_stroke_preserve (cr); 
//cairo_close_path(cr); // this will draw to start point
//cairo_set_source_rgba (cr, 255/255.0, 255/255.0, 255/255.0, 1.0);
//cairo_fill (cr);

cairo_new_sub_path(cr);
cairo_move_to (cr ,198.3600,248.4720);
cairo_line_to (cr, 198.3600,215.4240);
cairo_curve_to (cr,198.3600,210.0240, 198.3600,208.2240 ,190.0080,208.2240);
cairo_line_to (cr, 190.0080,204.0480);
cairo_line_to (cr, 225.3600,204.0480);
cairo_curve_to (cr,247.6080,205.8480, 248.7600,227.4480 ,228.9600,230.4720);
cairo_curve_to (cr,254.8080,235.2240, 246.3840,260.3520, 227.1600,259.2000);
cairo_line_to (cr, 190.0080,259.2000);
cairo_line_to (cr, 190.0080,255.0240);
cairo_curve_to (cr,194.7600,253.8720, 199.0080,255.6720 ,198.3600,248.4720);
//cairo_set_line_width (cr, 2.0160);
//cairo_set_source_rgba (cr, 27/255.0, 19/255.0, 18/255.0, 1.0);
//cairo_stroke_preserve (cr); 
//cairo_close_path(cr); // this will draw to start point
//cairo_set_source_rgba (cr, 255/255.0, 255/255.0, 255/255.0, 1.0);
//cairo_fill (cr);

cairo_new_sub_path(cr);
cairo_move_to (cr ,156.4560,219.6720);
cairo_line_to (cr, 148.6800,237.6720);
cairo_line_to (cr, 163.6560,237.6720);
cairo_line_to (cr, 156.4560,219.6720);
//cairo_set_line_width (cr, 2.0160);
//cairo_set_source_rgba (cr, 27/255.0, 19/255.0, 18/255.0, 1.0);
//cairo_stroke_preserve (cr); 
//cairo_close_path(cr); // this will draw to start point
//cairo_set_source_rgba (cr, 255/255.0, 255/255.0, 255/255.0, 1.0);
//cairo_fill (cr);

cairo_new_sub_path(cr);
cairo_move_to (cr ,139.6800,246.0240);
cairo_line_to (cr, 158.2560,202.8240);
cairo_line_to (cr, 163.6560,202.8240);
cairo_line_to (cr, 184.0320,249.6240);
cairo_curve_to (cr,185.2560,253.2240 ,187.6320,255.0240, 190.6560,255.0240);
cairo_line_to (cr, 190.6560,259.2000);
cairo_line_to (cr, 164.2320,259.2000);
cairo_line_to (cr, 164.2320,255.0240);
cairo_curve_to (cr,173.8800,255.6720, 169.6320,252.6480, 166.0320,243.0720);
cairo_line_to (cr, 146.8800,243.0720);
cairo_curve_to (cr,142.6320,252.0720 ,145.6560,254.4480,152.2800,255.0240);
cairo_line_to (cr, 152.2800,259.2000);
cairo_line_to (cr, 130.1760,259.2000);
cairo_line_to (cr, 130.1760,255.0240);
cairo_curve_to (cr,134.2800,254.4480 ,136.6560,250.8480 ,139.6800,246.0240);
cairo_set_line_width (cr, 2.0160);
cairo_set_source_rgba (cr, 27/255.0, 19/255.0, 18/255.0, 1.0);
cairo_stroke (cr); 
//cairo_close_path(cr); // this will draw to start point
cairo_set_source_rgba (cr, 255/255.0, 255/255.0, 255/255.0, 1.0);
cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);
cairo_fill (cr);		
		
		
		
		// output the surface and close up
		//cairo_surface_write_to_png (surface, "hello.png");
		cairo_write_surface_rgba(surface);
        cairo_destroy (cr);       
        cairo_surface_destroy (surface);
        return 0;
}