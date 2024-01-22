/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <array>
#include <QObject>
#include <QTest>

#include <iterator>
#include <models/search.h>

class TestSearch : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

private slots:
    void testSearchEmpty()
    {
        const std::array<int, 0> testArray = {};

        QCOMPARE(search(
                     testArray.begin(), testArray.end(), testArray.begin(), [](int) { return false; }, [] {}),
                 -1);
        QCOMPARE(search(
                     testArray.rbegin(), testArray.rend(), testArray.rbegin(), [](int) { return false; }, [] {}),
                 -1);
    }
    void testSearch()
    {
        const std::array<int, 5> testArray = {1, 2, 3, 4, 5};

        int maxOffset = testArray.size() - 1;
        for (int offset = 0; offset < maxOffset; offset++) {
            QCOMPARE(search(
                         testArray.begin(), testArray.end(), testArray.begin() + offset,
                         [](int num) { return num == 2; }, [] {}),
                     1);
            QCOMPARE(search(
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
            QCOMPARE(search(
                         testArray.begin(), testArray.end(), testArray.begin() + 1, [](int i) { return i == 1; },
                         [&endReached] { endReached = true; }),
                     0);
            QCOMPARE(endReached, true);
        }

        {
            bool endReached = false;
            QCOMPARE(search(
                         testArray.rbegin(), testArray.rend(), std::make_reverse_iterator(testArray.begin() + 4 - 1),
                         [](int i) { return i == 4; }, [&endReached] { endReached = true; }),
                     1);
            QCOMPARE(endReached, true);
        }
    }

    void testArrayIsEmpty()
    {
        const std::array<int, 0> testArray;

        for (int i = 0; i < 2; i++) {
            QCOMPARE(search(
                         testArray.cbegin(), testArray.cend(), testArray.cend(), [](int) { return true; }, [] {}),
                     -1);
        }
    }

    void testOutOfRangeIfCurrentIsEnd()
    {
        const std::array<int, 1> testArray = {0};

        QCOMPARE(search(
                     testArray.cbegin(), testArray.cend(), testArray.cbegin() + 1, [](int i) { return i == 0; }, [] {}),
                 0);
    }

    void testSearchOnIterators()
    {
        const std::array<int, 5> testArray = {0, 1, 2, 3, 0};

        for (int i = 1; i < 3; i++) {
            QCOMPARE(search(
                         testArray.cbegin() + 1, testArray.cbegin() + 3, testArray.cbegin() + i,
                         [](int i) { return i == 0; }, [] {}),
                     -1);
        }
    }
};

QTEST_GUILESS_MAIN(TestSearch)

#include "tst_search.moc"
