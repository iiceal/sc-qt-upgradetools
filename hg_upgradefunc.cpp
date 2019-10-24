#include "hg_upgradefunc.h"
#include <QDebug>
#include <cstring>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QObject>

#include <QTime>
#include "masterthread.h"

QTime mtime;

HG_UpgradeFunc::HG_UpgradeFunc()
{
//    serial_init();
    QObject::connect(&m_thread, &MasterThread::response, this, &HG_UpgradeFunc::showResponse);
    QObject::connect(&m_thread, &MasterThread::error, this, &HG_UpgradeFunc::processError);
    QObject::connect(&m_thread, &MasterThread::timeout, this, &HG_UpgradeFunc::processTimeout);

}

HG_UpgradeFunc::~HG_UpgradeFunc()
{
//    serial_deinit();
    m_thread.terminate();
    m_thread.quit();
}

void HG_UpgradeFunc::showResponse(const QByteArray s, qint32 mode)
{
    hg_mutex.lock();
     qDebug("show Respons array size = %d, mode = %d\n",s.size(),mode);
    serialBuff.clear();
    if(mode == 1) //parse version
    {
        if(s.size() < 64) //parse error and continue
            responseStatus = 0;
        else {
            serialBuff = s;
            responseStatus = 1;
        }
    }

    if(mode == 0) // parse frame
    {
        if(s.size() < 10)
            responseStatus = 0;
        else {
            serialBuff = s;
            responseStatus = 1;
        }

    }

    qDebug("End == show Respons array size = %d, mode = %d response = %d\n",serialBuff.size(),mode,responseStatus);
    hg_mutex.unlock();
}

void HG_UpgradeFunc::processError(const QString &s,qint32 mode)
{
    hg_mutex.lock();
    responseStatus = 0;
    qDebug("mode = %d , %s\n",mode, qPrintable(s));
    qDebug("get emit error signal\n");
    hg_mutex.unlock();
}

void HG_UpgradeFunc::processTimeout(const QString &s,qint32 mode)
{
    hg_mutex.lock();
    responseStatus = 0;
    timeoutStatus = true;
    qDebug("mode = %d , %s\n",mode, qPrintable(s));
    qDebug("get emit timeout signal\n");
    hg_mutex.unlock();
}

void HG_UpgradeFunc::serial_config(QString name, QString bondrate)
{
    portName.clear();
    PortBondrate.clear();
    portName = name;
    PortBondrate = bondrate;
}

void HG_UpgradeFunc::reset_global_variable(void)
{
    m_send_complete_flag = 0;
    m_reset_link_flag = 0;
    m_version_got_flag = 0;
    m_repeat_send_flag = 0;
    m_upgrade_status = UPGRADE_STATUS_E_START;
    m_send_total_byte = 0;
    m_nFileSize = 0;
    m_nack_count = 0;
}

int HG_UpgradeFunc::construct_frame(UPGRADE_CMD_TYPE_E cmd_type, unsigned char * buff, int len)
{
	unsigned int crc_32;
	m_sendBuf[0] = UPGRADE_PROCESS_START_FLAG;
	m_sendBuf[1] = (cmd_type & 0xFF00) >> 8;
	m_sendBuf[2] = (cmd_type & 0x00FF);
	m_sendBuf[3] = (len & 0xFF00) >> 8;
	m_sendBuf[4] = (len & 0x00FF);

	if((NULL != buff) && (0 != len))
	{
		memcpy(&(m_sendBuf[5]), buff, len);
	}

	crc_32 = CRC_32(0, m_sendBuf, len + 5);
	m_sendBuf[len + 5] = (crc_32 & 0xFF000000) >> 24;
	m_sendBuf[len + 6] = (crc_32 & 0x00FF0000) >> 16;
	m_sendBuf[len + 7] = (crc_32 & 0x0000FF00) >> 8;
	m_sendBuf[len + 8] = (crc_32 & 0x000000FF) >> 0;

	m_sendBuf[len + 9] = UPGRADE_PROCESS_START_FLAG;

	return len + 10;
}


int HG_UpgradeFunc::connect_device(void)
{
    return construct_frame(UPGRADE_CMD_TYPE_E_CONNECT, NULL, 0);
}


int HG_UpgradeFunc::construct_start_data(unsigned int load_addr, unsigned int len)
{
	m_tmpBuf[0] = (load_addr & 0xff000000) >> 24;
	m_tmpBuf[1] = (load_addr & 0x00ff0000) >> 16;
	m_tmpBuf[2] = (load_addr & 0x0000ff00) >> 8;
	m_tmpBuf[3] = (load_addr & 0x000000ff);
	
	m_tmpBuf[4] = (len & 0xff000000) >> 24;
	m_tmpBuf[5] = (len & 0x00ff0000) >> 16;
	m_tmpBuf[6] = (len & 0x0000ff00) >> 8;
	m_tmpBuf[7] = (len & 0x000000ff);

	return construct_frame(UPGRADE_CMD_TYPE_E_START_DATA, m_tmpBuf, 8);
}

int HG_UpgradeFunc::construct_data_frame(UPGRADE_FRAME_PHASE_E data_phase, unsigned char * buff, unsigned int len)
{
	UPGRADE_CMD_TYPE_E cmd;
	if((UPGRADE_FRAME_PHASE_E_FIRST == data_phase) || (UPGRADE_FRAME_PHASE_E_MID == data_phase))
	{
		cmd = UPGRADE_CMD_TYPE_E_MIDST_DATA;
	}else if(UPGRADE_FRAME_PHASE_E_END == data_phase)
	{
		cmd = UPGRADE_CMD_TYPE_E_END_DATA;
	}else{
		return 0xFFFFFFFF;
	}

	return construct_frame(cmd, buff, len);
}

int HG_UpgradeFunc::construct_exec_frame(unsigned int exec_addr)
{
	m_tmpBuf[0] = (exec_addr & 0xff000000) >> 24;
	m_tmpBuf[1] = (exec_addr & 0x00ff0000) >> 16;
	m_tmpBuf[2] = (exec_addr & 0x0000ff00) >> 8;
	m_tmpBuf[3] = (exec_addr & 0x000000ff);

	return construct_frame(UPGRADE_CMD_TYPE_E_EXEC_DATA, m_tmpBuf, 4);
}

//UPGRADE_STATUS_E HG_UpgradeFunc::current_status()
//{
//    return m_upgrade_status;
//}

PARSE_RESULT_E HG_UpgradeFunc::parse_frame(unsigned char * buff, int len, unsigned short *ret)
{
	unsigned short cmd_type, data_len;
	unsigned char  err_no, cmd;
	unsigned int crc;

	*ret = 0xFFFF;
	// 1. 检查头和尾字符是否正确
	if((UPGRADE_PROCESS_START_FLAG == buff[0]) && (UPGRADE_PROCESS_START_FLAG == buff[len-1]))
	{
		err_no = buff[1];
		cmd = buff[2];
		cmd_type = (err_no << 8)|cmd;
		data_len = (buff[3] << 8)|buff[4];

		if(data_len + 10 == len)
		{
			//if(cmd_type <= UPGRADE_CMD_TYPE_E_ERR)
			if(cmd <= UPGRADE_CMD_TYPE_E_ERR)
			{
				crc = (buff[len - 5] << 24)|(buff[len - 4] << 16)|(buff[len - 3] << 8)|(buff[len - 2] << 0);

				// 校验CRC
				unsigned int crc_cal = CRC_32(0, buff, len - 5);
				//if(crc == CRC_32(0, buff, len - 5))
				if(crc == crc_cal)
				{
					*ret = cmd_type;
					return PARSE_RESULT_E_OK;
				}
			}else
			{
				return PARSE_RESULT_E_CMD_ERR;
			}
		}else
		{
			return PARSE_RESULT_E_LEN_ERR;
		}
	}else
	{
		// 解析失败/出错
		return PARSE_RESULT_E_HEADER_ERR;
	}
}

bool HG_UpgradeFunc::parse_version(unsigned char * rx_buff, int len, unsigned char *ver, int *ver_len)
{
	unsigned short ret = 0;
	if(PARSE_RESULT_E_OK != parse_frame(rx_buff, len, &ret))
	{
		return false;
	}

	if((ret & 0xFF) == UPGRADE_CMD_TYPE_E_VERSION)
	{
		*ver_len = (rx_buff[3] << 8)|rx_buff[4];
		memcpy(ver, &rx_buff[5], (*ver_len > 64? 64 : *ver_len));
		return true;
	}
    
    
	return false;
}

bool HG_UpgradeFunc::downProgramFile(char * data,int dateLen, unsigned int dwLoadAddr, unsigned int dwFlashAddr)
{
    bool ret;
    ctrlTracePrint(tr("*****Down Load Program File Begin: file size %1 (bytes), \nLoad Addr = 0x%2, \nFlash Addr = 0x%3\n\n")
                   .arg(dateLen).arg(dwLoadAddr,8,16).arg(dwFlashAddr,8,16));

    ret = UpgradeProcess(data,dateLen, dwLoadAddr, dwFlashAddr);

    if(ret == false ){
        ctrlTracePrint(tr("*****Down Load Program File Failed!!!!!!!!!!!!!")
                       .arg(dateLen).arg(dwLoadAddr,8,16).arg(dwFlashAddr,8,16));
    }else {
        ctrlTracePrint(tr("*****Down Load Program File Success: file size %1 (bytes), \nLoad Addr = 0x%2, \nFlash Addr = 0x%3")
                       .arg(dateLen).arg(dwLoadAddr,8,16).arg(dwFlashAddr,8,16));
    }

    return ret;
}

bool HG_UpgradeFunc::downLoadFlashFile(char * data,int dateLen, unsigned int dwLoadAddr, unsigned int dwFlashAddr)
{
    //128k is a block, limited by bootrom design and sram usage.
    int	nLoadSize = 1572864, offset, tmp_size;
    bool bRet = false;
    char *tmp_buf;
    int repeat_flag = 0;
    int try_count;

    offset = 0;
    tmp_buf = data;
    ctrlTracePrint(tr("*****Down Load Flash Begin: file size %1 (bytes), Load Addr = 0x%2, Flash Addr = 0x%3\n\n")
                   .arg(dateLen).arg(dwLoadAddr,8,16).arg(dwFlashAddr,8,16));

    while(dateLen > 0) {
        //flag_abort check loop1;
        if(true == flag_abort)
        {
            qDebug("Down load flash check flag abort true. abort now !!!!\n\n");
            return false;
        }
        if(repeat_flag == 0){
            try_count = 0;
            if(dateLen > nLoadSize)
                tmp_size = nLoadSize;
            else
                tmp_size = dateLen;
        }else {
            try_count ++;
            qDebug("try count = %d \n",try_count);
        }

        if(try_count > 10)
        {
            qDebug("Can not complete this transation, break!!!!\n");
            bRet = false;
            break;
        }
        qDebug("*****Down Load Flash:upgrade flash addr 0x%x\r\n", dwFlashAddr + offset);
        ctrlTracePrint(tr("Down Load Flash Processing: write %1 bytes to Flash Addr 0x%2 \n\n")
                       .arg(tmp_size).arg((dwFlashAddr+offset),8,16));

        bRet= UpgradeProcess(tmp_buf, tmp_size, dwLoadAddr, dwFlashAddr + offset);
        if(false == bRet)
        {
            qDebug("upgrade fail,repeat this transation.\r\n");
            repeat_flag = 1;
        }
        else
        {
            tmp_buf += tmp_size;
            offset += tmp_size;
            dateLen -= tmp_size;
            qDebug("DownLoadFlashFile --- %d\r\n", dateLen);
            repeat_flag = 0;
        }
    }
    ctrlTracePrint(tr("*****Down Load Flash Done: file size %1 (bytes), Load Addr = 0x%2, Flash Addr = 0x%3\n\n")
                   .arg(dateLen).arg(dwLoadAddr,8,16).arg(dwFlashAddr,8,16));

//    ctrlTracePrint(tr("\n\n"));
    qDebug("Down Load Flash Done:upgrade done,ret = %d.\r\n",bRet);
    return bRet;
}

bool HG_UpgradeFunc::UpgradeProcess(char *data, int nRdSize, unsigned int dwLoadAddr, unsigned int dwFlashAddr)
{
    qint32 tryCount = 0;
    unsigned int dwReadRet = 0,TX_PKG_MAX_SIZE = 0;
	int nFileSize;
	UPGRADE_STATUS_E loop_state = UPGRADE_STATUS_E_START;
    int i, rdlen, dlen, last_dlen = 0, default_wait_timeout = 1000, upgrade_exe_timeout;
    unsigned char * buff_r;
	unsigned short parse_ret;
	int ver_len;
	
	int left_size = nRdSize;
	int copy_size = 0;
    qDebug("\n");
    ctrlTracePrint(tr("******pgrade Process Start******\n"));

start_again:

    qDebug("bink debug nRdSize %d \n",nRdSize);
    buff_r = (unsigned char *)data;
	left_size = nRdSize;
	reset_global_variable();
	m_sendBuf[0] = UPGRADE_PROCESS_START_FLAG;
    while(1) {

        //flag abort check loop2.
        if(flag_abort == true)
        {
            qDebug("Connect borad abort.\n");
            return false;
        }
        tryCount ++;
        if(tryCount > 500)
        {
            qDebug("Try %d times to connect borad,no borad ack recv.\n",tryCount);
            ctrlTracePrint(tr("Connect borad failed.try_count = %1\n").arg(tryCount));
            return false;

        }
        responseStatus = -1;
        timeoutStatus = false;
        ctrlTracePrint(tr("Try to handshake with targe, try_count = %1").arg(tryCount));
        qDebug("Send handshake cmd to target.\n");
        m_thread.transaction(portName,PortBondrate.toInt(),default_wait_timeout,(char *)m_sendBuf,1,1);

        while((responseStatus == -1)&&(timeoutStatus == false))
            hg_msleep(1);

        if(timeoutStatus == true){
            qDebug("receive ack timeoutStatus = %d \n",timeoutStatus);
            continue;
        }
        if(responseStatus == 1)
        {
            if(parse_version((unsigned char * )serialBuff.data(), 64, m_version_get, &ver_len) != true){ //parse error and continue
                qDebug("parse_version failed.\n");
                continue;
            }
        }else if(responseStatus == 0) {
            qDebug("response failed.\n");
            continue;
        }

        qDebug("state: %d==>%d\r\n", loop_state, m_upgrade_status);
        m_upgrade_status = UPGRADE_STATUS_E_GOT_VERSION;
        loop_state = m_upgrade_status;
        ctrlTracePrint("Handshake to target Done.\n");
        break;
    }

    nFileSize = nRdSize;
    m_nFileSize = nFileSize;
    left_size = nRdSize;
    tryCount = 0;

    if(PortBondrate.toInt() < 921600){
        TX_PKG_MAX_SIZE = TX_PKG_MAX_SIZE_8K;
    }else {
        TX_PKG_MAX_SIZE = TX_PKG_MAX_SIZE_32K;
    }
    while(1) {

        //flag abort check loop3.
        if(flag_abort == true)
        {
            qDebug("Translate abort.\n");
            return false;
        }
        emit sendProgreassPos((qint32) get_progress_pos());

        //only try 400 times, if failed return false.
        tryCount ++;
        if(tryCount > 50){
            qDebug("Send data %d times ,still failed.\n",tryCount);
            ctrlTracePrint(tr("Send data too many times still failed, try_count = %1").arg(tryCount));
            return false;
        }

        memset (m_sendBuf, 0, TX_PKG_MAX_SIZE + 32);
        qDebug(" current status:%d \r\n", loop_state);

        switch(loop_state) {
        case UPGRADE_STATUS_E_GOT_VERSION:
            dlen = connect_device();
            break;

        case UPGRADE_STATUS_E_CONNECTED:
            dlen = construct_start_data(dwLoadAddr, nFileSize);
            qDebug(" dwLoadAddr:0x%x, len:0x%x \r\n", dwLoadAddr, nFileSize);
            break;

        case UPGRADE_STATUS_E_START_DATA:
        case UPGRADE_STATUS_E_MIDST_DATA:
            if(m_repeat_send_flag)
            {
                if(m_send_complete_flag == 1)
                    m_send_complete_flag = 0;
                m_send_total_byte -= last_dlen;
                buff_r -= last_dlen;  // buffer指针前移
                left_size += last_dlen;	// 剩余字节数更新，加上上次未成功发送的
                m_repeat_send_flag = 0;
                qDebug("repeat send packet, size = %d....", last_dlen);
            }
			
            // 拷贝数据到临时buffer
            if(left_size >= TX_PKG_MAX_SIZE){
                copy_size = TX_PKG_MAX_SIZE;
            }else{
                copy_size = left_size;
            }

            if(copy_size == left_size) {
                qDebug("Send buff end.\n");
                dlen = construct_data_frame(UPGRADE_FRAME_PHASE_E_END, buff_r, copy_size);
                m_send_complete_flag = 1;
            }else{
                dlen = construct_data_frame(UPGRADE_FRAME_PHASE_E_MID, buff_r, copy_size);
            }

            buff_r += copy_size;  // buffer指针后移
            left_size -= copy_size;	// 剩余字节数更新
            m_send_total_byte += copy_size;
            last_dlen = copy_size;
            qDebug("send_total_byte = %d, current packet size %d, left size %d\r\n",
                m_send_total_byte, copy_size,left_size);

            break;
        case UPGRADE_STATUS_E_END_DATA: //execute pkt
            qDebug("send UPGRADE_CMD_TYPE_E_EXEC_DATA, flash addr 0x%x\r\n", dwFlashAddr);
            dlen = construct_exec_frame(dwFlashAddr);
            if(nFileSize <= 128 * 1024)
                upgrade_exe_timeout = default_wait_timeout;
            else
                upgrade_exe_timeout = default_wait_timeout * (nFileSize / (128 * 1024) + 1);
            break;
        case UPGRADE_STATUS_E_EXEC_DATA:
            qDebug("upgrade  complete ########################\r\n");
            goto start_end;
        }

        if(m_repeat_send_flag)
        {
            m_repeat_send_flag = 0;
        }

        responseStatus = -1;
        timeoutStatus = false;

        ctrlTracePrint(tr("Send data to targe: size: %1").arg(dlen));
        serialBuff.clear();
        if(UPGRADE_STATUS_E_END_DATA != loop_state)
                m_thread.transaction(portName,PortBondrate.toInt(),default_wait_timeout,(char *)m_sendBuf,dlen,0);
        else
                m_thread.transaction(portName,PortBondrate.toInt(),upgrade_exe_timeout,(char *)m_sendBuf,dlen,0);

        while((responseStatus == -1)&& (timeoutStatus == false))
            hg_msleep(10);

        if(timeoutStatus == true)
            continue;

        if(responseStatus == 1){
            if(parse_frame((unsigned char*)serialBuff.data(), 10, &parse_ret) != PARSE_RESULT_E_OK)
                continue;
        }else{
            continue;
        }

        // 解析结果，修改状态
        state_parse(parse_ret);
        ctrlTracePrint(tr("process machine state: %1 ==> %2 ").arg(loop_state).arg(m_upgrade_status));
        qDebug("state: %d==>%d\r\n\n", loop_state, m_upgrade_status);
        loop_state = m_upgrade_status;
        if(UPGRADE_STATUS_E_START == loop_state){
            goto start_again;
        }

    }

start_end:
    ctrlTracePrint(tr("******Upgrade Process Complete******\n"));
	return true;
}

void HG_UpgradeFunc::state_parse(unsigned short parse_ret)
{

    unsigned char cmd_ret = (unsigned char)(parse_ret & 0xFF);
    unsigned char cmd_err_no = (unsigned char)((parse_ret & 0xFF00) >> 8);
    qDebug("state parse cmd ret = %d\n",cmd_ret);
    switch(cmd_ret)
    {
    case UPGRADE_CMD_TYPE_E_ACK:
        switch(m_upgrade_status)
        {
        case UPGRADE_STATUS_E_GOT_VERSION:
            m_upgrade_status = UPGRADE_STATUS_E_CONNECTED;
            break;

        case UPGRADE_STATUS_E_CONNECTED:
            m_upgrade_status = UPGRADE_STATUS_E_START_DATA;
            break;

        case UPGRADE_STATUS_E_START_DATA:
        case UPGRADE_STATUS_E_MIDST_DATA:
            if(m_send_complete_flag)
            {
                m_upgrade_status = UPGRADE_STATUS_E_END_DATA;
            }
            else
            {
                m_upgrade_status = UPGRADE_STATUS_E_MIDST_DATA;
            }

            break;

        case UPGRADE_STATUS_E_END_DATA:
            m_upgrade_status = UPGRADE_STATUS_E_EXEC_DATA;
            break;
        }
        break;

    case UPGRADE_CMD_TYPE_E_NACK:
        m_nack_count++;
        if((m_nack_count >= TRANS_NACK_CNT_MAX) || (NACK_RESET == cmd_err_no))
        {
            m_upgrade_status = UPGRADE_STATUS_E_START;
            reset_global_variable();
            qDebug("recv (%d)nack, err_no is (%d)\r\n", m_nack_count, cmd_err_no);
            qDebug("recv nack to much, restart~~~\r\n");
        }
        qDebug("recv (%d)nack, err_no is (%d)\r\n", m_nack_count, cmd_err_no);
        m_repeat_send_flag = 1;
        break;

    case UPGRADE_CMD_TYPE_E_ERR:
    default :
        m_reset_link_flag = 1;
        m_upgrade_status = UPGRADE_STATUS_E_START;
        reset_global_variable();
        break;
    }
}

void HG_UpgradeFunc::ctrlTracePrint(const QString &s)
{
    emit ctrFlowTrace(s);
}

int HG_UpgradeFunc::get_progress_pos()
{
    if(m_nFileSize == 0)
		return 0;
	return m_send_total_byte * 100 / m_nFileSize;
}
