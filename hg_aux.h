#ifndef HG_AUX_H
#define HG_AUX_H

//#define TestiOutputText HGTestiOutputTest
#define	FLASH_ADDR_LIMITED_SIZE			(16*1024*1024)

typedef unsigned char		u8;
typedef unsigned short      u16;
typedef unsigned int		u32;

#define	BL2FW_TYPE_BL		(1)
#define	BL2FW_TYPE_BB_FW	(2)
#define	BL2FW_TYPE_AP_FW 	(3)

//void HGTestiOutputTest(const char *fmt, ...);
u16 crc16(u8 *buf, int len);
unsigned int CRC_32(unsigned int crc, unsigned char * buff, int len);
void hg_msleep(int msec);

#endif // HG_AUX_H
