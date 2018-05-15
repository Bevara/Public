/* BEVARA
** 
** Adapted from the qdbmp code; see http://qdbmp.sourceforge.net/
** 
**
*/


#include "bmp.h"
#include <stdlib.h>
#include <string.h>



/* Holds the last error code */
static BMP_STATUS BMP_LAST_ERROR_CODE;





/*********************************** Forward declarations **********************************/
int		ReadHeader	( const char* bmp_data, const int size );
int		ReadUINT	(  UINT* x, const char* bmp_data,  const int size );
int		ReadUSHORT	(  USHORT *x, const char* bmp_data, const int size );



/* our BMP */
struct BMP_struct * bmp;

long dataInd;

// BEVARA: this is for debugging using html interface
// int debugVal;
// char debugValChar;
// char debugString[50];


char *mystrcpy (char *s1, const char *s2)
{
  char *s=s1;
  
  for (s=s1; (*s++ = *s2++) != '\0'; )  ;
  return(s1);
}



/*********************************** Public methods **********************************/



/**************************************************************
	Reads the specified BMP image file.
**************************************************************/
int EMSCRIPTEN_KEEPALIVE BMP_Decode( const char* bmp_data, const int size)
{

  BMP_LAST_ERROR_CODE=BMP_OK;

	/* Allocate */
  bmp = (struct BMP_struct*) malloc( sizeof( struct BMP_struct ) );
	if ( bmp == NULL )
	{
		BMP_LAST_ERROR_CODE = BMP_OUT_OF_MEMORY;
		return BMP_LAST_ERROR_CODE;
	}
	
	dataInd = 0;
	/* Read header */
	if ( ReadHeader( bmp_data, size) != BMP_OK || bmp->Header.Magic != 0x4D42 )
	{
		BMP_LAST_ERROR_CODE = BMP_FILE_INVALID;
		free( bmp );
		return BMP_LAST_ERROR_CODE;
	}



	
	/* Verify that the bitmap variant is supported */
	if ( ( bmp->Header.BitsPerPixel != 32 && bmp->Header.BitsPerPixel != 24 && bmp->Header.BitsPerPixel != 8 )
		|| bmp->Header.CompressionType != 0 || bmp->Header.HeaderSize != 40 )
	{
		BMP_LAST_ERROR_CODE = BMP_FILE_NOT_SUPPORTED;
		free( bmp );
		return BMP_LAST_ERROR_CODE;
	}


	/* Allocate and read palette */
	if ( bmp->Header.BitsPerPixel == 8 )
	{
		bmp->Palette = (UCHAR*) malloc( BMP_PALETTE_SIZE * sizeof( UCHAR ) );
		if ( bmp->Palette == NULL )
		{
			BMP_LAST_ERROR_CODE = BMP_OUT_OF_MEMORY;
			free( bmp );
			return BMP_LAST_ERROR_CODE;
		}

		if ( dataInd+ BMP_PALETTE_SIZE > size )
		{
			BMP_LAST_ERROR_CODE = BMP_FILE_INVALID;
			free( bmp->Palette );
			free( bmp );
			return BMP_LAST_ERROR_CODE;
		}
		else
		  {
		    memcpy(bmp->Palette,bmp_data+dataInd, BMP_PALETTE_SIZE);
		    dataInd += BMP_PALETTE_SIZE;
		  }
	}
	else	/* Not an indexed image */
	{
		bmp->Palette = NULL;
	}


	/* Allocate memory for image data */
	/*bmp->Data = (UCHAR*) malloc( bmp->Header.ImageDataSize );*/
	bmp->Data = (UCHAR*) malloc( bmp->Header.Width* bmp->Header.Height * 4); /* forcing RGBA output*/
	if ( bmp->Data == NULL )
	{
		BMP_LAST_ERROR_CODE = BMP_OUT_OF_MEMORY;
		free( bmp->Palette );
		free( bmp );
		return BMP_LAST_ERROR_CODE;
	}


	/* Read image data */
	if (dataInd + bmp->Header.ImageDataSize > size )
	{
		BMP_LAST_ERROR_CODE = BMP_FILE_INVALID;
		free( bmp->Data );
		free( bmp->Palette );
		free( bmp );
		return BMP_LAST_ERROR_CODE;
	}
	//TODO: do we need to flip from  BGR to RGB?
	// what to do with palette?
        else
	  {
		  if ( bmp->Header.BitsPerPixel == 32)
			   memcpy( bmp->Data, bmp_data+dataInd, bmp->Header.Width* bmp->Header.Height * 4);			  		  
		  else if ( bmp->Header.BitsPerPixel == 24)
		  {  /* we need to insert that alpha; rather than memcpy RGB chunks, let's go one-by-one in case we need to debug  */
			int i,j;
			int stride = bmp->Header.Width;
			if (bmp->Header.Width%4 !=0 )
				stride = bmp->Header.Width + (4- (bmp->Header.Width%4)); // typically 4-byte aligned
			int diff = bmp->Header.FileSize - (stride*bmp->Header.Height*3) - dataInd;
			if (diff>4 || diff<0)
				{printf("error in stride\n"); return BMP_FILE_INVALID;} // need to handcheck if not aligned

			UCHAR *tmp = bmp->Data;
			// ugh we have to debug....
			//for (i=0; i<bmp->Header.Height; ++i) // flipped vertically
			for (i=(bmp->Header.Height)-1; i>-1; --i)
				{
				 for (j = 0; j<bmp->Header.Width*3;  j=j+3)
				 	 {
					 //typically stored in BGR so switch to RGB
					 if (i<987)
					 {
					 char *ctmp;
					 ctmp = *(bmp_data+dataInd + i*stride*3 +j);
					 }
					 *(tmp) = *(bmp_data+dataInd + i*stride*3 +j+2); ++tmp;
					*(tmp) = *(bmp_data+dataInd + i*stride*3 +j+1); ++tmp;
					*(tmp) = *(bmp_data+dataInd + i*stride*3 +j); ++tmp;
					*(tmp) = 255; ++tmp;
				 	 }
				}
		  }
		  else if (bmp->Header.BitsPerPixel == 8 )
			  {  /* we need to expand to RGB and insert the alpha  */
			int i,j;
			UCHAR *tmp = bmp->Data;
			for (i=0; i<bmp->Header.Height*bmp->Header.Width; ++i)
			{
					*(tmp) = *(bmp_data+dataInd + i); ++tmp;
					*(tmp) = *(bmp_data+dataInd + i); ++tmp;
					*(tmp) = *(bmp_data+dataInd + i); ++tmp;
					*(tmp) = 255; ++tmp;				
			}	  
		  }
		  	   
	    //dataInd += bmp->Header.ImageDataSize; //for completeness 
	  }

	

	return BMP_LAST_ERROR_CODE;
}



/**************************************************************
	Returns the image's width.
**************************************************************/
int EMSCRIPTEN_KEEPALIVE BMP_GetWidth(  )
{
	if ( bmp == NULL )
	{
		BMP_LAST_ERROR_CODE = BMP_INVALID_ARGUMENT;
		return -1;
	}

	BMP_LAST_ERROR_CODE = BMP_OK;

	return ( bmp->Header.Width );
}


/**************************************************************
	Returns the image's height.
**************************************************************/
int EMSCRIPTEN_KEEPALIVE BMP_GetHeight( )
{
	if ( bmp == NULL )
	{
		BMP_LAST_ERROR_CODE = BMP_INVALID_ARGUMENT;
		return -1;
	}

	BMP_LAST_ERROR_CODE = BMP_OK;

	return ( bmp->Header.Height );
}


/**************************************************************
	Returns the image's color depth (bits per pixel).
**************************************************************/
int EMSCRIPTEN_KEEPALIVE BMP_GetDepth( )
{
	if ( bmp == NULL )
	{
		BMP_LAST_ERROR_CODE = BMP_INVALID_ARGUMENT;
		return -1;
	}

	BMP_LAST_ERROR_CODE = BMP_OK;

	return ( (int) bmp->Header.BitsPerPixel );
}





/**************************************************************
	Returns the last error code.
**************************************************************/
BMP_STATUS BMP_GetError()
{
	return BMP_LAST_ERROR_CODE;
}


/**************************************************************
	Returns a description of the last error code.
**************************************************************/
const char* BMP_GetErrorDescription()
{
	if ( BMP_LAST_ERROR_CODE > 0 && BMP_LAST_ERROR_CODE < BMP_ERROR_NUM )
	{
		return BMP_ERROR_STRING[ BMP_LAST_ERROR_CODE ];
	}
	else
	{
		return NULL;
	}
}





/*********************************** Private methods **********************************/


/**************************************************************
	Reads the BMP file's header into the data structure.
	Returns BMP_OK on success.
**************************************************************/
int	ReadHeader(const char* bmp_data, const int size )
{
	if ( bmp == NULL  )
	{
		return BMP_INVALID_ARGUMENT;
	}


	/* The header's fields are read one by one, and converted from the format's
	little endian to the system's native representation. */
	if ( !ReadUSHORT( &( bmp->Header.Magic ),bmp_data,size ) )			return BMP_IO_ERROR;
	if ( !ReadUINT( &( bmp->Header.FileSize ),bmp_data,size ) )		return BMP_IO_ERROR;
	if ( !ReadUSHORT( &( bmp->Header.Reserved1 ), bmp_data ,size) )		return BMP_IO_ERROR;
	if ( !ReadUSHORT( &( bmp->Header.Reserved2 ), bmp_data,size ) )		return BMP_IO_ERROR;
	if ( !ReadUINT( &( bmp->Header.DataOffset ), bmp_data,size ) )		return BMP_IO_ERROR;
	if ( !ReadUINT( &( bmp->Header.HeaderSize ), bmp_data ,size) )		return BMP_IO_ERROR;
	if ( !ReadUINT( &( bmp->Header.Width ), bmp_data,size ) )			return BMP_IO_ERROR;
	if ( !ReadUINT( &( bmp->Header.Height ), bmp_data ,size) )			return BMP_IO_ERROR;
	if ( !ReadUSHORT( &( bmp->Header.Planes ), bmp_data,size ) )		return BMP_IO_ERROR;
	if ( !ReadUSHORT( &( bmp->Header.BitsPerPixel ), bmp_data ,size) )	return BMP_IO_ERROR;
	if ( !ReadUINT( &( bmp->Header.CompressionType ), bmp_data ,size) )	return BMP_IO_ERROR;
	if ( !ReadUINT( &( bmp->Header.ImageDataSize ), bmp_data ,size) )	return BMP_IO_ERROR;
	if ( !ReadUINT( &( bmp->Header.HPixelsPerMeter ), bmp_data ,size) )	return BMP_IO_ERROR;
	if ( !ReadUINT( &( bmp->Header.VPixelsPerMeter ), bmp_data,size ) )	return BMP_IO_ERROR;
	if ( !ReadUINT( &( bmp->Header.ColorsUsed ), bmp_data,size ) )		return BMP_IO_ERROR;
	if ( !ReadUINT( &( bmp->Header.ColorsRequired ), bmp_data ,size) )	return BMP_IO_ERROR;

	return BMP_OK;
}




/**************************************************************
	Reads a little-endian unsigned int from the file.
	Returns non-zero on success.
**************************************************************/
int	ReadUINT( UINT* x, const char* bmp_data,  const int size)
{
	UCHAR little[ 4 ];	/* BMPs use 32 bit ints */

	if ( x == NULL || (dataInd + 4) > size )
	{
		return 0;
	}

	memcpy(&little[0],bmp_data+dataInd,4);
	dataInd = dataInd+4;

	*x = ( little[ 3 ] << 24 | little[ 2 ] << 16 | little[ 1 ] << 8 | little[ 0 ] );

	return 1;
}


/**************************************************************
	Reads a little-endian unsigned short int from the file.
	Returns non-zero on success.
**************************************************************/
int	ReadUSHORT( USHORT *x, const char* bmp_data, const int size )
{
	UCHAR little[ 2 ];	/* BMPs use 16 bit shorts */

	if ( x == NULL || (dataInd + 2) > size )
	{
		return 0;
	}

	memcpy(&little[0],bmp_data+dataInd,2);
	dataInd = dataInd+2;


	*x = ( little[ 1 ] << 8 | little[ 0 ] );

	return 1;
}

unsigned char* EMSCRIPTEN_KEEPALIVE BMP_GetImage(void) 
{ 
	return  bmp->Data;	
}


// BEVARA: use this for debugging
// int main(int argc, char* argv[]) {
    // int insize;
    // int outwidth;
    // int outheight;
    // int totalout;
    // char *inbuf;
    // unsigned char *outbuf;
    // FILE *f;
    // FILE *f2;

    // if (argc <2)
    // {   
      // printf ("You should provide the filename as the first argument");
      // return 0;
    // }
   
    // f = fopen(argv[1], "rb");
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

	
	
	// here's what to use to get a simple BMP image
	// BMP_Decode(inbuf, insize);
    // outwidth = BMP_GetWidth();
	// outheight = BMP_GetHeight();
	// outbuf = bmp->Data;

    
    // free (inbuf);
    
	// following use of outbuf call all of these:
	//if ( bmp->Data != NULL ) free( bmp->Data );
	//if ( bmp->Palette == NULL ) free( bmp->Palette );
	//free( bmp );

//}
