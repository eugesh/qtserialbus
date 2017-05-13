/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSerialBus module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include <QScopedPointer>
#include <QtSerialBus/qcanbus.h>
#include <QtSerialBus/qcanbusdevice.h>
#include <QtSerialBus/qcanbusdeviceinfo.h>
#include <QtSerialBus/qcanbusfactory.h>
#include <QtSerialBus/qcanbusframe.h>
#include <QtTest/QtTest>

class tst_QSerialBus : public QObject
{
    Q_OBJECT
public:
    explicit tst_QSerialBus() {}

    static void enterLoop(int secs)
    {
        ++loopLevel;
        QTestEventLoop::instance().enterLoop(secs);
        --loopLevel;
    }

    static void enterLoopMsecs(int msecs)
    {
        ++loopLevel;
        QTestEventLoop::instance().enterLoopMSecs(msecs);
        --loopLevel;
    }

    static void exitLoop()
    {
        if (loopLevel > 0)
            QTestEventLoop::instance().exitLoop();
    }

    static bool timeout()
    {
        return QTestEventLoop::instance().timeout();
    }

private slots:
    void initTestCase();
    void loopback();
    void loopback_data();

private:
    QString m_senderName, m_receiverName, m_pluginName;
    static int loopLevel;
};

int tst_QSerialBus::loopLevel = 0;

void tst_QSerialBus::initTestCase()
{
    qputenv("QTEST_FUNCTION_TIMEOUT", QByteArray("90000000"));
    m_senderName = QString::fromLocal8Bit(qgetenv("QTEST_SERIALBUS_SENDER"));
    m_receiverName = QString::fromLocal8Bit(qgetenv("QTEST_SERIALBUS_RECEIVER"));
    m_pluginName = QString::fromLocal8Bit(qgetenv("QTEST_SERIALBUS_PLUGIN_NAME"));

    if (m_senderName.isEmpty() || m_receiverName.isEmpty() || m_pluginName.isEmpty()) {
        static const char message[] =
              "Test doesn't work because the names of CAN ports aren't found in env.\n"
              "Please set environment variables:\n"
              " QTEST_SERIALBUS_SENDER to name of output CAN port\n"
              " QTEST_SERIALBUS_RECEIVER to name of input CAN port\n"
              " QTEST_SERIALBUS_PLUGIN_NAME to name CAN plugin (backend)\n"
              "Specify short names of port, like: vcan0\n"
              "Specify name of backend, like: socketcan\n";

        QSKIP(message);
    }
}

void tst_QSerialBus::loopback_data()
{
    QTest::addColumn<long int>("maxFramesCount");
    QTest::addColumn<int>("frameId");
    QTest::addColumn<int>("maxTimeout");
    QTest::newRow("100600 frames with 0x123 id and 10000 timeout") << 100600l << 0x123 << 10000;
}

void tst_QSerialBus::loopback()
{
    QFETCH(long int, maxFramesCount);
    QFETCH(int, frameId);
    QFETCH(int, maxTimeout);
    long int currentReadFrameNumber = 0;
    long int currentWriteFrameNumber = 0;
    QCanBusFrame frameW;
    frameW.setFrameId(quint32(frameId));
    bool is_error_state = false;

    QString errorString;

    QScopedPointer<QCanBusDevice> receiver(QCanBus::instance()->createDevice(m_pluginName, m_receiverName, &errorString));
    QScopedPointer<QCanBusDevice> sender(QCanBus::instance()->createDevice(m_pluginName, m_senderName, &errorString));

    QVERIFY(receiver && sender);
    QVERIFY(receiver->connectDevice());
    QVERIFY(sender->connectDevice());

    auto frameWriter = [&]() {
        if (currentWriteFrameNumber >= maxFramesCount)
            return;
        const auto payload = QByteArray::number(static_cast<int>(++currentWriteFrameNumber % 100), 16);
        frameW.setPayload(payload);
        sender->writeFrame(frameW);
    };

    auto errorHandler = [&](QCanBusDevice::CanBusError err) {
        is_error_state = true;
        if (err != QCanBusDevice::NoError) {
            exitLoop();
        }
        QVERIFY(err != QCanBusDevice::NoError);
    };

    connect(sender.get(), &QCanBusDevice::framesWritten, frameWriter);
    connect(sender.get(), &QCanBusDevice::errorOccurred, errorHandler);

    auto frameReader = [&]() {
        const auto frames = receiver->readAllFrames();
        int currentReadFrameNumberMod = 0;
        for (const auto &frame : frames) {
            QCOMPARE(frame.frameId(), frameId);
            bool *ok = nullptr;
            const int frameNumber = frame.payload().toInt(ok, 16);
            currentReadFrameNumberMod = ++currentReadFrameNumber % 100;
            if (frameNumber != currentReadFrameNumberMod)
                exitLoop();
            QCOMPARE(currentReadFrameNumberMod, frameNumber);
        }

        if (currentReadFrameNumber == maxFramesCount)
            exitLoop();
    };

    connect(receiver.get(), &QCanBusDevice::framesReceived, frameReader);
    connect(receiver.get(), &QCanBusDevice::errorOccurred, errorHandler);

    frameWriter();

    if (! is_error_state)
        enterLoop(maxTimeout);

    QVERIFY(currentWriteFrameNumber == currentReadFrameNumber);
}

QTEST_MAIN(tst_QSerialBus)

#include "tst_qserialbus.moc"
