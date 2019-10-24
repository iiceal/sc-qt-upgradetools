#include <stdio.h>
#include <stdlib.h>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QDebug>
#include <QDateTime>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QErrorMessage>

#include "upgrade_tool.h"
#include "ui_upgrade_tool.h"
#include "hg_aux.h"

upgrade_tool::upgrade_tool(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::upgrade_tool)
{
    ui->setupUi(this);
    upgrade_handler = new(HG_UpgradeFile);
    errDlg_file = new QErrorMessage(this);
    serial_init();
    connect(upgrade_handler->upgradeAux,&HG_UpgradeFunc::ctrFlowTrace,this,&upgrade_tool::dumpCtrlFlowTrace);
    connect(upgrade_handler->upgradeAux,&HG_UpgradeFunc::sendProgreassPos,this,&upgrade_tool::dumpProgressPos);
}

upgrade_tool::~upgrade_tool()
{
    delete ui;
    delete upgrade_handler;
    delete errDlg_file;
}

qint32 upgrade_tool::serial_init()
{
    ui->COM_SET_INDEX->clear();
    ui->COM_SET_BR->clear();

    foreach(const QSerialPortInfo &info,QSerialPortInfo::availablePorts())
    {
        qDebug()<<"Name:"<<info.portName() << "\n";
        qDebug()<<"Description:"<<info.description() << "\n";
        qDebug()<<"Manufacturer:"<<info.manufacturer() << "\n";

        QSerialPort serial222;
        serial222.setPort(info);
        if(serial222.open(QIODevice::ReadWrite))
        {
            ui->COM_SET_INDEX->addItem(serial222.portName());
            serial222.close();
        }
    }

    QStringList baudList;

    baudList <<"9600"<<"38400"<<"56000"<<"57600"<<"115200"<<"230400"<<"380400"<<"460800"<<"921600";
    ui->COM_SET_BR->addItems(baudList);
    ui->COM_SET_BR->setCurrentIndex(8);

    ui->COM_SET_INDEX->setCurrentIndex(0);

    return 0;

}

qint32 upgrade_tool::serial_open()
{
    QString serialName,serailBaudrate;
    serialName = ui->COM_SET_INDEX->currentText();
    serailBaudrate = ui->COM_SET_BR->currentText();

    qDebug("open serial done.serial name = %s, serial baudrate = %s.\n",
          qPrintable(serialName),qPrintable(serailBaudrate));
    upgrade_handler->serial_config_update(serialName,serailBaudrate);

    ui->COM_SET_BR->setEnabled(false);
    ui->COM_SET_INDEX->setEnabled(false);
    ui->COM_SET_REFRESH->setEnabled(false);

    return 0;
}

qint32 upgrade_tool::serial_close()
{
    ui->COM_SET_BR->setEnabled(true);
    ui->COM_SET_INDEX->setEnabled(true);
    ui->COM_SET_REFRESH->setEnabled(true);

    return 0;
}

qint32 upgrade_tool::serial_refresh()
{
    serial_init();
}

qint32 upgrade_tool::processBar_updata(QProgressBar *bar, qint32 value)
{
    if(value > 100)
    {
        qDebug("set value = %d is invalid, please check.\n\n" ,value);
        return -1;
    }
    bar->setValue(value);
    return 0;
}

quint32 upgrade_tool::get_path_from_sellect_file(QLineEdit *lineEdit)
{
    QString filter;
    QDir dir;

    QString selectedFilter;
    QFileDialog::Options options;

    QString path;

    filter.clear();
    filter = "Binary File (*.bin)";
    path = QFileDialog::getOpenFileName(this,"Get Program File","",filter,&selectedFilter,options);
//        path = QFileDialog::getOpenFileName(this,"Get Program File",dir.absolutePath(),filter,options);
    lineEdit->setText(path);
}

qint32 upgrade_tool::get_data_from_sellect_file(QString *f_path)
{
    qDebug("get_data_from_sellect_file :file dir = %s .\n",qPrintable(*f_path));
    if(f_path->isEmpty())
    {
//        qDebug("path is empty.please check.\n" );

        errDlg_file->showMessage(tr("Flash file not sellected."));
        return -1;
    }else {

        QFile file(*f_path);

        if(!file.open(QIODevice::ReadOnly))
        {
            qDebug("Flash file Open faile.please check.\n" );
            errDlg_file->showMessage(tr("Flash file Open failed.\n\n"));
            return -1;
        }

        file.seek(0);

        bbBinfile.clear();
        bbBinfile.append(file.readAll());
        qDebug("bbBinfile size = %d.\n" ,bbBinfile.size());
        file.flush();
        file.close();
    }

    qDebug("get data success!\n" );

    return 0;
}

qint32 upgrade_tool::flash_hdr_create(FF_HDR_T *hdr_data,quint32 plain_data_size)
{
    hdr_data->magic = 0x77667766;
    hdr_data->version = 0x10000;
    hdr_data->flags = 0x82; //verify disable,type 2;
    hdr_data->load_addr = 0x20000000;
    hdr_data->exec_addr = 0x20000000;
    hdr_data->flash_addr = 0x200;
    hdr_data->size = plain_data_size;
    hdr_data->crc16 = crc16((quint8 *)hdr_data, sizeof(FF_HDR_T) - 2);

    qDebug("Create Header:magic = 0x%8x, version = 0x%8x, flag = 0x%2x\nload_addr = 0x%8x, exec_addr = 0x%8x,flash_addr = 0x%8x,size = 0x%8x(%d KB)\n\n",
           hdr_data->magic,hdr_data->version,hdr_data->flags,hdr_data->load_addr,
           hdr_data->exec_addr,hdr_data->flash_addr,hdr_data->size,hdr_data->size/1024);
}

qint32 upgrade_tool::bb_flash_file_create()
{
// 1. 256 bytes align;
// 2. size of flash file to burn is header size + bb bin size + padding size
    quint32 plan_size; //the size of add padding to align 256b
    qint32 hdr_size;

    quint32 ff_crc32;
    FF_SUF_T suf_data;
    FF_HDR_T* hdr_data;
    quint8 bl2fw_hdr_dbuf[BL2FW_HDR_SIZE];

    if(bbBinfile.isEmpty())
        return -1;

    hdr_size = sizeof(FF_HDR_T);

    plan_size = bbBinfile.size();
    if(bbBinfile.size() % FF_PAGE_ALIGN_SIZE > 0)
    {
        plan_size += FF_PAGE_ALIGN_SIZE;
        plan_size &= ~(FF_PAGE_ALIGN_SIZE -1);
    }
    qDebug("CreateBBflashFile: origin bin size = 0x%x, after padding size = 0x%x\n\n",bbBinfile.size(),plan_size);

    memset(bl2fw_hdr_dbuf,0xff,BL2FW_HDR_SIZE);

    hdr_data = (FF_HDR_T*) bl2fw_hdr_dbuf;
    hdr_data->magic = 0x77667766;
    hdr_data->version = 0x10000;
    hdr_data->flags = 0x82; //verify disable,type 2;
    hdr_data->load_addr = 0x20000000;
    hdr_data->exec_addr = 0x20000000;
    hdr_data->flash_addr = BL2FW_HDR_SIZE;
    hdr_data->size = plan_size;
    hdr_data->crc16 = crc16((quint8 *)hdr_data, sizeof(FF_HDR_T) - 2);

    qDebug("Create Header:magic = 0x%8x, version = 0x%8x, flag = 0x%2x\nload_addr = 0x%8x, exec_addr = 0x%8x,flash_addr = 0x%8x,size = 0x%8x(%d KB)\nhdr_size = %d\n\n",
           hdr_data->magic,hdr_data->version,hdr_data->flags,hdr_data->load_addr,
           hdr_data->exec_addr,hdr_data->flash_addr,hdr_data->size,hdr_data->size/1024,sizeof(FF_HDR_T));

    bbBurnfile.append((char *)bl2fw_hdr_dbuf,BL2FW_HDR_SIZE);
    bbBurnfile.append(bbBinfile);
    bbBurnfile.resize(BL2FW_HDR_SIZE + plan_size);

    //crc for bin file
    ff_crc32 = CRC_32(0, (quint8 *)bbBurnfile.data() + BL2FW_HDR_SIZE,(plan_size - sizeof(FF_SUF_T)));

    suf_data.chipid = 0;
    suf_data.crc32 = CRC_32(ff_crc32,(quint8 *)&suf_data, 4);
    qDebug("1 bbBurnfile size = %d FF_SUR_T = %d.\n",bbBurnfile.size(),sizeof(FF_SUF_T));
    /* shit : replace 会导致 bbBurnfile size 少8；insert 会导致 bbBurnfile size 加8*/
//    bbBurnfile.replace(BL2FW_HDR_SIZE + plan_size - sizeof(FF_SUF_T), sizeof(FF_SUF_T), (char *)&suf_data);
//    bbBurnfile.insert(BL2FW_HDR_SIZE + plan_size - sizeof(FF_SUF_T), (char *)&suf_data, sizeof(FF_SUF_T));
    memcpy(bbBurnfile.data() +  BL2FW_HDR_SIZE + plan_size - sizeof(FF_SUF_T), (quint8 *)&suf_data,sizeof(FF_SUF_T));
    qDebug("size 1 = %d,size 2 = %d , size 3 = %d \n",
           BL2FW_HDR_SIZE + plan_size,BL2FW_HDR_SIZE + plan_size-sizeof(FF_SUF_T),bbBurnfile.size());

    qDebug("hdrf size = %d bb_bin_size = %d, plan_size = %d. bbBurnfile size = %d, bb_ff_crc = 0x%8x, sizeof quint32 = %d suf_crc32 = 0x%x suf_size = %d\n",
          BL2FW_HDR_SIZE,bbBinfile.size(),plan_size ,bbBurnfile.size(),ff_crc32, sizeof(quint32),suf_data.crc32,sizeof(FF_SUF_T));

    qDebug("ff_crc32 = 0x%x, dsuf_crc32 = 0x%x  \n\n",ff_crc32,suf_data.crc32);
    qDebug("Create success!\n");
    return 0;
}

qint32 upgrade_tool::bb_flash_file_burn()
{
    quint32 flashaddr;
    flashaddr = ui->FLASH_BURN_ADDR->text().toInt(0,16);

    if(bbBurnfile.isEmpty())
    {
        qDebug("flash file size = 0, please check.\n\n" );
        return -1;
    }
    upgrade_handler->currentUpgradeProcess(HG_UpgradeFile::CompatibilityMode,HG_UpgradeFile::BB_File);

    //bb flashfile sram buff address = 0x100000; Flash stored address 0xc0000;
    if(false == upgrade_handler->downLoadFlashFile(bbBurnfile.data(),
                                                   bbBurnfile.size(),
                                                   0x100000,                        //sram buff address
                                                   SPIFLASH_D_BASE + 0xc0000))      //flash stored address
    {
        qDebug("burn failed!\n");
        return -1;
    }

    qDebug("burn success!\n");
    return 0;
}

qint32 upgrade_tool::bb_burn_process()
{
    qDebug("start.\n\n" );
    qint32 ret;

    if(bbBinfile.isEmpty())
    {
        qDebug("point bb bin file is empty, please check.\n\n");
        errDlg_file->showMessage(tr("Bin file is empty."));
        return -1;
    }
    processBar_updata(ui->BBFW_BURN_PROBAR,0);
    ui->BBFW_BURN_DIR->setEnabled(false);
    ui->BBFW_BURN_TOOL->setEnabled(false);
    ui->BBFW_BURN_START->setEnabled(false);
//    ui->FLASH_BURN_ADDR->setEnabled(false);
//    ui->checkBox->setEnabled(false);

    bb_flash_file_create();
    qDebug("create burn file done.burn start...");
    ret = bb_flash_file_burn();
    if(ret != 0)
    {
        errDlg_file->showMessage(tr("Flash file burn failed.\n\n"));
        qDebug("flash file burn failed.ret = %d\n\n",ret);

    }else{
        errDlg_file->showMessage(tr("Flash file burn OK.\n\n"));
        qDebug("flash file burn success.ret = %d\n\n",ret);
    }

    ui->BBFW_BURN_DIR->setEnabled(true);
    ui->BBFW_BURN_TOOL->setEnabled(true);
    ui->BBFW_BURN_START->setEnabled(true);
//    ui->FLASH_BURN_ADDR->setEnabled(true);
//    ui->checkBox->setEnabled(true);

    qDebug("flash file burn done!!!\n\n" );
    return 0;
}

quint32 upgrade_tool::sys_burn_get_path_from_sellect_file(QLineEdit *lineEdit)
{
    QString filter;
    QDir dir;

    QString selectedFilter;
    QFileDialog::Options options;

    QString path;

    filter.clear();
    filter = "Imge File (*.img);;(*.*)";
    path = QFileDialog::getOpenFileName(this,"Get Program File","",filter,&selectedFilter,options);
//        path = QFileDialog::getOpenFileName(this,"Get Program File",dir.absolutePath(),filter,options);
    lineEdit->setText(path);
}

QByteArray upgrade_tool::sys_burn_get_data_from_sellect_file(QString f_path)
{
    QByteArray sysFlashFile;
    sysFlashFile.clear();
    if(f_path.isEmpty())
    {
//        qDebug("path is empty.please check.\n" );
//        errDlg_file->showMessage(tr("Flash file not sellected."));
        return sysFlashFile;
    }else {

        QFile file(f_path);

        if(!file.open(QIODevice::ReadOnly))
        {
            qDebug("Flash file Open faile.please check.\n" );
//            errDlg_file->showMessage(tr("Flash file Open failed.\n\n"));
            return sysFlashFile;
        }

        file.seek(0);

        sysFlashFile.clear();
        sysFlashFile.append(file.readAll());
        qDebug("bbBinfile size = %d.\n" ,sysFlashFile.size());
        file.flush();
        file.close();
    }

    qDebug("get data success!\n" );

    return sysFlashFile;
}

qint32 upgrade_tool::sys_burn_process()
{
    ui->SYS_BURN_AC_DIR->setEnabled(false);
    ui->SYS_BURN_MBR_DIR->setEnabled(false);
    ui->SYS_BURN_BL_DIR->setEnabled(false);
    ui->SYS_BURN_BB_DIR->setEnabled(false);
    ui->SYS_BURN_AC_TOOL->setEnabled(false);
    ui->SYS_BURN_MBR_TOOL->setEnabled(false);
    ui->SYS_BURN_BL_TOOL->setEnabled(false);
    ui->SYS_BURN_BB_TOOL->setEnabled(false);
    ui->SYS_BURN_START->setEnabled(false);

    upgrade_handler->upgradeAux->flag_abort = false;

    upgrade_handler->mbrByteArray = sys_burn_get_data_from_sellect_file(ui->SYS_BURN_MBR_DIR->text());
    if(upgrade_handler->mbrByteArray.isEmpty())
    {
        qDebug("get mbr flash file failed.\n");
    }
    upgrade_handler->authcodeByteArray = sys_burn_get_data_from_sellect_file(ui->SYS_BURN_AC_DIR->text());
    if(upgrade_handler->authcodeByteArray.isEmpty())
    {
        qDebug("get ac flash file failed.\n");

    }
    upgrade_handler->blByteArray = sys_burn_get_data_from_sellect_file(ui->SYS_BURN_BL_DIR->text());
    if(upgrade_handler->blByteArray.isEmpty())
    {
        qDebug("get bl flash file failed.\n");

    }
    upgrade_handler->bbByteArray = sys_burn_get_data_from_sellect_file(ui->SYS_BURN_BB_DIR->text());
    if(upgrade_handler->bbByteArray.isEmpty())
    {
        qDebug("get bb flash file failed.\n");
    }

    upgrade_handler->doLoadFlashFile(HG_UpgradeFile::SecureEncryptionMode);

    ui->SYS_BURN_AC_DIR->setEnabled(true);
    ui->SYS_BURN_MBR_DIR->setEnabled(true);
    ui->SYS_BURN_BL_DIR->setEnabled(true);
    ui->SYS_BURN_BB_DIR->setEnabled(true);
    ui->SYS_BURN_AC_TOOL->setEnabled(true);
    ui->SYS_BURN_MBR_TOOL->setEnabled(true);
    ui->SYS_BURN_BL_TOOL->setEnabled(true);
    ui->SYS_BURN_BB_TOOL->setEnabled(true);
    ui->SYS_BURN_START->setEnabled(true);

    errDlg_file->showMessage(tr("Flash File Burn Ok!\n\n"));

    return 0;
}

void upgrade_tool::dumpCtrlFlowTrace(const QString &s)
{
    qDebug("%s\n",qPrintable(s));
}

void upgrade_tool::dumpProgressPos(qint32 pos)
{
    qint32 mode,fileType;

    upgrade_handler->getCurrentUpgradeProgocess(&mode,&fileType);
    if(mode != HG_UpgradeFile::SecureEncryptionMode)
    {
        processBar_updata(ui->BBFW_BURN_PROBAR,pos);
    }else{
        switch(fileType)
        {
            case HG_UpgradeFile::MBR_File:
                processBar_updata(ui->SYS_BURN_MBR_PROBAR,pos);
                qDebug("Bink: set mbr progress bar pos = %d.\n",pos);
                break;
            case HG_UpgradeFile::AuthcodeFile:
                processBar_updata(ui->SYS_BURN_AC_PROBAR,pos);
                qDebug("Bink: set ac progress bar pos = %d.\n",pos);
                break;
            case HG_UpgradeFile::BL_File:
                processBar_updata(ui->SYS_BURN_BL_PROBAR,pos);
                qDebug("Bink: set bl progress bar pos = %d.\n",pos);
                break;
            case HG_UpgradeFile::BB_File:
                processBar_updata(ui->SYS_BURN_BB_PROBAR,pos);
                qDebug("Bink: set bb progress bar pos = %d.\n",pos);
                break;
            default:
            break;

        }
    }
    qDebug("Bink: set mode = %d, fileType = %d, pos = %d.\n",mode,fileType,pos);
}

void upgrade_tool::on_COM_SET_INDEX_currentIndexChanged(const QString &arg1)
{

}

void upgrade_tool::on_COM_SET_BR_currentIndexChanged(const QString &arg1)
{

}

void upgrade_tool::on_COM_SET_REFRESH_clicked()
{
    serial_refresh();
}

void upgrade_tool::on_COM_SET_OPEN_clicked()
{
    if(serialStatusFlag == false)
    {
        serial_open();
        ui->COM_SET_OPEN->setText("Close");
        serialStatusFlag = true;  //flag means serial opened;
    }else{
        serial_close();
        upgrade_handler->upgradeAux->m_thread.serialClose();
        ui->COM_SET_OPEN->setText("Open");
        serialStatusFlag = false;  //flag means serial opened;
        upgrade_handler->upgradeAux->flag_abort = true;
    }
}

void upgrade_tool::on_BBFW_BURN_TOOL_clicked()
{
    get_path_from_sellect_file(ui->BBFW_BURN_DIR);
    qDebug("file dir = %s .\n",qPrintable(ui->BBFW_BURN_DIR->text()));
}

void upgrade_tool::on_BBFW_BURN_START_clicked()
{
    QString filePath;
    qint32 ret;

    if(serialStatusFlag == false)
    {
        errDlg_file->showMessage(tr("Serial port is not opened."));
        return;
    }
    filePath = ui->BBFW_BURN_DIR->text();
    get_data_from_sellect_file(&filePath);
    upgrade_handler->upgradeAux->flag_abort = false;
    ret = bb_burn_process();

    return;

}

void upgrade_tool::on_SYS_BURN_MBR_TOOL_clicked()
{
    sys_burn_get_path_from_sellect_file(ui->SYS_BURN_MBR_DIR);
}

void upgrade_tool::on_SYS_BURN_AC_TOOL_clicked()
{
    sys_burn_get_path_from_sellect_file(ui->SYS_BURN_AC_DIR);
}

void upgrade_tool::on_SYS_BURN_BL_TOOL_clicked()
{
    sys_burn_get_path_from_sellect_file(ui->SYS_BURN_BL_DIR);
}

void upgrade_tool::on_SYS_BURN_BB_TOOL_clicked()
{
    sys_burn_get_path_from_sellect_file(ui->SYS_BURN_BB_DIR);
}

void upgrade_tool::on_SYS_BURN_START_clicked()
{
    if(serialStatusFlag == false)
    {
        errDlg_file->showMessage(tr("Serial port is not opened."));
//        qDebug("flash file burn failed. Serial port is not opened\n\n" );
        return;
    }
    sys_burn_process();
}

void upgrade_tool::on_SYS_BURN_ABORT_clicked()
{
    upgrade_handler->upgradeAux->flag_abort = true;
}

void upgrade_tool::on_BBFW_BURN_ABORT_clicked()
{
     upgrade_handler->upgradeAux->flag_abort = true;
}
