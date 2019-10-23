#ifndef UPGRADE_TOOL_H
#define UPGRADE_TOOL_H

#include <QMainWindow>
#include "hg_upgradefile.h"
#include <QLineEdit>
#include <QProgressBar>
#include <QErrorMessage>

#define FF_PAGE_ALIGN_SIZE  256
#define FF_PLAIN_DATA_SIZE  (3*1024*1024)  //plain burn data buff max size set 3MB

namespace Ui {
class upgrade_tool;
}

class upgrade_tool : public QMainWindow
{
    Q_OBJECT

public:
    explicit upgrade_tool(QWidget *parent = nullptr);
    ~upgrade_tool();

    /*hdr size 256 bytes*/
typedef struct ff_hdr{
        quint32      size;
        quint32  	crc32;
        quint32  reserved[62];
}FF_HDR_T;

    /*hdr size 256 bytes*/
typedef struct ff_plain{
        quint8   data[FF_PLAIN_DATA_SIZE];
        quint32  	size;
}FF_PLAIN_T;


    bool serialStatusFlag = false; //false means close

    qint32 serial_init(void);
    qint32 serial_open(void);
    qint32 serial_close(void);

    qint32 serial_refresh(void);

    qint32 processBar_updata(QProgressBar* bar,qint32 value);

    quint32 get_path_from_sellect_file(QLineEdit *lineEdit);
    qint32 get_data_from_sellect_file(QString *f_path);
    /*return added hdr flash file*/
    qint32 bb_flash_file_create();
    qint32 bb_flash_file_burn();
    qint32 bb_burn_process();

    quint32 sys_burn_get_path_from_sellect_file(QLineEdit *lineEdit);

    QByteArray sys_burn_get_data_from_sellect_file(QString f_path);
    qint32 sys_burn_process();

public slots:
    void dumpCtrlFlowTrace(const QString &s);
    void dumpProgressPos(qint32 pos);

private slots:

    void on_COM_SET_INDEX_currentIndexChanged(const QString &arg1);

    void on_COM_SET_BR_currentIndexChanged(const QString &arg1);

    void on_COM_SET_REFRESH_clicked();

    void on_COM_SET_OPEN_clicked();

    void on_BBFW_BURN_TOOL_clicked();

    void on_BBFW_BURN_START_clicked();

    void on_SYS_BURN_MBR_TOOL_clicked();

    void on_SYS_BURN_AC_TOOL_clicked();

    void on_SYS_BURN_BL_TOOL_clicked();

    void on_SYS_BURN_BB_TOOL_clicked();

    void on_SYS_BURN_START_clicked();

private:
    Ui::upgrade_tool *ui;
    HG_UpgradeFile *upgrade_handler;
    QErrorMessage * errDlg_file;

    QByteArray hdrFile;
    QByteArray bbBinfile;
    QByteArray bbBurnfile;
};

#endif // UPGRADE_TOOL_H
