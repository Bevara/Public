

#ifndef JBIG2_DEC_H
#define JBIG2_DEC_H

#ifdef __cplusplus
extern "C" {
#endif


// for debugging
extern int debugVal;
extern char debugValChar;
extern char debugString[50];

//global params
extern int current_page;
extern int total_pages;
extern int outwidth;
extern int outheight;
extern unsigned char* outBuf;

extern int jbig2_setup();

extern void jbig2_shutdown();

extern int parseJBIG2(unsigned char *inbuf, int insize);


#ifdef __cplusplus
}
#endif


#endif
