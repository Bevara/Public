
#ifndef PGM_DEC_H
#define PGM_DEC_H

#ifdef __cplusplus
extern "C" {
#endif


extern int imgHeight;
extern int  imgWidth;
extern unsigned char* outbuf;

extern int decodePGM(char *inbuf, int insize);


#ifdef __cplusplus
}
#endif


#endif
