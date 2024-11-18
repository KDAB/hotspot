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

        QCOMPARE(
            search(testArray.cbegin(), testArray.cend(), 0, Direction::Forward, [](int) { return false; }, [] {}), -1);
        QCOMPARE(
            search(testArray.cbegin(), testArray.cend(), 0, Direction::Backward, [](int) { return false; }, [] {}), -1);
    }
    void testSearch()
    {
        const std::array<int, 5> testArray = {1, 2, 3, 4, 5};

        int maxOffset = testArray.size() - 1;
        for (int offset = 0; offset < maxOffset; offset++) {
            QCOMPARE(search(
                         testArray.cbegin(), testArray.cend(), offset, Direction::Forward,
                         [](int num) { return num == 2; }, [] {}),
                     1);
            QCOMPARE(search(
                         testArray.cbegin(), testArray.cend(), offset, Direction::Backward,
                         [](int num) { return num == 2; }, [] {}),
                     1);
        }
    }

    void testEndReached()
    {
        const std::array<int, 5> testArray = {1, 2, 3, 4, 5};
        {
            bool endReached = false;
            QCOMPARE(search(
                         testArray.cbegin(), testArray.cend(), 1, Direction::Forward, [](int i) { return i == 1; },
                         [&endReached] { endReached = true; }),
                     0);
            QCOMPARE(endReached, true);
        }

        {
            bool endReached = false;
            QCOMPARE(search(
                         testArray.cbegin(), testArray.cend(), 1, Direction::Backward, [](int i) { return i == 4; },
                         [&endReached] { endReached = true; }),
                     3);
            QCOMPARE(endReached, true);
        }
    }

    void testArrayIsEmpty()
    {
        const std::array<int, 0> testArray;

        for (int i = 0; i < 2; i++) {
            QCOMPARE(search(
                         testArray.cbegin(), testArray.cend(), 0, Direction::Forward, [](int) { return true; }, [] {}),
                     -1);
        }
    }

    void testOutOfRangeIfCurrentIsEnd()
    {
        const std::array<int, 1> testArray = {0};

        QCOMPARE(search(
                     testArray.cbegin(), testArray.cend(), 1, Direction::Forward, [](int i) { return i == 0; }, [] {}),
                 0);
    }

    void testSearchOnIterators()
    {
        const std::array<int, 5> testArray = {0, 1, 2, 3, 0};

        for (std::size_t i = 0; i < testArray.size(); i++) {
            QCOMPARE(search(
                         testArray.cbegin() + 1, testArray.cbegin() + 3, i, Direction::Forward,
                         [](int i) { return i == 0; }, [] {}),
                     -1);
        }
    }
};

QTEST_GUILESS_MAIN(TestSearch)

#include "tst_search.moc"
