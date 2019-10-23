/****************************************************************************
**
** Copyright (C) 2012 Denis Shienkov <denis.shienkov@gmail.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSerialPort module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "masterthread.h"
#include "hg_aux.h"
#include <QSerialPort>
#include <QTime>

MasterThread::MasterThread(QObject *parent) :
    QThread(parent)
{
}

//! [0]
MasterThread::~MasterThread()
{
    m_mutex.lock();
    m_quit = true;
    m_cond.wakeOne();
    terminate();
    quit();
    m_mutex.unlock();
 //   wait();

}

//void MasterThread::transaction(QSerialPort *serialPort, int waitTimeout, char* data,qint32 len, qint32 mode)
void MasterThread::transaction(const QString &portName, const qint32 bondRate,int waitTimeout, char* data,qint32 len, qint32 mode)
{
    const QMutexLocker locker(&m_mutex);
    m_waitTimeout = waitTimeout;
    m_request_data = data;
    m_request_data_len = len;
    m_request_mode = mode;
    m_portBondRate = bondRate;

    m_portName = portName;
    if (!isRunning()){
        qDebug("Serial transfer Begin.Thread start.\n");
        qDebug("Serial PortName = %s, BondRate = %d\n",qPrintable(portName),bondRate);
        start();
    }else{
        qDebug("Serial transfer Begin.Thread wake.\n");
        qDebug("Serial PortName = %s, BondRate = %d\n",qPrintable(portName),bondRate);
        m_cond.wakeOne();
    }
}

void MasterThread::recieve(const QString &portName, const qint32 bondRate,qint32 TimeOut)
{
    const QMutexLocker locker(&m_mutex);
    m_portBondRate = bondRate;
    m_request_data_len = 0;
    m_portName = portName;
    m_waitTimeout = TimeOut;

    if (!isRunning()){
        m_quit = false;
        qDebug("Serial transfer Begin.Thread start.\n");
        qDebug("Serial PortName = %s, BondRate = %d\n",qPrintable(portName),bondRate);
        start(TimeCriticalPriority);
    }else{
        qDebug("Serial transfer Begin.Thread wake.\n");
        qDebug("Serial PortName = %s, BondRate = %d\n",qPrintable(portName),bondRate);
        m_cond.wakeOne();
    }
}

void MasterThread::run()
{
    bool currentPortNameChanged = false;
    QSerialPort serial;

    m_mutex.lock();
    if (m_portName.isEmpty()) {
        emit error(tr("No port name specified"),m_request_mode);
        return;
    }
    QString currentPortName;
    if (currentPortName != m_portName) {
        currentPortName = m_portName;
        currentPortNameChanged = true;
    }

    int currentWaitTimeout = m_waitTimeout;
    m_mutex.unlock();

    while (!m_quit) {
    // write request
        qDebug("Serial transfer running.\n");
        if (currentPortNameChanged) {
            serial.close();
            serial.setPortName(currentPortName);
            serial.setBaudRate(m_portBondRate);
            serial.setParity(QSerialPort::NoParity);
            serial.setDataBits(QSerialPort::Data8);
            serial.setStopBits(QSerialPort::OneStop);
            serial.setFlowControl(QSerialPort::NoFlowControl);

            if (!serial.open(QIODevice::ReadWrite)) {
                emit error(tr("Can't open %1, error code %2")
                           .arg(m_portName).arg(serial.error()),m_request_mode);
                return;
            }
        }

        serial.write(m_request_data,m_request_data_len);
        if (serial.waitForBytesWritten(currentWaitTimeout)) {
        qDebug("Serial transfer wrtie data done.len = %d,timeOut setting = %d.\n",m_request_data_len,currentWaitTimeout);
            // read response
            if (serial.waitForReadyRead(currentWaitTimeout)) {

                QByteArray responseData = serial.readAll();
                while (serial.waitForReadyRead(20))
                    responseData += serial.readAll();

                qDebug("3 ,response data size = %d\n",responseData.size());
                qDebug("Serial transfer read data done.len = %d,timeOut setting = %d.\n",serial.size(),currentWaitTimeout);
                emit this->response(responseData,m_request_mode);


            } else {
                emit timeout(tr("Wait read response timeout %1")
                             .arg(QTime::currentTime().toString()),m_request_mode);
            }

        } else {
            emit timeout(tr("Wait write request timeout %1")
                         .arg(QTime::currentTime().toString()),m_request_mode);
        }

        m_mutex.lock();
        qDebug("Serial transfer Done.Thread sleep.\n");
        m_cond.wait(&m_mutex);

        if (currentPortName != m_portName) {
            currentPortName = m_portName;
            currentPortNameChanged = true;
        } else {
            currentPortNameChanged = false;
        }
        currentWaitTimeout = m_waitTimeout;

        m_mutex.unlock();
    }

}
