#include "hg_upgradefile.h"
#include <stdio.h>
#include <QDebug>
#include <QString>
#include <QObject>

static u8 mbr_dbuf[MBR_SIZE];

u8 * HG_UpgradeFile::get_flash_mbr_dbuf(void)
{
    return mbr_dbuf;
}

HG_UpgradeFile::HG_UpgradeFile()
{
    upgradeAux = new HG_UpgradeFunc();
}

HG_UpgradeFile::~HG_UpgradeFile()
{
    delete upgradeAux;
}

void HG_UpgradeFile::currentUpgradeProcess(qint32 mode, qint32 file_index)
{
    current_mode = mode;
    current_file = file_index;
}

void HG_UpgradeFile::getCurrentUpgradeProgocess(qint32 *mode, qint32 *file_index)
{
    *mode = current_mode;
    *file_index = current_file;
}

void HG_UpgradeFile::doCtrlTracePrint(const QString &s)
{
    upgradeAux->ctrlTracePrint(s);
}

void HG_UpgradeFile::serial_config_update(QString name, QString bondrate)
{
    upgradeAux->serial_config(name,bondrate);
}

/*fl_addr is only offset address*/
static bool mbr_addr_valid(struct fls_fmt_data * fls)
{
    if(fls->fl_addr >= FLASH_ADDR_LIMITED_SIZE)
        return false;

    if((fls->fl_addr + fls->fl_size) >= FLASH_ADDR_LIMITED_SIZE)
        return false;

    return true;
}

u32 HG_UpgradeFile::mbr_entry_dump(u8*  mbr_entry)
{
    u32 i;
    struct fls_fmt_data *p_mbr = (fls_fmt_data *)mbr_entry;

    qDebug("%-19s%-12s%-12s%-10s\n","Name","Saddr","Eaddr","Length");
    doCtrlTracePrint(tr(""));
    for(i = 0; i < 20 ;i++)
    {
        if(mbr_addr_valid(p_mbr))
            qDebug("[%d]%-16s0x%08x  0x%08x  0x%08x\n",
                    i+1,
                    p_mbr->name,
                    p_mbr->fl_addr,
                    p_mbr->fl_addr + p_mbr->fl_size,
                    p_mbr->fl_size);
        p_mbr ++;
    }

    return 0;
}

bool HG_UpgradeFile::is_mbr_cfg_valid(void)
{
    return HG_UpgradeFile::get_flash_mbr(MBR_NAME, NULL, NULL);
}

bool HG_UpgradeFile::get_flash_mbr(char * name, u32 * fl_addr, u32 * fl_size)
{
    struct fls_fmt_data * fls;
    int i, num_0xff, length;
    u16 crc16_v;

    if(name == NULL)
        return false;

    if(fl_addr != NULL)
        *fl_addr = 0;
    if(fl_size != NULL)
        *fl_size = 0;

    fls = (struct fls_fmt_data *)mbr_dbuf;
    length = 0;

    while(1) {
        num_0xff = 0;
        crc16_v = 0;
        i = 0;

        for(i = 0; i < 16; i++) {
            if((u8)fls->name[i] == 0xff)
                num_0xff ++;
        }

        if(num_0xff == 16)
            break;

        if(strcmp(fls->name, name) == 0) {

            if(mbr_addr_valid(fls) ==false)
                return false;

            crc16_v = crc16((u8*)fls, sizeof(struct fls_fmt_data) - 2);

            if((fls->flags & FLS_FLAGS_VALID) && (crc16_v == fls->crc16)) {
                if(fl_addr != NULL)
                    *fl_addr = fls->fl_addr;
                if(fl_size != NULL)
                    *fl_size = fls->fl_size;
                qDebug("\r\nmbr %s check ok\r\n", fls->name);
                return true;
            }
            return false;
        }
        fls++;
        length += sizeof(struct fls_fmt_data);
        if(length >= MBR_SIZE)
            break;
    }

    return false;
}

void HG_UpgradeFile::doLoadUpgrade()
{
//    qDebug("Bink Debug  upgradeByteArray lenth = %d",upgradeByteArray.length());
    if(upgradeByteArray.isEmpty())
        return;

    doCtrlTracePrint("");
    currentUpgradeProcess(ProgramFileMode,ProgramFile);
    downloadProgramFile(upgradeByteArray.data(),upgradeByteArray.length(),ProgramFileLoadAddr);
    return;
}

void HG_UpgradeFile::doLoadFlashFile(HG_UpgradeFile::BootMode mode)
{
    uint offset = 0;
    uint i;

    uint loadAddr;
    uint flashAddr;
    uint flashSize;

    switch(mode)
    {
    case FastBootMode:
        qDebug("FastBootMode Download Flash File Progress\n");
        if(blByteArray.isEmpty())
        {
            qDebug("BL file is empy.\n");
        }else{
            currentUpgradeProcess(mode,BL_File);
            downLoadFlashFile(blByteArray.data(),blByteArray.size(),
                                FlashFileLoadAddr,SPIFLASH_D_BASE + offset);
        }
        break;
    case SecureEncryptionMode:

        qDebug("SecureEncryptionMode Download Flash File Progress\n");
        //downloard mbr flash file
        if(mbrByteArray.isEmpty())
        {
            qDebug("mbr file is empy.\n");
        }
//        else if(is_mbr_cfg_valid())
//        {
//            qDebug("mbr file size is not corrected.\n");
//            qDebug("max mbr file size:%d\n",MBR_SIZE);
//            qDebug("current mbr file size:%d\n\n",mbrByteArray.size());
//        }

        memcpy(mbr_dbuf, (unsigned char*)mbrByteArray.data(),MBR_SIZE);
        mbr_entry_dump(mbr_dbuf);
/*****************************************************************************************************************/
        if(false == get_flash_mbr(MBR_NAME,&flashAddr,&flashSize))
        {
            qDebug("bl file can not find mbr config info.\n");
        }else if(0 == flashSize)
        {
            qDebug("error. mbr file contain mbr flash size:0\n");
        }else{
            currentUpgradeProcess(mode,MBR_File);
            downLoadFlashFile((char *)mbr_dbuf,MBR_SIZE,FlashFileLoadAddr,SPIFLASH_D_BASE + flashAddr);
            qDebug("**************download mbr done,size %d bytes*************** \n",mbrByteArray.size());
        }
/*****************************************************************************************************************/


/*****************************************************************************************************************/
        //download authcode flash file
        if(authcodeByteArray.isEmpty())
        {
            qDebug("authcode file is empy.\n");
        }else if(false == get_flash_mbr(AUTHCODE_NAME,&flashAddr,&flashSize))
        {
            qDebug("mbr file can not find authcode config info.\n");
        }else if(0 == flashSize)
        {
            qDebug("error.mbr file contain authcode flash size:0\n");
        }else{
            currentUpgradeProcess(mode,AuthcodeFile);
            downLoadFlashFile(authcodeByteArray.data(),authcodeByteArray.size(),FlashFileLoadAddr,SPIFLASH_D_BASE + flashAddr);
            qDebug("**************download authcode done,size %d bytes*************** \n",authcodeByteArray.size());
        }

/*****************************************************************************************************************/


/*****************************************************************************************************************/
        //download bl flash file
        if(blByteArray.isEmpty())
        {
            qDebug("bootloader file is empy.\n");
        }else if(false == get_flash_mbr(BL_NAME,&flashAddr,&flashSize))
        {
            qDebug("bootloader file can not find mbr config info.\n");
        }else if(0 == flashSize)
        {
            qDebug("error.mbr file contain bl flash size:0\n");
        }else{
            currentUpgradeProcess(mode,BL_File);
            downLoadFlashFile(blByteArray.data(),blByteArray.size(),FlashFileLoadAddr,SPIFLASH_D_BASE + flashAddr);
            qDebug("**************download bl done,size %d bytes*************** \n",blByteArray.size());
        }

        if(true == get_flash_mbr(BL_BACKUP_NAME,&flashAddr,&flashSize))
        {
            if(0 == flashSize)
            {
                qDebug("error.mbr file contain bl backup flash size:0\n");
            }else{
                currentUpgradeProcess(mode,BL_BK_File);
                downLoadFlashFile(blByteArray.data(),blByteArray.size(),FlashFileLoadAddr,SPIFLASH_D_BASE + flashAddr);
                qDebug("**************download bl bk done,size %d bytes*************** \n",blByteArray.size());
            }
        }
/*****************************************************************************************************************/

/*****************************************************************************************************************/
        //download bb flash file
        if(bbByteArray.isEmpty())
        {
            qDebug("bb file is empy.\n");
        }else if(false == get_flash_mbr(BB_FW_NAME,&flashAddr,&flashSize))
        {
            qDebug("bb file can not find mbr config info.\n");

        }else if(0 == flashSize)
        {
            qDebug("error.bb file contain bb flash size:0\n");
        }else{
            currentUpgradeProcess(mode,BB_File);
            downLoadFlashFile(bbByteArray.data(),bbByteArray.size(),FlashFileLoadAddr,SPIFLASH_D_BASE + flashAddr);
            qDebug("**************download bb done,size %d bytes*************** \n",bbByteArray.size());
        }

        if(true == get_flash_mbr(BB_FW_BACKUP_NAME,&flashAddr,&flashSize))
        {
            if(0 == flashSize)
            {
                qDebug("error.mbr file contain bb bk flash size:0\n");
            }else{
                currentUpgradeProcess(mode,BB_BK_File);
                downLoadFlashFile(bbByteArray.data(),bbByteArray.size(),FlashFileLoadAddr,SPIFLASH_D_BASE + flashAddr);
                qDebug("**************download bb bk done,size %d bytes*************** \n",bbByteArray.size());
            }
        }
/*****************************************************************************************************************/


/*****************************************************************************************************************/
//        //download ab flash file
//        if(apByteArray.isEmpty())
//        {
//            qDebug("ap file is empy.\n");
//        }else if(false == get_flash_mbr(AP_FW_NAME,&flashAddr,&flashSize))
//        {
//            qDebug("mbr file can not find ap config info.\n");
//        }else if(0 == flashSize)
//        {
//            qDebug("error.bb file contain ap flash size:0\n");
//        }else{
//            currentUpgradeProcess(mode,AP_File);
//            downLoadFlashFile(bbByteArray.data(),bbByteArray.size(),FlashFileLoadAddr,SPIFLASH_D_BASE + flashAddr);
//            qDebug("**************download ap done,size %d bytes*************** \n",bbByteArray.size());
//        }

//        if(true == get_flash_mbr(AP_FW_BACKUP_NAME,&flashAddr,&flashSize))
//        {
//            if(0 == flashSize)
//            {
//                qDebug("error.mbr file contain ap bk flash size:0\n");
//                return;
//            }else{
//                currentUpgradeProcess(mode,AP_BK_File);
//                downLoadFlashFile(bbByteArray.data(),bbByteArray.size(),FlashFileLoadAddr,SPIFLASH_D_BASE + flashAddr);
//                qDebug("**************download bl bk done.size %d bytes*************** \n",bbByteArray.size());
//            }
//        }

/*****************************************************************************************************************/
        break;

    case CompatibilityMode:
        qDebug("CompatibilityMode Download Flash File Progress\n\n");
        if(mbrByteArray.isEmpty())
        {
            qDebug("mbr file is empy.\n");

        }else if(is_mbr_cfg_valid())
        {
            qDebug("mbr file size is not corrected.\n");
            qDebug("max mbr file size:%d\n",MBR_SIZE);
            qDebug("current mbr file size:%d\n\n",mbrByteArray.size());
            memcpy(mbr_dbuf, (unsigned char*)mbrByteArray.data(),MBR_SIZE);
            mbr_entry_dump(mbr_dbuf);
        }


/*****************************************************************************************************************/
        if(false == get_flash_mbr(MBR_NAME,&flashAddr,&flashSize))
        {
            qDebug("bl file can not find mbr config info.\n");

        }else if(0 == flashSize)
        {
            qDebug("error.mbr file contain mbr flash size:0\n");

        }else{
            currentUpgradeProcess(mode,MBR_File);
            downLoadFlashFile(mbrByteArray.data(),mbrByteArray.size(),FlashFileLoadAddr,SPIFLASH_D_BASE + flashAddr);
            qDebug("**************download mbr done.size %d bytes*************** \n",mbrByteArray.size());
        }

/*****************************************************************************************************************/

/*****************************************************************************************************************/
        //download bl flash file
        if(blByteArray.isEmpty())
        {
            qDebug("bootloader file is empy.\n");
        }else if(false == get_flash_mbr(BL_NAME,&flashAddr,&flashSize))
        {
            qDebug("bootloader file can not find mbr config info.\n");

        }else if(0 == flashSize)
        {
            qDebug("error.mbr file contain bl flash size:0\n");
        }else{
            currentUpgradeProcess(mode,BL_File);
            downLoadFlashFile(blByteArray.data(),blByteArray.size(),FlashFileLoadAddr,SPIFLASH_D_BASE + flashAddr);
            qDebug("**************download bl done.size %d bytes*************** \n",blByteArray.size());
        }

        if(true == get_flash_mbr(BL_BACKUP_NAME,&flashAddr,&flashSize))
        {
            if(0 == flashSize)
            {
                qDebug("error.mbr file contain bl backup flash size:0\n");
            }else{
                currentUpgradeProcess(mode,BL_BK_File);
                downLoadFlashFile(blByteArray.data(),blByteArray.size(),FlashFileLoadAddr,SPIFLASH_D_BASE + flashAddr);
                qDebug("**************download bl bk done,size %d bytes*************** \n",blByteArray.size());
            }
        }
/*****************************************************************************************************************/

/*****************************************************************************************************************/
        //download bb flash file
        if(bbByteArray.isEmpty())
        {
            qDebug("bb file is empy.\n");
        }else if(false == get_flash_mbr(BB_FW_NAME,&flashAddr,&flashSize))
        {
            qDebug("bb file can not find mbr config info.\n");
        }else if(0 == flashSize)
        {
            qDebug("error.bb file contain bb flash size:0\n");
        }else{
            currentUpgradeProcess(mode,BB_File);
            downLoadFlashFile(bbByteArray.data(),bbByteArray.size(),FlashFileLoadAddr,SPIFLASH_D_BASE + flashAddr);
            qDebug("**************download bb done.size %d bytes*************** \n",bbByteArray.size());
        }

        if(true == get_flash_mbr(BB_FW_BACKUP_NAME,&flashAddr,&flashSize))
        {
            if(0 == flashSize)
            {
                qDebug("error.mbr file contain bb bk flash size:0\n");
            }else{
                currentUpgradeProcess(mode,BB_BK_File);
                downLoadFlashFile(bbByteArray.data(),bbByteArray.size(),FlashFileLoadAddr,SPIFLASH_D_BASE + flashAddr);
                qDebug("**************download bb bk done,size %d bytes*************** \n",bbByteArray.size());
            }

        }
/*****************************************************************************************************************/

/*****************************************************************************************************************/
        //download ap flash file
        if(apByteArray.isEmpty())
        {
            qDebug("ap file is empy.\n");
        }else if(false == get_flash_mbr(AP_FW_NAME,&flashAddr,&flashSize))
        {
            qDebug("mbr file can not find ap config info.\n");
        }else if(0 == flashSize)
        {
            qDebug("error.bb file contain ap flash size:0\n");
        }else{
            currentUpgradeProcess(mode,AP_File);
            downLoadFlashFile(bbByteArray.data(),bbByteArray.size(),FlashFileLoadAddr,SPIFLASH_D_BASE + flashAddr);
            qDebug("**************download ap done,size %d bytes*************** \n",bbByteArray.size());
        }

        if(true == get_flash_mbr(AP_FW_BACKUP_NAME,&flashAddr,&flashSize))
        {
            if(0 == flashSize)
            {
                qDebug("error.mbr file contain ap bk flash size:0\n");
            }else{
                currentUpgradeProcess(mode,AP_BK_File);
                downLoadFlashFile(bbByteArray.data(),bbByteArray.size(),FlashFileLoadAddr,SPIFLASH_D_BASE + flashAddr);
                qDebug("**************download ap bk done,size %d bytes*************** \n",bbByteArray.size());
            }

        }

/*****************************************************************************************************************/
        break;
    default:
        break;

    }
    return;
}

bool HG_UpgradeFile::downloadProgramFile(char * data, int len, unsigned int dwLoadAddr)
{
    
    return upgradeAux->downProgramFile(data, len, dwLoadAddr, dwLoadAddr);

}

bool HG_UpgradeFile::downLoadFlashFile(char * data,  int len,  u32 dwLoadAddr, u32 dwFlashAddr)
{

    return upgradeAux->downLoadFlashFile(data,len,dwLoadAddr,dwFlashAddr);
}


