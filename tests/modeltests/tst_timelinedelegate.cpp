/*
  tst_timelinedelegate.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017-2019 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Milian Wolff <milian.wolff@kdab.com>

  Licensees holding valid commercial KDAB Hotspot licenses may use this file in
  accordance with Hotspot Commercial License Agreement provided with the Software.

  Contact info@kdab.com if any conditions of this licensing are not clear to you.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

QTEST_GUILESS_MAIN(TestTimeLineDelegate);

#include "tst_timelinedelegate.moc"
