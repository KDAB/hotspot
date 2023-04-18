/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QVector>

enum class Direction
{
    Forward,
    Backward
};

template<typename entry, typename SearchFunc, typename EndReached>
int search(QVector<entry> source, SearchFunc&& searchFunc, EndReached&& endReached, Direction direction, int offset)
{
    if (offset > source.size() || offset < 0) {
        offset = 0;
    }

    auto start = direction == Direction::Forward
        ? (source.begin() + offset)
        : (source.end() - (source.size() - offset - 1)); // 1 one due to offset of the reverse iterator

    auto it = direction == Direction::Forward
        ? std::find_if(start, source.end(), searchFunc)
        : (std::find_if(std::make_reverse_iterator(start), source.rend(), searchFunc) + 1).base();

    // it is less than source.begin() if the backward search ends at source.rend()
    if (it >= source.begin() && it < source.end()) {
        auto distance = std::distance(source.begin(), it);
        return distance;
    }

    it = direction == Direction::Forward
        ? std::find_if(source.begin(), start, searchFunc)
        : (std::find_if(source.rbegin(), std::make_reverse_iterator(start), searchFunc) + 1).base();

    if (it != source.end()) {
        auto distance = std::distance(source.begin(), it);
        endReached();
        return distance;
    }

    return -1;
}
