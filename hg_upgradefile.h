#ifndef HG_UPGRADEFILE_H
#define HG_UPGRADEFILE_H

#include <QObject>
#include <QWidget>

#include "hg_aux.h"
#include "hg_upgradefunc.h"

#define     MBR_SIZE            4096
/*name*/
#define		MBR_NAME            "mbr(self)"
#define		AUTHCODE_NAME		"auth.code"
#define		BL_NAME             "ramloader"
#define		BL_BACKUP_NAME		"ramloader-bk"
#define		BB_FW_NAME          "bb.firmware"
#define		BB_FW_BACKUP_NAME	"bb.firmware-bk"
#define		AP_FW_NAME          "ap.firmware"
#define		AP_FW_BACKUP_NAME	"ap.firmware-bk"

struct fls_fmt_data {
    char 	name[16];
    u32 	fl_addr;
    u32 	fl_size;
    u32 	resvered;
    u16  	flags;
    u16  	crc16;
};


#define SPIFLASH_D_BASE     (0x10000000)
#define ProgramFileLoadAddr  (0x100000)
#define FlashFileLoadAddr    (0x140000) //fw sram buff addr

/*flags*/
#define		FLS_FLAGS_VALID		(1 << 0)

class HG_UpgradeFile : public QObject
{
    Q_OBJECT

public:
    HG_UpgradeFile();
    ~HG_UpgradeFile();

    HG_UpgradeFunc *upgradeAux;

    QString mbr_filename;
    QString authcode_filename;
    QString bl_filename;
    QString bb_filename;
    QString ap_filename;

    QByteArray upgradeByteArray;
    
    QByteArray mbrByteArray;
    QByteArray authcodeByteArray;
    QByteArray blByteArray;
    QByteArray bbByteArray;
    QByteArray apByteArray;

    enum BootMode{
        FastBootMode = 0,
        SecureEncryptionMode = 1,
        CompatibilityMode = 3,

        ProgramFileMode = 11
    };

    enum FileIndex{
        ProgramFile     = 0,
        MBR_File        = 1,
        AuthcodeFile    = 2,
        BL_File         = 3,
        BL_BK_File      = 4,
        BB_File         = 5,
        BB_BK_File      = 6,
        AP_File         = 7,
        AP_BK_File      = 8
    };

    volatile qint32 current_mode;
    volatile qint32 current_file;

    void currentUpgradeProcess(qint32 mode, qint32 file_index);
    void getCurrentUpgradeProgocess(qint32 *mode, qint32 *file_index);

    void serial_config_update(QString name,QString bondrate);

    u32 mbr_entry_dump(u8*  mbr_entry);

    u8 * get_flash_mbr_dbuf(void);
    bool is_mbr_cfg_valid(void);
    bool get_flash_mbr(char * name, u32 * fl_addr, u32 * fl_size); 
    
    bool is_bl2fw_hdr_valid(u32 op_type);  
    bool get_bl2fw_hdr_aes_enkey(u8 * key, int len);
    
    u32 get_bl2fw_hdr_version(void); 
    bool is_bl2fw_hdr_apkey_enabled(void);
    u32 get_bl2fw_hdr_fls_addr(void);
    u32 get_bl2fw_hdr_fls_size(void);
    u32 get_bl2fw_hdr_load_addr(void);
    u8 * get_bl2fw_hdr_dbuf(void);
    u32 get_bl2fw_hdr_flags(void);
    void print_bl2fw_hdr_info(void);
    bool is_bl2fw_data_valid(u32 chipid, u8 * input_data, int dsize);

    void doLoadUpgrade();
    void doLoadFlashFile(BootMode mode);
    
    bool downloadProgramFile(char * data,  int len, unsigned int dwLoadAddr);
    bool downLoadFlashFile(char * data,  int len,  u32 dwLoadAddr, u32 dwFlashAddr);

    void doCtrlTracePrint(const QString &s);

signals:
    void doCtrFlowTrace(const QString &s);

};

#endif // HG_UPGRADEFILE_H
