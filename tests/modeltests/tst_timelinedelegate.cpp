/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QDebug>
#include <QObject>
#include <QTest>
#include <QTextStream>

#include <models/timelinedelegate.h>

#include <cmath>

class TestTimeLineDelegate : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        qRegisterMetaType<TimeLineData>();
    }

    void testXMapping()
    {
        QFETCH(TimeLineData, data);
        QFETCH(int, x);
        QFETCH(quint64, time);
        QFETCH(Data::TimeRange, timeRange);

        // make sure the mapping is within a certain threshold
        const auto relativeErrorThreshold = 0.01; // 1%
        const auto timeErrorThreshold = relativeErrorThreshold * data.time.end;
        const auto xErrorThreshold = relativeErrorThreshold * data.w;
        auto validate = [&](quint64 value, quint64 expected, double threshold) {
            const auto actual = std::abs(static_cast<qint64>(value - expected));
            if (actual > threshold) {
                qWarning() << value << expected << actual << threshold;
                return false;
            }
            return true;
        };

        QVERIFY(validate(data.time.start, timeRange.start, timeErrorThreshold));
        QVERIFY(validate(data.time.end, timeRange.end, timeErrorThreshold));
        QVERIFY(validate(data.time.delta(), timeRange.delta(), timeErrorThreshold));

        QVERIFY(validate(data.mapXToTime(x), time, timeErrorThreshold));
        QVERIFY(validate(data.mapTimeToX(time), x, xErrorThreshold));
    }

    void testXMapping_data()
    {
        QTest::addColumn<TimeLineData>("data");
        QTest::addColumn<int>("x");
        QTest::addColumn<quint64>("time");
        QTest::addColumn<Data::TimeRange>("timeRange");

        QRect rect(0, 0, 1000, 10);
        auto time = Data::TimeRange(1000, 1000 + 10000);
        TimeLineData data({}, 0, time, time, rect);
        QCOMPARE(data.w, rect.width() - 2 * data.padding);
        QCOMPARE(data.h, rect.height() - 2 * data.padding);

        QTest::newRow("minTime") << data << 0 << time.start << time;
        QTest::newRow("halfTime") << data << (rect.width() / 2) << (time.start + time.delta() / 2) << time;
        QTest::newRow("maxTime") << data << rect.width() << time.end << time;

        // zoom into the 2nd half
        time.start = 6000;
        data.zoom(time);
        QTest::newRow("minTime_zoom_2nd_half") << data << 0 << time.start << time;
        QTest::newRow("halfTime_zoom_2nd_half")
            << data << (rect.width() / 2) << (time.start + time.delta() / 2) << time;
        QTest::newRow("maxTime_zoom_2nd_half") << data << rect.width() << time.end << time;

        // zoom into the 4th quadrant
        time.start = 8500;
        data.zoom(time);
        QTest::newRow("minTime_zoom_4th_quadrant") << data << 0 << time.start << time;
        QTest::newRow("halfTime_zoom_4th_quadrant")
            << data << (rect.width() / 2) << (time.start + time.delta() / 2) << time;
        QTest::newRow("maxTime_zoom_4th_quadrant") << data << rect.width() << time.end << time;
    }
};

QTEST_GUILESS_MAIN(TestTimeLineDelegate)

#include "tst_timelinedelegate.moc"
