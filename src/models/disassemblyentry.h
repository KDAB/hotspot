/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "disassemblyoutput.h"
#include <iterator>
#include <QTextLine>
#include <QVector>

class DisassemblyEntry
{
public:
    DisassemblyEntry(DisassemblyEntry* parent, const DisassemblyOutput::DisassemblyLine& disassemblyLine = {},
                     QTextLine textLine = {});

    ~DisassemblyEntry() = default;

    DisassemblyEntry* parent() const
    {
        return m_parent;
    }

    int row() const
    {
        return m_row;
    }

    DisassemblyEntry* child(int row)
    {
        return &m_lines[row];
    }

    DisassemblyEntry* child(int row) const
    {
        return const_cast<DisassemblyEntry*>(&m_lines[row]);
    }

    DisassemblyEntry* lastChild();

    DisassemblyOutput::DisassemblyLine disassemblyLine() const
    {
        return m_disassemblyLine;
    }

    QTextLine textLine() const
    {
        return m_textLine;
    }

    int childCount() const
    {
        return m_lines.size();
    }

    void clear();
    void addChild(const DisassemblyEntry& line);

    bool operator==(const DisassemblyEntry& other) const;

    int findOffsetOf(const DisassemblyEntry* entry) const;

    // an iterator that will iterate over all children and grandchildren
    class TreeIterator : public std::iterator<std::forward_iterator_tag, DisassemblyEntry>
    {
    public:
        TreeIterator(const DisassemblyEntry* entry, int child);
        DisassemblyEntry& operator*() const;
        DisassemblyEntry* operator->() const;

        TreeIterator& operator++();
        TreeIterator operator++(int)
        {
            TreeIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const TreeIterator other) const;
        bool operator!=(const TreeIterator other) const
        {
            return !(*this == other);
        }

    private:
        DisassemblyEntry* m_entry = nullptr;
        int m_child = -1;
    };

    TreeIterator tree_begin() const;
    TreeIterator tree_end() const;

private:
    DisassemblyEntry* m_parent;
    QVector<DisassemblyEntry> m_lines;
    DisassemblyOutput::DisassemblyLine m_disassemblyLine;
    QTextLine m_textLine;
    int m_row;
};
