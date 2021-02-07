/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the QtSerialBus module.
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

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "connectdialog.h"
#include "receivedframesmodel.h"

#ifdef Q_OS_LINUX
#include <linux/can.h>
#else

#endif
#include <QCanBus>
#include <QCanBusFrame>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QLabel>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    m_ui(new Ui::MainWindow),
    m_busStatusTimer(new QTimer(this))
{
    m_ui->setupUi(this);

    m_connectDialog = new ConnectDialog;

    m_status = new QLabel;
    m_ui->statusBar->addPermanentWidget(m_status);

    m_written = new QLabel;
    m_ui->statusBar->addWidget(m_written);

    m_received = new QLabel;
    m_ui->statusBar->addWidget(m_received);

    m_model = new ReceivedFramesModel(this);
    m_ui->receivedFramesView->setModel(m_model);
    m_model->setQueueLimit(1000);
    // m_ui->receivedFramesView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    initActionsConnections();
    QTimer::singleShot(50, m_connectDialog, &ConnectDialog::show);

    connect(m_busStatusTimer, &QTimer::timeout, this, &MainWindow::busStatus);
    m_appendTimer = new QTimer;
    connect(m_appendTimer, &QTimer::timeout, this, &MainWindow::onAppendFramesTimeout);
    m_appendTimer->start(250);

    // Activity check
    m_sessionTimer = new QTimer;
    connect(m_sessionTimer, &QTimer::timeout, this, &MainWindow::onActivitiyTimeout);
    m_ui->activeSessionLabel->setText("Time spent: ");
    m_ui->activeSessionTime->setText("0 s, ");
    m_ui->bitrateIndicator->setText("0 kbit/s");
}

MainWindow::~MainWindow()
{
    delete m_connectDialog;
    delete m_ui;
}

void MainWindow::initActionsConnections()
{
    m_ui->actionDisconnect->setEnabled(false);
    m_ui->sendFrameBox->setEnabled(false);

    connect(m_ui->sendFrameBox, &SendFrameBox::sendFrame, this, &MainWindow::sendFrame);
    connect(m_ui->actionConnect, &QAction::triggered, [this]() {
        m_canDevice.release()->deleteLater();
        m_connectDialog->show();
    });
    connect(m_connectDialog, &QDialog::accepted, this, &MainWindow::connectDevice);
    connect(m_ui->actionDisconnect, &QAction::triggered, this, &MainWindow::disconnectDevice);
    connect(m_ui->actionResetController, &QAction::triggered, this, [this]() {
        m_canDevice->resetController();
    });
    connect(m_ui->actionQuit, &QAction::triggered, this, &QWidget::close);
    connect(m_ui->actionAboutQt, &QAction::triggered, qApp, &QApplication::aboutQt);
    connect(m_ui->actionClearLog, &QAction::triggered, m_model, &ReceivedFramesModel::clear);
    connect(m_ui->actionPluginDocumentation, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(QUrl("http://doc.qt.io/qt-5/qtcanbus-backends.html#can-bus-plugins"));
    });
}

void MainWindow::processErrors(QCanBusDevice::CanBusError error) const
{
    switch (error) {
    case QCanBusDevice::ReadError:
    case QCanBusDevice::WriteError:
    case QCanBusDevice::ConnectionError:
    case QCanBusDevice::ConfigurationError:
    case QCanBusDevice::UnknownError:
        m_status->setText(m_canDevice->errorString());
        break;
    default:
        break;
    }
}

void MainWindow::connectDevice()
{
    const ConnectDialog::Settings p = m_connectDialog->settings();

    QString errorString;
    m_canDevice.reset(QCanBus::instance()->createDevice(p.pluginName, p.deviceInterfaceName,
                                                        &errorString));
    if (!m_canDevice) {
        m_status->setText(tr("Error creating device '%1', reason: '%2'")
                          .arg(p.pluginName).arg(errorString));
        return;
    }

    m_numberFramesWritten = 0;

    connect(m_canDevice.get(), &QCanBusDevice::errorOccurred,
            this, &MainWindow::processErrors);
    connect(m_canDevice.get(), &QCanBusDevice::framesReceived,
            this, &MainWindow::processReceivedFrames);
    connect(m_canDevice.get(), &QCanBusDevice::framesWritten,
            this, &MainWindow::processFramesWritten);

    if (p.useConfigurationEnabled) {
        for (const ConnectDialog::ConfigurationItem &item : p.configurations)
            m_canDevice->setConfigurationParameter(item.first, item.second);
    }

    if (!m_canDevice->connectDevice()) {
        m_status->setText(tr("Connection error: %1").arg(m_canDevice->errorString()));

        m_canDevice.reset();
    } else {
        m_ui->actionConnect->setEnabled(false);
        m_ui->actionDisconnect->setEnabled(true);

        m_ui->sendFrameBox->setEnabled(true);

        const QVariant bitRate = m_canDevice->configurationParameter(QCanBusDevice::BitRateKey);
        if (bitRate.isValid()) {
            const bool isCanFd =
                    m_canDevice->configurationParameter(QCanBusDevice::CanFdKey).toBool();
            const QVariant dataBitRate =
                    m_canDevice->configurationParameter(QCanBusDevice::DataBitRateKey);
            if (isCanFd && dataBitRate.isValid()) {
                m_status->setText(tr("Plugin: %1, connected to %2 at %3 / %4 kBit/s")
                                  .arg(p.pluginName).arg(p.deviceInterfaceName)
                                  .arg(bitRate.toInt() / 1000).arg(dataBitRate.toInt() / 1000));
            } else {
                m_status->setText(tr("Plugin: %1, connected to %2 at %3 kBit/s")
                                  .arg(p.pluginName).arg(p.deviceInterfaceName)
                                  .arg(bitRate.toInt() / 1000));
            }
        } else {
            m_status->setText(tr("Plugin: %1, connected to %2")
                    .arg(p.pluginName).arg(p.deviceInterfaceName));
        }

        if (m_canDevice->hasBusStatus())
            m_busStatusTimer->start(2000);
        else
            m_ui->busStatus->setText(tr("No CAN bus status available."));
    }
}

void MainWindow::busStatus()
{
    if (!m_canDevice || !m_canDevice->hasBusStatus()) {
        m_ui->busStatus->setText(tr("No CAN bus status available."));
        m_busStatusTimer->stop();
        return;
    }

    switch (m_canDevice->busStatus()) {
    case QCanBusDevice::CanBusStatus::Good:
        m_ui->busStatus->setText("CAN bus status: Good.");
        break;
    case QCanBusDevice::CanBusStatus::Warning:
        m_ui->busStatus->setText("CAN bus status: Warning.");
        break;
    case QCanBusDevice::CanBusStatus::Error:
        m_ui->busStatus->setText("CAN bus status: Error.");
        break;
    case QCanBusDevice::CanBusStatus::BusOff:
        m_ui->busStatus->setText("CAN bus status: Bus Off.");
        break;
    default:
        m_ui->busStatus->setText("CAN bus status: Unknown.");
        break;
    }
}

void MainWindow::disconnectDevice()
{
    if (!m_canDevice)
        return;

    m_busStatusTimer->stop();

    m_canDevice->disconnectDevice();

    m_ui->actionConnect->setEnabled(true);
    m_ui->actionDisconnect->setEnabled(false);

    m_ui->sendFrameBox->setEnabled(false);

    m_status->setText(tr("Disconnected"));
}

void MainWindow::processFramesWritten(qint64 count)
{
    m_numberFramesWritten += count;
    m_written->setText(tr("%1 frames written").arg(m_numberFramesWritten));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_connectDialog->close();
    event->accept();
}

static QString frameFlags(const QCanBusFrame &frame)
{
    QString result = QLatin1String(" --- ");

    if (frame.hasBitrateSwitch())
        result[1] = QLatin1Char('B');
    if (frame.hasErrorStateIndicator())
        result[2] = QLatin1Char('E');
    if (frame.hasLocalEcho())
        result[3] = QLatin1Char('L');

    return result;
}

void MainWindow::processReceivedFrames()
{
    if (!m_canDevice)
        return;

    while (m_canDevice->framesAvailable()) {
        m_numberFramesReceived++;
        const QCanBusFrame frame = m_canDevice->readFrame();

        QString data;
        if (frame.frameType() == QCanBusFrame::ErrorFrame) {
            data = m_canDevice->interpretErrorFrame(frame);
        } else {
            data = QLatin1String(frame.payload().toHex(' ').toUpper());
        }

        const QString time = QString::fromLatin1("%1.%2  ")
                .arg(frame.timeStamp().seconds(), 10, 10, QLatin1Char(' '))
                .arg(frame.timeStamp().microSeconds() / 100, 4, 10, QLatin1Char('0'));

        const QString flags = frameFlags(frame);

        const char * const idFormat = frame.hasExtendedFrameFormat() ? "%08X" : "     %03X";
        const char * const dlcFormat = frame.hasFlexibleDataRateFormat() ? "%02d" : "%d";
        const QString id = QString::asprintf(idFormat, static_cast<uint>(frame.frameId()));
        const QString dlc = QString::asprintf(dlcFormat, frame.payload().size());

        m_framesAccumulator <<
           QStringList({QString::number(m_numberFramesReceived), time, flags, id, dlc, data});

        m_last_timestamp = frame.timeStamp().seconds();
        m_bytesCounter += frame.hasFlexibleDataRateFormat() ? CANFD_MTU : CAN_MTU;
    }

    if (!m_sessionTimer->isActive())
        m_sessionTimer->start(activityTimeout);
}

void MainWindow::sendFrame(const QCanBusFrame &frame) const
{
    if (!m_canDevice)
        return;

    m_canDevice->writeFrame(frame);
}

void MainWindow::onAppendFramesTimeout()
{
    if (m_framesAccumulator.count()) {
        m_model->appendFrames(m_framesAccumulator);
        m_framesAccumulator.clear();
        m_ui->receivedFramesView->scrollToBottom();
        m_received->setText(tr("%1 frames received").arg(m_numberFramesReceived));
    }
}

void MainWindow::onActivitiyTimeout() {
    static qint64 time = 0;
    const qint64 timeStamp = QDateTime::currentDateTime().toSecsSinceEpoch();

    if (qAbs(timeStamp - m_last_timestamp) > 1) {
        m_sessionTimer->stop();
        m_ui->bitrateIndicator->setText("0 kbit/s");
        m_bytesCounter = 0;
        return;
    }
    time++; // += static_cast<double>(activityTimeout) / 1000.0;
    m_ui->activeSessionTime->setText(QString("%1 s, ").arg(time));
    m_ui->bitrateIndicator->setText(QString("%1 kbit/s").arg(m_bytesCounter * 0.008)); // (8 / 1000)
    m_bytesCounter = 0;
}
