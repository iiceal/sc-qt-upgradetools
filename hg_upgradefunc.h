#ifndef HG_UPGRADEFUNC_H
#define HG_UPGRADEFUNC_H

#include "hg_aux.h"
#include "masterthread.h"
#include <QSerialPort>

typedef enum {
    UPGRADE_FRAME_PHASE_E_FIRST = 0,
    UPGRADE_FRAME_PHASE_E_MID, // 1,
    UPGRADE_FRAME_PHASE_E_END, // 2,
}UPGRADE_FRAME_PHASE_E;

typedef enum {
    PARSE_RESULT_E_OK = 0,
    PARSE_RESULT_E_HEADER_ERR, // 1,
    PARSE_RESULT_E_LEN_ERR, // 2,
    PARSE_RESULT_E_CMD_ERR, // 3,
    PARSE_RESULT_E_CRC_ERR, // 4,
}PARSE_RESULT_E;

typedef enum {
    NACK_OK = 0,  // 正确
    NACK_ZERO_LEN = 1,  // 未收到数据包
    NACK_LESS_LEN,  // 数据包长度小于格式头
    NACK_HEADER_ERR,  // 数据包头或包尾错误
    NACK_LEN_ERR,  // 数据包长度错误
    NACK_CMD_ERR,  // 命令字错误
    NACK_CRC_ERR,  // 数据包校验错误
    NACK_STAT_ERR,  // 状态错误（在某种状态下收到不符合预期的命令字）
    NACK_START_DATA_ERR,  // 下载地址或长度错误
    NACK_EXEC_ADDR_ERR,  // 执行地址错误
    NACK_RESET  // 需要重启状态机，恢复开始状态
} NACK_TYPE;

typedef enum {
    UPGRADE_CMD_TYPE_E_START = 0,
    UPGRADE_CMD_TYPE_E_CONNECT, // 1,
    UPGRADE_CMD_TYPE_E_START_DATA, // 2,
    UPGRADE_CMD_TYPE_E_MIDST_DATA, // 3,
    UPGRADE_CMD_TYPE_E_END_DATA, // 4,
    UPGRADE_CMD_TYPE_E_EXEC_DATA, // 5,

    UPGRADE_CMD_TYPE_E_VERSION, // 6,
    UPGRADE_CMD_TYPE_E_ACK, // 7,
    UPGRADE_CMD_TYPE_E_NACK, // 8,
    UPGRADE_CMD_TYPE_E_ERR, // 9,
}UPGRADE_CMD_TYPE_E;

#define UPGRADE_PROCESS_START_FLAG      (0x7E)

#define  MAX_PACKAGE_LEN            (2048)
#define  PACKAGE_HEADER_LEN         (5)
#define  PACKAGE_TAIL_LEN           (5)
#define  MAX_BUFF                   (2048 + 2048)
#define  CONNECT_CMD_LEN            (10)

#define  TRANS_NACK_CNT_MAX         (200)

typedef enum {
	UPGRADE_STATUS_E_START = 0,
	UPGRADE_STATUS_E_GOT_VERSION = 1,
	UPGRADE_STATUS_E_CONNECTED = 2,
	UPGRADE_STATUS_E_START_DATA = 3,
	UPGRADE_STATUS_E_MIDST_DATA = 4,
	UPGRADE_STATUS_E_END_DATA = 5,
	UPGRADE_STATUS_E_EXEC_DATA = 6,
}UPGRADE_STATUS_E;


#define TX_PKG_MAX_SIZE_32K    (32768)   //32*1024Bytes

#define TX_PKG_MAX_SIZE_8K    (8192)   //8*1024 Bytes

#define TX_BLK_MAX_SIZE    (1572864)  //1.5MBytes

class HG_UpgradeFunc : public QObject
{
    Q_OBJECT

public:
    HG_UpgradeFunc();
    ~HG_UpgradeFunc();

    QString portName;
    QString PortBondrate;
    QByteArray serialBuff;
    MasterThread m_thread;
    void serial_config(QString name,QString bondrate);

    void reset_global_variable(void);
    // 构造connect数据
    int  connect_device(void);
    // 构造 start_data 数据
    int  construct_start_data(unsigned int load_addr, unsigned int len);
    // 构造数据帧
    int  construct_data_frame(UPGRADE_FRAME_PHASE_E data_phase, unsigned char * buff, unsigned int len);
    // 构造启动执行帧
    int  construct_exec_frame(unsigned int exec_addr);
    // 解析版本号
    bool  parse_version(unsigned char * buff, int len, unsigned char *ver, int *ver_len);  // 再商定入参
    PARSE_RESULT_E  parse_frame(unsigned char * buff, int len, unsigned short *ret);
    //UPGRADE_STATUS_E current_status(void);

    int  construct_frame(UPGRADE_CMD_TYPE_E cmd_type, unsigned char * buff, int len);
    unsigned char m_tmpBuf[TX_PKG_MAX_SIZE_32K + 32];
    unsigned char m_sendBuf[TX_PKG_MAX_SIZE_32K + 32];
    
    int get_progress_pos();
    bool downProgramFile(char * data,int dateLen, unsigned int dwLoadAddr, unsigned int dwFlashAddr);
    bool downLoadFlashFile(char *data, int nRdSize,unsigned int dwLoadAddr, unsigned int dwFlashAddr);
    bool UpgradeProcess(char * data, int nRdSize, unsigned int dwLoadAddr, unsigned int dwFlashAddr);
    int  construct_data(UPGRADE_CMD_TYPE_E cmd_type, char * buff, int len);
    int  parse_rxdata(unsigned char * buff, int len);
    
    void  state_parse(unsigned short ret);
    
    unsigned char m_rcvBuf[128];
	unsigned char m_version_get[256];
	
    UPGRADE_STATUS_E m_upgrade_status;
	int  m_send_complete_flag;
	int  m_reset_link_flag;
	int  m_version_got_flag;
	int  m_repeat_send_flag;
	int  m_send_total_byte;
	int  m_nFileSize;

	int  m_nack_count;
    //UPGRADE_STATUS_E m_upgrade_status;

    qint32 responseStatus;
    volatile bool timeoutStatus;
    void ctrlTracePrint(const QString &s);

signals:
    void ctrFlowTrace(const QString &s);
    void sendProgreassPos(qint32 pos);
    void updataProgressBar(qint32 pos,qint32 index);

public slots:
//        void serial_update();
        void showResponse(const QByteArray s,qint32 mode);
        void processError(const QString &s,qint32 mode);
        void processTimeout(const QString &s,qint32 mode);

private:
        QMutex hg_mutex;


};

#endif // HG_UPGRADEFUNC_H
