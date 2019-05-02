
#ifndef J2K_DEC_H
#define J2K_DEC_H

#ifdef __cplusplus
extern "C" {
#endif


/*!
 * \brief Pixel Formats
 *
 *	Supported pixel formats for everything using video
*/
#ifndef GF_4CC
#define GF_4CC(a,b,c,d) (((a)<<24)|((b)<<16)|((c)<<8)|(d))
#endif
typedef enum
{
	/*!8 bit GREY */
	GF_PIXEL_GREYSCALE	=	GF_4CC('G','R','E','Y'),
	/*!16 bit greyscale*/
	GF_PIXEL_ALPHAGREY	=	GF_4CC('G','R','A','L'),
	/*!12 bit RGB on 16 bits (4096 colors)*/
	GF_PIXEL_RGB_444	=	GF_4CC('R','4','4','4'),
	/*!15 bit RGB*/
	GF_PIXEL_RGB_555	=	GF_4CC('R','5','5','5'),
	/*!16 bit RGB*/
	GF_PIXEL_RGB_565	=	GF_4CC('R','5','6','5'),
	/*!24 bit RGB*/
	GF_PIXEL_RGB_24		=	GF_4CC('R','G','B','3'),
	/*!24 bit BGR*/
	GF_PIXEL_BGR_24		=	GF_4CC('B','G','R','3'),
	/*!32 bit RGB. Component ordering in bytes is B-G-R-X.*/
	GF_PIXEL_RGB_32		=	GF_4CC('R','G','B','4'),
	/*!32 bit BGR. Component ordering in bytes is R-G-B-X.*/
	GF_PIXEL_BGR_32		=	GF_4CC('B','G','R','4'),

	/*!32 bit ARGB. Component ordering in bytes is B-G-R-A.*/
	GF_PIXEL_ARGB		=	GF_4CC('A','R','G','B'),
	/*!32 bit RGBA (openGL like). Component ordering in bytes is R-G-B-A.*/
	GF_PIXEL_RGBA		=	GF_4CC('R','G','B', 'A'),
	/*!RGB24 + depth plane. Component ordering in bytes is R-G-B-D.*/
	GF_PIXEL_RGBD		=	GF_4CC('R', 'G', 'B', 'D'),
	/*!RGB24 + depth plane (7 lower bits) + shape mask. Component ordering in bytes is R-G-B-(S+D).*/
	GF_PIXEL_RGBDS		=	GF_4CC('3', 'C', 'D', 'S'),
	/*!Stereo RGB24 */
	GF_PIXEL_RGBS		=	GF_4CC('R', 'G', 'B', 'S'),
	/*!Stereo RGBA. Component ordering in bytes is R-G-B-A. */
	GF_PIXEL_RGBAS		=	GF_4CC('R', 'G', 'A', 'S'),

	/*internal format for OpenGL using pachek RGB 24 bit plus planar depth plane at the end of the image*/
	GF_PIXEL_RGB_24_DEPTH = GF_4CC('R', 'G', 'B', 'd'),

	/*!YUV packed format*/
	GF_PIXEL_YUY2		=	GF_4CC('Y','U','Y','2'),
	/*!YUV packed format*/
	GF_PIXEL_YVYU		=	GF_4CC('Y','V','Y','U'),
	/*!YUV packed format*/
	GF_PIXEL_UYVY		=	GF_4CC('U','Y','V','Y'),
	/*!YUV packed format*/
	GF_PIXEL_VYUY		=	GF_4CC('V','Y','U','Y'),
	/*!YUV packed format*/
	GF_PIXEL_Y422		=	GF_4CC('Y','4','2','2'),
	/*!YUV packed format*/
	GF_PIXEL_UYNV		=	GF_4CC('U','Y','N','V'),
	/*!YUV packed format*/
	GF_PIXEL_YUNV		=	GF_4CC('Y','U','N','V'),
	/*!YUV packed format*/
	GF_PIXEL_V422		=	GF_4CC('V','4','2','2'),

	/*!YUV planar format*/
	GF_PIXEL_YV12		=	GF_4CC('Y','V','1','2'),
	/*!YUV planar format*/
	GF_PIXEL_IYUV		=	GF_4CC('I','Y','U','V'),
	/*!YUV planar format*/
	GF_PIXEL_I420		=	GF_4CC('I','4','2','0'),
	/*!YUV planar format*/
	GF_PIXEL_I444		=	GF_4CC('I','4','4','4'),
	/*!YUV planar format*/
	GF_PIXEL_NV21		=	GF_4CC('N','V','2','1'),

	/*!YV12 + Alpha plane*/
	GF_PIXEL_YUVA		=	GF_4CC('Y', 'U', 'V', 'A'),

	/*!YV12 + Depth plane*/
	GF_PIXEL_YUVD		=	GF_4CC('Y', 'U', 'V', 'D'),

	/*!YUV planar format in 10 bits mode, all components are stored as shorts*/
	GF_PIXEL_YV12_10		=	GF_4CC('P','0','1','0')
} GF_PixelFormat;




extern int imgHeight;
extern int  imgWidth;
extern unsigned char* outbuf;

extern int decodeJ2K(char *inbuf, int insize);

// for debugging
extern int debugVal;
extern char debugValChar;
extern char debugString[50];

#ifdef __cplusplus
}
#endif


#endif
