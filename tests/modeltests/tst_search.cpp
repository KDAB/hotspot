/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <array>
#include <QObject>
#include <QTest>
#include <qtest.h>
#include <qtestcase.h>

#include <iterator>
#include <models/search.h>

class TestSearch : public QObject
{
    Q_OBJECT
private slots:
    void testSearchEmpty()
    {
        const std::array<int, 0> testArray = {};

        QCOMPARE(search_impl(
                     testArray.begin(), testArray.end(), testArray.begin(), [](int) { return false; }, [] {}),
                 -1);
        QCOMPARE(search_impl(
                     testArray.rbegin(), testArray.rend(), testArray.rbegin(), [](int) { return false; }, [] {}),
                 -1);
    }
    void testSearch()
    {
        const std::array<int, 5> testArray = {1, 2, 3, 4, 5};

        int maxOffset = testArray.size() - 1;
        for (int offset = 0; offset < maxOffset; offset++) {
            QCOMPARE(search_impl(
                         testArray.begin(), testArray.end(), testArray.begin() + offset,
                         [](int num) { return num == 2; }, [] {}),
                     1);
            QCOMPARE(search_impl(
                         testArray.rbegin(), testArray.rend(), testArray.rbegin() + offset,
                         [](int num) { return num == 2; }, [] {}),
                     3);
        }
    }

    void testEndReached()
    {
        const std::array<int, 5> testArray = {1, 2, 3, 4, 5};
        {
            bool endReached = false;
            QCOMPARE(search_impl(
                         testArray.begin(), testArray.end(), testArray.begin() + 1, [](int i) { return i == 1; },
                         [&endReached] { endReached = true; }),
                     0);
            QCOMPARE(endReached, true);
        }

        {
            bool endReached = false;
            QCOMPARE(search_impl(
                         testArray.rbegin(), testArray.rend(), std::make_reverse_iterator(testArray.begin() + 4 - 1),
                         [](int i) { return i == 4; }, [&endReached] { endReached = true; }),
                     1);
            QCOMPARE(endReached, true);
        }
    }

    void testWrapper()
    {
        const std::array<int, 5> testArray = {1, 2, 3, 4, 5};

        int maxOffset = testArray.size() - 1;
        for (int i = 0; i < maxOffset; i++) {
            QCOMPARE(search(
                         testArray, i, Direction::Forward, [](int i) { return i == 4; }, [] {}),
                     3);
            QCOMPARE(search(
                         testArray, i, Direction::Backward, [](int i) { return i == 4; }, [] {}),
                     3);
        }
    }
};

QTEST_GUILESS_MAIN(TestSearch)

#include "tst_search.moc"
