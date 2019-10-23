#include "upgrade_tool.h"
#include <QApplication>
#include <QMutex>
#include <QFile>
#include <QTextStream>
#include <QTime>
#include <QDateTime>

static QString timePoint;

void outputMessage(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    static QMutex mutex;
    mutex.lock();

    QString text;
    switch(type)
    {

    case QtInfoMsg:
        text = QString("Info:");
        break;
    case QtDebugMsg:
        text = QString("Debug:");
        break;

    case QtWarningMsg:
        text = QString("Warning:");
        break;

    case QtCriticalMsg:
        text = QString("Critical:");
        break;

    case QtFatalMsg:
        text = QString("Fatal:");
    }

    QString current_time = QTime::currentTime().toString("hh:mm:ss:zzz");
    QString message = QString("[%1]%2 %3").arg(current_time).arg(text).arg(msg);

    QFile file;
    QString path = QString("CSHG018-UPGRADE-TOOL%1.log").arg(timePoint);
    file.setFileName(path);
    file.open(QIODevice::WriteOnly | QIODevice::Append);
    QTextStream text_stream(&file);
    text_stream << message;
    file.flush();
    file.close();

    mutex.unlock();
}


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    upgrade_tool w;
    timePoint  = QDateTime::currentDateTime().toString("yyyyMMddHHmmss");
    qInstallMessageHandler(outputMessage);
    w.show();

    return a.exec();
}

