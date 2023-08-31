/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <algorithm>
#include <iterator>
#include <QVector>

enum class Direction
{
    Forward,
    Backward
};

/** a search function that wrap around at the end
 * this function evaluates searchFunc starting from current and returns the offset from begin. In case end is reached,
 * it calls endReached and starts again at begin
 *
 * return: offset from begin
 * */
template<typename iter, typename SearchFunc, typename EndReached>
int search_impl(iter begin, iter end, iter current, SearchFunc searchFunc, EndReached endReached)
{
    if (begin == end)
        return -1;

    auto start = current + 1;
    auto found = std::find_if(start, end, searchFunc);

    if (found != end) {
        return std::distance(begin, found);
    }

    endReached();

    found = std::find_if(begin, start, searchFunc);

    if (found != end) {
        return std::distance(begin, found);
    }

    return -1;
}

// a wrapper around search_impl that returns the result index from begin
template<typename Array, typename SearchFunc, typename EndReached>
int search(const Array& array, int current, Direction direction, SearchFunc searchFunc, EndReached endReached)
{
    current = std::clamp(0, static_cast<int>(array.size()), current);
    int resultIndex = 0;

    if (direction == Direction::Forward) {
        resultIndex = ::search_impl(array.begin(), array.end(), array.begin() + current, searchFunc, endReached);
    } else {
        resultIndex = ::search_impl(array.rbegin(), array.rend(),
                                    std::make_reverse_iterator(array.begin() + current) - 1, searchFunc, endReached);
        if (resultIndex != -1) {
            resultIndex = array.size() - resultIndex - 1;
        }
    }
    return resultIndex;
}
