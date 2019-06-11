
#ifndef __CDRSUPPORT_H__
#define __CDRSUPPORT_H__


// set this to zero in the setup function to default it to BEV_S
//int supportCode;

//success/failure codes; this goes into lower byte of output
enum{
BEV_NS, // format not supported
BEV_GNS, // this generation of the format not supported
BEV_FNS, // one or  more feature is not supported
BEV_S //format fully supported;keep this as last in the list
};

// format-specific feature support for CDR case
// at the moment, these go into upper byte/bytes of output

#define BEV_CDR_NO_RGB_TEXT  0x01; // text
#define BEV_CDR_NO_RGB_LAYER 0x02;  // layers
#define BEV_CDR_NO_EBG 0x04; // embedded graphics
#define BEV_CDR_NO_CONN 0x08; // connector
#define BEV_CDR_NO_RGB_GRAPHIC 0x10; //no graphic object
#define BEV_CDR_NO_RGB_TABLE 0x20; //no table 



#endif /* __CDRSUPPORT_H__ */