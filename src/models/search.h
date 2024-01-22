/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <algorithm>
#include <iterator>

enum class Direction
{
    Forward,
    Backward
};

/** a search function that wraps around at the end
 * this function evaluates searchFunc starting from current and returns the offset from begin. In case end is reached,
 * it calls endReached and starts again at begin
 *
 * return: offset from begin
 * */
template<typename it, typename SearchFunc, typename EndReached>
int search_helper(const it begin, const it end, const it current, SearchFunc searchFunc, EndReached endReached)
{
    // if current points to the last line, current will now point to end -> wrap around
    const auto start = (current == end) ? begin : current;
    auto found = std::find_if(start, end, searchFunc);

    if (found != end) {
        return std::distance(begin, found);
    }

    endReached();

    found = std::find_if(begin, start, searchFunc);

    if (found != start) {
        return std::distance(begin, found);
    }

    return -1;
}

template<typename it, typename SearchFunc, typename EndReached>
int search(const it begin, const it end, int current, Direction direction, SearchFunc searchFunc, EndReached endReached)
{
    if (begin == end)
        return -1;

    const auto size = std::distance(begin, end);
    current = std::clamp(current, 0, static_cast<int>(size) - 1);
    const auto currentIt = begin + current;

    if (direction == Direction::Forward) {
        return search_helper(begin, end, std::next(currentIt), searchFunc, endReached);
    }

    int resultIndex = search_helper(std::make_reverse_iterator(end), std::make_reverse_iterator(begin),
                                    std::make_reverse_iterator(currentIt), searchFunc, endReached);
    return resultIndex != -1 ? (size - resultIndex - 1) : -1;
}
