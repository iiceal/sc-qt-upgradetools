#ifndef HG_FF_HDR_H
#define HG_FF_HDR_H
#include <qglobal.h>

class hg_ff_hdr
{
public:
    hg_ff_hdr();

    /*hdr size 256 bytes*/
typedef struct ff_hdr{

        qint32 	magic;
        qint32 	version;

        qint32 	flash_addr;
        qint32 	size;
        qint32 	load_addr;
        qint32 	exec_addr;

        qint16  	flags;
        qint16  	crc16;
        qint32  reserved[57];
}FF_HDR_T;

};

#endif // HG_FF_HDR_H
