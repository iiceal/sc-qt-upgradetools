#include "hg_aux.h"
#include <QTime>
#include <QCoreApplication>

#define cpu_to_le32(x)		(x)
#define tole(x) cpu_to_le32(x)
static const unsigned int crc32table_le[] = {
tole(0x00000000L), tole(0x1db71064L), tole(0x3b6e20c8L), tole(0x26d930acL),
tole(0x76dc4190L), tole(0x6b6b51f4L), tole(0x4db26158L), tole(0x5005713cL),
tole(0xedb88320L), tole(0xf00f9344L), tole(0xd6d6a3e8L), tole(0xcb61b38cL),
tole(0x9b64c2b0L), tole(0x86d3d2d4L), tole(0xa00ae278L), tole(0xbdbdf21cL)
};

void hg_msleep(int msec)
{
    QTime dieTime = QTime::currentTime().addMSecs(msec);
    while( QTime::currentTime() < dieTime )
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
}

static unsigned int crc32_no_comp(unsigned int crc, unsigned char const *p, int len)
{
    while (len--) {
        crc ^= *p++;
        crc = (crc >> 4) ^ crc32table_le[crc & 15];
        crc = (crc >> 4) ^ crc32table_le[crc & 15];
    }
    return crc;
}

//static unsigned int crc32 (unsigned int crc, const unsigned char *p, unsigned int len)
//{
//        return crc32_no_comp(crc ^ 0xffffffffL, p, len) ^ 0xffffffffL;
//}

u16 crc16(u8 *buf, int len)
{
    int i;
    u16 cksum;
    u8 s, t;
    u32 r;

    cksum = 0;
    for (i = 0;  i < len;  i++) {
        s = *buf++ ^ (cksum >> 8);
        t = s ^ (s >> 4);
        r = (cksum << 8) ^
            t       ^
            (t << 5) ^
            (t << 12);
        cksum = r;
    }
    return cksum;
}

unsigned int CRC_32(unsigned int crc, unsigned char * buff, int len)
{
    return crc32_no_comp(crc ^ 0xffffffffL, buff, len) ^ 0xffffffffL;
}

