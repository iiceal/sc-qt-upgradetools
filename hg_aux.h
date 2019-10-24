#ifndef HG_AUX_H
#define HG_AUX_H
#include <qglobal.h>

//#define TestiOutputText HGTestiOutputTest
#define	FLASH_ADDR_LIMITED_SIZE			(16*1024*1024)

typedef unsigned char		u8;
typedef unsigned short      u16;
typedef unsigned int		u32;

#define BL2FW_HDR_SIZE  512
#define BL_MAGIC    0x64616f6c      /*load*/
#define FW_MAGIC    0x77667766      /*fwfw*/

#define BL2FW_FLAGS_BL          (1)
#define BL2FW_FLAGS_BB_FW       (2)
#define BL2FW_FLAGS_AP_FW       (3)
#define BL2FW_FLAGS_AP_DES_EN       (1 << 3)
#define BL2FW_FLAGS_AP_FW_LINUX     (1 << 4)
#define BL2FW_FLAGS_SE_DIS          (1 << 7)

#define BL2FW_FLAGS_TYPE_INDEX(n)      ((n & 0x7) << 0)

//void HGTestiOutputTest(const char *fmt, ...);
u16 crc16(u8 *buf, int len);
unsigned int CRC_32(unsigned int crc, unsigned char * buff, int len);
void hg_msleep(int msec);

#endif // HG_AUX_H
