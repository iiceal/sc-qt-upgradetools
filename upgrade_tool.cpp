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

qint32 upgrade_tool::bb_flash_file_create()
{
// 1. 256 bytes align;
// 2. size of flash file to burn is header size + bb bin size + padding size
    quint32 plan_size; //the size of add padding to align 256b
    quint32 hdr_size;

    quint32 ff_crc32;

    if(bbBinfile.isEmpty())

    hdr_size = sizeof(FF_HDR_T);
    plan_size = bbBinfile.size();
    if(bbBinfile.size() % FF_PAGE_ALIGN_SIZE > 0)
    {
        plan_size += FF_PAGE_ALIGN_SIZE;
        plan_size &= ~(FF_PAGE_ALIGN_SIZE -1);
    }

//    hdrFile.data(0) = plan_size ;   //hdr->size stored the bb plan size(bin size + padding size)
    hdrFile.resize(256);
    memcpy(hdrFile.data(),&plan_size,sizeof(quint32));


    bbBurnfile.append(hdrFile);
    bbBurnfile.append(bbBinfile);
    bbBurnfile.resize(256 + plan_size);

//    memset(ff_burn->data,0xff,plan_size + hdr_size);
//    memcpy(ff_burn->data,hdr,hdr_size);
//    memcpy(ff_burn->data + hdr_size,bbBinFile->data,bbBinFile->size);

    ff_crc32 = CRC_32(0,(unsigned char*)bbBurnfile.data() + hdr_size,(plan_size - sizeof(quint32)));

//    ff_burn->size = plan_size + hdr_size;
//        memcpy(ff_burn->data + (ff_burn->size - sizeof(ff_crc32)),&ff_crc32,sizeof(ff_crc32));

//    memcpy((bbBurnfile.data()+ bbBurnfile.size() - sizeof (quint32)),&ff_crc32,sizeof(quint32));
    bbBurnfile.replace(bbBurnfile.size() - sizeof(quint32) - 1, sizeof(quint32), (char *)&ff_crc32);

    qDebug("hdr_size = %d, bb_bin_size = %d, plan_size = %d. bbBurnfile size = %d, bb_ff_crc = 0x%8d, sizeof quint32 = %d\n",
          hdrFile.size(),bbBinfile.size(),plan_size ,bbBurnfile.size(),ff_crc32, sizeof(quint32));
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
    if(false == upgrade_handler->downLoadFlashFile(bbBurnfile.data(),bbBurnfile.size(),
                                                   FlashFileLoadAddr,SPIFLASH_D_BASE + flashaddr))
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
    qDebug("create burn file done.burn start.");
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

    qDebug("flash file burn done!\n\n" );
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
