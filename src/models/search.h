/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <algorithm>
#include <iterator>

/** a search function that wrap around at the end
 * this function evaluates searchFunc starting from current and returns the offset from begin. In case end is reached,
 * it calls endReached and starts again at begin
 *
 * return: offset from begin
 * */

template<typename it, typename SearchFunc, typename EndReached>
int search(const it begin, const it end, const it current, SearchFunc searchFunc, EndReached endReached)
{
    if (begin == end)
        return -1;

    const auto start = (current == end) ? begin : (current + 1);
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
